#pragma once

#include <cstddef>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include "game/input/net.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/net/latest_streams.hpp"

namespace game::net
{

struct PlayerInputLatestEventAdapter
{
    using Stream = input::PlayerInputStream;
    using Frame = input::net::PlayerInputFrame;
    using Packet = input::net::PackedPlayerInputFrame;

    static bool ExtractFrame(
        const Stream& stream,
        Stream::Cursor& cursor,
        Frame& frame)
    {
        frame.inputEvents.clear();
        frame.moveDelta = {};
        frame.panDelta = {};

        bool hasAny = false;

        input::PlayerInputEvent action{};
        while (stream.PollAction(cursor, action))
        {
            frame.inputEvents.push_back(action);
            hasAny = true;
        }

        math::vec2 move{};
        while (stream.PollMoveDelta(cursor, move))
        {
            frame.moveDelta += move;
            hasAny = true;
        }

        math::vec2 aim{};
        while (stream.PollAim(cursor, aim))
        {
            /*
                Latest-value semantics.

                If multiple aim values exist in the cursor range, only the most
                recent one is kept.
            */
            frame.panDelta = aim;
            hasAny = true;
        }

        return hasAny;
    }

    static void InsertFrame(
        Stream& stream,
        const Frame& frame)
    {
        for (const input::PlayerInputEvent& action : frame.inputEvents)
        {
            stream.InsertAction(action);
        }

        if (frame.moveDelta.x != 0.0f || frame.moveDelta.y != 0.0f)
        {
            stream.InsertMoveDelta(frame.moveDelta);
        }

        if (frame.panDelta.x != 0.0f || frame.panDelta.y != 0.0f)
        {
            stream.SetAim(frame.panDelta);
        }
    }
};

using PlayerInputNetOutputStream =
    LatestEventNetOutputStream<PlayerInputLatestEventAdapter>;

using PlayerInputNetInputStream =
    LatestEventNetInputStream<PlayerInputLatestEventAdapter>;

}  // namespace game::net
