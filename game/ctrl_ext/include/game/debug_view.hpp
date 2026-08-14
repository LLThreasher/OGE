#pragma once

#include <cassert>
#include <string>

#include "game/scene.hpp"
#include "game/scene_view.hpp"
#include "game/scene_runner.hpp"
#include "game/sim/subsystem.hpp"
#include "game/view/renderer.hpp"
#include "oge/runtime/type_name.hpp"

namespace game
{
template <typename InnerScene>
class DebugView : public SceneView
{
   public:
    DebugView(const Scene::Def& def)
        : SceneView(def, oge::runtime::TypeName<InnerScene>::Get())
    {
        m_innerScene.GetConfig().subsystems.push_back(Id<sim::SubsystemDebugText>());
        m_innerScene.Load();

        m_renderers.AddStage<view::DebugInfoRenderer>(AF());
    }

    void Update(Frame f, SceneContext sctx) override
    {
        SceneView::Update(std::move(f), sctx);
    }
};
}  // namespace game

template <typename S>
struct oge::runtime::TypeName<game::DebugView<S>>
{
    static constexpr std::string Get()
    {
        return "core::DebugView<" + ::oge::runtime::TypeName<S>::Get() + ">";
    }
};
