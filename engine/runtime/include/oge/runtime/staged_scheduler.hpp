#pragma once
#include <functional>
#include <memory>
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
class Pipeline
{
    using TCtx = typename TStage::Ctx;
    using TFrameCtx = typename TStage::FrameCtx;

   public:
    Pipeline(TCtx& ctx, AnythingFactory& factory) : m_ctx(ctx), m_factory(factory) {}

    template <typename TSys>
    void AddStage()
    {
        AddStage(TSys::Id);
    }

    void AddStage(oge_id_type id)
    {
        m_stages.push_back(m_factory.BuildABC<TStage>(id));
        assert(m_stages.back());
        m_stages.back()->onAttach(m_ctx);
    }

    void RemoveStage(oge_id_type id)
    {
        auto it = std::find_if(m_stages.begin(), m_stages.end(), [id](const auto& s) { return s->id() == id; });
        if (it != m_stages.end())
        {
            it->onDetach(m_ctx);
            m_stages.erase(it);
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

   private:
    std::vector<std::unique_ptr<TStage>> m_stages;
    AnythingFactory& m_factory;
    TCtx& m_ctx;
};

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
