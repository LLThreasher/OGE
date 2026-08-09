#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game/input/player_input_stream.hpp"
#include "oge/math.hpp"
#include "oge/runtime/net_traits.hpp"
#include "oge/runtime/type_name.hpp"

namespace game
{
struct ReplicatedTag
{
};
}  // namespace game

DECL_TYPE_NAME(game::ReplicatedTag, "core::ReplicatedTag")

namespace game::input::net
{
enum class LifetimeEventKind
{
    Add,
    Update,
    Remove,
};

template <typename TPayload>
struct LifetimeEvent
{
    using Payload = TPayload;

    LifetimeEventKind kind{};
    entt::entity entity{};
    TPayload payload{};

    static LifetimeEvent Add(
        entt::entity entity,
        const TPayload& payload)
    {
        return LifetimeEvent{
            .kind = LifetimeEventKind::Add,
            .entity = entity,
            .payload = payload,
        };
    }

    static LifetimeEvent Update(
        entt::entity entity,
        const TPayload& payload)
    {
        return LifetimeEvent{
            .kind = LifetimeEventKind::Update,
            .entity = entity,
            .payload = payload,
        };
    }

    static LifetimeEvent Remove(entt::entity entity)
    {
        return LifetimeEvent{
            .kind = LifetimeEventKind::Remove,
            .entity = entity,
            .payload = {},
        };
    }
};

template <typename TAdapter, typename TEvent>
concept IsLifetimePacketAdapter =
    requires(
        const TAdapter& adapter,
        const TEvent& event)
{
    typename TAdapter::Packet;
    {
        adapter.MakePacket(event)
    } -> std::same_as<typename TAdapter::Packet>;
};

struct PlayerInputFrame
{
    std::vector<PlayerInputEvent> inputEvents;
    math::vec2 moveDelta;
    math::vec2 panDelta;
};

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

    PackedPlayerInputEvent() {}
    PackedPlayerInputEvent(PlayerInputEvent& e);
    operator PlayerInputEvent() const;
};

struct PackedPlayerInputFrame
{
    uint8_t flags = 0;

    int8_t moveX = 0;
    int8_t moveY = 0;

    int16_t panX = 0;
    int16_t panY = 0;

    std::vector<PackedPlayerInputEvent> inputEvents;

    PackedPlayerInputFrame() {}
    PackedPlayerInputFrame(PlayerInputFrame& e);
    operator PlayerInputFrame() const;
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

inline uint16_t QuantizeRangeU16(float v, float minValue, float maxValue)
{
    if (maxValue <= minValue)
    {
        return 0;
    }

    v = std::clamp(v, minValue, maxValue);

    const float t = (v - minValue) / (maxValue - minValue);

    return static_cast<uint16_t>(std::lround(t * 65535.0f));
}

inline float DequantizeRangeU16(uint16_t v, float minValue, float maxValue)
{
    if (maxValue <= minValue)
    {
        return minValue;
    }

    const float t = static_cast<float>(v) / 65535.0f;

    return minValue + t * (maxValue - minValue);
}

inline PackedPlayerInputEvent PackEvent(
    const PlayerInputEvent& src)
{
    PackedPlayerInputEvent dst;

    dst.actionX = QuantizeSNorm16(src.actionPos.x);
    dst.actionY = QuantizeSNorm16(src.actionPos.y);
    dst.actionMask = src.actionMask;

    return dst;
}

inline PlayerInputEvent UnpackEvent(
    const PackedPlayerInputEvent& src)
{
    PlayerInputEvent dst;

    dst.actionPos.x = DequantizeSNorm16(src.actionX);
    dst.actionPos.y = DequantizeSNorm16(src.actionY);
    dst.actionMask = src.actionMask;

    return dst;
}

inline PackedPlayerInputFrame PackFrame(
    const PlayerInputFrame& src)
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

        dst.panX = QuantizeRangeU16(src.panDelta.x, 0.f, math::pi * 2);
        dst.panY = QuantizeRangeS16(src.panDelta.y, math::pi);
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

inline PlayerInputFrame UnpackFrame(const PackedPlayerInputFrame& src)
{
    PlayerInputFrame dst;

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
        dst.panDelta.x = DequantizeRangeU16(src.panX, 0.f, math::pi * 2);
        dst.panDelta.y = DequantizeRangeS16(src.panY, math::pi);
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

inline PackedPlayerInputEvent::PackedPlayerInputEvent(PlayerInputEvent& src)
{
    *this = PackEvent(src);
}

inline PackedPlayerInputEvent::operator PlayerInputEvent() const
{
    return UnpackEvent(*this);
}

inline PackedPlayerInputFrame::PackedPlayerInputFrame(PlayerInputFrame& src)
{
    *this = PackFrame(src);
}

inline PackedPlayerInputFrame::operator PlayerInputFrame() const
{
    return UnpackFrame(*this);
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

DECL_NET_OBJ_PACKED(game::input::net::PlayerInputFrame,
                    game::input::net::PackedPlayerInputFrame,
                    game::input::net::PackFrame, game::input::net::UnpackFrame)
