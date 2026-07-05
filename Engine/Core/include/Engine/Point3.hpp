#pragma once

#include <cassert>
#include <cinttypes>
#include <unordered_map>

#include "Engine/Math.hpp"
#include "Engine/TypeTraits.hpp"

namespace OneGame::Engine
{
template <typename T>
struct IntTriple
{
    T x, y, z;

    bool operator==(const IntTriple<T>& other) const noexcept { return x == other.x && y == other.y && z == other.z; }

    template <typename U>
    IntTriple<wider_t<T, U>> operator+(const IntTriple<U>& other) const noexcept { return {x + other.x, y + other.y, z + other.z}; }

    template <typename U>
    IntTriple<wider_t<T, U>> operator-(const IntTriple<U>& other) const noexcept { return {x - other.x, y - other.y, z - other.z}; }

    const T& operator[](size_t index) const
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

    operator math::vec3() const {
        return {x, y, z};
    }

    operator IntTriple<int32_t>() const {
        return {static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)};
    }

    static IntTriple<T> FromVec3(const math::vec3& v) { return {static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z)}; }
};

using LocalPoint3 = IntTriple<int8_t>;
using Point3 = IntTriple<int32_t>;

constexpr Point3 perFaceOffset[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

}  // namespace OneGame::Engine
namespace std
{
template <>
struct hash<OneGame::Engine::Point3>
{
    size_t operator()(const OneGame::Engine::Point3& p) const noexcept
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
}
