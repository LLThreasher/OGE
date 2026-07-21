#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
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

    TStage* AddStage(std::unique_ptr<TStage> stage)
    {
        m_stages.push_back(std::move(stage));
        assert(m_stages.back() != nullptr && "nullptr stage");
        m_stages.back()->onAttach(m_ctx);
        return m_stages.back().get();
    }

    std::unique_ptr<TStage> SwapOutStage(std::unique_ptr<TStage> newStage, TStage* oldStage)
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
        auto it =
            std::find_if(m_stages.begin(), m_stages.end(), [handle](const auto& ptr) { return ptr.get() == handle; });
        return it;
    }

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
    TStage* AddStage()
    {
        return BasePipeline<TControl, TStage, TFrameData>::AddStage(this->m_factory.template BuildABC<TStage>(entt::type_hash<TSys>::value()));
    }
};

template <typename TControl, typename TStage, typename TFrameData = float>
class DefPipeline : public BasePipeline<TControl, TStage, TFrameData>
{
   public:
    DefPipeline(TStage::Ctx& ctx, AnythingFactory& af) : BasePipeline<TControl, TStage, TFrameData>(ctx, af) {}
    template <typename TSys>
    TStage* AddStage(TStage::Def def)
    {
        return BasePipeline<TControl, TStage, TFrameData>::AddStage(
            this->m_factory.template BuildABC<TStage>(entt::type_hash<TSys>::value(), def));
    }
};

template <typename TControl, typename TStage, typename TFrameData = float>
using Pipeline = std::conditional_t<IsABC<TStage>, DefPipeline<TControl, TStage, TFrameData>,
                                    DefaultPipeline<TControl, TStage, TFrameData>>;

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
    FixedStepPipeline(TStage::Ctx& ctx, AnythingFactory& af, float updateInterval = 1.f / 60.f)
        : TPipeline(ctx, af), m_tickScheduler(updateInterval)
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

    float GetAlpha() { return m_tickScheduler.GetAlpha(); }

   private:
    TickScheduler m_tickScheduler;
};

}  // namespace oge::runtime
