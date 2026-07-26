#pragma once
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

#include "oge/macros.hpp"
#include "oge/math.hpp"

namespace oge::runtime::net
{

class Buffer
{
   public:
    Buffer(void* ptr, size_t len) : data({static_cast<std::byte*>(ptr), len}) {}
    Buffer(std::byte* ptr, size_t len) : data({ptr, len}) {}
    Buffer(std::span<std::byte> data) : data(data) {}

    Buffer& ToReadOnly()
    {
        writePos = data.size();
        return *this;
    }

    template <typename T>
    void Write(const T& value)
    {
        assert(writePos + sizeof(T) <= data.size());

        std::memcpy(&data[0] + writePos, &value, sizeof(T));
        writePos += sizeof(T);
    }

    template <typename T>
    T Read()
    {
        T value;
        ReadRaw(&value, sizeof(T));
        return value;
    }

    void ReadRaw(void* dst, size_t size)
    {
        assert(readPos + size <= writePos);
        std::memcpy(dst, &data[0] + readPos, size);
        readPos += size;
    }

    template <typename T>
    T ReadAndDeserialize()
    {
        T value;
        value.Deserialize(*this);
        return std::move(value);
    }

    void Reset()
    {
        writePos = 0;
        readPos = 0;
    }

    std::span<std::byte>& RawData() { return data; }

    const std::span<std::byte> Data() const
    {
        return data.subspan(0, writePos);
    }

   private:
    std::span<std::byte> data;
    size_t writePos = 0;
    size_t readPos = 0;
};

template <typename T>
struct SimpleNetValue
{
    T value;

    SimpleNetValue(T val = {}) : value(val) {}

    constexpr uint64_t Size() { return sizeof(T); }

    void Serialize(Buffer& buffer) { buffer.Write<T>(value); }

    void Deserialize(Buffer& buffer) { value = buffer.Read<T>(); }

    operator T&() { return value; }
    operator const T&() const { return value; }
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
    constexpr uint64_t Size()
    {
        uint64_t res = 0;
        static_cast<Derived*>(this)->VisitFields([&](auto& field)
                                                 { res += field.Size(); });
        return res;
    }

    void Serialize(Buffer& buffer)
    {
        static_cast<Derived*>(this)->VisitFields([&](auto& field)
                                                 { field.Serialize(buffer); });
    }

    void Deserialize(Buffer& buffer)
    {
        static_cast<Derived*>(this)->VisitFields(
            [&](auto& field) { field.Deserialize(buffer); });
    }
};

template <typename T>
struct List : Object<List<T>>
{
    std::pmr::vector<T> data;

    void Serialize(Buffer& buffer)
    {
        buffer.Write(data.size());
        for (const auto& val : data)
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

    auto begin() { return data.begin(); }

    auto end() { return data.end(); }

    auto begin() const { return data.begin(); }

    auto end() const { return data.end(); }

    void Add(T&& item) { data.push_back(item); }
};

#define NET_OBJ(Name) struct Name : public ::oge::runtime::net::Object<Name>
#define NET_OBJ_SIMPLE(Name) \
    struct Name : public ::oge::runtime::net::SimpleNetValue<Name>
#define NET_OBJ_FN        \
    template <typename F> \
    void VisitFields(F&& visit)

}  // namespace oge::runtime::net
