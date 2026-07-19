#pragma once

#include "oge/event_stream.hpp"
#include "oge/math.hpp"

namespace game
{
    namespace math
    {
        using namespace oge::math;
    }
}

namespace game::input
{
using oge::DiscreteEventStream;
using oge::AccumulativeEventStream;

enum class PlayerAction : uint8_t
{
    Digging = 0,
    Placing,
    Jump,
};

struct PlayerInputEvent
{
    math::vec2 actionPos;
    uint8_t actionMask;

    template <PlayerAction... actions>
    inline bool get() const
    {
        return actionMask & ((1 << static_cast<uint32_t>(actions))|...);
    }

    template <PlayerAction action>
    inline void set()
    {
        actionMask |= (1 << static_cast<uint32_t>(action));
    }

    inline bool empty() const { return actionMask == 0; }

    template <PlayerAction action>
    inline void unset()
    {
        actionMask &= ~(1 << static_cast<uint32_t>(action));
    }
};

class PlayerInputStream
{
    DiscreteEventStream<PlayerInputEvent> actions;
    AccumulativeEventStream<math::vec2> move;
    AccumulativeEventStream<math::vec2> pan;
public:
    struct Cursor
    {
        DiscreteEventStream<PlayerInputEvent>::Cursor actionCursor;
        AccumulativeEventStream<math::vec2>::Cursor moveCursor;
        AccumulativeEventStream<math::vec2>::Cursor panCursor;
    };

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

    math::vec2 PollMoveDelta(Cursor& cursor) const
    {
        return move.PollDelta(cursor.moveCursor);
    }

    math::vec2 PollPanDelta(Cursor& cursor) const
    {
        return pan.PollDelta(cursor.panCursor);
    }

    void InsertAction(PlayerInputEvent& event)
    {
        actions.Push(event);
    }

    void InsertMoveDelta(math::vec2 delta)
    {
        move.Push(delta);
    }

    void InsertPanDelta(math::vec2 delta)
    {
        pan.Push(delta);
    }
};

} // namespace OneGame::Engine::Input
