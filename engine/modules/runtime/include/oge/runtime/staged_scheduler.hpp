#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "oge/runtime/tick_scheduler.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace oge::runtime
{

template <typename TCtx, typename TFrameCtx>
class Stage
{
   protected:
    explicit Stage(oge_id_type id) : id_(id) {}

   public:
    using Ctx = TCtx;
    using FrameCtx = TFrameCtx;
    oge_id_type id() const noexcept { return id_; }

    virtual ~Stage() = default;

    virtual void onAttach(TCtx& ctx) = 0;
    virtual void onDetach(TCtx& ctx) = 0;
    virtual void onUpdate(TFrameCtx& ctx) = 0;

   private:
    oge_id_type id_;
};

template <typename TControl, typename TStage, typename TFrameData = float>
class BasePipeline
{
    using TCtx = typename TStage::Ctx;
    using TFrameCtx = typename TStage::FrameCtx;

   public:
    BasePipeline(TCtx& ctx, AnythingFactory& factory) : m_ctx(ctx), m_factory(factory) {}

    size_t AddStage(std::unique_ptr<TStage> stage)
    {
        m_stages.push_back(std::move(stage));
        assert(m_stages.back() && "stage not registered");
        m_stages.back()->onAttach(m_ctx);
        return m_stages.size() - 1;
    }

    void RemoveStage(size_t idx)
    {
        if (idx < m_stages.size())
        {
            m_stages[idx].onDetach(m_ctx);
            m_stages.erase(m_stages.begin() + idx);
        }
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

   protected:
    std::vector<std::unique_ptr<TStage>> m_stages;
    AnythingFactory& m_factory;
    TCtx& m_ctx;
};

template <typename TControl, typename TStage, typename TFrameData = float>
class DefaultPipeline : public BasePipeline<TControl, TStage, TFrameData>
{
public:
    DefaultPipeline(TStage::Ctx& ctx, AnythingFactory& af) : BasePipeline<TControl, TStage, TFrameData>(ctx, af) {}
    template <typename TSys>
    void AddStage()
    {
        BasePipeline<TControl, TStage, TFrameData>::AddStage(this->m_factory.template BuildABC<TStage>(TSys::Id));
    }
};

template <typename TControl, typename TStage, typename TFrameData = float>
class DefPipeline : public BasePipeline<TControl, TStage, TFrameData>
{
public:
    DefPipeline(TStage::Ctx& ctx, AnythingFactory& af) : BasePipeline<TControl, TStage, TFrameData>(ctx, af) {}
    template <typename TSys>
    void AddStage(TStage::Def def)
    {
        BasePipeline<TControl, TStage, TFrameData>::AddStage(this->m_factory.template BuildABC<TStage>(TSys::Id, def));
    }
};

template <typename TControl, typename TStage, typename TFrameData = float>
using Pipeline = std::conditional_t<IsABC<TStage>, DefPipeline<TControl, TStage, TFrameData>, DefaultPipeline<TControl, TStage, TFrameData>>;

template <typename TStage, typename TFrameData = float>
class FramePipeline : public Pipeline<FramePipeline<TStage, TFrameData>, TStage, TFrameData>
{
   public:
    using TPipeline = Pipeline<FramePipeline<TStage, TFrameData>, TStage, TFrameData>;
    FramePipeline(TStage::Ctx& ctx, AnythingFactory& af) : TPipeline(ctx, af) {}
    template <typename Fn>
    void onUpdate(TFrameData dt, typename TStage::Ctx& ctx, Fn&& update)
    {
        update(typename TStage::FrameCtx(dt, ctx));
    }
};

template <typename TStage, typename FrameData = float>
class FixedStepPipeline : public Pipeline<FixedStepPipeline<TStage, FrameData>, TStage, FrameData>
{
   public:
    using TPipeline = Pipeline<FixedStepPipeline<TStage, FrameData>, TStage, FrameData>;
    FixedStepPipeline(TStage::Ctx& ctx, AnythingFactory& af) : TPipeline(ctx, af) {}
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

   private:
    TickScheduler m_tickScheduler;
};

}  // namespace oge::runtime
