#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game/input/player_input_stream.hpp"
#include "oge/runtime/net_traits.hpp"

namespace game::input::net
{
constexpr float INPUT_EPSILON = 0.000001f;

// ±0.5 radians per frame, about ±28.6 degrees.
constexpr float PAN_MAX_RAD = 0.5f;

enum PlayerInputFrameFlags : uint8_t
{
    HasEvents = 1 << 0,
    HasMove = 1 << 1,
    HasPan = 1 << 2,
};

struct PackedPlayerInputEvent
{
    uint16_t actionX = 0;
    uint16_t actionY = 0;
    uint8_t actionMask = 0;
};

struct PackedPlayerInputFrame
{
    uint8_t flags = 0;

    int8_t moveX = 0;
    int8_t moveY = 0;

    int16_t panX = 0;
    int16_t panY = 0;

    std::vector<PackedPlayerInputEvent> inputEvents;
};

inline uint16_t QuantizeUNorm16(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);

    return static_cast<uint16_t>(std::lround(v * 65535.0f));
}

inline float DequantizeUNorm16(uint16_t v)
{
    return static_cast<float>(v) / 65535.0f;
}

inline int16_t QuantizeSNorm16(float v)
{
    v = std::clamp(v, -1.0f, 1.0f);

    return static_cast<int16_t>(std::lround(v * 32767.0f));
}

inline float DequantizeSNorm16(int16_t v)
{
    return std::max(-1.0f, static_cast<float>(v) / 32767.0f);
}

inline int8_t QuantizeSNorm8(float v)
{
    v = std::clamp(v, -1.0f, 1.0f);

    return static_cast<int8_t>(std::lround(v * 127.0f));
}

inline float DequantizeSNorm8(int8_t v)
{
    return std::max(-1.0f, static_cast<float>(v) / 127.0f);
}

inline int16_t QuantizeRangeS16(float v, float maxAbs)
{
    if (maxAbs <= 0.0f)
    {
        return 0;
    }

    v = std::clamp(v, -maxAbs, maxAbs);

    return static_cast<int16_t>(std::lround(v / maxAbs * 32767.0f));
}

inline float DequantizeRangeS16(int16_t v, float maxAbs)
{
    if (maxAbs <= 0.0f)
    {
        return 0.0f;
    }

    return static_cast<float>(v) / 32767.0f * maxAbs;
}

inline PackedPlayerInputEvent PackEvent(
    const game::input::PlayerInputEvent& src)
{
    PackedPlayerInputEvent dst;

    dst.actionX = QuantizeSNorm16(src.actionPos.x);
    dst.actionY = QuantizeSNorm16(src.actionPos.y);
    dst.actionMask = src.actionMask;

    return dst;
}

inline game::input::PlayerInputEvent UnpackEvent(
    const PackedPlayerInputEvent& src)
{
    game::input::PlayerInputEvent dst;

    dst.actionPos.x = DequantizeSNorm16(src.actionX);
    dst.actionPos.y = DequantizeSNorm16(src.actionY);
    dst.actionMask = src.actionMask;

    return dst;
}

inline PackedPlayerInputFrame PackFrame(
    const game::input::PlayerInputFrame& src)
{
    PackedPlayerInputFrame dst;

    bool hasEvents = !src.inputEvents.empty();

    bool hasMove = std::abs(src.moveDelta.x) > INPUT_EPSILON ||
                   std::abs(src.moveDelta.y) > INPUT_EPSILON;

    bool hasPan = std::abs(src.panDelta.x) > INPUT_EPSILON ||
                  std::abs(src.panDelta.y) > INPUT_EPSILON;

    if (hasEvents)
    {
        dst.flags = static_cast<uint8_t>(dst.flags | HasEvents);
    }

    if (hasMove)
    {
        dst.flags = static_cast<uint8_t>(dst.flags | HasMove);

        dst.moveX = QuantizeSNorm8(src.moveDelta.x);
        dst.moveY = QuantizeSNorm8(src.moveDelta.y);
    }

    if (hasPan)
    {
        dst.flags = static_cast<uint8_t>(dst.flags | HasPan);

        dst.panX = QuantizeRangeS16(src.panDelta.x, PAN_MAX_RAD);
        dst.panY = QuantizeRangeS16(src.panDelta.y, PAN_MAX_RAD);
    }

    if (hasEvents)
    {
        const std::size_t count =
            std::min<std::size_t>(src.inputEvents.size(), 255);

        dst.inputEvents.resize(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            dst.inputEvents[i] = PackEvent(src.inputEvents[i]);
        }
    }

    return dst;
}

inline game::input::PlayerInputFrame UnpackFrame(
    const PackedPlayerInputFrame& src)
{
    game::input::PlayerInputFrame dst;

    dst.moveDelta = {};
    dst.panDelta = {};
    dst.inputEvents.clear();

    if ((src.flags & HasMove) != 0)
    {
        dst.moveDelta.x = DequantizeSNorm8(src.moveX);
        dst.moveDelta.y = DequantizeSNorm8(src.moveY);
    }

    if ((src.flags & HasPan) != 0)
    {
        dst.panDelta.x = DequantizeRangeS16(src.panX, PAN_MAX_RAD);
        dst.panDelta.y = DequantizeRangeS16(src.panY, PAN_MAX_RAD);
    }

    if ((src.flags & HasEvents) != 0)
    {
        dst.inputEvents.resize(src.inputEvents.size());

        for (std::size_t i = 0; i < src.inputEvents.size(); ++i)
        {
            dst.inputEvents[i] = UnpackEvent(src.inputEvents[i]);
        }
    }

    return dst;
}
}  // namespace game::input::net

DECL_NET_OBJ(game::input::net::PackedPlayerInputEvent, {
    visit(self.actionX);
    visit(self.actionY);
    visit(self.actionMask);
})

DECL_NET_OBJ(game::input::net::PackedPlayerInputFrame, {
    visit(self.flags);

    if ((self.flags & game::input::net::HasMove) != 0)
    {
        visit(self.moveX);
        visit(self.moveY);
    }

    if ((self.flags & game::input::net::HasPan) != 0)
    {
        visit(self.panX);
        visit(self.panY);
    }

    if ((self.flags & game::input::net::HasEvents) != 0)
    {
        visit(self.inputEvents);
    }
})

DECL_NET_OBJ_PACKED(game::input::PlayerInputEvent,
                    game::input::net::PackedPlayerInputEvent,
                    game::input::net::PackEvent, game::input::net::UnpackEvent)

DECL_NET_OBJ_PACKED(game::input::PlayerInputFrame,
                    game::input::net::PackedPlayerInputFrame,
                    game::input::net::PackFrame, game::input::net::UnpackFrame)
