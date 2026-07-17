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
    using Ctx = TCtx;
    using FrameCtx = TFrameCtx;

   protected:
    explicit Stage(oge_id_type id) : id_(id) {}

   public:
    oge_id_type id() const noexcept { return id_; }

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

    void AddStage(oge_id_type id)
    {
        m_stages.push_back(m_factory.BuildABC<TStage>(id));
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
        auto& impl = static_cast<TControl*>(this);
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
class FramePipeline : Pipeline<TStage, FramePipeline<TStage>, TFrameData>
{
public:
    template <typename Fn>
    void onUpdate(TFrameData dt, typename TStage::Ctx& ctx, Fn&& update)
    {
        update(typename TStage::FrameCtx(dt, ctx));
    }
};

template <typename TStage>
class FixedStepPipeline : Pipeline<TStage, FixedStepPipeline<TStage>, float>
{
public:
    template <typename Fn>
    void onUpdate(float dt, typename TStage::Ctx& ctx, Fn&& update)
    {
        if (!m_tickScheduler.Poll(dt)) return;
        float _dt = m_tickScheduler.ConsumeTick();
        while (_dt > 0.f)
        {
            update(typename TStage::FrameCtx(_dt, ctx));
            _dt = m_tickScheduler.ConsumeTick();
        }
    }
private:
    TickScheduler m_tickScheduler;
};

}  // namespace oge::runtime
