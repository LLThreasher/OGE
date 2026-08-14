#include "game/scene.hpp"

#include "game/components.hpp"
#include "game/game_world.hpp"
#include "oge/json.hpp"
#include "game/net/replication_events.hpp"
#include "game/sim/player_sim_config.hpp"
#include "game/sim/subsystem.hpp"
#include "game/sim/subsystem_physics.hpp"
#include "game/sim/terrain/subsystem_terrain.hpp"
#include "oge/assert.hpp"
#include "oge/platform/io.hpp"

using namespace game;

Scene::Scene(const Def& def)
    : AppRuntime(def.ctx),
      m_subsystems({m_world, def.ctx.events, def.ctx.memory}, 1.f / 30.f),
      m_realtimeSubsystems({m_world, def.ctx.events, def.ctx.memory})
{
    auto it = def.args.find("scene_config");
    if (it != def.args.end())
    {
        m_sceneConfig = json::FromJson<SceneConfig>(it->second);
    }
}

Scene::~Scene()
{
}

void Scene::Update(Frame f, SceneContext sctx)
{
    m_ctx.memory.Update(f.dt);

    // Scene-driven fixed-step loop (D2): accumulate render dt and execute
    // the fixed pipeline as sub-steps of sim::kSubStepDt once the fixed
    // frame is due.  The sub-step boundaries are visible to the stages via
    // SimTickContext.subStepIdx (0 = decisions, 1.. = re-application).
    // kFixedEps covers the harness's POLL_DT (0.016) vs 1/60 drift.
    constexpr float kFixedEps = 0.005f;
    m_fixedAccum += f.dt;
    if (m_fixedAccum + kFixedEps >= m_fixedFrameDuration)
    {
        const int subSteps =
            (int)std::lround(m_fixedFrameDuration / sim::kSubStepDt);
        OGE_ASSERT(subSteps >= 1,
                   "fixed frame duration {} is shorter than one sub-step {}",
                   m_fixedFrameDuration, sim::kSubStepDt);
        if (m_world.ctx().contains<sim::SimTickContext>())
        {
            auto& tickCtx = m_world.ctx().get<sim::SimTickContext>();
            for (int s = 0; s < subSteps; ++s)
            {
                tickCtx.subStepIdx = (uint8_t)s;
                m_subsystems.Update(sim::kSubStepDt);
            }
        }
        else
        {
            for (int s = 0; s < subSteps; ++s)
            {
                m_subsystems.Update(sim::kSubStepDt);
            }
        }
        m_fixedAccum = 0.f;
    }

    m_realtimeSubsystems.Update(f.dt);
}

void Scene::Load()
{
#if OGE_DEBUG
    OGE_ASSERT(m_lifetimeStage != LifetimeStage::Loaded, "double load");
    m_lifetimeStage = LifetimeStage::Loaded;
#endif
    if (m_sceneConfig.loadMask & SceneConfig::LOAD_MASK_BLOCKS)
    {
        auto& blocks = m_world.ctx().emplace<::game::terrain::BlockRegistry>();
        for (const auto& [name, config] : m_sceneConfig.blocks)
        {
            blocks.RegisterBlock(std::move(std::string(name)), config);
        }
    }
    if (m_sceneConfig.loadMask & SceneConfig::LOAD_MASK_TERRAIN)
    {
        m_world.ctx().emplace<::game::terrain::TerrainDesc>(
            m_sceneConfig.terrainDesc);
        m_world.ctx().emplace<::game::terrain::TerrainView>();
    }
    for (auto stage : m_sceneConfig.subsystems)
    {
        m_subsystems.AddStage(m_ctx.any_factory, stage);
    }
    for (auto stage : m_sceneConfig.realtimeSubsystems)
    {
        m_realtimeSubsystems.AddStage(m_ctx.any_factory, stage);
    }
}

void Scene::Unload()
{
#if OGE_DEBUG
    OGE_ASSERT(m_lifetimeStage != LifetimeStage::Unloaded, "double unload");
    m_lifetimeStage = LifetimeStage::Unloaded;
#endif
    m_subsystems.Clear();
    m_realtimeSubsystems.Clear();
}

namespace game
{
SceneConfig GetDefaultSceneConfig(AnythingFactory& af)
{
    SceneConfig config{};
    config.loadMask = SceneConfig::LOAD_MASK_BLOCKS | SceneConfig::LOAD_MASK_TERRAIN;
    config.blocks = {
        {"dirt",
         {
             "Dirt",
             "dirt.png",
             1,
         }},
        {"wood", {"Wood", "wood_plank.png", 1}},
        {"stone", {"Stone", "green_stone.png", 1}},
    };
    config.terrainDesc.chunkViewDistance = 4;

    config.subsystems.push_back(af.Id<sim::SubsystemTerrain>());
    config.subsystems.push_back(
        af.Id<sim::SubsystemPlayer<UpdateType::FixedStep>>());
    config.subsystems.push_back(
        af.Id<sim::SubsystemCreature<UpdateType::FixedStep>>());
    config.subsystems.push_back(
        af.Id<sim::SubsystemPhysics<UpdateType::FixedStep>>());

    config.realtimeSubsystems.push_back(
        af.Id<sim::SubsystemPlayer<UpdateType::Realtime>>());
    config.realtimeSubsystems.push_back(
        af.Id<sim::SubsystemCreature<UpdateType::Realtime>>());
    config.realtimeSubsystems.push_back(
        af.Id<sim::SubsystemPhysics<UpdateType::Realtime>>());
    return config;
}

PlayerInfo LoadOrCreatePlayer()
{
    std::vector<char> data;
    if (!oge::platform::TryLoadBlob("player.bin", data))
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        uuids::uuid const id = uuids::uuid_random_generator{gen}();
        data.resize(16 + sizeof(math::vec3));
        memcpy(data.data(), id.as_bytes().data(), id.as_bytes().size_bytes());
        math::vec3 pos{20.f, 20.f, 20.f};
        memcpy(&data[16], &pos, sizeof(math::vec3));
        oge::platform::TrySaveBlob("player.bin", data);
    }
    assert(data.size() == 16 + sizeof(math::vec3));
    std::array<uint8_t, 16> _uuid;
    memcpy(_uuid.data(), data.data(), 16);
    math::vec3 pos;
    memcpy(&pos, &data[16], sizeof(math::vec3));
    LOG_INFO("player loaded with uuid {}",
             uuids::to_string(uuids::uuid{_uuid}));
    return {std::move(_uuid), pos};
}
}  // namespace game
