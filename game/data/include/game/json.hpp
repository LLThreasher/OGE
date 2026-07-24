#pragma once

#include <unordered_map>
#include <vector>

namespace game::json
{
struct Value;

using Array = std::vector<Value>;
using Object = std::unordered_map<std::string, Value>;

struct Value : std::variant<std::nullptr_t, bool, int64_t, double, std::string,
                            Array, Object>
{
    using variant::variant;
};
}  // namespace game::json
