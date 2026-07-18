#pragma once

#include "oge/runtime/entt.hpp"
#include "oge/runtime/staged_scheduler.hpp"
#include "game/view/submission_queue.hpp"

namespace oge::runtime
{
struct AssetContext;
}

namespace game::view
{

struct ViewPanel
{
    GameViewType activeSlot = GameViewType::Slot0;
    entt::entity activeCamera = entt::null;
};

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
};
}  // namespace oge::runtime
