#pragma once

#include <array>

#include "oge/color.hpp"
#include "oge/math.hpp"
#include "oge/rect.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace game::view::gfx
{

namespace math = ::oge::math;
using oge::colors::ColorRGBA8;
using oge::colors::WHITE;
using oge::rects::IRect;
using oge::rects::IRect16;
using oge::runtime::PSprite;
using oge::runtime::CmdDrawSprite;

// =========================================================================
// PanelSprite — 9-slice panel rendering
//
// Splits a source sprite into a 3x3 grid and issues 9 draw commands that
// stretch the middle sections to fill a target rectangle.
//
//   ┌───┬───────┬───┐
//   │ 0 │   1   │ 2 │  ← top row (fixed height)
//   ├───┼───────┼───┤
//   │ 3 │   4   │ 5 │  ← middle row (stretched)
//   ├───┼───────┼───┤
//   │ 6 │   7   │ 8 │  ← bottom row (fixed height)
//   └───┴───────┴───┘
//
// Usage:
//   1. Emplace<PanelSprite>(entity, sprite, targetRect, margin);
//   2. PanelSprite::EmitCommands(sprite, target, margin, out);
// =========================================================================

struct PanelSprite
{
    PSprite sourceSprite;          // the source texture region
    IRect targetRect;              // where to draw in screen / UI space
    Margin margin;                 // border thickness in pixels (fixed)
    ColorRGBA8 tint = WHITE;

    /// How many slices per side (default 3x3 = 9 total).
    static constexpr int kSlices = 3;

    /// Emit the 9 CmdDrawSprite commands for a panel sprite.
    /// @param sprite   Source sprite definition
    /// @param target   Target rectangle in UI coordinates
    /// @param margin   Fixed border margin in pixels
    /// @param out      Destination vector to append commands to
    static void EmitCommands(const PanelSprite& panel,
                             std::pmr::vector<CmdDrawSprite>& out)
    {
        const auto& s = panel.sourceSprite;
        const auto& t = panel.targetRect;
        const auto& m = panel.margin;
        const auto& tint = panel.tint;

        // Source subdivisions (in texture coordinates / pixels).
        float srcW = static_cast<float>(s.rect.w);
        float srcH = static_cast<float>(s.rect.h);
        float srcX[4] = {0.0f, static_cast<float>(m.left),
                         srcW - static_cast<float>(m.right), srcW};
        float srcY[4] = {0.0f, static_cast<float>(m.top),
                         srcH - static_cast<float>(m.bottom), srcH};

        // Target subdivisions (in screen pixels).
        float dstW = static_cast<float>(t.w);
        float dstH = static_cast<float>(t.h);
        float dstX[4] = {static_cast<float>(t.x),
                         static_cast<float>(t.x) + static_cast<float>(m.left),
                         static_cast<float>(t.x) + dstW -
                             static_cast<float>(m.right),
                         static_cast<float>(t.x) + dstW};
        float dstY[4] = {static_cast<float>(t.y),
                         static_cast<float>(t.y) + static_cast<float>(m.top),
                         static_cast<float>(t.y) + dstH -
                             static_cast<float>(m.bottom),
                         static_cast<float>(t.y) + dstH};

        for (int row = 0; row < kSlices; ++row)
        {
            for (int col = 0; col < kSlices; ++col)
            {
                // Source rect (in sprite texture coords).
                IRect16 srcRect{
                    static_cast<int16_t>(s.rect.x + srcX[col]),
                    static_cast<int16_t>(s.rect.y + srcY[row]),
                    static_cast<int16_t>(srcX[col + 1] - srcX[col]),
                    static_cast<int16_t>(srcY[row + 1] - srcY[row]),
                };

                // Target rect (in screen coords).
                IRect16 dstRect{
                    static_cast<int16_t>(dstX[col]),
                    static_cast<int16_t>(dstY[row]),
                    static_cast<int16_t>(dstX[col + 1] - dstX[col]),
                    static_cast<int16_t>(dstY[row + 1] - dstY[row]),
                };

                CmdDrawSprite cmd{};
                cmd.sprite = PSprite{s.texture, srcRect};
                cmd.dstRect = dstRect;
                cmd.color = tint;
                out.push_back(cmd);
            }
        }
    }
};

}  // namespace game::view::gfx
