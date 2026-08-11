#pragma once

#include <cassert>
#include <cstdint>

#include "oge/math.hpp"
#include "oge/type_traits.hpp"

namespace oge
{
template <typename T>
struct IntTriple
{
    T x, y, z;

    bool operator==(const IntTriple<T>& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }

    template <typename U>
    IntTriple<wider_t<T, U>> operator+(const IntTriple<U>& other) const noexcept
    {
        return {x + other.x, y + other.y, z + other.z};
    }

    template <typename U>
    IntTriple<wider_t<T, U>> operator-(const IntTriple<U>& other) const noexcept
    {
        return {x - other.x, y - other.y, z - other.z};
    }

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

    IntTriple<int32_t> operator<<(size_t size) const
    {
        return {x << size, y << size, z << size};
    }

    IntTriple<int32_t> operator>>(size_t size) const
    {
        return {x >> size, y >> size, z >> size};
    }

    IntTriple<int32_t> operator&(size_t size) const
    {
        return {x & size, y & size, z & size};
    }

    operator math::vec3() const
    {
        return {x, y, z};
    }

    operator IntTriple<int32_t>() const
    {
        return {static_cast<int32_t>(x), static_cast<int32_t>(y),
                static_cast<int32_t>(z)};
    }

    static IntTriple<T> FromVec3(const math::vec3& v)
    {
        return {static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z)};
    }
};

using LocalPoint3 = IntTriple<int8_t>;
using LocalUPoint3 = IntTriple<int8_t>;
using Point3 = IntTriple<int32_t>;

struct CompactLocalPoint3
{
    uint16_t val;

    CompactLocalPoint3(Point3 pt = {})
        : val((pt.x & 0xF) | ((pt.y & 0xF) << 4) | ((pt.z & 0xF) << 8))
    {
        assert(pt.x >= 0 && pt.x < 16);
        assert(pt.y >= 0 && pt.y < 16);
        assert(pt.z >= 0 && pt.z < 16);
    }

    template <typename T>
    operator IntTriple<T>() const
    {
        return {static_cast<T>(val & 0xF), static_cast<T>((val >> 4) & 0xF),
                static_cast<T>((val >> 8) & 0xF)};
    }
};

constexpr Point3 perFaceOffset[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

}  // namespace oge
namespace std
{
template <>
struct hash<oge::Point3>
{
    size_t operator()(const oge::Point3& p) const noexcept
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
}  // namespace std
