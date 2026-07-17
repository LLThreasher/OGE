#pragma once

#include <cinttypes>

namespace oge
{
    
struct ColorRGBA8
{
    uint8_t r, g, b, a;

    uint32_t AsInt32() { return (uint32_t(r) << 0) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24); }
};

constexpr ColorRGBA8 COLOR_WHITE = {255, 255, 255, 255};
constexpr ColorRGBA8 COLOR_GREY = {128, 128, 128, 255};
constexpr ColorRGBA8 COLOR_BLACK = {0, 0, 0, 128};
constexpr ColorRGBA8 COLOR_RED = {255, 0, 0, 255};
constexpr ColorRGBA8 COLOR_GREEN = {0, 255, 0, 255};

} // namespace oge
