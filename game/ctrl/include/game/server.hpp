#pragma once

#include "game/scene.hpp"
#include "game/scene_runner.hpp"
#include "game/sim/registry.hpp"
#include "oge/runtime/asset_manager.hpp"
#include "oge/runtime/tick_scheduler.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::AssetManager;

class Server : public SceneRunner<Scene>
{
    oge::runtime::BlockingTickScheduler m_tick;
    entt::registry m_metaWorld;
    OGEContext m_ctx;

    AssetManager& m_am;

   public:
    Server(float tickInterval = 1.f / 20.f);
    int Run();
};
}  // namespace game
