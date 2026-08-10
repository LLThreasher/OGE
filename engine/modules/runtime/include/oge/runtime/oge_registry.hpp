#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "oge/assert.hpp"
#include "oge/macros.hpp"
#include "oge/runtime/debug.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/type_name.hpp"

namespace oge::runtime
{

// =========================================================================
// OgeRegistry  —  safe entt::registry wrapper
//
// Owns an entt::registry by value (stack-allocated).  All methods are
// inline so the compiler fully elides the wrapper in release builds.
// =========================================================================

// #define CHECK_NULL_REGISTRY(fn) OGE_ASSERT(m_registry != nullptr, #fn "()
// called on nullptr")
#define CHECK_NULL_REGISTRY(fn)

template <typename Impl>
class OgeRegistryABC
{
   public:
    using Entity = entt::entity;

    entt::registry& Raw()
    {
        CHECK_NULL_REGISTRY(Raw);
        return static_cast<Impl*>(this)->Raw();
    }
    const entt::registry& Raw() const
    {
        CHECK_NULL_REGISTRY(Raw);
        return static_cast<const Impl*>(this)->Raw();
    }

    decltype(auto) ctx()
    {
        CHECK_NULL_REGISTRY(ctx);
        return Raw().ctx();
    }
    decltype(auto) ctx() const
    {
        CHECK_NULL_REGISTRY(ctx);
        return Raw().ctx();
    }

    // -- entity lifecycle -------------------------------------------------

    Entity create()
    {
        CHECK_NULL_REGISTRY(create);
        return Raw().create();
    }
    Entity create(Entity hint)
    {
        CHECK_NULL_REGISTRY(create);
        return Raw().create(hint);
    }

    void destroy(Entity e)
    {
        CHECK_NULL_REGISTRY(destroy);
        OGE_ASSERT(valid(e), "destroy() on invalid entity {}",
                   static_cast<uint32_t>(e));
        Raw().destroy(e);
    }

    bool valid(Entity e) const
    {
        CHECK_NULL_REGISTRY(valid);
        return Raw().valid(e);
    }

    // -- views ------------------------------------------------------------

    template <typename... Comp, typename... Exclude>
    auto view(entt::exclude_t<Exclude...> excl = entt::exclude_t<Exclude...>{})
    {
        CHECK_NULL_REGISTRY(view);
        return Raw().template view<Comp...>(excl);
    }

    template <typename... Comp, typename... Exclude>
    auto view(
        entt::exclude_t<Exclude...> excl = entt::exclude_t<Exclude...>{}) const
    {
        CHECK_NULL_REGISTRY(view);
        return Raw().template view<const Comp...>(excl);
    }

    // -- component access -------------------------------------------------

