#pragma once

#include <cstdint>

#include "oge/event_stream.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/math.hpp"

namespace game
{
namespace math
{
using namespace oge::math;
}
}  // namespace game

namespace game::input
{
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
    math::vec2 actionPos = {};
    uint8_t actionMask = 0;

    PlayerInputEvent() {}
    PlayerInputEvent(math::vec2 pos) : actionPos(pos), actionMask(0) {}
    PlayerInputEvent(math::vec2 pos, PlayerAction a) : actionPos(pos), actionMask(1 << static_cast<uint32_t>(a)) {}

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

    inline bool empty() const { return actionMask == 0; }

    template <PlayerAction action>
    inline void unset()
    {
        actionMask &= ~(1 << static_cast<uint32_t>(action));
    }
};

using PlayerActionStream = DiscreteEventStream<PlayerInputEvent, 16>;

class PlayerInputStream
{
    PlayerActionStream actions;
    math::vec2 move = {};
    math::vec2 pan = {};
    bool moveDirty = false;
    bool panDirty = false;
    PlayerActionStream::Cursor actionCursor = {};

   public:
    int LatestAction() const { return actions.Head().actionMask; }

    bool HasAction() const
    {
        DiscreteEventStream<PlayerInputEvent>::Cursor _c;
        actions.AdvanceCursor(_c);
        return _c != actionCursor;
    }

    bool PollAction(PlayerInputEvent& event)
    {
        return actions.PollOne(actionCursor, event);
    }

    bool PollMoveDelta(math::vec2& out)
    {
        if (moveDirty)
        {
            out = move;
            move = {};
            moveDirty = false;
            return true;
        }
        return false;
    }

    bool PollPanDelta(math::vec2& out)
    {
        if (panDirty)
        {
            out = pan;
            pan = {};
            panDirty = false;
            return true;
        }
        return false;
    }

    void InsertAction(PlayerInputEvent event) { actions.Push(event); }

    void InsertMoveDelta(math::vec2 delta)
    {
        move += delta;
        moveDirty = true;
    }

    void InsertPanDelta(math::vec2 delta)
    {
        pan += delta;
        panDirty = true;
    }
};

}  // namespace game::input
