#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include "oge/math.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace oge::runtime::net
{
template <typename T>
struct NetTraits;

template <typename T>
concept HasNetTraits = requires(T& value, const T& constValue, Buffer& buffer) {
    typename NetTraits<std::remove_cvref_t<T>>;

    {
        NetTraits<std::remove_cvref_t<T>>::Size(constValue)
    } -> std::convertible_to<uint64_t>;

    { NetTraits<std::remove_cvref_t<T>>::Serialize(buffer, constValue) };

    { NetTraits<std::remove_cvref_t<T>>::Deserialize(buffer, value) };
};

template <typename T>
uint64_t Size(const T& value)
{
    return NetTraits<std::remove_cvref_t<T>>::Size(value);
}

template <typename T>
void Serialize(Buffer& buffer, const T& value)
{
    NetTraits<std::remove_cvref_t<T>>::Serialize(buffer, value);
}

template <typename T>
void Deserialize(Buffer& buffer, T& value)
{
    NetTraits<std::remove_cvref_t<T>>::Deserialize(buffer, value);
}

template <typename T>
struct SimpleValueTraits
{
    static constexpr uint64_t Size(const T&)
    {
        return sizeof(T);
    }

    static void Serialize(Buffer& buffer, const T& value)
    {
        buffer.Write<T>(value);
    }

    static void Deserialize(Buffer& buffer, T& value)
    {
        value = buffer.Read<T>();
    }
};

template <typename T>
concept FixedWidthInteger =
    std::same_as<T, int8_t>  ||
    std::same_as<T, uint8_t> ||
    std::same_as<T, int16_t> ||
    std::same_as<T, uint16_t> ||
    std::same_as<T, int32_t> ||
    std::same_as<T, uint32_t> ||
    std::same_as<T, int64_t> ||
    std::same_as<T, uint64_t>;

template <FixedWidthInteger T>
struct NetTraits<T> : SimpleValueTraits<T>
{
};

template <>
struct NetTraits<float> : SimpleValueTraits<float>
{
};

template <>
struct NetTraits<bool> : SimpleValueTraits<bool>
{
};

template <>
struct NetTraits<math::vec2>
{
    static constexpr uint64_t Size(const math::vec2&)
    {
        return sizeof(float) * 2;
    }

    static void Serialize(Buffer& buffer, const math::vec2& value)
    {
        buffer.Write<float>(value.x);
        buffer.Write<float>(value.y);
    }

    static void Deserialize(Buffer& buffer, math::vec2& value)
    {
        value.x = buffer.Read<float>();
        value.y = buffer.Read<float>();
    }
};

template <>
struct NetTraits<math::vec3>
{
    static constexpr uint64_t Size(const math::vec3&)
    {
        return sizeof(float) * 3;
    }

    static void Serialize(Buffer& buffer, const math::vec3& value)
    {
        buffer.Write<float>(value.x);
        buffer.Write<float>(value.y);
        buffer.Write<float>(value.z);
    }

    static void Deserialize(Buffer& buffer, math::vec3& value)
    {
        value.x = buffer.Read<float>();
        value.y = buffer.Read<float>();
        value.z = buffer.Read<float>();
    }
};

template <typename T>
struct ObjectTraits
{
    static uint64_t Size(const T& value)
    {
        uint64_t result = 0;

        NetTraits<T>::VisitFields(
            value, [&](const auto& field) { result += net::Size(field); });

        return result;
    }

    static void Serialize(Buffer& buffer, const T& value)
    {
        NetTraits<T>::VisitFields(
            value, [&](const auto& field) { net::Serialize(buffer, field); });
    }

    static void Deserialize(Buffer& buffer, T& value)
    {
        NetTraits<T>::VisitFields(
            value, [&](auto& field) { net::Deserialize(buffer, field); });
    }
};

#define DECL_NET_OBJ(Type, BODY)                             \
    template <>                                              \
    struct ::oge::runtime::net::NetTraits<Type>              \
        : ::oge::runtime::net::ObjectTraits<Type>            \
    {                                                        \
        template <typename F>                                \
        static void VisitFields(Type& self, F&& visit)       \
        {                                                    \
            BODY                                             \
        }                                                    \
                                                             \
        template <typename F>                                \
        static void VisitFields(const Type& self, F&& visit) \
        {                                                    \
            BODY                                             \
        }                                                    \
    };

#define DECL_NET_OBJ_PACKED(Type, Packed, ToPacked, FromPacked)  \
    template <>                                                  \
    struct ::oge::runtime::net::NetTraits<Type>                  \
    {                                                            \
        static uint64_t Size(const Type& value)                  \
        {                                                        \
            return net::Size(ToPacked(value));                   \
        }                                                        \
                                                                 \
        static void Serialize(Buffer& buffer, const Type& value) \
        {                                                        \
            Packed packed = ToPacked(value);                     \
            net::Serialize(buffer, packed);                      \
        }                                                        \
                                                                 \
        static void Deserialize(Buffer& buffer, Type& value)     \
        {                                                        \
            Packed packed;                                       \
            net::Deserialize(buffer, packed);                    \
            value = FromPacked(packed);                          \
        }                                                        \
    };

template <typename T>
struct FixedSpanTraits
{
    static uint64_t Size(const T& list)
    {
        uint64_t result = 0;

        for (const auto& element : list)
        {
            result += net::Size(element);
        }

        return result;
    }

    static void Serialize(Buffer& buffer, const T& list)
    {
        for (const auto& element : list)
        {
            net::Serialize(buffer, element);
        }
    }

    static void Deserialize(Buffer& buffer, T& list)
    {
        for (auto& element : list)
        {
            net::Deserialize(buffer, element);
        }
    }
};

template <typename T, size_t size>
struct NetTraits<std::array<T, size>> : FixedSpanTraits<std::array<T, size>>
{
};

template <typename T>
struct DynListTraits
{
    static uint64_t Size(const T& list)
    {
        uint64_t result = sizeof(uint32_t);

        for (const auto& element : list)
        {
            result += net::Size(element);
        }

        return result;
    }

    static void Serialize(Buffer& buffer, const T& list)
    {
        uint32_t count = static_cast<uint32_t>(list.size());
        buffer.Write<uint32_t>(count);

        for (const auto& element : list)
        {
            net::Serialize(buffer, element);
        }
    }

    static void Deserialize(Buffer& buffer, T& list)
    {
        uint32_t count = buffer.Read<uint32_t>();

        list.resize(count);

        for (auto& element : list)
        {
            net::Deserialize(buffer, element);
        }
    }
};

template <typename T>
struct NetTraits<std::pmr::vector<T>> : DynListTraits<std::pmr::vector<T>>
{
};

template <typename T>
struct NetTraits<std::vector<T>> : DynListTraits<std::vector<T>>
{
};

}  // namespace oge::runtime::net
