#pragma once

#include <memory>

#include "game/memory_context.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/gfx/uniform_arena.hpp"
#include "oge/runtime/staged_scheduler.hpp"
#include "oge/runtime/tick_scheduler.hpp"
#include "oge/runtime/ui/objects.hpp"
#include "oge/timer.hpp"

namespace oge::runtime
{
struct AssetContext;
}

namespace game::view
{

struct RendererState
{
    oge::runtime::OgeRegistryRef world;
    oge::runtime::OgeRegistryRef uiWorld;
    oge::runtime::OgeRegistryRef renderWorld;
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
    const oge::runtime::OgeRegistryRef world;
    const oge::runtime::OgeRegistryRef uiWorld;
    oge::runtime::OgeRegistryRef renderWorld;
    entt::dispatcher& events;
    MemoryContext& memory;

    FRendererState(RendererFrameData& frame, RendererState& state)
        : dt(frame.dt),
          alpha(frame.alpha),
          assets(frame.assets),
          world(state.world),
          uiWorld(state.uiWorld),
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
    RenderPipeline(RendererState&& state)
        : m_state(state), FramePipeline<Renderer, RendererFrameData>(m_state)
    {
    }
};

void RegisterRenderers(AnythingFactory& af);

class DebugInfoRenderer : public Renderer
{
   public:
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
    std::string debugString;
};

class CameraRenderer : public Renderer
{
    static void onViewPanelUpdate(oge::runtime::OgeRegistryRef gameWorld, oge::runtime::OgeRegistryRef uiWorld, entt::entity entity);

   public:
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};

class UIRenderer : public Renderer
{
   public:
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};

class GizmoRenderer : public Renderer
{
   public:
    void onAttach(RendererState&) override;
    void onDetach(RendererState&) override;
    void onUpdate(FRendererState&) override;
};
}  // namespace game::view

namespace oge::runtime
{
    using namespace game::view;

template <>
struct TypeName<Renderer>
{
    static constexpr std::string Get()
    {
        return "core::Renderer";
    }
};

template <>
struct TypeName<DebugInfoRenderer>
{
    static constexpr std::string Get()
    {
        return "core::DebugInfoRenderer";
    }
};

template <>
struct TypeName<CameraRenderer>
{
    static constexpr std::string Get()
    {
        return "core::CameraRenderer";
    }
};

template <>
struct TypeName<UIRenderer>
{
    static constexpr std::string Get()
    {
        return "core::UIRenderer";
    }
};

template <>
struct TypeName<GizmoRenderer>
{
    static constexpr std::string Get()
    {
        return "core::GizmoRenderer";
    }
};
}