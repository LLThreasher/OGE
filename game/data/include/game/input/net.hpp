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

enum PlayerInputFrameFlags : uint8_t
{
    HasMove = 1 << 0,
    HasJumpInput = 1 << 1,  // the aggregated jump input flag (movement frame)
    HasJumpStamp = 1 << 2,  // client-decided lift-off stamp (until Phase 3)
};

struct PackedPlayerInputFrame
{
    // The producing tick — always serialized (not flag-gated): the fixed
    // stage anchors application to it (D3/D8).
    uint32_t tick = 0;

    uint8_t flags = 0;

    int8_t moveX = 0;
    int8_t moveY = 0;
    int8_t moveZ = 0;

    // Client-decided jump stamp: lift-off position (raw floats — world
    // coordinates exceed the small quantized ranges; jumps are rare so the
    // 12 bytes are negligible).  Until Phase 3.
    float jumpX = 0.f;
    float jumpY = 0.f;
    float jumpZ = 0.f;

    PackedPlayerInputFrame() {}
    PackedPlayerInputFrame(PlayerInputFrame& e);
    operator PlayerInputFrame() const;
};

// Ray-encoded action — raw floats (R14: do not quantize v1; the rays are
// ≤3/tick and correctness outranks bytes here).
struct PackedPlayerAction
{
    uint8_t actionMask = 0;
    float originX = 0.f;
    float originY = 0.f;
    float originZ = 0.f;
    float dirX = 0.f;
    float dirY = 0.f;
    float dirZ = 0.f;

    PackedPlayerAction() {}
    PackedPlayerAction(const PlayerAction& e);
    operator PlayerAction() const;
};

inline int8_t QuantizeSNorm8(float v)
{
    v = std::clamp(v, -1.0f, 1.0f);

    return static_cast<int8_t>(std::lround(v * 127.0f));
}

inline float DequantizeSNorm8(int8_t v)
{
    return std::max(-1.0f, static_cast<float>(v) / 127.0f);
}

inline PackedPlayerAction PackAction(
    const PlayerAction& src)
{
    PackedPlayerAction dst;

    dst.actionMask = src.actionMask;
    dst.originX = src.origin.x;
    dst.originY = src.origin.y;
    dst.originZ = src.origin.z;
    dst.dirX = src.dir.x;
    dst.dirY = src.dir.y;
    dst.dirZ = src.dir.z;

    return dst;
}

inline PlayerAction UnpackAction(
    const PackedPlayerAction& src)
{
    PlayerAction dst;

    dst.actionMask = src.actionMask;
    dst.origin = {src.originX, src.originY, src.originZ};
    dst.dir = {src.dirX, src.dirY, src.dirZ};

    return dst;
}

inline PackedPlayerInputFrame PackFrame(
    const PlayerInputFrame& src)
{
    // Note: the move average is applied by PlayerInputStream::AdvanceTick at
    // commit time, so local consumers and the packed copy stay in sync.
    PackedPlayerInputFrame dst;

    dst.tick = src.tick;

    bool hasMove = std::abs(src.move.x) > INPUT_EPSILON ||
                   std::abs(src.move.y) > INPUT_EPSILON ||
                   std::abs(src.move.z) > INPUT_EPSILON;

    if (src.jump)
    {
        dst.flags = static_cast<uint8_t>(dst.flags | HasJumpInput);
    }

    if (hasMove)
    {
        dst.flags = static_cast<uint8_t>(dst.flags | HasMove);

        dst.moveX = QuantizeSNorm8(src.move.x);
        dst.moveY = QuantizeSNorm8(src.move.y);
        dst.moveZ = QuantizeSNorm8(src.move.z);
    }

    if constexpr (kUseJumpStamp)
    {
        if (src.jumped)
        {
            dst.flags = static_cast<uint8_t>(dst.flags | HasJumpStamp);

            dst.jumpX = src.jumpPos.x;
            dst.jumpY = src.jumpPos.y;
            dst.jumpZ = src.jumpPos.z;
        }
    }

    return dst;
}

inline PlayerInputFrame UnpackFrame(const PackedPlayerInputFrame& src)
{
    PlayerInputFrame dst{};

    dst.tick = src.tick;

    if ((src.flags & HasMove) != 0)
    {
        dst.move.x = DequantizeSNorm8(src.moveX);
        dst.move.y = DequantizeSNorm8(src.moveY);
        dst.move.z = DequantizeSNorm8(src.moveZ);
    }

    if ((src.flags & HasJumpInput) != 0)
    {
        dst.jump = true;
    }

    if constexpr (kUseJumpStamp)
    {
        if ((src.flags & HasJumpStamp) != 0)
        {
            dst.jumped = true;
            dst.jumpPos = {src.jumpX, src.jumpY, src.jumpZ};
        }
    }

    return dst;
}

inline PackedPlayerAction::PackedPlayerAction(const PlayerAction& src)
{
    *this = PackAction(src);
}

inline PackedPlayerAction::operator PlayerAction() const
{
    return UnpackAction(*this);
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

DECL_NET_OBJ(game::input::net::PackedPlayerAction, {
    visit(self.actionMask);
    visit(self.originX);
    visit(self.originY);
    visit(self.originZ);
    visit(self.dirX);
    visit(self.dirY);
    visit(self.dirZ);
})

DECL_NET_OBJ(game::input::net::PackedPlayerInputFrame, {
    visit(self.tick);

    visit(self.flags);

    if ((self.flags & game::input::net::HasMove) != 0)
    {
        visit(self.moveX);
        visit(self.moveY);
        visit(self.moveZ);
    }

    if constexpr (game::input::kUseJumpStamp)
    {
        if ((self.flags & game::input::net::HasJumpStamp) != 0)
        {
            visit(self.jumpX);
            visit(self.jumpY);
            visit(self.jumpZ);
        }
    }
})

DECL_NET_OBJ_PACKED(game::input::PlayerAction,
                    game::input::net::PackedPlayerAction,
                    game::input::net::PackAction, game::input::net::UnpackAction)

DECL_NET_OBJ_PACKED(game::input::PlayerInputFrame,
                    game::input::net::PackedPlayerInputFrame,
                    game::input::net::PackFrame, game::input::net::UnpackFrame)
