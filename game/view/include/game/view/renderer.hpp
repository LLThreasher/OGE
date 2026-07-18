#pragma once

#include "game/view/submission_queue.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/staged_scheduler.hpp"

namespace oge::runtime
{
struct AssetContext;
}

namespace game::view
{

struct RendererState
{
    entt::registry& world;
    entt::dispatcher& events;
};

struct RendererFrameData
{
    float dt;
    AssetContext& assets;
    SubmissionQueue& submissionQueue;
};

struct FRendererState
{
    float dt;
    AssetContext& assets;
    SubmissionQueue& submissionQueue;
    const entt::registry& world;
    entt::dispatcher& events;

    FRendererState(RendererFrameData& frame, RendererState& state)
        : dt(frame.dt),
          assets(frame.assets),
          world(state.world),
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
   public:
    DECL_ID(DebugInfoRenderer);
    DebugInfoRenderer() : Renderer(Id) {}
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};
}  // namespace game::view
