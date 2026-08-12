#pragma once

#include <array>
#include <cassert>
#include <cstdint>

#include "game/components.hpp"
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

enum class PlayerAction : uint8_t
{
    Digging = 0,
    Placing,
    Jump,
};

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

    PlayerInputEvent(math::vec2 pos, PlayerAction a)
        : actionPos(pos), actionMask(1 << static_cast<uint32_t>(a))
    {
    }

    template <PlayerAction... actions>
    inline bool get() const
    {
        return actionMask & ((1 << static_cast<uint32_t>(actions)) | ...);
    }

    template <PlayerAction action>
    inline void set()
    {
        actionMask |= (1 << static_cast<uint32_t>(action));
    }

    inline bool empty() const
    {
        return actionMask == 0;
    }

    template <PlayerAction action>
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

struct PlayerInputFrame
{
    size_t inputEventCnt = 0;
    std::array<PlayerInputEvent, 4> inputEvents = {};
    math::vec3 move = {};
    math::vec2 aim = {};
    size_t deltaCnt = 0;
    bool hasAim = false;

    PlayerInputFrame(math::vec2 aimBase = {}) : aim(aimBase) {}

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

using PlayerFrameStream = DiscreteEventStream<PlayerInputFrame, 16>;

class PlayerInputStream
{
   public:
    using Cursor = PlayerFrameStream::Cursor;

   private:
    PlayerFrameStream m_frames;

    PlayerInputFrame m_accumFrame = {};

    bool moveDirty = false;
    bool aimDirty = false;

   public:
    // Moves the cursor to the newest item in each substream.
    void AdvanceCursor(Cursor& cursor) const
    {
        m_frames.AdvanceCursor(cursor);
    }

    uint8_t LatestAction() const
    {
        auto& head = m_frames.Head();
        return head.inputEventCnt == 0 ? 0 : head.inputEvents[head.inputEventCnt - 1].actionMask;
    }

    // Flushes pending per-tick state into the streams.
    //
    // Call this once per simulation/input tick before polling this stream
    // for frames to send or consume.
    //
    // Only commits frames that carry input.  Committing empty frames on
    // every tick — e.g. on the server, which never accumulates locally and
    // only receives replicated frames via PushTick — interleaves empties
    // into the 16-slot ring; once it wraps, real frames get overwritten and
    // consumers read garbage.  Releasing all keys needs no explicit frame:
    // consumers already reset moveOrder every tick when no frame arrives.
    void AdvanceTick()
    {
        // Average per-delta move so a tick that accumulated several deltas
        // still yields a unit-magnitude move order (consumers assert
        // len(moveOrder) <= 1).  Normalize at commit so local consumers and
        // the packed wire copy observe the same value.
        m_accumFrame.normalize();

        const bool dirty = m_accumFrame.inputEventCnt > 0 ||
                           m_accumFrame.move != math::vec3{} ||
                           m_accumFrame.hasAim;
        if (dirty)
        {
            m_frames.Push(m_accumFrame);
        }

        auto aim = m_accumFrame.aim;
        m_accumFrame = {aim};
    }

    void PushFrame(PlayerInputFrameDelta delta)
    {
        m_accumFrame.apply(delta);
    }

    void PushTick(const PlayerInputFrame& frame)
    {
        m_frames.Push(frame);
    }

    bool PollFrame(Cursor& cursor, PlayerInputFrame& frame) const
    {
        return m_frames.PollOne(cursor, frame);
    }
};

}  // namespace game::input

DECL_TYPE_NAME(game::input::PlayerInputStream, "core::PlayerInputStream")
