#pragma once

#include <memory>
#include "game/view/submission_queue.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/gfx/uniform_arena.hpp"
#include "oge/runtime/staged_scheduler.hpp"
#include "oge/runtime/tick_scheduler.hpp"
#include "oge/runtime/ui/objects.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/timer.hpp"

namespace oge::runtime
{
struct AssetContext;
}

namespace game::view
{

struct RendererState
{
    entt::registry& world;
    entt::registry& renderWorld;
    entt::dispatcher& events;
    AssetContext assets;
};

struct RendererFrameData
{
    float dt;
    AssetContext& assets;
    SubmissionQueue& submissionQueue;
    float alpha = 0.f;
};

struct FRendererState
{
    float dt;
    float alpha;
    AssetContext& assets;
    SubmissionQueue& submissionQueue;
    const entt::registry& world;
    entt::registry& renderWorld;
    entt::dispatcher& events;

    FRendererState(RendererFrameData& frame, RendererState& state)
        : dt(frame.dt),
        alpha(frame.alpha),
          assets(frame.assets),
          world(state.world),
          renderWorld(state.renderWorld),
          submissionQueue(frame.submissionQueue),
          events(state.events)
    {
    }
};

class Renderer : public Stage<RendererState, FRendererState>
{
   public:
    Renderer(oge_id_type id) : Stage<RendererState, FRendererState>(id) {}
};

class RenderPipeline : public FramePipeline<Renderer, RendererFrameData>
{
   public:
    RenderPipeline(RendererState& state, AnythingFactory& af) : FramePipeline<Renderer, RendererFrameData>(state, af) {}
};

void RegisterRenderers(AnythingFactory& af);

class DebugInfoRenderer : public Renderer
{
    oge::runtime::TickScheduler tickScheduler{1.f};
    std::shared_ptr<ui::IFont> debugFont;
    std::string debugString;
    std::string gpuDebugString;
   public:
    DECL_ID(DebugInfoRenderer);
    DebugInfoRenderer() : Renderer(Id) {}
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
private:
    float m_duration = 0;
    uint32_t m_currentGPUMem = 0;
    uint32_t m_budgetGPUMem = 0;
};

class CameraRenderer : public Renderer
{
    void onViewPanelUpdate(entt::registry& world, entt::entity entity);
   public:
    DECL_ID(CameraRenderer);
    CameraRenderer() : Renderer(Id) {}
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};

class UIRenderer : public Renderer
{
   public:
    DECL_ID(UIRenderer);
    UIRenderer() : Renderer(Id) {}
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};
}  // namespace game::view
