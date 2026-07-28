#pragma once

#include <cstdint>

#include "oge/event_stream.hpp"
#include "oge/math.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace game::input
{
namespace math = ::oge::math;
namespace net = ::oge::runtime::net;
using oge::AccumulativeEventStream;
using oge::DiscreteEventStream;

enum class PlayerAction : uint8_t
{
    Digging = 0,
    Placing,
    Jump,
};

NET_OBJ(PlayerInputEvent)
{
    net::Vec2 actionPos = {};
    net::UInt8 actionMask = 0;

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

    NET_OBJ_FN
    {
        visit(actionPos);
        visit(actionMask);
    }
};

NET_OBJ(PlayerInputFrame)
{
    net::List<PlayerInputEvent> inputEvents;
    net::Vec2 moveDelta;
    net::Vec2 panDelta;

    NET_OBJ_FN
    {
        visit(inputEvents);
        visit(moveDelta);
        visit(panDelta);
    }
};

using PlayerActionStream = DiscreteEventStream<PlayerInputEvent, 16>;
using PlayerDeltaStream = DiscreteEventStream<math::vec2, 16>;

class PlayerInputStream
{
   public:
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
