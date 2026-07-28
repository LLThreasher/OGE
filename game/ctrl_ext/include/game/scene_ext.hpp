#pragma once

#include "game/app_context.hpp"
#include "game/input/input_source.hpp"
#include "game/scene.hpp"
#include "game/scene_runner.hpp"
#include "game/view/gfx/debug_info_pass.hpp"
#include "game/view/gfx/terrain_pass2.hpp"
#include "game/view/gfx/view_executor.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/gfx/ui_pass.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using game::view::gfx::DebugInfoPass;
using game::view::gfx::TerrainPass2;
using oge::input::RawInputStream;
using oge::runtime::AnythingFactory;
using oge::runtime::AssetContext;
using oge::runtime::OGEContext;
using oge::runtime::gfx::UIPass;

class SceneExt : public Scene
{
    using ViewExecutor =
        view::ViewExecutor<TerrainPass2, UIPass, DebugInfoPass>;

    struct Ctx : AppContext
    {
        AssetContext assets;

        Ctx(AppContext actx) : AppContext(actx), assets(actx.any_ctx)
        {
        }
    };

   public:
    struct Frame : Scene::Frame
    {
        oge::input::RawInputStream& is;
        FramePerfStatus perfStats;
        AppFrameAction& frameAction;
    };

    Ctx m_ctx;
    WindowCtx m_windowCtx;

    input::InputPipeline m_inputs;

    entt::registry m_renderWorld;
    view::RenderPipeline m_renderers;

    view::SubmissionQueue m_squeue;
    ViewExecutor m_viewExecutor;

    SceneExt(const Def& def);
    virtual ~SceneExt();
    virtual void Update(Frame f, SceneContext sctx);

    ViewExecutor& GetPasses();
    void Render(float dt);
};
}  // namespace game

namespace oge::runtime {
template<>
struct TypeName<game::SceneExt>
{
    static consteval std::string_view Get()
    {
        return "core::SceneExt";
    }
};
}
