#pragma once

#include <array>
#include <cstdint>

#include "oge/event_stream.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/runtime/type_name.hpp"

namespace game::input
{
namespace math = ::oge::math;

// Action bit space shared by input events (PlayerInputEvent) and ray-encoded
// actions (PlayerAction).  (Renamed from the old PlayerAction enum — the
// struct below took that name.)
enum class PlayerActionKind : uint8_t
{
    Digging = 0,
    Placing,
    Jump,
};

// Ray-encoded action (D6): the exact origin/direction at emission time.
// The fixed stage casts this ray on both sides, so the server needs no
// camera reconstruction to hit the same block the client aimed at.
struct PlayerAction
{
    uint8_t actionMask = 0;  // 1 << PlayerActionKind bits (Digging | Placing)
    math::vec3 origin{};
    math::vec3 dir{};
};

// One per tick, emission order, capped (D6).
constexpr size_t kMaxActionsPerTick = 3;

struct PlayerActionFrame
{
    uint32_t tick = 0;
    uint8_t actionCnt = 0;
    std::array<PlayerAction, kMaxActionsPerTick> actions{};
};

// Action window shared by the realtime stage (emission), the replication
// poller (per-tick aggregation), and the fixed stage (stamp-anchored reads).
// Same contract as PlayerInputStream: readers own cursors and never consume;
// replicated frames arrive via PushTick into the tick ring.
class PlayerActionStream
{
   public:
    using Cursor = uint64_t;
    using Frame = PlayerActionFrame;

    // Client realtime stage: emit a ray-encoded action (per frame).  The
    // per-tick aggregation packs up to kMaxActionsPerTick in emission order;
    // overflow is dropped with a warning — the dig/place cooldown makes it
    // unreachable in practice.
    void PushAction(const PlayerAction& action)
    {
        if (m_accumCnt < kMaxActionsPerTick)
        {
            m_accum[m_accumCnt++] = action;
        }
        else
        {
            LOG_WARN("player action overflow: >{} actions in one tick",
                     (size_t)kMaxActionsPerTick);
        }
    }

    // Commit the per-tick accumulation into a stamped frame.  Returns false
    // when no actions were emitted since the last aggregation.
    bool AggregateTick(uint32_t tick, PlayerActionFrame& out)
    {
        if (m_accumCnt == 0)
        {
            return false;
        }
        out.tick = tick;
        out.actionCnt = m_accumCnt;
        out.actions = m_accum;
        m_accumCnt = 0;
        return true;
    }

    // Replication apply path: action frames replicated from the client
    // arrive here (the stream is the per-tick buffer — D3).
    void PushTick(const PlayerActionFrame& frame)
    {
        m_tickFrames.Push(frame);
    }

    // ---- tick-frame window (stamp-anchored reads, D3) ----
    Cursor HeadCursor() const
    {
        return m_tickFrames.HeadCursor();
    }

    const PlayerActionFrame& At(Cursor c) const
    {
        return m_tickFrames.Get(c);
    }

   private:
    // 16 ticks at 20 Hz = 0.8 s of buffered actions.
    using TickStream = oge::DiscreteEventStream<PlayerActionFrame, 16>;
    TickStream m_tickFrames;
    std::array<PlayerAction, kMaxActionsPerTick> m_accum{};
    uint8_t m_accumCnt = 0;
};

}  // namespace game::input

DECL_TYPE_NAME(game::input::PlayerActionStream, "core::PlayerActionStream")
