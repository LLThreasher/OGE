#pragma once
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "oge/runtime/typed_registry.hpp"
#include "oge/runtime/tick_scheduler.hpp"

namespace oge::runtime
{

class IStage
{
   public:
    struct Def
    {
        float updateInterval;
        std::vector<std::tuple<oge_id_type, const Def>> children;
        std::vector<oge_id_type> processors;
    };
    virtual ~IStage() = default;
    virtual void Attach(OGEContext& ctx) = 0;
    virtual void Update(float dt) = 0;
    virtual void Detach(OGEContext& ctx) = 0;
};

template <typename... Args>
class IProcessor
{
    struct Def
    {
    };
    virtual void Attach(Args&... args) = 0;
    virtual void Update(float dt, Args&... args) = 0;
    virtual void Detach(Args&... args) = 0;
};

template<typename Impl, typename... Args>
concept IsStageImpl =
requires(Impl impl, float dt, Args&... args)
{
    {impl.onAttach(args...)};
    {impl.onDetach(args...)};
    {impl.onUpdate(dt, args...)};
};

template <typename Impl, typename... Args>
    requires IsStageImpl<Impl, Args...>
class Stage : public IStage
{
   public:
    void Attach(OGEContext& ctx) override
    {
        m_args = ctx.GetMultiple<Args...>();
        std::apply([&](auto*... args) { static_cast<Impl*>(this)->onAttach(*args...); }, m_args);
    }

    void Detach(OGEContext& ctx) override
    {
        std::apply([&](auto*... args) { static_cast<Impl*>(this)->onDetach(*args...); }, m_args);
    }

    void Update(float dt) override
    {
        std::apply([&](auto*... args) { static_cast<Impl*>(this)->onApply(dt, *args...); }, m_args);
    }

   private:
    std::tuple<Args*...> m_args;
};

template <typename Impl, typename... Args>
class ProcessorStage : public Stage<ProcessorStage<Impl, Args...>, Args...>
{
   public:
    using Processor = IProcessor<float, Args...>;

    ProcessorStage(std::vector<std::unique_ptr<Processor>> processors = {}) : m_processors(std::move(processors)) {}

    void onAttach(Args&... args)
    {
        for (auto& p : m_processors)
        {
            p->Attach(args...);
        }
    }

    void onDetach(Args&... args)
    {
        for (auto& p : m_processors.rbegin())
        {
            p->Detach(args...);
        }
    }

   protected:
    void RunUpdate(float dt, Args&... args)
    {
        for (auto& p : m_processors)
        {
            p->Apply(dt, args...);
        }
    }

   private:
    std::vector<std::unique_ptr<Processor>> m_processors;
};

template <typename Impl, typename... Args>
class FrameUpdateStage : public ProcessorStage<FrameUpdateStage<Impl, Args...>, Args...>
{
   public:
    using Processor = IProcessor<float, Args...>;
    using Parent = ProcessorStage<Args...>;

    FrameUpdateStage(std::vector<std::unique_ptr<Processor>> processors = {}) : Parent(std::move(processors)) {}

    static std::unique_ptr<IStage> Build(const IStage::Def& def, AnythingFactory& fa)
    {
        return std::make_unique<Impl>(fa.BuildABCVec<Processor>(def.processors));
    }

    void onUpdate(float dt, Args&... args) { Parent::RunUpdate(dt, args...); }
};

template <typename Impl, typename... Args>
class FixedUpdateStage : public ProcessorStage<Args...>
{
   public:
    using Processor = IProcessor<float, Args...>;
    using Parent = ProcessorStage<Args...>;

    FixedUpdateStage(float interval = 1.f / 60.f, std::vector<std::unique_ptr<Processor>> processors = {})
        : m_tickScheduler(interval), Parent(std::move(processors))
    {
    }

    static std::unique_ptr<IStage> Build(const IStage::Def& def, AnythingFactory& fa)
    {
        return std::make_unique<Impl>(fa.BuildABCVec<Processor>(def.processors));
    }

    void onUpdate(float dt, Args&... args)
    {
        m_tickScheduler.Poll(dt);
        while (auto fdt = m_tickScheduler.ConsumeTick())
        {
            Parent::RunUpdate(fdt, args...);
        }
    }

   private:
    TickScheduler m_tickScheduler;
};

class Pipeline : public IStage
{
   public:
    static std::unique_ptr<IStage> Build(const IStage::Def& def, AnythingFactory& fa)
    {
        return std::make_unique<Pipeline>(fa.BuildABCVec<IStage>(def.children));
    }

    Pipeline(std::vector<std::unique_ptr<IStage>> stages = {}) : m_stages(std::move(stages)) {}

    void Attach(OGEContext& ctx) override
    {
        for (auto& stage : m_stages)
        {
            stage->Attach(ctx);
        }
    }

    void Update(float dt) override
    {
        for (auto& stage : m_stages)
        {
            stage->Update(dt);
        }
    }

   private:
    std::vector<std::unique_ptr<IStage>> m_stages;
};

class Environment
{
public:
    Environment(OGEContext& ctx) : m_ctx(ctx) {}

private:
    std::unique_ptr<IStage> m_stage;
    OGEContext& m_ctx;
};
}  // namespace OneGame::Engine
