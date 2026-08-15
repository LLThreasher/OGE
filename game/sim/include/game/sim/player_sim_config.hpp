#pragma once

#include <cstdint>
#include <memory_resource>
#include <vector>

#include "oge/runtime/type_name.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
struct SceneConfig;
}

namespace game::sim
{
// =========================================================================
// Shared simulation tick space (D1): one tick = one fixed frame at 20 Hz,
// executed as kSubStepsPerTick sub-steps of kSubStepDt.  The server's fixed
// pipeline and the client's authoritative-world parity sim run the same
// ordered (player -> creature -> physics) sub-step chain with the same dt —
// determinism by construction.
// =========================================================================

constexpr float kSubStepDt = 1.f / 60.f;
constexpr int kSubStepsPerTick = 3;
constexpr float kFixedFrameDuration = kSubStepDt * kSubStepsPerTick;  // 1/20

// World ctx: the tick the fixed stages are simulating, and which sub-step
// of that tick is currently executing (0 = decisions, 1..2 = re-application).
// Scenes write currentTick before Update; the scene-driven sub-step loop
// writes subStepIdx.
struct SimTickContext
{
    uint32_t currentTick = 0;
    uint8_t subStepIdx = 0;
};

// =========================================================================
// Stage-list builders (Phase 2, single source): the player sim's stage
// lists are assembled here so the server scene, the client scene and the
// test harness can never drift from each other (the config-drift bug
// class).  Ids come from the caller's factory, so the lists are portable
// across runner instances.
// =========================================================================

// The fixed (20 Hz) player sim trio, in execution order: player ->
// creature -> physics.  Runs on the server world and the client's
// authoritative mirror — both drive the same sub-step chain over the
// shared tick space (D1/D9).
std::pmr::vector<oge::runtime::oge_id_type> FixedStepPlayerStages(
    oge::runtime::AnythingFactory& af);

// The realtime (60 Hz) trio: player (input drain + aim + camera chase),
// creature, physics — prediction-world only, filtered to locally-predicted
// entities (D9).
std::pmr::vector<oge::runtime::oge_id_type> RealtimePlayerStages(
    oge::runtime::AnythingFactory& af);

// The prediction world's fixed pipeline (Phase 3 parity fix): the fixed
// player stage only — jump/action decisions + the one-tick frame read stay
// fixed-tick decisions (D3/D6).  The fixed creature/physics do NOT run in
// the prediction world: the realtime trio already integrates the body at
// 60 Hz, and running both trios would double-integrate the body.  The
// authoritative mirror keeps the full fixed trio (FixedStepPlayerStages).
std::pmr::vector<oge::runtime::oge_id_type> FixedStepPredictionStages(
    oge::runtime::AnythingFactory& af);

// Server config: the fixed pipeline is terrain (first) + the fixed trio.
// NO realtime stages — the server sims once per tick; the old realtime
// trio double-integrated gravity at 20 Hz.
void ApplyServerSimConfig(SceneConfig& cfg, oge::runtime::AnythingFactory& af);

// Client config: the fixed pipeline is the fixed trio WITHOUT the terrain
// stage (a client-side terrain generator diverges from the server's
// replicated chunks — the config-flake class); the realtime pipeline is
// the realtime trio + SubsystemDebugText.
void ApplyClientSimConfig(SceneConfig& cfg, oge::runtime::AnythingFactory& af);

}  // namespace game::sim

DECL_TYPE_NAME(game::sim::SimTickContext, "core::SimTickContext")
