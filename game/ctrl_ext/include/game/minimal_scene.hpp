#pragma once

#include <cassert>
#include <string>

#include "game/events.hpp"
#include "game/game_world.hpp"
#include "game/scene.hpp"
#include "game/scene_ext.hpp"
#include "game/scene_runner.hpp"
#include "game/sim/subsystem.hpp"
#include "game/view/renderer.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
class DebugScene : public SceneExt
{
   public:
    DebugScene(const Def& def) : SceneExt(def)
    {
        if (m_sceneConfig.empty())
        {
            m_sceneConfig.subsystems.push_back(Id<sim::SubsystemDebugText>());
        }

        Load();

        m_renderers.AddStage<view::UIRenderer>(AF());
        m_renderers.AddStage<view::DebugInfoRenderer>(AF());
    }
};
}  // namespace game

namespace oge::runtime
{
template <>
struct TypeName<game::DebugScene>
{
    static constexpr std::string Get()
    {
        return "core::DebugScene3";
    }
};
}  // namespace oge::runtime
