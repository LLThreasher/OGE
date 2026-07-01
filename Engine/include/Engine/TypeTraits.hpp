#pragma once

template<class T, class U>
struct wider {
    using type = typename std::conditional<sizeof(T) >= sizeof(U), T, U>::type;
};
template<class T, class U>
using wider_t = typename wider<T, U>::type;
