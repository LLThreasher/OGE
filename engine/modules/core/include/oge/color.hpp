#pragma once

#include <array>
#include <cinttypes>

namespace oge
{

struct ColorRGBA8
{
    uint8_t r, g, b, a;

    uint32_t AsInt32()
    {
        return (uint32_t(r) << 0) | (uint32_t(g) << 8) | (uint32_t(b) << 16) |
               (uint32_t(a) << 24);
    }
};

struct ColorRGBAF32
{
    float r, g, b, a;

    ColorRGBAF32(float r = 0.f, float g = 0.f, float b = 0.f, float a = 0.f) :
        r(r), g(g), b(b), a(a)
    {
    }

    ColorRGBAF32(ColorRGBA8 c) :
        r((float)c.r / 255.f),
        g((float)c.g / 255.f),
        b((float)c.b / 255.f),
        a((float)c.a / 255.f)
    {
    }

    operator std::array<float, 4>() const
    {
        return {r, g, b, a};
    }
};

namespace colors
{
using oge::ColorRGBA8;
constexpr ColorRGBA8 WHITE = {255, 255, 255, 255};
constexpr ColorRGBA8 GREY = {128, 128, 128, 255};
constexpr ColorRGBA8 BLACK = {0, 0, 0, 128};
constexpr ColorRGBA8 RED = {255, 0, 0, 255};
constexpr ColorRGBA8 GREEN = {0, 255, 0, 255};
constexpr ColorRGBA8 CORNFLOWER_BLUE = {26, 51, 102, 255};
}  // namespace colors

}  // namespace oge
