#pragma once

#include "oge/event_stream.hpp"
#include "oge/math.hpp"

namespace game::input
{
using namespace oge;
enum class PlayerAction : uint32_t
{
    Digging = 0,
    Placing,
    Jump,
};

struct PlayerInputEvent
{
    math::vec2 actionPos;
    uint8_t actionMask;

    template <PlayerAction action>
    inline bool get() const
    {
        return actionMask & (1 << static_cast<uint32_t>(action));
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

struct PlayerInputStream
{
    DiscreteEventStream<PlayerInputEvent> actions;
    AccumulativeEventStream<math::vec2> move;
    AccumulativeEventStream<math::vec2> pan;
    
    struct Cursor
    {
        DiscreteEventStream<PlayerInputEvent>::Cursor actionCursor;
        AccumulativeEventStream<math::vec2>::Cursor moveCursor;
        AccumulativeEventStream<math::vec2>::Cursor panCursor;
    };

    bool PollAction(Cursor& cursor, PlayerInputEvent& event)
    {
        return actions.PollOne(cursor.actionCursor, event);
    }

    math::vec2 PollMoveDelta(Cursor& cursor)
    {
        return move.PollDelta(cursor.moveCursor);
    }

    math::vec2 PollPanDelta(Cursor& cursor)
    {
        return move.PollDelta(cursor.panCursor);
    }
};

} // namespace OneGame::Engine::Input
