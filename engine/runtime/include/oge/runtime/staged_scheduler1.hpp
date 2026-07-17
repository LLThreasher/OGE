#pragma once
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "oge/runtime/entt.hpp"

namespace oge::runtime
{

class OGEContextReadOnly
{
   protected:
    entt::registry& m_registry;

   public:
    explicit OGEContextReadOnly(entt::registry& registry) : m_registry(registry) {}

    template <typename T>
    T* Get()
    {
        return &m_registry.ctx().get<T>();
    }

    template <typename... Args>
    std::tuple<Args*...> GetMultiple()
    {
        return {(&m_registry.ctx().get<Args>())...};
    }
};

class OGEContextWrite : public OGEContextReadOnly
{
   public:
    explicit OGEContextWrite(entt::registry& registry) : OGEContextReadOnly(registry) {}

    template <typename T>
    T* Emplace()
    {
        return &m_registry.ctx().emplace<T>();
    }
};

class OGEResource
{
   public:
    virtual ~OGEResource() = default;

    virtual void Initialize(OGEContextWrite& context) = 0;
    virtual void Shutdown(OGEContextWrite& context) = 0;
};

class OGEPipeline
{
   public:
    virtual ~OGEPipeline() = default;

    virtual void Attach(OGEContextReadOnly& context) = 0;
    virtual void Detach() = 0;
    virtual void Update(float dt) = 0;
};

template <typename TCtx, typename Impl>
class OGEPipelineCtx : public OGEPipeline
{
   public:
    void Attach(OGEContextReadOnly& context)
    {
        m_ctx = TCtx(context);
        static_cast<Impl*>(this)->onAttach(m_ctx);
    }

    void Detach() { static_cast<Impl*>(this)->onDetach(m_ctx); }

    void Update(float dt) { static_cast<Impl*>(this)->onUpdate(m_ctx); }

   private:
    TCtx m_ctx;
};

template <typename TInitCtx, typename TCtx, typename Impl, typename... TProcess>
class OGEPipelineStaged : public OGEPipeline
{
   public:
    void Attach(OGEContextReadOnly& context)
    {
        m_ctx.emplace(context);
        std::apply([&](auto&&... args) { (args.onAttach(m_ctx.value), ...); }, m_processes);
    }

    void Detach()
    {
        std::apply([&](auto&&... args) { (args.onDetach(m_ctx.value), ...); }, m_processes);
        m_ctx.reset();
    }

    void Update(float dt)
    {
        static_cast<Impl*>(this)->onUpdate(
            dt, m_ctx,
            [this](TCtx& ctx) { std::apply([&](auto&&... args) { (args.onUpdate(ctx), ...); }, m_processes); });
    }

   private:
    std::optional<TInitCtx> m_ctx;
    std::tuple<TProcess...> m_processes;
};

class OGEEnvironment
{
    entt::registry m_registry;

    std::vector<std::unique_ptr<OGEResource>> m_resources;
    std::vector<std::unique_ptr<OGEPipeline>> m_pipelines;

   public:
    OGEEnvironment() = default;
    ~OGEEnvironment() = default;

    void AddResource(std::unique_ptr<OGEResource> resource)
    {
        OGEContextWrite ctx(m_registry);
        resource->Initialize(ctx);
        m_resources.emplace_back(std::move(resource));
    }

    void AddPipeline(std::unique_ptr<OGEPipeline> pipeline)
    {
        OGEContextReadOnly ctx(m_registry);
        pipeline->Attach(ctx);
        m_pipelines.emplace_back(std::move(pipeline));
    }

    void Update(float dt)
    {
        for (auto& p : m_pipelines) p->Update(dt);
    }

    void Shutdown()
    {
        OGEContextWrite ctx(m_registry);

        for (auto& p : m_pipelines) p->Detach();

        for (auto& r : m_resources) r->Shutdown(ctx);

        m_pipelines.clear();
        m_resources.clear();
    }
};
}  // namespace oge::runtime
