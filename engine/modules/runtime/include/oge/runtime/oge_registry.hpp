#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>

#include "oge/macros.hpp"
#include "oge/runtime/debug.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace oge::runtime
{

// =========================================================================
// OgeRegistry  —  safe entt::registry wrapper
//
// Owns an entt::registry by value (stack-allocated).  All methods are
// inline so the compiler fully elides the wrapper in release builds.
// =========================================================================

class OgeRegistryRef
{
   public:
    using Entity = entt::entity;

    OgeRegistryRef(entt::registry* registry = nullptr) : m_registry(registry)
    {
    }
    OgeRegistryRef(entt::registry& registry) : m_registry(&registry)
    {
    }

    bool operator==(std::nullptr_t)
    {
        return m_registry == nullptr;
    }

    entt::registry& Raw()
    {
        return *m_registry;
    }
    const entt::registry& Raw() const
    {
        return *m_registry;
    }
    operator entt::registry&()
    {
        return *m_registry;
    }
    operator const entt::registry&() const
    {
        return *m_registry;
    }

    decltype(auto) ctx() { return m_registry->ctx(); }
    decltype(auto) ctx() const { return m_registry->ctx(); }

    // -- entity lifecycle -------------------------------------------------

    Entity create()
    {
        return m_registry->create();
    }
    Entity create(Entity hint)
    {
        return m_registry->create(hint);
    }

    void destroy(Entity e)
    {
        OGE_ASSERT(valid(e), "destroy() on invalid entity {}",
                   static_cast<uint32_t>(e));
        m_registry->destroy(e);
    }

    bool valid(Entity e) const
    {
        return m_registry->valid(e);
    }

    // -- views ------------------------------------------------------------

    template <typename... Comp, typename... Exclude>
    auto view(entt::exclude_t<Exclude...> excl = entt::exclude_t<Exclude...>{})
    {
        return m_registry->view<Comp...>(excl);
    }

    template <typename... Comp, typename... Exclude>
    auto view(
        entt::exclude_t<Exclude...> excl = entt::exclude_t<Exclude...>{}) const
    {
        return m_registry->view<const Comp...>(excl);
    }

    // -- component access -------------------------------------------------

    template <typename T>
    T& get(Entity e)
    {
        using RawT = std::remove_const_t<T>;
        OGE_ASSERT(valid(e), "get<{}>() on invalid entity {}",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        OGE_ASSERT(all_of<RawT>(e),
                   "get<{}>() on entity {} lacks the component",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        return m_registry->template get<T>(e);
    }

    template <typename T>
    const T& get(Entity e) const
    {
        using RawT = std::remove_const_t<T>;
        OGE_ASSERT(valid(e), "const get<{}>() on invalid entity {}",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        OGE_ASSERT(all_of<RawT>(e),
                   "const get<{}>() on entity {} lacks the component",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        return m_registry->template get<T>(e);
    }

    template <typename T>
    T* try_get(Entity e)
    {
        return m_registry->template try_get<T>(e);
    }

    template <typename T>
    const T* try_get(Entity e) const
    {
        return m_registry->template try_get<T>(e);
    }

    // -- component queries ------------------------------------------------

    template <typename T>
    bool all_of(Entity e) const
    {
        return m_registry->template all_of<T>(e);
    }

    template <typename... Comp>
    bool any_of(Entity e) const
    {
        return m_registry->template any_of<Comp...>(e);
    }

    // -- component mutation -----------------------------------------------

    template <typename T, typename... Args>
    decltype(auto) emplace(Entity e, Args&&... args)
    {
        OGE_ASSERT(valid(e), "emplace<{}>() on invalid entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        return m_registry->template emplace<T>(e, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    decltype(auto) emplace_or_replace(Entity e, Args&&... args)
    {
        OGE_ASSERT(valid(e), "emplace_or_replace<{}>() on invalid entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        return m_registry->template emplace_or_replace<T>(
            e, std::forward<Args>(args)...);
    }

    template <typename T>
    void remove(Entity e)
    {
        OGE_ASSERT(valid(e), "remove<{}>() on invalid entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        m_registry->template remove<T>(e);
    }

    void clear()
    {
        m_registry->clear();
    }

    // -- signals ----------------------------------------------------------

    template <typename T>
    auto on_construct()
    {
        return m_registry->template on_construct<T>();
    }

    template <typename T>
    auto on_update()
    {
        return m_registry->template on_update<T>();
    }

    template <typename T>
    auto on_destroy()
    {
        return m_registry->template on_destroy<T>();
    }

   protected:
    entt::registry* m_registry = nullptr;
};

class OgeRegistry : public OgeRegistryRef
{
   public:
    OgeRegistry()
        : OgeRegistryRef(nullptr)
        , m_owned(std::make_unique<entt::registry>())
    {
        m_registry = m_owned.get();
    }
    NO_COPY(OgeRegistry)

   private:
    std::unique_ptr<entt::registry> m_owned;
};

}  // namespace oge::runtime
