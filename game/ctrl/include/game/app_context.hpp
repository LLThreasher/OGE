#pragma once

#include <string_view>

#include "game/memory_context.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::AnythingFactory;
struct AppContext
{
    oge::runtime::OGEContext& any_ctx;
    AnythingFactory& any_factory;
    entt::dispatcher& events;
    MemoryContext& memory;
};

class AppRuntime
{
   protected:
    AppContext m_ctx;

   public:
    AppRuntime(AppContext ctx) : m_ctx(ctx)
    {
    }

    template <typename T>
    oge::runtime::oge_id_type Id()
    {
        return m_ctx.any_factory.Id<T>();
    }

    oge::runtime::oge_id_type Id(std::string_view name)
    {
        return m_ctx.any_factory.Id(name);
    }

    AnythingFactory& AF()
    {
        return m_ctx.any_factory;
    }
};
}  // namespace game
