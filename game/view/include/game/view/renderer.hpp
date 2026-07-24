#pragma once

#include <memory>
#include "game/memory_context.hpp"
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
    MemoryContext& memory;
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
    MemoryContext& memory;

    FRendererState(RendererFrameData& frame, RendererState& state)
        : dt(frame.dt),
        alpha(frame.alpha),
          assets(frame.assets),
          world(state.world),
          renderWorld(state.renderWorld),
          submissionQueue(frame.submissionQueue),
          events(state.events),
          memory(state.memory)
    {
    }
};

class Renderer : public Stage<RendererState, FRendererState>
{
};

class RenderPipeline : public FramePipeline<Renderer, RendererFrameData>
{
    RendererState m_state;
   public:
    RenderPipeline(RendererState&& state, AnythingFactory& af) : m_state(state), FramePipeline<Renderer, RendererFrameData>(m_state, af) {}
};

void RegisterRenderers(AnythingFactory& af);

class DebugInfoRenderer : public Renderer
{
   public:
    DECL_ID(DebugInfoRenderer);
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
private:
    float m_duration = 0;
    uint32_t m_currentGPUMem = 0;
    uint32_t m_budgetGPUMem = 0;
    oge::runtime::TickScheduler tickScheduler{1.f};
    std::shared_ptr<ui::IFont> debugFont;
    std::string gpuDebugString;
};

class CameraRenderer : public Renderer
{
    void onViewPanelUpdate(entt::registry& world, entt::entity entity);
   public:
    DECL_ID(CameraRenderer);
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};

class UIRenderer : public Renderer
{
   public:
    DECL_ID(UIRenderer);
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};
}  // namespace game::view
