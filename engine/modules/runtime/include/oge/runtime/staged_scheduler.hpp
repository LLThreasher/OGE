#pragma once
#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

#include "oge/log.hpp"
#include "oge/runtime/tick_scheduler.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace oge::runtime
{

template <typename TCtx, typename TFrameCtx>
class Stage
{
   public:
    using Ctx = TCtx;
    using FrameCtx = TFrameCtx;

    virtual ~Stage() = default;

    virtual void onAttach(TCtx& ctx) = 0;
    virtual void onDetach(TCtx& ctx) = 0;
    virtual void onUpdate(TFrameCtx& ctx) = 0;

    // Concrete type id of this stage, set by BasePipeline::AddStage's
    // factory path.  0 = unknown (direct construction) — the duplicate
    // guard skips stages without an id.
    oge_id_type StageId() const { return id_; }
    void SetStageId(oge_id_type id) { id_ = id; }

   private:
    oge_id_type id_ = 0;
};

template <typename TControl, typename TStage, typename TFrameData = float>
class BasePipeline
{
    using TCtx = typename TStage::Ctx;
    using TFrameCtx = typename TStage::FrameCtx;

   public:
    BasePipeline(TCtx& ctx) : m_ctx(ctx)
    {
    }
    ~BasePipeline()
    {
        Clear();
    }

    template <typename TDerived>
    TDerived* AddStage(AnythingFactory& af)
    {
        return reinterpret_cast<TDerived*>(AddStage(af, af.Id<TDerived>()));
    }

    template <typename TDerived>
    TDerived* AddStage(AnythingFactory& af, typename TDerived::Def data)
    {
        return reinterpret_cast<TDerived*>(
            AddStage(af, af.Id<TDerived>(), data));
    }

    TStage* AddStage(AnythingFactory& af, oge_id_type id, entt::any data = {})
    {
        if (HasStageId(id))
        {
            LOG_WARN(
                "stage type {} already registered in pipeline; duplicate add "
                "ignored (a double add would run the stage twice per update)",
                id);
            return nullptr;
        }
        auto stage = af.BuildABC<TStage>(id, data);
        if (stage)
        {
            stage->SetStageId(id);
        }
        return AddStage(std::move(stage));
    }

    TStage* AddStage(std::unique_ptr<TStage> stage)
    {
        assert(stage);
        // Duplicate-stage guard: each stage type runs exactly once per
        // pipeline.  A second registration (double Load, duplicated config
        // list) would run the stage twice per update — double integration,
        // accelerated movement.  Stages built outside the factory path
        // carry no id (StageId() == 0) and bypass the check.
        const oge_id_type id = stage->StageId();
        if (id != 0 && HasStageId(id))
        {
            LOG_WARN(
                "stage type {} already registered in pipeline; duplicate add "
                "ignored (a double add would run the stage twice per update)",
                id);
            return nullptr;
        }
        m_stages.push_back(std::move(stage));
        assert(m_stages.back() != nullptr && "nullptr stage");
        m_stages.back()->onAttach(m_ctx);
        return m_stages.back().get();
    }

    std::unique_ptr<TStage> SwapOutStage(std::unique_ptr<TStage> newStage,
                                         TStage* oldStage)
    {
        auto it = FindStage(oldStage);
        if (it == m_stages.end()) return nullptr;

        it->get()->onDetach(m_ctx);

        std::unique_ptr<TStage> removed = std::move(*it);
        *it = std::move(newStage);

        it->get()->onAttach(m_ctx);

        return removed;
    }

    void RemoveStage(TStage* handle)
    {
        auto it = FindStage(handle);
        it->get()->onDetach(m_ctx);
        m_stages.erase(it);
    }

    void Update(TFrameData frame)
    {
        auto impl = static_cast<TControl*>(this);
        impl->onUpdate(frame, m_ctx,
                       [this](TFrameCtx ctx)
                       {
                           for (auto& stage : m_stages)
                           {
                               stage->onUpdate(ctx);
                           }
                       });
    }

    void Clear()
    {
        for (auto& stage : m_stages)
        {
            stage->onDetach(m_ctx);
        }
        m_stages.clear();
    }

   protected:
    auto FindStage(TStage* handle)
    {
        auto it = std::find_if(m_stages.begin(), m_stages.end(),
                               [handle](const auto& ptr)
                               { return ptr.get() == handle; });
        return it;
    }

    bool HasStageId(oge_id_type id) const
    {
        return std::find_if(m_stages.begin(), m_stages.end(),
                            [id](const auto& stage)
                            { return stage->StageId() == id; })
               != m_stages.end();
    }

    std::vector<std::unique_ptr<TStage>> m_stages;
    TCtx& m_ctx;
};

template <typename TControl, typename TStage, typename TFrameData = float>
using Pipeline = BasePipeline<TControl, TStage, TFrameData>;

template <typename TStage, typename TFrameData = float>
class FramePipeline
    : public Pipeline<FramePipeline<TStage, TFrameData>, TStage, TFrameData>
{
   public:
    using TPipeline =
        Pipeline<FramePipeline<TStage, TFrameData>, TStage, TFrameData>;
    FramePipeline(TStage::Ctx& ctx) : TPipeline(ctx)
    {
    }
    template <typename Fn>
    void onUpdate(TFrameData dt, typename TStage::Ctx& ctx, Fn&& update)
    {
        update(typename TStage::FrameCtx(dt, ctx));
    }
};

template <typename TStage, typename FrameData = float>
class FixedStepPipeline
    : public Pipeline<FixedStepPipeline<TStage, FrameData>, TStage, FrameData>
{
   public:
    using TPipeline =
        Pipeline<FixedStepPipeline<TStage, FrameData>, TStage, FrameData>;
    FixedStepPipeline(TStage::Ctx& ctx, float updateInterval = 1.f / 60.f)
        : TPipeline(ctx), m_tickScheduler(updateInterval)
    {
    }

    template <typename Fn>
    void onUpdate(FrameData frame, typename TStage::Ctx& ctx, Fn&& update)
    {
        float dt;
        if constexpr (std::is_same_v<FrameData, float>)
            dt = frame;
        else
            dt = frame.dt;

        if (!m_tickScheduler.Poll(dt)) return;
        float _dt = m_tickScheduler.ConsumeTick();
        while (_dt > 0.f)
        {
            if constexpr (std::is_same_v<FrameData, float>)
                frame = _dt;
            else
                frame.dt = _dt;
            update(typename TStage::FrameCtx(frame, ctx));
            _dt = m_tickScheduler.ConsumeTick();
        }
    }

    float GetAlpha() const
    {
        return m_tickScheduler.GetAlpha();
    }

    void SetUpdateInterval(float interval)
    {
        m_tickScheduler.SetInterval(interval);
    }

   private:
    TickScheduler m_tickScheduler;
};

}  // namespace oge::runtime
