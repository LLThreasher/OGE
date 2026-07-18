#pragma once

#include "game/sim/subsystem.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "game/input/input_srouce.hpp"

namespace game
{
using oge::runtime::OGEContext;
using oge::runtime::AssetContext;
using oge::runtime::AnythingFactory;
using oge::input::RawInputStream;

class Scene
{
    struct Ctx
    {
        OGEContext& ctx;
        view::SubmissionQueue& s_queue;
        AssetContext assets;

        Ctx(OGEContext& ctx) : ctx(ctx), s_queue(*ctx.Get<view::SubmissionQueue>()), assets(ctx) {}
    };

    std::optional<Ctx> m_ctx;
    sim::SubsystemPipeline m_subsystems;
    view::RenderPipeline m_renderers;
    input::InputPipeline m_inputs;

    entt::registry m_world;
    entt::dispatcher m_event;
    input::PlayerInputStream m_playerIn;

    sim::GameState m_gameState;
    view::RendererState m_renderState;
    input::InputContext m_inputState;

   public:
    Scene(AnythingFactory& af, WindowCtx& windowCtx, RawInputStream& is)
        : m_gameState(m_world, m_event),
          m_renderState(m_world, m_event),
          m_subsystems(m_gameState, af),
          m_renderers(m_renderState, af),
          m_inputState(windowCtx, is, m_playerIn, m_world),
          m_inputs(m_inputState, af)
    {
    }

    void Attach(OGEContext& ctx) { m_ctx.emplace(ctx); }

    void Detach() { m_ctx.reset(); }

    void Update(float dt)
    {
        m_inputs.Update(dt);
        m_subsystems.Update(dt);
        m_renderers.Update(view::RendererFrameData{dt, m_ctx.value().assets, m_ctx.value().s_queue});
    }
};
}  // namespace game
