#include "game/sim/player_sim_config.hpp"

#include "game/game_world.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"

namespace game::sim
{
std::pmr::vector<oge::runtime::oge_id_type> FixedStepPlayerStages(
    oge::runtime::AnythingFactory& af)
{
    return {
        af.Id<sim::SubsystemPlayer<UpdateType::FixedStep>>(),
        af.Id<sim::SubsystemCreature<UpdateType::FixedStep>>(),
        af.Id<sim::SubsystemPhysics<UpdateType::FixedStep>>(),
    };
}

std::pmr::vector<oge::runtime::oge_id_type> RealtimePlayerStages(
    oge::runtime::AnythingFactory& af)
{
    return {
        af.Id<sim::SubsystemPlayer<UpdateType::Realtime>>(),
        af.Id<sim::SubsystemCreature<UpdateType::Realtime>>(),
        af.Id<sim::SubsystemPhysics<UpdateType::Realtime>>(),
    };
}

std::pmr::vector<oge::runtime::oge_id_type> FixedStepPredictionStages(
    oge::runtime::AnythingFactory& af)
{
    // Fixed player only (see the header): the prediction world's body
    // motion comes from the realtime trio; the fixed creature/physics
    // would double-integrate it.
    return {
        af.Id<sim::SubsystemPlayer<UpdateType::FixedStep>>(),
    };
}

void ApplyServerSimConfig(SceneConfig& cfg, oge::runtime::AnythingFactory& af)
{
    // Fixed pipeline: terrain first, then the player sim trio.  No realtime
    // stages — the server sims once per tick; the realtime trio used to run
    // in the fixed loop at 20 Hz (double-integrating gravity) or not at
    // all.
    cfg.subsystems.clear();
    cfg.subsystems.push_back(af.Id<sim::SubsystemTerrain>());
    auto fixed = FixedStepPlayerStages(af);
    cfg.subsystems.insert(cfg.subsystems.end(), fixed.begin(), fixed.end());

    cfg.realtimeSubsystems.clear();
}

void ApplyClientSimConfig(SceneConfig& cfg, oge::runtime::AnythingFactory& af)
{
    // Fixed pipeline: the fixed player stage only (Phase 3) — jump/action
    // decisions stay fixed-tick; the realtime trio integrates the body at
    // 60 Hz, so the fixed creature/physics must NOT also run here (double
    // integration).  No terrain stage: a client-side terrain generator
    // diverges from the server's replicated chunks and produced false
    // rollbacks (the config-flake class).  loadMask/blocks/terrainDesc
    // stay untouched: they only seed the terrain ctx, not a terrain stage.
    cfg.subsystems = FixedStepPredictionStages(af);

    // Realtime pipeline: the trio (local-prediction-filtered) + the debug
    // text overlay.
    cfg.realtimeSubsystems = RealtimePlayerStages(af);
    cfg.realtimeSubsystems.push_back(af.Id<sim::SubsystemDebugText>());
}
}  // namespace game::sim
