#pragma once

#include <type_traits>

namespace oge
{
template <typename T>
    requires std::is_enum_v<T>
inline T operator|(T a, T b)
{
    return static_cast<T>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
template <typename T>
    requires std::is_enum_v<T>
inline T operator&(T a, T b)
{
    return static_cast<T>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
template <typename T>
    requires std::is_enum_v<T>
inline T& operator|=(T& a, T b)
{
    a = a | b;
    return a;
}

template <typename T>
    requires std::is_enum_v<T>
inline bool HasFlag(T value, T flag)
{
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}
}  // namespace oge