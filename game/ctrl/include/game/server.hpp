#pragma once

#include "game/scene.hpp"
#include "game/scene_runner.hpp"
#include "game/server_scene.hpp"
#include "game/sim/registry.hpp"
#include "oge/runtime/asset_manager.hpp"
#include "oge/runtime/tick_scheduler.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::AssetManager;

class Server : public SceneRunner<DebugServerScene>
{
    oge::runtime::BlockingTickScheduler m_tick;
    oge::runtime::OgeRegistry m_metaWorld;
    OGEContext m_ctx;

    AssetManager& m_am;

   public:
    Server(float tickInterval = 1.f / 60.f);
    int Run();
};
}  // namespace game
