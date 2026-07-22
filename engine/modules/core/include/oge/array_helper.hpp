#pragma once

#include <array>

template <typename T, std::size_t N, typename... Args, std::size_t... I>
std::array<T, N> MakeArrayImpl(std::index_sequence<I...>, Args&&... args)
{
    return { ((void)I, T(std::forward<Args>(args)...))... };
}

template <typename T, std::size_t N, typename... Args>
std::array<T, N> MakeArray(Args&&... args)
{
    return MakeArrayImpl<T, N>(
        std::make_index_sequence<N>{},
        std::forward<Args>(args)...
    );
}
