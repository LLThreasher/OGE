#pragma once
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

#include "oge/macros.hpp"
#include "oge/math.hpp"

namespace oge::runtime::net
{
class Buffer
{
public:
    // -----------------------------
    // Owning constructor
    // -----------------------------
    explicit Buffer(size_t initialSize = 1024)
        : owned(initialSize),
          data(owned)
    {
        assert(false && "not allowed");
    }

    // -----------------------------
    // Non-owning constructors
    // -----------------------------
    Buffer(void* ptr, size_t len)
        : data(static_cast<std::byte*>(ptr), len)
    {}

    Buffer(std::byte* ptr, size_t len)
        : data(ptr, len)
    {}

    Buffer(std::span<std::byte> span)
        : data(span)
    {}

    // -----------------------------
    // Writing
    // -----------------------------
    template <typename T>
    requires std::is_trivially_copyable_v<T>
    void Write(const T& value)
    {
        EnsureCapacity(sizeof(T));
        std::memcpy(data.data() + writePos, &value, sizeof(T));
        writePos += sizeof(T);
    }

    void WriteRaw(const void* src, size_t size)
    {
        EnsureCapacity(size);
        std::memcpy(data.data() + writePos, src, size);
        writePos += size;
    }

    // -----------------------------
    // Reading
    // -----------------------------
    template <typename T>
    requires std::is_trivially_copyable_v<T>
    T Read()
    {
        T value;
        ReadRaw(&value, sizeof(T));
        return value;
    }

    template <typename T>
    void Read(T& out)
    {
        out = Read<T>();
    }

    void ReadRaw(void* dst, size_t size)
    {
        assert(readPos + size <= writePos);
        std::memcpy(dst, data.data() + readPos, size);
        readPos += size;
    }

    // -----------------------------
    // State
    // -----------------------------
    Buffer& ToReadOnly()
    {
        writePos = data.size();
        return *this;
    }

    void Reset()
    {
        writePos = 0;
        readPos = 0;
    }

    std::span<std::byte> Data() const
    {
        return data.subspan(0, writePos);
    }

    std::span<std::byte>& RawData()
    {
        return data;
    }

    size_t Size() const { return writePos; }
    size_t Capacity() const { return data.size(); }

private:
    void EnsureCapacity(size_t additional)
    {
        if (writePos + additional <= data.size())
            return;

        // Cannot grow non-owning buffer
        assert(!owned.empty() && "Attempting to grow non-owning Buffer");

        size_t newSize = std::max(
            data.size() * 2,
            writePos + additional
        );

        owned.resize(newSize);
        data = owned;  // refresh span
    }

private:
    std::vector<std::byte> owned;   // empty if non-owning
    std::span<std::byte> data;

    size_t writePos = 0;
    size_t readPos = 0;
};

class BufferOutputArchive
{
public:
    explicit BufferOutputArchive(Buffer& buf)
        : buffer(buf)
    {}

    template<typename T>
    void operator()(const T& value)
    {
        write(value);
    }

private:
    Buffer& buffer;

    // ---------- POD ----------
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void write(const T& value)
    {
        buffer.Write(value);
    }

    // ---------- std::string ----------
    void write(const std::string& str)
    {
        uint32_t size = static_cast<uint32_t>(str.size());
        buffer.Write(size);
        buffer.WriteRaw(str.data(), size);
    }

    // ---------- std::vector ----------
    template<typename T>
    void write(const std::vector<T>& vec)
    {
        uint32_t size = static_cast<uint32_t>(vec.size());
        buffer.Write(size);

        if constexpr (std::is_trivially_copyable_v<T>)
        {
            buffer.WriteRaw(vec.data(), sizeof(T) * size);
        }
        else
        {
            for (auto& v : vec)
                write(v);
        }
    }

    // ---------- std::unordered_map ----------
    template<typename K, typename V>
    void write(const std::unordered_map<K, V>& map)
    {
        uint32_t size = static_cast<uint32_t>(map.size());
        buffer.Write(size);

        for (auto& [k, v] : map)
        {
            write(k);
            write(v);
        }
    }
};

class BufferInputArchive
{
public:
    explicit BufferInputArchive(Buffer& buf)
        : buffer(buf)
    {}

