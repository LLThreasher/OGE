#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "game/components.hpp"
#include "game/input/player_action_stream.hpp"
#include "oge/event_stream.hpp"
#include "oge/math.hpp"
#include "oge/runtime/type_name.hpp"

namespace game::input
{
namespace math = ::oge::math;
using oge::AccumulativeEventStream;
using oge::DiscreteEventStream;

constexpr float INPUT_EPSILON = 0.000001f;

// ±0.5 radians per frame, about ±28.6 degrees.
constexpr float PAN_MAX_RAD = 0.25f;

// One-tick pipeline delay (D3): input produced during tick T is applied by
// the fixed stage during tick T + kInputPipelineDelayTicks on both sides.
constexpr uint32_t kInputPipelineDelayTicks = 1;

// Bounded wait for a missing frame (D3): after this many ticks without an
// applied frame the read degrades — lastAppliedTick advances past the gap so
// late frames count as stale instead of blocking the scan.  8 ticks = 400 ms.
constexpr uint32_t kMaxInputWaitTicks = 8;

struct PlayerInputEvent
{
    // normalized to view
    math::vec2 actionPos = {};
    uint8_t actionMask = 0;

    PlayerInputEvent()
    {
    }

    PlayerInputEvent(math::vec2 pos) : actionPos(pos), actionMask(0)
    {
    }

    PlayerInputEvent(math::vec2 pos, PlayerActionKind a)
        : actionPos(pos), actionMask(1 << static_cast<uint32_t>(a))
    {
    }

    template <PlayerActionKind... actions>
    inline bool get() const
    {
        return actionMask & ((1 << static_cast<uint32_t>(actions)) | ...);
    }

    template <PlayerActionKind action>
    inline void set()
    {
        actionMask |= (1 << static_cast<uint32_t>(action));
    }

    inline bool empty() const
    {
        return actionMask == 0;
    }

    template <PlayerActionKind action>
    inline void unset()
    {
        actionMask &= ~(1 << static_cast<uint32_t>(action));
    }
};

struct PlayerInputFrameDelta
{
    PlayerInputEvent inputEvent;
    math::vec2 moveDelta = {};
    math::vec2 panDelta = {};
    float dt = 1.f;  // default: treat moveDelta as a per-frame value
};

static float WrapRadians0To2Pi(float radians)
{
    constexpr float TwoPi = 6.28318530717958647692f;

    radians = std::fmod(radians, TwoPi);

    if (radians < 0.0f)
    {
        radians += TwoPi;
    }

    return radians;
}

static math::vec2 NormalizeAim(math::vec2 aim)
{
    aim.x = WrapRadians0To2Pi(aim.x);

    aim.y = math::clamp(aim.y, -math::radians(89.0f), math::radians(89.0f));

    return aim;
}

// =========================================================================
// Raw per-render-frame entry — the 60 Hz accumulation unit pushed by the
// view.  Action events + pan stay here so the realtime stage can poll them
// per frame (D3: readers never consume); raw frames never reach the wire —
// the per-tick aggregation produces the movement frame instead.
// =========================================================================
struct PlayerInputRawFrame
{
    size_t inputEventCnt = 0;
    std::array<PlayerInputEvent, 4> inputEvents = {};
    math::vec3 move = {};
    math::vec2 aim = {};
    size_t deltaCnt = 0;
    bool hasAim = false;

    // Client-decided jump stamp: the local simulation performed a jump this
    // frame (grounded && jump pressed).  jumpPos is the pre-impulse lift-off
    // position.  The server applies the impulse anchored to this position
    // instead of re-deriving the jump from its own physics.  (Until Phase 3.)
    bool jumped = false;
    math::vec3 jumpPos = {};

    PlayerInputRawFrame(math::vec2 aimBase = {}) : aim(aimBase)
    {
    }

