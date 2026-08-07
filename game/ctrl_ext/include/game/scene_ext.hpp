#pragma once

#include "game/app_context.hpp"
#include "game/input/input_source.hpp"
#include "game/scene.hpp"
#include "game/scene_runner.hpp"
#include "game/view/gfx/debug_info_pass.hpp"
#include "game/view/gfx/terrain_pass2.hpp"
#include "game/view/gfx/ui_pass.hpp"
#include "game/view/gfx/view_executor.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/input/raw_input_stream.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "oge/runtime/ui/objects.hpp"

namespace game
{
using ::game::view::gfx::DebugInfoPass;
using ::game::view::gfx::TerrainPass2;
using ::game::view::gfx::UIPass;
using oge::input::RawInputStream;
using oge::runtime::AnythingFactory;
using oge::runtime::AssetContext;
using oge::runtime::OGEContext;

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

    entt::registry m_uiWorld;
    entt::registry m_renderWorld;
    view::RenderPipeline m_renderers;

    view::SubmissionQueue m_squeue;
    ViewExecutor m_viewExecutor;

    SceneExt(const Def& def);
    virtual ~SceneExt();
    virtual void Update(Frame f, SceneContext sctx);

    ViewExecutor& GetPasses();
    void Render(float dt);

   protected:
    bool m_nextShowingCursor = true;

   private:
    bool m_showingCursor = false;
    std::array<entt::entity, input::RawInputStream::MaxMousePtrCount>
        m_cursors =
            MakeArray<entt::entity, input::RawInputStream::MaxMousePtrCount>(
                entt::null);

    void AddCursor(size_t mouseIdx)
    {
        namespace ui = oge::runtime::ui;
        assert(m_cursors[mouseIdx] == entt::null);
        auto& cursor = m_cursors[mouseIdx];
        ui::UISprite crossSprite{
            .sprite = m_ctx.assets.LoadTexture("cursors/hand_open.png")};
        cursor = m_uiWorld.create();
        m_uiWorld.emplace<ui::UIRect>(
            cursor, math::vec2{},
            math::vec2{0.02f, 0.02f * m_ctx.assets.backend.SwapchainAspect()});
        m_uiWorld.emplace<ui::UISprite>(cursor, crossSprite);
        m_uiWorld.emplace<ui::UIZLevel>(cursor, 1);
    }

    void RemoveCursor(size_t mouseIdx)
    {
        if (m_uiWorld.valid(m_cursors[mouseIdx]))
            m_uiWorld.destroy(m_cursors[mouseIdx]);
        m_cursors[mouseIdx] = entt::null;
    }

    void UpdateCursors(SceneExt::Frame& f)
    {
        namespace ui = oge::runtime::ui;
        if (m_showingCursor)
        {
            auto cursor = f.is.LastFrameCursor();
            oge::input::InputEvent e;
            while (f.is.PollEvent(cursor, e))
            {
                switch (e.type)
                {
                    case oge::input::InputEventType::AddMouse:
                        AddCursor(e.mouse.ptrIdx());
                        break;
                    case oge::input::InputEventType::RemoveMouse:
                        RemoveCursor(e.mouse.ptrIdx());
                        break;
                    case oge::input::InputEventType::MouseButtonDown:
                        if (m_uiWorld.valid(m_cursors[e.mouse.ptrIdx()]))
                            m_uiWorld.emplace_or_replace<ui::UISprite>(
                                m_cursors[e.mouse.ptrIdx()],
                                m_ctx.assets.LoadTexture(
                                    "cursors/hand_closed.png"));
                        break;
                    case oge::input::InputEventType::MouseButtonUp:
                        if (m_uiWorld.valid(m_cursors[e.mouse.ptrIdx()]))
                            m_uiWorld.emplace_or_replace<ui::UISprite>(
                                m_cursors[e.mouse.ptrIdx()],
                                m_ctx.assets.LoadTexture(
                                    "cursors/hand_open.png"));
                        break;
                    default:
                        break;
                }
            }
            for (auto ptr : f.is.ActivePtrs())
            {
                if (!f.is.IsMouse(ptr) || !m_uiWorld.valid(m_cursors[ptr]))
                    continue;
                auto rect = m_uiWorld.get<ui::UIRect>(m_cursors[ptr]);
                math::vec2 extent = m_ctx.assets.backend.SwapchainExtent();
                math::vec2 pos = f.is.PollPtrLatest(ptr, cursor);
                rect.pos = pos - (rect.extent * 0.5f);
                m_uiWorld.emplace_or_replace<ui::UIRect>(m_cursors[ptr], rect);
            }
        }
        if (m_nextShowingCursor != m_showingCursor)
        {
            if (!m_nextShowingCursor)
            {
                for (auto ptr : f.is.ActivePtrs())
                {
                    if (!f.is.IsMouse(ptr)) continue;
                    RemoveCursor(ptr);
                }
            }
            else
            {
                for (auto ptr : f.is.ActivePtrs())
                {
                    if (!f.is.IsMouse(ptr)) continue;
                    AddCursor(ptr);
                }
            }
            m_showingCursor = m_nextShowingCursor;
        }
    }
};
}  // namespace game

namespace oge::runtime
{
template <>
struct TypeName<game::SceneExt>
{
    static constexpr std::string Get()
    {
        return "core::SceneExt";
    }
};
}  // namespace oge::runtime
