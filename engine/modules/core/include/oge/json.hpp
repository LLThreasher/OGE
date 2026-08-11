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

namespace oge::json
{
using Bool = bool;
using Int = int64_t;
using UInt = uint64_t;
using Float = double;
using Str = std::string;

struct Value;

using Array = std::vector<Value>;
using Object = std::unordered_map<std::string, Value>;

struct Value
    : std::variant<std::nullptr_t, Bool, Int, UInt, Float, Str, Array, Object>
{
    using variant::variant;

    Value(uint32_t v) : variant(UInt(v)) {}
    Value(uint64_t v) : variant(UInt(v)) {}
    Value(int64_t v) : variant(Int(v)) {}
};

void ToString(const Value& value, std::string& out);
void FromString(const std::string& str, Value& out);

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

// -- Primitives -----------------------------------------------------------

inline Value ToJson(const bool& value)
{
    return value;
}

inline void FromJson(const Value& json, bool& value)
{
    if (auto* p = std::get_if<Bool>(&json))
        value = *p;
    else
        throw std::runtime_error("JSON: expected bool");
}

inline Value ToJson(const std::string& value)
{
    return value;
}

inline void FromJson(const Value& json, std::string& value)
{
    if (auto* p = std::get_if<Str>(&json))
        value = *p;
    else
        throw std::runtime_error("JSON: expected string");
}

inline Value ToJson(const std::pmr::string& value)
{
    return Str(value);
}

inline void FromJson(const Value& json, std::pmr::string& value)
{
    if (auto* p = std::get_if<Str>(&json))
        value = p->c_str();
    else
        throw std::runtime_error("JSON: expected string");
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
    if (auto* p = std::get_if<Float>(&json))
        value = static_cast<float>(*p);
    else if (auto* p = std::get_if<Int>(&json))
        value = static_cast<float>(static_cast<int64_t>(*p));
    else
        throw std::runtime_error("JSON: expected number");
}

inline Value ToJson(const double& value)
{
    return static_cast<Float>(value);
}

inline void FromJson(const Value& json, double& value)
{
    if (auto* p = std::get_if<Float>(&json))
        value = *p;
    else if (auto* p = std::get_if<Int>(&json))
        value = static_cast<double>(*p);
    else
        throw std::runtime_error("JSON: expected number");
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
    if (auto* p = std::get_if<Int>(&json))
        value = static_cast<T>(*p);
    else
        throw std::runtime_error("JSON: expected integer");
}

// -- Arrays ---------------------------------------------------------------

template <typename T>
Value ToJson(const std::vector<T>& values)
{
    Array array;
    array.reserve(values.size());
    for (const auto& v : values) array.push_back(ToJson(v));
    return array;
}

template <typename T>
void FromJson(const Value& json, std::vector<T>& values)
{
    const auto* array = std::get_if<Array>(&json);
    if (!array) throw std::runtime_error("JSON: expected array");
    values.clear();
    values.reserve(array->size());
    for (const auto& item : *array)
    {
        T v{};
        FromJson(item, v);
        values.push_back(std::move(v));
    }
}

template <typename T, size_t N>
Value ToJson(const std::array<T, N>& values)
{
    Array array;
    array.reserve(values.size());
    for (const auto& v : values) array.push_back(ToJson(v));
    return array;
}

template <typename T, size_t N>
void FromJson(const Value& json, std::array<T, N>& values)
{
    const auto* array = std::get_if<Array>(&json);
    if (!array) throw std::runtime_error("JSON: expected array");
    if (array->size() < N) throw std::runtime_error("JSON: array too short");
    for (size_t i = 0; i < N; ++i)
    {
        T v{};
        FromJson((*array)[i], v);
        values[i] = std::move(v);
    }
}

template <typename T>
Value ToJson(const std::pmr::vector<T>& values)
{
    Array array;
    array.reserve(values.size());
    for (const auto& v : values) array.push_back(ToJson(v));
    return array;
}

template <typename T>
void FromJson(const Value& json, std::pmr::vector<T>& values)
{
    const auto* array = std::get_if<Array>(&json);
    if (!array) throw std::runtime_error("JSON: expected array");
    values.clear();
    values.reserve(array->size());
    for (const auto& item : *array)
    {
        T v{};
        FromJson(item, v);
        values.push_back(std::move(v));
    }
}

// -- Objects --------------------------------------------------------------

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
        if (!object) throw std::runtime_error("JSON: expected object");
        JsonTraits<T>::VisitFields(value,
                                   [&](const char* name, auto& field)
                                   {
                                       auto it = object->find(name);
                                       if (it != object->end())
                                           FromJson(it->second, field);
                                   });
    }
};
}  // namespace oge::json

#define DECL_JSON_OBJ(Type, BODY)                                          \
    template <>                                                            \
    struct ::oge::json::JsonTraits<Type> : ::oge::json::ObjectTraits<Type> \
    {                                                                      \
        template <typename F>                                              \
        static void VisitFields(Type& self, F&& visit)                     \
        {                                                                  \
            BODY                                                           \
        }                                                                  \
                                                                           \
        template <typename F>                                              \
        static void VisitFields(const Type& self, F&& visit)               \
        {                                                                  \
            BODY                                                           \
        }                                                                  \
    };