    void apply(PlayerInputFrameDelta delta)
    {
        if (inputEventCnt < 4)
        {
            inputEvents[inputEventCnt] = delta.inputEvent;
            ++inputEventCnt;
        }
        ComponentCamera dummyCamera{};
        dummyCamera.SetYawPitch(aim.x, aim.y);
        move += (delta.moveDelta.x * dummyCamera.right() + delta.moveDelta.y * dummyCamera.forward) * delta.dt;
        aim = NormalizeAim(aim + delta.panDelta);
        ++deltaCnt;
        hasAim = hasAim || delta.panDelta != math::vec2{};
    }

    void normalize()
    {
        if (deltaCnt > 0)
        {
            move /= (float)deltaCnt;
        }
    }
};

// =========================================================================
// Aggregated per-tick movement frame — the wire unit and the fixed stage's
// input.  One per 20 Hz tick, produced by AggregateTickInput.  (D3/D6: the
// action + aim sections left the movement frame.)
// =========================================================================
struct PlayerInputFrame
{
    uint32_t tick = 0;  // the producing tick (D3/D8)
    math::vec3 move = {};
    bool jump = false;  // OR of the window's Jump events (movement input)

    // Client-decided jump stamp (until Phase 3).
    bool jumped = false;
    math::vec3 jumpPos = {};
};

using PlayerRawFrameStream = DiscreteEventStream<PlayerInputRawFrame, 16>;
using PlayerTickFrameStream = DiscreteEventStream<PlayerInputFrame, 16>;

// 16 slots at 60 Hz raw frames ≈ 0.27 s (5+ ticks) — the aggregation window
// is one tick (3 frames), so the ring can never wrap under a healthy reader.
static_assert(PlayerRawFrameStream::MCapacity >= 6,
              "raw frame ring must hold a full tick window plus margin");

// Tick ring: 16 ticks at 20 Hz = 0.8 s — comfortably outlives
// kMaxInputWaitTicks (400 ms).
static_assert(PlayerTickFrameStream::MCapacity >= 8,
              "tick frame ring must outlive kMaxInputWaitTicks");

class PlayerInputStream
{
   public:
    using Cursor = uint64_t;
    using Frame = PlayerInputFrame;
    using RawFrame = PlayerInputRawFrame;

   private:
    // Raw ring: 60 Hz client accumulation (PushFrame + AdvanceTick).
    // Tick ring: aggregated per-tick frames — locally produced (client
    // fixed-stage reads) or replicated (server + client authoritative world).
    PlayerRawFrameStream m_rawFrames;
    PlayerTickFrameStream m_tickFrames;

    PlayerInputRawFrame m_accumFrame{};

    // True once PushFrame is used — local input accumulation only happens
    // on the client.  Server streams receive replicated frames via PushTick
    // and must neither stamp jumps nor consume stamps.
    bool m_isLocalInput = false;

    // Pending jump stamp: latched by MarkJumpPerformed, committed into the
    // next raw frame by AdvanceTick.  (Deleted in Phase 3.)
    bool m_pendingJump = false;
    math::vec3 m_jumpPos = {};

   public:
    // ---- raw accumulation (client view path) ----

    void PushFrame(PlayerInputFrameDelta delta)
    {
        m_isLocalInput = true;
        m_accumFrame.apply(delta);
    }

    // Commits the accumulated raw frame (dirty-only) into the raw ring.
    // Call once per render frame before readers poll (today's mechanics,
    // minus the tick stamp — aggregation stamps).
    //
    // Committing empty frames on every tick would interleave empties into
    // the ring; once it wraps, real frames get overwritten and consumers
    // read garbage.  Releasing all keys needs no explicit frame: consumers
    // already reset moveOrder every tick when no frame arrives.
    void AdvanceTick()
    {
        // Average per-delta move so a frame that accumulated several deltas
        // still yields a unit-magnitude move order (consumers assert
        // len(moveOrder) <= 1).
        m_accumFrame.normalize();

        // Stamp a pending jump decision into this frame.  A jump must be
        // committed even when the frame otherwise carries nothing.
        m_accumFrame.jumped = m_pendingJump;
        m_accumFrame.jumpPos = m_jumpPos;
        m_pendingJump = false;

        const bool dirty = m_accumFrame.inputEventCnt > 0 ||
                           m_accumFrame.move != math::vec3{} ||
                           m_accumFrame.hasAim || m_accumFrame.jumped;
        if (dirty)
        {
            m_rawFrames.Push(m_accumFrame);
        }

        auto aim = m_accumFrame.aim;
        m_accumFrame = {aim};
    }