    template <typename T>
    T& get(Entity e)
    {
        CHECK_NULL_REGISTRY(get);
        using RawT = std::remove_const_t<T>;
        OGE_ASSERT(valid(e), "get<{}>() on invalid entity {}",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        OGE_ASSERT(all_of<RawT>(e),
                   "get<{}>() on entity {} lacks the component",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        return Raw().template get<T>(e);
    }

    template <typename T>
    const T& get(Entity e) const
    {
        CHECK_NULL_REGISTRY(get);
        using RawT = std::remove_const_t<T>;
        OGE_ASSERT(valid(e), "const get<{}>() on invalid entity {}",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        OGE_ASSERT(all_of<RawT>(e),
                   "const get<{}>() on entity {} lacks the component",
                   TypeName<RawT>::Get(), static_cast<uint32_t>(e));
        return Raw().template get<T>(e);
    }

    template <typename T>
    T* try_get(Entity e)
    {
        CHECK_NULL_REGISTRY(try_get);
        return Raw().template try_get<T>(e);
    }

    template <typename T>
    const T* try_get(Entity e) const
    {
        CHECK_NULL_REGISTRY(try_get);
        return Raw().template try_get<T>(e);
    }

    template <typename T1, typename T2, typename... Rest>
        requires (sizeof...(Rest) >= 0)
    auto try_get(Entity e)
    {
        CHECK_NULL_REGISTRY(try_get);
        return Raw().template try_get<T1, T2, Rest...>(e);
    }

    template <typename T1, typename T2, typename... Rest>
        requires (sizeof...(Rest) >= 0)
    auto try_get(Entity e) const
    {
        CHECK_NULL_REGISTRY(try_get);
        return Raw().template try_get<T1, T2, Rest...>(e);
    }

    template <typename T, typename... Func>
    decltype(auto) patch(const Entity e, Func&&... fn)
    {
        CHECK_NULL_REGISTRY(patch)
        OGE_ASSERT(valid(e), "patch<{}>() on invalid entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        OGE_ASSERT(all_of<T>(e), "patch<{}>() on invalid component on entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        return Raw().template patch<T>(e, std::forward<Func>(fn)...);
    }

    // -- component queries ------------------------------------------------

    template <typename T>
    bool all_of(Entity e) const
    {
        CHECK_NULL_REGISTRY(all_of);
        return Raw().template all_of<T>(e);
    }

    template <typename... Comp>
    bool any_of(Entity e) const
    {
        CHECK_NULL_REGISTRY(any_of);
        return Raw().template any_of<Comp...>(e);
    }

    // -- component mutation -----------------------------------------------

    template <typename T, typename... Args>
    decltype(auto) emplace(Entity e, Args&&... args)
    {
        CHECK_NULL_REGISTRY(emplace);
        OGE_ASSERT(valid(e), "emplace<{}>() on invalid entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        return Raw().template emplace<T>(e, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    decltype(auto) emplace_or_replace(Entity e, Args&&... args)
    {
        CHECK_NULL_REGISTRY(emplace_or_replace);
        OGE_ASSERT(valid(e), "emplace_or_replace<{}>() on invalid entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        return Raw().template emplace_or_replace<T>(
            e, std::forward<Args>(args)...);
    }

    template <typename T>
    void remove(Entity e)
    {
        CHECK_NULL_REGISTRY(remove);
        OGE_ASSERT(valid(e), "remove<{}>() on invalid entity {}",
                   TypeName<T>::Get(), static_cast<uint32_t>(e));
        Raw().template remove<T>(e);
    }

    void clear()
    {
        CHECK_NULL_REGISTRY(clear);
        Raw().clear();
    }

    // -- signals ----------------------------------------------------------

    template <typename T>
    auto on_construct()
    {
        CHECK_NULL_REGISTRY(remove);
        return Raw().template on_construct<T>();
    }

    template <typename T>
    auto on_update()
    {
        CHECK_NULL_REGISTRY(on_update);
        return Raw().template on_update<T>();
    }

    template <typename T>
    auto on_destroy()
    {
        CHECK_NULL_REGISTRY(on_destroy);
        return Raw().template on_destroy<T>();
    }

    template <typename T>
    auto erase(Entity e)
    {
        CHECK_NULL_REGISTRY(erase);
        return Raw().template erase<T>(e);
    }

    template <typename T>
    auto clear()
    {
        CHECK_NULL_REGISTRY(clear);
        return Raw().template clear<T>();
    }
};

class OgeRegistry : public OgeRegistryABC<OgeRegistry>
{
public:
    OgeRegistry() = default;
    NO_COPY(OgeRegistry)

    entt::registry& Raw()
    {
        return m_registry;
    }
    const entt::registry& Raw() const
    {
        return m_registry;
    }

   private:
    entt::registry m_registry;
};

class OgeRegistryRef : public OgeRegistryABC<OgeRegistryRef>
{
public:
    OgeRegistryRef(OgeRegistry& ref) : m_registry(ref.Raw())
    {
    }
    OgeRegistryRef(entt::registry& ref) : m_registry(ref)
    {
    }

    entt::registry& Raw()
    {
        return m_registry;
    }
    const entt::registry& Raw() const
    {
        return m_registry;
    }

   private:
    entt::registry& m_registry;
};

class OgeRegistryPtr : public OgeRegistryABC<OgeRegistryRef>
{
public:
    OgeRegistryPtr(std::nullptr_t) : m_registry(nullptr)
    {
    }

    OgeRegistryPtr(OgeRegistry* reg) : m_registry(&reg->Raw())
    {
    }

    OgeRegistryPtr(OgeRegistryRef* reg) : m_registry(&reg->Raw())
    {
    }

    OgeRegistryPtr(entt::registry* ref = nullptr) : m_registry(ref)
    {
    }

    bool operator==(std::nullptr_t)
    {
        return m_registry == nullptr;
    }

    OgeRegistryRef operator*()
    {
        return OgeRegistryRef{Raw()};
    }

    entt::registry& Raw()
    {
        OGE_ASSERT(m_registry != nullptr, "nullptr");
        return *m_registry;
    }
    const entt::registry& Raw() const
    {
        OGE_ASSERT(m_registry != nullptr, "nullptr");
        return *m_registry;
    }

   private:
    entt::registry* m_registry = nullptr;
};

}  // namespace oge::runtime