    template<typename T>
    void operator()(T& value)
    {
        read(value);
    }

private:
    Buffer& buffer;

    // ---------- POD ----------
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void read(T& value)
    {
        buffer.ReadRaw(&value, sizeof(T));
    }

    // ---------- std::string ----------
    void read(std::string& str)
    {
        uint32_t size;
        buffer.ReadRaw(&size, sizeof(size));

        str.resize(size);
        buffer.ReadRaw(str.data(), size);
    }

    // ---------- std::vector ----------
    template<typename T>
    void read(std::vector<T>& vec)
    {
        uint32_t size;
        buffer.ReadRaw(&size, sizeof(size));

        vec.resize(size);

        if constexpr (std::is_trivially_copyable_v<T>)
        {
            buffer.ReadRaw(vec.data(), sizeof(T) * size);
        }
        else
        {
            for (auto& v : vec)
                read(v);
        }
    }

    // ---------- std::unordered_map ----------
    template<typename K, typename V>
    void read(std::unordered_map<K, V>& map)
    {
        uint32_t size;
        buffer.ReadRaw(&size, sizeof(size));

        map.clear();

        for (uint32_t i = 0; i < size; ++i)
        {
            K k;
            V v;
            read(k);
            read(v);
            map.emplace(std::move(k), std::move(v));
        }
    }
};

template <typename T>
struct SimpleNetValue
{
    T value;

    SimpleNetValue(T val = {}) : value(val)
    {
    }

    constexpr uint64_t Size() const
    {
        return sizeof(T);
    }

    void Serialize(Buffer& buffer)
    {
        buffer.Write<T>(value);
    }

    void Deserialize(Buffer& buffer)
    {
        value = buffer.Read<T>();
    }

    operator T&()
    {
        return value;
    }
    operator const T&() const
    {
        return value;
    }
};

using Int32 = SimpleNetValue<int32_t>;
using UInt8 = SimpleNetValue<uint8_t>;
using UInt32 = SimpleNetValue<uint32_t>;
using Single = SimpleNetValue<float>;
using Bool = SimpleNetValue<bool>;
using Vec2 = SimpleNetValue<math::vec2>;
using Vec3 = SimpleNetValue<math::vec3>;

template <typename Derived>
class Object
{
   public:
    constexpr uint64_t Size() const
    {
        uint64_t res = 0;
        Derived::VisitFields(*static_cast<const Derived*>(this),
                             [&](auto& field) { res += field.Size(); });
        return res;
    }

    void Serialize(Buffer& buffer)
    {
        Derived::VisitFields(*static_cast<Derived*>(this),
                             [&](auto& field) { field.Serialize(buffer); });
    }

    void Deserialize(Buffer& buffer)
    {
        Derived::VisitFields(*static_cast<Derived*>(this),
                             [&](auto& field) { field.Deserialize(buffer); });
    }
};

template <typename T>
struct List
{
    std::pmr::vector<T> data;

    constexpr uint64_t Size() const
    {
        uint64_t res = 0;
        for (const auto& val : data)
        {
            res += val.Size();
        }
        return res;
    }

    void Serialize(Buffer& buffer)
    {
        buffer.Write(data.size());
        for (auto& val : data)
        {
            val.Serialize(buffer);
        }
    }

    void Deserialize(Buffer& buffer)
    {
        size_t size = buffer.Read<size_t>();
        data.resize(size);
        for (auto& val : data)
        {
            val.Deserialize(buffer);
        }
    }

    auto begin()
    {
        return data.begin();
    }

    auto end()
    {
        return data.end();
    }

    auto begin() const
    {
        return data.begin();
    }

    auto end() const
    {
        return data.end();
    }

    void Add(T item)
    {
        data.push_back(item);
    }

    template <typename... Args>
    void EmplaceBack(Args&&... args)
    {
        data.emplace_back(args...);
    }

    bool empty()
    {
        return data.empty();
    }
};

#define NET_OBJ(Name) struct Name : public ::oge::runtime::net::Object<Name>
#define NET_OBJ_SIMPLE(Name) \
    struct Name : public ::oge::runtime::net::SimpleNetValue<Name>
#define NET_OBJ_FN                    \
    template <typename T, typename F> \
    static void VisitFields(T& self, F&& visit)

}  // namespace oge::runtime::net
