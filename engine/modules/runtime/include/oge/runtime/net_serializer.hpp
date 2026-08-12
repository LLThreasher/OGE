#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "oge/macros.hpp"

namespace oge::runtime::net
{
template <typename Impl>
class BufferTraits
{
   public:
    BufferTraits(std::span<std::byte> span) : data(span)
    {
    }

    void Align(size_t alignment)
    {
        size_t misalignment = writePos % alignment;
        if (misalignment != 0)
        {
            size_t padding = alignment - misalignment;
            static_cast<Impl*>(this)->EnsureCapacity(padding);
            std::memset(data.data() + writePos, 0, padding);
            writePos += padding;
        }
    }

    // -----------------------------
    // Writing
    // -----------------------------
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void Write(const T& value)
    {
        static_cast<Impl*>(this)->EnsureCapacity(sizeof(T));
        std::memcpy(data.data() + writePos, &value, sizeof(T));
        writePos += sizeof(T);
    }

    void WriteRaw(const void* src, size_t size)
    {
        static_cast<Impl*>(this)->EnsureCapacity(size);
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

    template <typename T>
    std::span<T> ReadNoCpy(size_t count)
    {
        size_t byteSize = count * sizeof(T);
        assert(readPos + byteSize <= writePos);
        auto* ptr = reinterpret_cast<T*>(data.data() + readPos);
        readPos += byteSize;
        return std::span<T>(ptr, count);
    }

    template <typename T, size_t N>
    std::span<T, N> ReadNoCpy()
    {
        constexpr size_t byteSize = N * sizeof(T);

        assert(readPos + byteSize <= writePos);
        assert(reinterpret_cast<uintptr_t>(data.data() + readPos) %
                   alignof(T) ==
               0);

        auto* ptr = reinterpret_cast<T*>(data.data() + readPos);

        readPos += byteSize;

        return std::span<T, N>{ptr, N};
    }

    // -----------------------------
    // State
    // -----------------------------
    Impl& ToReadOnly()
    {
        writePos = data.size();
        return *static_cast<Impl*>(this);
    }

    bool IsEmpty()
    {
        return readPos == writePos;
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

    size_t Size() const
    {
        return writePos;
    }
    size_t Capacity() const
    {
        return data.size();
    }

   protected:
    std::span<std::byte> data = {};

    size_t writePos = 0;
    size_t readPos = 0;
};

class Buffer : public BufferTraits<Buffer>
{
    using Base = BufferTraits<Buffer>;

   public:
    // -----------------------------
    // Non-owning constructors
    // -----------------------------
    Buffer(void* ptr, size_t len) : Base({static_cast<std::byte*>(ptr), len})
    {
    }

    Buffer(std::byte* ptr, size_t len) : Base({ptr, len})
    {
    }

    Buffer(std::span<std::byte> span) : Base(span)
    {
    }

    Buffer(std::vector<std::byte>& scratchpad)
        : Base(std::span<std::byte>()), scratch(&scratchpad)
    {
        // we just assume the scratchpad is already initalized full packed
        assert(scratch->size() == scratch->capacity());
    }

    void EnsureCapacity(size_t additional)
    {
        if (writePos + additional <= this->data.size()) return;

        // Cannot grow non-owning buffer
        assert(scratch != nullptr && "Attempting to grow non-owning Buffer");

        size_t newSize = std::max(this->data.size() * 2, writePos + additional);

        scratch->resize(newSize);
        this->data = *scratch;  // refresh span
    }

   private:
    std::vector<std::byte>* scratch = nullptr;
};

class BufferOutputArchive
{
   public:
    explicit BufferOutputArchive(Buffer& buf) : buffer(buf)
    {
    }

    template <typename T>
    void operator()(const T& value)
    {
        write(value);
    }

   private:
    Buffer& buffer;

    // ---------- POD ----------
    template <typename T>
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
    template <typename T>
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
            for (auto& v : vec) write(v);
        }
    }

    // ---------- std::unordered_map ----------
    template <typename K, typename V>
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
    explicit BufferInputArchive(Buffer& buf) : buffer(buf)
    {
    }

    template <typename T>
    void operator()(T& value)
    {
        read(value);
    }

   private:
    Buffer& buffer;

    // ---------- POD ----------
    template <typename T>
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
    template <typename T>
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
            for (auto& v : vec) read(v);
        }
    }

    // ---------- std::unordered_map ----------
    template <typename K, typename V>
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
}  // namespace oge::runtime::net
