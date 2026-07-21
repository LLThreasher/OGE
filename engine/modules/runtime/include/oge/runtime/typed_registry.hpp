#pragma once

#include <functional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <cassert>

#include "oge/log.hpp"
#include "oge/runtime/entt.hpp"

#define DECL_ID(Name)                           \
    static constexpr const char* IdStr = #Name; \
    static constexpr oge_id_type Id = entt::hashed_string(IdStr).value();

namespace oge::runtime
{
using oge_id_type = entt::id_type;

class AnythingFactory;

template <typename T>
concept IsABC = requires { typename T::Def; };

template <typename T, typename ABC>
concept BuildableToABC = requires(const typename ABC::Def& def, AnythingFactory& factory) {
    std::derived_from<T, ABC>;

    // must have static Id of correct type
    { T::Id } -> std::convertible_to<oge_id_type>;

    // must have static Build function
    { T::Build(def, factory) } -> std::same_as<std::unique_ptr<ABC>>;
};

template <typename T, typename ABC>
concept DefaultBuildableToABC = requires() {
    std::derived_from<T, ABC>;
    { T::Id } -> std::convertible_to<oge_id_type>;
    std::is_default_constructible_v<T>;
};

template <typename T>
concept Buildable = requires(const typename T::Def& def, AnythingFactory& factory) {
    typename T::Def;

    // must have static Build function
    { T::Build(def, factory) } -> std::same_as<T>;
};

template <typename T>
class DefaultABCFactory
{
    using id_type = oge_id_type;

   public:
    template <typename Derived>
        requires DefaultBuildableToABC<Derived, T>
    void Register()
    {
        builders.emplace(Derived::Id, []() { return std::unique_ptr<T>(std::make_unique<Derived>()); });
    }

    std::unique_ptr<T> Build(id_type id)
    {
        auto it = builders.find(id);
        if (it == builders.end()) return nullptr;

        return it->second();
    }

   private:
    std::unordered_map<id_type, std::function<std::unique_ptr<T>()>> builders;
};

template <typename T>
    requires IsABC<T>
class ABCFactory
{
    using def_type = typename T::Def;
    using id_type = oge_id_type;

   public:
    template <typename Derived>
        requires BuildableToABC<Derived, T>
    void Register()
    {
        builders.emplace(Derived::Id, &Derived::Build);
    }

    std::unique_ptr<T> Build(id_type id, const def_type& def, AnythingFactory& af)
    {
        auto it = builders.find(id);
        if (it == builders.end()) return nullptr;

        return it->second(def, af);
    }

   private:
    std::unordered_map<id_type, std::unique_ptr<T> (*)(const def_type&, AnythingFactory&)> builders;
};

class OGEContextReadOnly
{
   protected:
    entt::registry& m_registry;

   public:
    OGEContextReadOnly(entt::registry& registry) : m_registry(registry) {}

    template <typename T>
    T* Get()
    {
        return m_registry.ctx().find<T>();
    }

    template <typename... Args>
    std::tuple<Args*...> GetMultiple()
    {
        return {(m_registry.ctx().find<Args>())...};
    }
};

class OGEContext : public OGEContextReadOnly
{
   public:
    template <typename T, typename... Args>
    T* Emplace(Args... args)
    {
        return &m_registry.ctx().emplace<T>(args...);
    }
};

class AnythingFactory
{
    OGEContext& registry;

   public:
    AnythingFactory(OGEContext& ctx) : registry(ctx) {}
    template <typename T>
    void RegisterABC()
    {
        if constexpr (IsABC<T>)
        {
            registry.Emplace<ABCFactory<T>>();
        }
        else
        {
            registry.Emplace<DefaultABCFactory<T>>();
        }
    }

    template <typename TBase, typename TDrived>
        requires IsABC<TBase> && BuildableToABC<TDrived, TBase> || DefaultBuildableToABC<TDrived, TBase>
                 void RegisterDrived()
    {
        LOG_INFO("[AF] registering {}", TDrived::IdStr);
        if constexpr (IsABC<TBase>)
        {
            static_assert(BuildableToABC<TDrived, TBase>);
            registry.Get<ABCFactory<TBase>>()->template Register<TDrived>();
        }
        else
        {
            registry.Get<DefaultABCFactory<TBase>>()->template Register<TDrived>();
        }
    }

    template <typename T>
    std::unique_ptr<T> BuildABC(oge_id_type name)
    {
        auto factory = registry.Get<DefaultABCFactory<T>>();
        assert(factory);
        return factory->Build(name);
    }

    template <typename T>
        requires IsABC<T>
    std::unique_ptr<T> BuildABC(oge_id_type name, const T::Def& def)
    {
        auto factory = registry.Get<ABCFactory<T>>();
        assert(factory);
        return factory->Build(name, def, *this);
    }

    template <typename T>
        requires IsABC<T>
    std::vector<std::unique_ptr<T>> BuildABCVec(const std::vector<oge_id_type>& defs)
    {
        std::vector<std::unique_ptr<T>> res;
        typename T::Def def{};
        res.reserve(defs.size());
        for (const auto& id : defs)
        {
            res.push_back(this->BuildABC<T>(id, def));
        }
        return res;
    }

    template <typename T>
        requires IsABC<T>
    std::vector<std::unique_ptr<T>> BuildABCVec(const std::vector<std::tuple<oge_id_type, const typename T::Def>>& defs)
    {
        std::vector<std::unique_ptr<T>> res;
        res.reserve(defs.size());
        for (const auto& [id, def] : defs)
        {
            res.push_back(BuildABC<T>(id, def));
        }
        return res;
    }

    template <typename T>
        requires Buildable<T>
    T Build(const T::Def& def)
    {
        return T::Build(def, *this);
    }

    template <typename T>
        requires Buildable<T>
    std::vector<T> BuildVec(const std::vector<typename T::Def>& defs)
    {
        std::vector<T> res;
        res.reserve(defs.size());
        for (const auto& def : defs)
        {
            res.push_back(Build(def, *this));
        }
        return res;
    }
};
}  // namespace oge::runtime
