#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

#include "oge/event_stream.hpp"
#include "oge/math.hpp"
#include "oge/runtime/typed_registry.hpp"

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

struct PlayerInputFrame
{
    std::vector<PlayerInputEvent> inputEvents;
    math::vec2 moveDelta;
    math::vec2 panDelta;
};

using PlayerActionStream = DiscreteEventStream<PlayerInputEvent, 16>;
using PlayerDeltaStream = DiscreteEventStream<math::vec2, 16>;

inline float WrapRadians0To2Pi(float radians)
{
    constexpr float TwoPi = math::pi * 2;

    radians = std::fmod(radians, TwoPi);

    if (radians < 0.0f)
    {
        radians += TwoPi;
    }

    return radians;
}

class PlayerInputStream
{
public:
    using TEvent = PlayerInputFrame;

    struct Cursor
    {
        PlayerActionStream::Cursor actionCursor = {};
        PlayerDeltaStream::Cursor moveCursor = {};
        PlayerDeltaStream::Cursor aimCursor = {};
    };

private:
    PlayerActionStream actions;
    PlayerDeltaStream moves;

    // Stores absolute yaw/pitch, not raw pan deltas.
    PlayerDeltaStream aims;

    // Accumulates movement deltas for the current tick.
    math::vec2 pendingMoveDelta = {};

    // Current absolute aim orientation.
    // x = yaw   in [0, 2pi)
    // y = pitch in [-89deg, +89deg]
    math::vec2 currentAim = {};

    bool moveDirty = false;
    bool aimDirty = false;

private:
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

        aim.y = math::clamp(
            aim.y,
            -math::radians(89.0f),
             math::radians(89.0f));

        return aim;
    }

public:
    // Moves the cursor to the newest item in each substream.
    void AdvanceCursor(Cursor& cursor) const
    {
        actions.AdvanceCursor(cursor.actionCursor);
        moves.AdvanceCursor(cursor.moveCursor);
        aims.AdvanceCursor(cursor.aimCursor);
    }

    // Flushes pending per-tick state into the streams.
    //
    // Call this once per simulation/input tick before polling this stream
    // for frames to send or consume.
    void AdvanceTick()
    {
        if (moveDirty)
        {
            moves.Push(pendingMoveDelta);
            pendingMoveDelta = {};
            moveDirty = false;
        }

        if (aimDirty)
        {
            currentAim = NormalizeAim(currentAim);
            aims.Push(currentAim);
            aimDirty = false;
        }
    }

    // Polls all stream changes since the cursor and merges them into one frame.
    //
    // Returns true if there was any new input.
    bool PollOne(Cursor& cursor, PlayerInputFrame& frame)
    {
        AdvanceTick();

        frame.inputEvents.clear();
        frame.moveDelta = {};
        frame.panDelta = {};

        bool hasAnyInput = false;

        PlayerInputEvent event;
        while (actions.PollOne(cursor.actionCursor, event))
        {
            frame.inputEvents.push_back(event);
            hasAnyInput = true;
        }

        math::vec2 moveDelta;
        while (moves.PollOne(cursor.moveCursor, moveDelta))
        {
            frame.moveDelta += moveDelta;
            hasAnyInput = true;
        }

        // Important:
        // panDelta is now being used as absolute aim yaw/pitch.
        // If possible, rename PlayerInputFrame::panDelta to aim.
        math::vec2 aim;
        while (aims.PollOne(cursor.aimCursor, aim))
        {
            frame.panDelta = aim;
            hasAnyInput = true;
        }

        return hasAnyInput;
    }

    // Pushes a fully formed input frame into this stream.
    //
    // This is useful for replaying received input frames locally.
    // Assumption:
    // - frame.moveDelta is a movement delta.
    // - frame.panDelta is absolute aim yaw/pitch.
    void Push(const PlayerInputFrame& frame)
    {
        for (const PlayerInputEvent& event : frame.inputEvents)
        {
            InsertAction(event);
        }

        if (frame.moveDelta.x != 0.0f || frame.moveDelta.y != 0.0f)
        {
            InsertMoveDelta(frame.moveDelta);
        }

        // Because frame.panDelta now means absolute aim, do not call
        // InsertPanDelta() here.
        //
        // InsertPanDelta() is for local mouse/touch deltas.
        if (frame.panDelta.x != 0.0f || frame.panDelta.y != 0.0f)
        {
            SetAim(frame.panDelta);
        }
    }

    // Latest action mask.
    int LatestAction() const
    {
        return actions.Head().actionMask;
    }

    bool HasAction(Cursor& cursor) const
    {
        PlayerActionStream::Cursor latest = {};
        actions.AdvanceCursor(latest);
        return latest != cursor.actionCursor;
    }

    bool PollAction(Cursor& cursor, PlayerInputEvent& event) const
    {
        return actions.PollOne(cursor.actionCursor, event);
    }

    bool PollMoveDelta(Cursor& cursor, math::vec2& out) const
    {
        return moves.PollOne(cursor.moveCursor, out);
    }

    // Polls absolute aim yaw/pitch.
    //
    // x = yaw   in [0, 2pi)
    // y = pitch in [-89deg, +89deg]
    bool PollAim(Cursor& cursor, math::vec2& out) const
    {
        return aims.PollOne(cursor.aimCursor, out);
    }

    // Kept for compatibility with older callers.
    // This no longer returns a raw accumulated pan delta; it returns absolute aim.
    bool PollAccumPan(Cursor& cursor, math::vec2& out) const
    {
        return PollAim(cursor, out);
    }

    void InsertAction(PlayerInputEvent event)
    {
        actions.Push(event);
    }

    void InsertMoveDelta(math::vec2 delta)
    {
        pendingMoveDelta += delta;
        moveDirty = true;
    }

    // Local input path:
    // Adds a camera/mouse/touch delta to the current absolute aim.
    void InsertPanDelta(math::vec2 delta)
    {
        currentAim += delta;
        currentAim = NormalizeAim(currentAim);
        aimDirty = true;
    }

    // Network/replay path:
    // Directly sets absolute aim.
    void SetAim(math::vec2 aim)
    {
        currentAim = NormalizeAim(aim);
        aimDirty = true;
    }

    math::vec2 GetAim() const
    {
        return currentAim;
    }

    math::vec2 GetPendingMoveDelta() const
    {
        return pendingMoveDelta;
    }

    bool HasPendingMove() const
    {
        return moveDirty;
    }

    bool HasPendingAim() const
    {
        return aimDirty;
    }
};

}  // namespace game::input

namespace oge::runtime
{
template <>
struct TypeName<game::input::PlayerInputStream>
{
    static constexpr std::string Get()
    {
        return "core::PlayerInputStream";
    }
};
}  // namespace oge::runtime
