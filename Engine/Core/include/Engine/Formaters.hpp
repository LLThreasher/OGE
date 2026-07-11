#pragma once

#include <format>

#include "Engine/Math.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Point3.hpp"

namespace std
{
template <>
struct formatter<OneGame::Engine::math::vec2>
{
    template <class Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <class Context>
    auto format(const OneGame::Engine::math::vec2& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};
template <>
struct formatter<OneGame::Engine::math::vec3>
{
    template <class Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <class Context>
    auto format(const OneGame::Engine::math::vec3& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {}, {})", p.x, p.y, p.z);
    }
};
template <>
struct formatter<OneGame::Engine::Point3>
{
    template <class Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <class Context>
    auto format(const OneGame::Engine::Point3& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {}, {})", p.x, p.y, p.z);
    }
};
template <>
struct formatter<OneGame::Engine::Point2>
{
    template <typename Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <typename Context>
    auto format(const OneGame::Engine::Point2& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};

template <>
struct formatter<OneGame::Engine::UPoint2>
{
    template <typename Context>
    constexpr auto parse(Context& ctx)
    {
        return ctx.begin();
    }

    template <typename Context>
    auto format(const OneGame::Engine::UPoint2& p, Context& ctx) const
    {
        return format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};
}  // namespace std
