#pragma once

template <class T, class U>
struct wider
{
    using type = typename std::conditional<sizeof(T) >= sizeof(U), T, U>::type;
};
template <class T, class U>
using wider_t = typename wider<T, U>::type;

template <class T>
using unsigned_t = std::make_unsigned_t<T>;

template <class T, class U>
using wider_unsigned_t = std::make_unsigned_t<wider_t<T, U>>;