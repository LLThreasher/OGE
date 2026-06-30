#pragma once

#include <cassert>
#include <cinttypes>
#include <unordered_map>
#include <format>

#include "Engine/Math.hpp"

namespace OneGame::Engine
{
template <typename T>
struct IntPair
{
    T x, y;

    bool operator==(const IntPair<T>& other) const noexcept { return x == other.x && y == other.y; }

    IntPair<T> operator+(const IntPair<T>& other) const noexcept { return {x + other.x, y + other.y}; }

    IntPair<T> operator-(const IntPair<T>& other) const noexcept { return {x - other.x, y - other.y}; }

    const T& operator[](size_t index) const
    {
        switch (index)
        {
            case 0:
                return x;
            case 1:
                return y;
            default:
                assert(false);
                return x;
        }
    }

    operator math::vec2() const {
        return {x, y};
    }
};

template<typename T>
struct IntPairHash
{
    size_t operator()(const IntPair<T>& p) const noexcept
    {
        size_t hx = std::hash<int32_t>{}(p.x);
        size_t hy = std::hash<int32_t>{}(p.y);

        // Mix the hashes
        size_t seed = hx;
        seed ^= hy + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        return seed;
    }
};

struct Point2 : IntPair<int32_t>
{
};

struct UPoint2 : IntPair<uint32_t>
{
};
}

namespace std
{
    template <>
    struct formatter<OneGame::Engine::Point2>
    {
        constexpr auto parse(format_parse_context& ctx)
        {
            return ctx.begin();
        }

        auto format(const OneGame::Engine::Point2& p, format_context& ctx) const
        {
            return format_to(
                ctx.out(),
                "({}, {})",
                p.x, p.y
            );
        }
    };
    
    template <>
    struct formatter<OneGame::Engine::UPoint2>
    {
        constexpr auto parse(format_parse_context& ctx)
        {
            return ctx.begin();
        }

        auto format(const OneGame::Engine::UPoint2& p, format_context& ctx) const
        {
            return format_to(
                ctx.out(),
                "({}, {})",
                p.x, p.y
            );
        }
    };
}