    void PushTick(const PlayerInputFrame& frame)
    {
        m_tickFrames.Push(frame);
    }

    // Record that the local simulation performed a jump this tick at the
    // given lift-off position.  No-op on server streams — they receive
    // replicated frames and must not stamp their own.
    void MarkJumpPerformed(math::vec3 pos)
    {
        if (!m_isLocalInput)
        {
            return;
        }
        m_pendingJump = true;
        m_jumpPos = pos;
    }

    bool IsLocalInput() const
    {
        return m_isLocalInput;
    }

    // ---- raw window readers (realtime stage + aggregation) ----
    // PollEvents is non-consuming (D3): frame data stays in the window for
    // the per-tick aggregation and any other readers.  Each reader owns an
    // independent cursor; a fresh cursor (0) snaps to the frontier so late
    // readers start at "now".

    void AdvanceCursor(Cursor& cursor) const
    {
        m_rawFrames.AdvanceCursor(cursor);
    }

    uint8_t LatestAction() const
    {
        auto& head = m_rawFrames.Head();
        return head.inputEventCnt == 0
                   ? 0
                   : head.inputEvents[head.inputEventCnt - 1].actionMask;
    }

    bool PollEvents(Cursor& cursor, PlayerInputRawFrame& frame) const
    {
        return m_rawFrames.PollOne(cursor, frame);
    }

    Cursor RawHeadCursor() const
    {
        return m_rawFrames.HeadCursor();
    }

    const PlayerInputRawFrame& RawFrameAt(Cursor c) const
    {
        return m_rawFrames.Get(c);
    }

    // ---- tick window (stamp-anchored reads, D3) ----
    // Named like the action stream's accessors so TryReadTickFrame is
    // agnostic to the concrete stream type.

    Cursor HeadCursor() const
    {
        return m_tickFrames.HeadCursor();
    }

