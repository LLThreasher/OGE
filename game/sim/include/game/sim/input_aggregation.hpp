#pragma once

#include "game/input/net.hpp"
#include "game/input/player_input_stream.hpp"
#include "oge/runtime/oge_registry.hpp"

namespace game::sim
{
// =========================================================================
// Local-input aggregation for transport-less scenes (D3/D6): a bare
// game::Scene owns its tick space (Scene::Update advances SimTickContext),
// so it must also produce the tick-stamped frames the fixed player stage
// reads — the networked equivalent runs in the transport layer
// (PollPlayerInputs / PollPlayerActions in ClientScene2).  The fixed
// SubsystemPlayer has one code path: it reads stamped frames from the
// stream rings and never asks whether a transport layer exists.
//
// Called from Scene::Update once per fixed frame, after the tick advanced,
// with stamp = currentTick - input::kInputPipelineDelayTicks — the same
// one-tick pipeline delay the transport pollers use, so frames produced
// during tick T apply during tick T+1 in every configuration.
//
// Only local streams (IsLocalInput) are aggregated: replicated players
// fill their rings via ApplyEvent(PushTick), exactly like the server.
// =========================================================================
inline void AggregateLocalInputs(oge::runtime::OgeRegistry& world,
                                 uint32_t tick)
{
    for (auto [entity, input, actions, simState] :
         world
             .view<input::PlayerInputStream, input::PlayerActionStream,
                   input::PlayerSimInputState>()
             .each())
    {
        (void)entity;
        if (!input.IsLocalInput())
        {
            continue;
        }

        // Movement: the raw 60 Hz window since the last aggregation becomes
        // one stamped tick frame (empty windows ship nothing — the
        // empty-tick contract).  The frame is quantized exactly like the
        // wire (SNorm8 round-trip — PollPlayerInputs pushes the packed
        // value into the local tick ring), so a transport-less fixed stage
        // consumes bit-identical frames to the networked one: the fixed
        // sim must not branch on the transport layer's existence.  The
        // 60 Hz realtime stage keeps draining raw frames — it is
        // prediction-only and diverges by cadence anyway.
        input::PlayerInputFrame frame{};
        if (input::AggregateTickInput(input, simState.aggregateCursor, tick,
                                      frame))
        {
            input.PushTick(static_cast<input::PlayerInputFrame>(
                input::net::PackedPlayerInputFrame{frame}));
        }

        // Actions: the ray-encoded accumulation becomes one stamped action
        // frame (up to kMaxActionsPerTick in emission order).
        input::PlayerActionFrame actionFrame{};
        if (actions.AggregateTick(tick, actionFrame))
        {
            actions.PushTick(actionFrame);
        }
    }
}
}  // namespace game::sim
