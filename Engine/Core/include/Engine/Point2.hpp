#pragma once

#include <cassert>
#include <cinttypes>
#include <algorithm>

#include "Engine/Math.hpp"
#include "Engine/TypeTraits.hpp"

namespace OneGame::Engine
{
template <typename T>
struct IntPair
{
    T x, y;

    bool operator==(const IntPair<T>& other) const noexcept { return x == other.x && y == other.y; }

    template <typename U>
    IntPair<wider_t<T, U>> operator+(const IntPair<U>& other) const noexcept { return {static_cast<wider_t<T, U>>(x + other.x), static_cast<wider_t<T, U>>(y + other.y)}; }
    template <typename U>
    IntPair<wider_t<T, U>> operator-(const IntPair<U>& other) const noexcept { return {static_cast<wider_t<T, U>>(x - other.x), static_cast<wider_t<T, U>>(y - other.y)}; }

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

    IntPair<unsigned_t<T>> clampToZero() const
    {
        return {static_cast<unsigned_t<T>>(math::max(static_cast<T>(0), x)), static_cast<unsigned_t<T>>(math::max(static_cast<T>(0), y))};
    }

    operator math::vec2() const {
        return {x, y};
    }

    operator IntPair<int32_t>() const {
        return {static_cast<int32_t>(x), static_cast<int32_t>(y)};
    }
    
    template<typename U>
    explicit operator IntPair<U>() const {
        return {static_cast<U>(x), static_cast<U>(y)};
    }

    static IntPair<T> FromVec2(const math::vec2& v) { return {static_cast<T>(v.x), static_cast<T>(v.y)}; }
};

using Point2 = IntPair<int32_t>;
using UPoint2 = IntPair<uint32_t>;

using I16Point2 = IntPair<int16_t>;
using U16Point2 = IntPair<uint16_t>;

struct U16Norm
{
    static uint16_t Encode(float value)
    {
        value = std::clamp(value, 0.0f, 1.0f);
        return static_cast<uint16_t>(std::round(value * 65535.0f));
    }

    uint16_t val;
    U16Norm() : val(0) {}
    U16Norm(float value) : val(Encode(value)) {}
    U16Norm operator+(const U16Norm& other) const {return val + other.val;}
};

struct U16Norm2
{
    U16Norm x;
    U16Norm y;

    U16Norm2() : x(0), y(0)
    {
    }

    U16Norm2(math::vec2 in) : x(U16Norm::Encode(in.x)), y(U16Norm::Encode(in.y))
    {
    }

    U16Norm2(U16Norm x, U16Norm y) : x(x), y(y)
    {
    }

    U16Norm2 operator+(const U16Norm2& other) const {return {x + other.x, y + other.y};}
};
}

namespace std
{
template <>
struct hash<OneGame::Engine::Point2>
{
    size_t operator()(const OneGame::Engine::Point2& p) const noexcept
    {
        return static_cast<size_t>(p.x) << 32 | p.y;
    }
};
template <>
struct hash<OneGame::Engine::UPoint2>
{
    size_t operator()(const OneGame::Engine::UPoint2& p) const noexcept
    {
        return static_cast<size_t>(p.x) << 32 | p.y;
    }
};
}  // namespace std
