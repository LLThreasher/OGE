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

class PlayerInputStream
{
   public:
    using TEvent = PlayerInputFrame;
    struct Cursor
    {
        PlayerActionStream::Cursor actionCursor = {};
        PlayerDeltaStream::Cursor moveCursor = {};
        PlayerDeltaStream::Cursor panCursor = {};
    };

   private:
    PlayerActionStream actions;
    PlayerDeltaStream moves;
    PlayerDeltaStream pans;
    math::vec2 move = {};
    math::vec2 pan = {};
    bool moveDirty = false;
    bool panDirty = false;

   public:
    void AdvanceCursor(Cursor& cursor)
    {
        actions.AdvanceCursor(cursor.actionCursor);
        moves.AdvanceCursor(cursor.moveCursor);
        pans.AdvanceCursor(cursor.panCursor);
    }

    bool PollOne(Cursor& cursor, PlayerInputFrame& frame)
    {
        AdvanceTick();
        frame.inputEvents.clear();
        frame.moveDelta = {};
        frame.panDelta = {};

        bool flag = false;
        PlayerInputEvent e;
        while (actions.PollOne(cursor.actionCursor, e))
        {
            frame.inputEvents.push_back(e);
            flag = true;
        }
        math::vec2 move;
        while (moves.PollOne(cursor.moveCursor, move))
        {
            frame.moveDelta += move;
            flag = true;
        }
        math::vec2 pan;
        while (pans.PollOne(cursor.panCursor, pan))
        {
            frame.panDelta += pan;
            flag = true;
        }
        return flag;
    }

    void Push(const PlayerInputFrame& frame)
    {
        for (const auto& ie : frame.inputEvents)
        {
            actions.Push(ie);
        }
        InsertMoveDelta(frame.moveDelta);
        InsertPanDelta(frame.panDelta);
    }

    void AdvanceTick()
    {
        if (moveDirty)
        {
            moves.Push(move);
            move = {};
            moveDirty = false;
        }
        if (panDirty)
        {
            pans.Push(pan);
            pan = {};
            panDirty = false;
        }
    }

    int LatestAction() const
    {
        return actions.Head().actionMask;
    }

    bool HasAction(Cursor& cursor) const
    {
        DiscreteEventStream<PlayerInputEvent>::Cursor _c;
        actions.AdvanceCursor(_c);
        return _c != cursor.actionCursor;
    }

    bool PollAction(Cursor& cursor, PlayerInputEvent& event) const
    {
        return actions.PollOne(cursor.actionCursor, event);
    }

    bool PollMoveDelta(Cursor& cursor, math::vec2& out) const
    {
        return moves.PollOne(cursor.moveCursor, out);
    }

    bool PollPanDelta(Cursor& cursor, math::vec2& out) const
    {
        return pans.PollOne(cursor.panCursor, out);
    }

    void InsertAction(PlayerInputEvent event)
    {
        actions.Push(event);
    }

    void InsertMoveDelta(math::vec2 delta)
    {
        move = delta;
        moveDirty = true;
    }

    void InsertPanDelta(math::vec2 delta)
    {
        pan += delta;
        panDirty = true;
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
