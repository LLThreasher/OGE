#include "game/scene.hpp"
#include "game/input/entity_event_stream.hpp"

using namespace game;

Scene::Scene(const Def& def)
    : AppRuntime(def.ctx),
      m_subsystems({m_world, def.ctx.events, def.ctx.memory}, 1.f / 30.f),
      m_realtimeSubsystems({m_world, def.ctx.events, def.ctx.memory})
{
    auto it = def.args.find("scene_config");
    if (it != def.args.end())
    {
        auto jsonConfig = std::get<json::Object>(it->second);
        for (auto val : std::get<json::Array>(jsonConfig["subsystems"]))
        {
            m_sceneConfig.subsystems.Add(std::get<int64_t>(val));
        }
        for (auto val :
             std::get<json::Array>(jsonConfig["realtime_subsystems"]))
        {
            m_sceneConfig.realtimeSubsystems.Add(std::get<int64_t>(val));
        }
    }
}

Scene::~Scene()
{
}

void Scene::CreateEntityEventStream()
{
    m_world.ctx().emplace<input::EntityEventStream>();
}

void Scene::Update(Frame f, SceneContext sctx)
{
    m_ctx.memory.Update(f.dt);
    m_subsystems.Update(f.dt);
    m_realtimeSubsystems.Update(f.dt);
}

void Scene::Load()
{
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
    m_subsystems.Clear();
    m_realtimeSubsystems.Clear();
}
