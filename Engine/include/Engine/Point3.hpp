#pragma once

#include <cassert>
#include <cinttypes>
#include <unordered_map>
#include <format>

namespace OneGame::Engine
{
struct Point3
{
    int32_t x, y, z;

    bool operator==(const Point3& other) const noexcept { return x == other.x && y == other.y && z == other.z; }

    Point3 operator+(const Point3& other) const noexcept { return {x + other.x, y + other.y, z + other.z}; }

    Point3 operator-(const Point3& other) const noexcept { return {x - other.x, y - other.y, z - other.z}; }

    const int32_t& operator[](size_t index) const
    {
        switch (index)
        {
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            default:
                assert(false);
                return x;
        }
    }
};

constexpr Point3 perFaceOffset[6] = {
    {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0},
};

struct Point3Hash
{
    size_t operator()(const Point3& p) const noexcept
    {
        size_t hx = std::hash<int32_t>{}(p.x);
        size_t hy = std::hash<int32_t>{}(p.y);
        size_t hz = std::hash<int32_t>{}(p.z);

        // Mix the hashes
        size_t seed = hx;
        seed ^= hy + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hz + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        return seed;
    }
};
}  // namespace OneGame::Engine

namespace std
{
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
        return format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};
}  // namespace std
