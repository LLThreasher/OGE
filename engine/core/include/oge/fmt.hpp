#pragma once

#include <format>

#include "oge/math.hpp"
#include "oge/point2.hpp"
#include "oge/point3.hpp"

namespace std
{
template <>
struct formatter<oge::math::vec2>
{
    template <class Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <class Context>
    auto format(const oge::math::vec2& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};
template <>
struct formatter<oge::math::vec3>
{
    template <class Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <class Context>
    auto format(const oge::math::vec3& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {}, {})", p.x, p.y, p.z);
    }
};

template <typename T>
struct formatter<oge::IntTriple<T>>
{
    template <class Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <class Context>
    auto format(const oge::IntTriple<T>& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {}, {})", p.x, p.y, p.z);
    }
};

template <typename T>
struct formatter<oge::IntPair<T>>
{
    template <typename Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <typename Context>
    auto format(const oge::IntPair<T>& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};
}  // namespace std
