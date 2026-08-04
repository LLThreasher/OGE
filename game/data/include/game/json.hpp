#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace game::json
{
using Bool = bool;
using Int = int64_t;
using Float = double;
using Str = std::string;

struct Value;

using Array = std::vector<Value>;
using Object = std::unordered_map<std::string, Value>;

struct Value
    : std::variant<std::nullptr_t, Bool, Int, Float, Str, Array, Object>
{
    using variant::variant;
};

template <typename T>
struct JsonTraits;

template <typename T>
struct AlwaysFalse : std::false_type
{
};

template <typename T>
inline constexpr bool AlwaysFalseV = AlwaysFalse<T>::value;

template <typename T>
Value ToJson(const T& value);

template <typename T>
void FromJson(const Value& json, T& value);

template <typename T>
T FromJson(const Value& json)
{
    T value{};
    FromJson(json, value);
    return value;
}

inline Value ToJson(const bool& value)
{
    return value;
}

inline void FromJson(const Value& json, bool& value)
{
    value = std::get<Bool>(json);
}

inline Value ToJson(const std::string& value)
{
    return value;
}

inline void FromJson(const Value& json, std::string& value)
{
    value = std::get<Str>(json);
}

inline Value ToJson(const std::pmr::string& value)
{
    return Str(value);
}

inline void FromJson(const Value& json, std::pmr::string& value)
{
    value = std::get<Str>(json);
}

inline Value ToJson(const char* value)
{
    return Str{value};
}

inline Value ToJson(const float& value)
{
    return static_cast<Float>(value);
}

inline void FromJson(const Value& json, float& value)
{
    value = static_cast<float>(std::get<Float>(json));
}

inline Value ToJson(const double& value)
{
    return static_cast<Float>(value);
}

inline void FromJson(const Value& json, double& value)
{
    value = std::get<Float>(json);
}

template <typename T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
Value ToJson(const T& value)
{
    return static_cast<Int>(value);
}

template <typename T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
void FromJson(const Value& json, T& value)
{
    value = static_cast<T>(std::get<Int>(json));
}

template <typename T>
Value ToJson(const std::vector<T>& values)
{
    Array array;
    array.reserve(values.size());

    for (const auto& value : values)
    {
        array.push_back(ToJson(value));
    }

    return array;
}

template <typename T>
void FromJson(const Value& json, std::vector<T>& values)
{
    const auto* array = std::get_if<Array>(&json);

    if (!array)
    {
        throw std::runtime_error("Expected JSON array");
    }

    values.clear();
    values.reserve(array->size());

    for (const auto& item : *array)
    {
        T value{};
        FromJson(item, value);
        values.push_back(std::move(value));
    }
}

template <typename T, size_t size>
Value ToJson(const std::array<T, size>& values)
{
    Array array;
    array.reserve(values.size());

    for (const auto& value : values)
    {
        array.push_back(ToJson(value));
    }

    return array;
}

template <typename T, size_t size>
void FromJson(const Value& json, std::array<T, size>& values)
{
    const auto* array = std::get_if<Array>(&json);

    if (!array)
    {
        throw std::runtime_error("Expected JSON array");
    }

    for (size_t i = 0; i < size; i++)
    {
        T value{};
        FromJson(array->at(i), value);
        values[i] = std::move(value);
    }
}

template <typename T>
Value ToJson(const std::pmr::vector<T>& values)
{
    Array array;
    array.reserve(values.size());

    for (const auto& value : values)
    {
        array.push_back(ToJson(value));
    }

    return array;
}

template <typename T>
void FromJson(const Value& json, std::pmr::vector<T>& values)
{
    const auto* array = std::get_if<Array>(&json);

    if (!array)
    {
        throw std::runtime_error("Expected JSON array");
    }

    values.clear();
    values.reserve(array->size());

    for (const auto& item : *array)
    {
        T value{};
        FromJson(item, value);
        values.push_back(std::move(value));
    }
}

template <typename T>
void FromJson(const Value& json, T& value)
{
    JsonTraits<std::remove_cvref_t<T>>::Deserialize(json, value);
}

template <typename T>
Value ToJson(const T& value)
{
    return JsonTraits<std::remove_cvref_t<T>>::Serialize(value);
}

template <typename T>
struct ObjectTraits
{
    static Value Serialize(const T& value)
    {
        Object object;

        JsonTraits<T>::VisitFields(
            value, [&](const char* name, const auto& field)
            { object.emplace(std::string{name}, ToJson(field)); });

        return object;
    }

    static void Deserialize(const Value& json, T& value)
    {
        const auto* object = std::get_if<Object>(&json);

        if (!object)
        {
            throw std::runtime_error("Expected JSON object");
        }

        JsonTraits<T>::VisitFields(value,
                                   [&](const char* name, auto& field)
                                   {
                                       auto it = object->find(name);
                                       if (it == object->end())
                                       {
                                           return;
                                       }

                                       FromJson(it->second, field);
                                   });
    }
};
}  // namespace game::json

#define DECL_JSON_OBJ(Type, BODY)                                            \
    template <>                                                              \
    struct ::game::json::JsonTraits<Type> : ::game::json::ObjectTraits<Type> \
    {                                                                        \
        template <typename F>                                                \
        static void VisitFields(Type& self, F&& visit)                       \
        {                                                                    \
            BODY                                                             \
        }                                                                    \
                                                                             \
        template <typename F>                                                \
        static void VisitFields(const Type& self, F&& visit)                 \
        {                                                                    \
            BODY                                                             \
        }                                                                    \
    };
