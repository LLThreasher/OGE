#include "oge/json.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace oge::json
{

namespace
{

// Convert a Value tree into a nlohmann::json tree (emission path).
nlohmann::json ToNlohmann(const Value& value)
{
    switch (value.index())
    {
        case 0:  // null
            return nullptr;
        case 1:  // bool
            return std::get<Bool>(value);
        case 2:  // int
            return std::get<Int>(value);
        case 3:  // uint
            return std::get<UInt>(value);
        case 4:  // float
            return std::get<Float>(value);
        case 5:  // string
            return std::get<Str>(value);
        case 6:  // array
        {
            nlohmann::json array = nlohmann::json::array();
            for (const auto& item : std::get<Array>(value))
                array.push_back(ToNlohmann(item));
            return array;
        }
        case 7:  // object
        {
            nlohmann::json object = nlohmann::json::object();
            for (const auto& [key, val] : std::get<Object>(value))
                object[key] = ToNlohmann(val);
            return object;
        }
    }
    return nullptr;  // unreachable — Value is a closed variant
}

// Convert a nlohmann::json tree into a Value tree (parse path).
Value FromNlohmann(const nlohmann::json& json)
{
    switch (json.type())
    {
        case nlohmann::json::value_t::null:
            return nullptr;
        case nlohmann::json::value_t::boolean:
            return json.get<Bool>();
        case nlohmann::json::value_t::number_integer:
        case nlohmann::json::value_t::number_unsigned:
            return static_cast<Int>(json.get<int64_t>());
        case nlohmann::json::value_t::number_float:
            return json.get<Float>();
        case nlohmann::json::value_t::string:
            return json.get<Str>();
        case nlohmann::json::value_t::array:
        {
            Array array;
            array.reserve(json.size());
            for (const auto& item : json)
                array.push_back(FromNlohmann(item));
            return array;
        }
        case nlohmann::json::value_t::object:
        {
            Object object;
            object.reserve(json.size());
            for (auto it = json.begin(); it != json.end(); ++it)
                object.emplace(it.key(), FromNlohmann(it.value()));
            return object;
        }
        default:
            throw std::runtime_error("JSON: unsupported value type");
    }
}

}  // namespace

void ToString(const Value& value, std::string& out)
{
    out = ToNlohmann(value).dump();
}

void FromString(const std::string& str, Value& out)
{
    nlohmann::json parsed;
    try
    {
        parsed = nlohmann::json::parse(str);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        throw std::runtime_error(std::string("JSON: ") + e.what());
    }
    out = FromNlohmann(parsed);
}

}  // namespace oge::core::json
