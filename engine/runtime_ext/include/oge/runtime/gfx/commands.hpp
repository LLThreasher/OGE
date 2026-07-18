#pragma once

#include "oge/color.hpp"
#include "oge/rect.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace oge::runtime::gfx
{
struct CmdDrawSprite
{
    IRect16 rect;
    ColorRGBA8 color;
    PSprite sprite;
};
}  // namespace oge::runtime::gfx