    const PlayerInputFrame& At(Cursor c) const
    {
        return m_tickFrames.Get(c);
    }
};

// =========================================================================
// Shared helpers (D3)
// =========================================================================

// Wrap-safe staleness comparison (D8): a tick more than ~2^31 steps behind
// counts as stale.  20 Hz wraps after ~6.8 years.
inline bool TickIsStale(uint32_t a, uint32_t b)
{
    return static_cast<int32_t>(a - b) <= 0;
}

// Aggregation: read the raw window since `cursor` (the frames produced
// during tick `tick`), average the move (weighted by delta counts — the
// same semantics as the per-frame normalize), OR the jump events, and stamp
// the synthetic per-tick movement frame.  Returns false when the window
// held nothing worth shipping (empty-tick contract: absence = no input).
inline bool AggregateTickInput(PlayerInputStream& stream,
                               PlayerInputStream::Cursor& cursor,
                               uint32_t tick, PlayerInputFrame& out)
{
    const auto frontier = stream.RawHeadCursor();
    if (cursor == 0)
    {
        cursor = frontier;  // fresh reader starts at "now"
    }
    if (cursor > frontier)
    {
        cursor = frontier;
    }

    bool any = false;
    math::vec3 moveSum{};
    size_t moveCnt = 0;
    bool jumped = false;
    math::vec3 jumpPos{};

    for (auto i = cursor; i < frontier; ++i)
    {
        const auto& raw = stream.RawFrameAt(i);
        if (raw.deltaCnt > 0)
        {
            moveSum += raw.move * static_cast<float>(raw.deltaCnt);
            moveCnt += raw.deltaCnt;
            any = true;
        }
        for (size_t k = 0; k < raw.inputEventCnt; ++k)
        {
            if (raw.inputEvents[k].get<PlayerActionKind::Jump>())
            {
                out.jump = true;
                any = true;
            }
        }
        // Client-decided jump stamp (until Phase 3).
        if (raw.jumped)
        {
            jumped = true;
            jumpPos = raw.jumpPos;
            any = true;
        }
    }
    cursor = frontier;
    if (!any)
    {
        return false;
    }

    out.tick = tick;
    if (moveCnt > 0)
    {
        out.move = moveSum / static_cast<float>(moveCnt);
    }
    out.jumped = jumped;
    out.jumpPos = jumpPos;
    return true;
}

// Stamp-anchored read contract (D3): returns true with `out` when a frame
// stamped simTick - kInputPipelineDelayTicks is available.  Otherwise:
// - frames stamped <= lastAppliedTick are stale/duplicates: skipped;
// - frames stamped ahead of the due tick: not due yet — wait (FIFO, so the
//   scan stops there);
// - frames stamped behind the due tick: their window already passed
//   (arrived late) — skipped; they can never apply at their stamped tick.
// After kMaxInputWaitTicks without an applied frame the read degrades:
// lastAppliedTick advances past the gap so late frames count as stale.
// Works for both PlayerInputStream and PlayerActionStream (D3: the same
// contract governs the movement and action windows).
template <typename TStream>
bool TryReadTickFrame(TStream& stream, typename TStream::Cursor& cursor,
                      uint32_t simTick, uint32_t& lastAppliedTick,
                      typename TStream::Frame& out)
{
    const auto frontier = stream.HeadCursor();
    if (cursor == 0)
    {
        cursor = frontier;  // fresh reader starts at "now"
    }
    if (cursor > frontier)
    {
        cursor = frontier;
    }

    const uint32_t dueTick = simTick - kInputPipelineDelayTicks;

    for (auto i = cursor; i < frontier; ++i)
    {
        const auto& frame = stream.At(i);
        if (TickIsStale(frame.tick, lastAppliedTick))
        {
            // Stale/duplicate — applied at an earlier tick (or wrapped).
            // Checked BEFORE the due match: a frame stamped exactly dueTick
            // must not re-apply when lastAppliedTick already passed it
            // (e.g. the gap degrade advanced past dueTick — late frames
            // count as stale per the contract above).
            cursor = i + 1;
            continue;
        }
        if (frame.tick == dueTick)
        {
            cursor = i + 1;
            lastAppliedTick = frame.tick;
            out = frame;
            return true;
        }
        if (!TickIsStale(frame.tick, dueTick))
        {
            // Stamped ahead of the due tick — not due yet.  Frames arrive
            // FIFO, so everything after it is even further ahead.
            break;
        }
        // frame.tick < dueTick: missed its window — skip.
        cursor = i + 1;
    }

    // Bounded wait: a missing frame stalls at most kMaxInputWaitTicks.
    if (static_cast<int32_t>(simTick - lastAppliedTick) >
        static_cast<int32_t>(kMaxInputWaitTicks))
    {
        lastAppliedTick = dueTick;
    }
    return false;
}

// =========================================================================
// Per-entity non-replicated state for the fixed-tick input read path (D3).
// Holds the independent reader cursors + the cached current-tick frame.
// Emplaced in ComponentPlayer::CreatePlayer (server) and
// DebugVoxelView::onConstructPlayer (client).
// =========================================================================
struct PlayerSimInputState
{
    uint32_t lastAppliedTick = 0;  // movement frames (D3 contract)
    uint32_t lastActionTick = 0;   // action frames (same contract)
    PlayerInputStream::Cursor moveCursor{};
    PlayerActionStream::Cursor actionCursor{};
    PlayerInputStream::Cursor realtimeCursor{};  // realtime per-frame drain
    uint32_t consumedTick = 0;  // tick the fixed-stage decisions were made for
    bool hasFrame = false;      // cached movement frame valid?
    PlayerInputFrame frame{};
};

}  // namespace game::input

DECL_TYPE_NAME(game::input::PlayerInputStream, "core::PlayerInputStream")
DECL_TYPE_NAME(game::input::PlayerSimInputState, "core::PlayerSimInputState")
