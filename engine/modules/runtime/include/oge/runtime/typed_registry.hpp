#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "oge/log.hpp"
#include "oge/runtime/entt.hpp"

#define DECL_ID(Name)                        \
    static const std::string_view name()     \
    {                                        \
        return entt::type_id<Name>().name(); \
    }

namespace oge::runtime
{
using oge_id_type = entt::id_type;

class AnythingFactory;

template <typename T>
concept IsABC = requires { typename T::Def; };

template <typename T, typename ABC>
concept BuildableToABC = requires() {
    std::derived_from<T, ABC>;
    typename T::Def;
    std::constructible_from<T, typename T::Def&&, AnythingFactory>;
};

template <typename T, typename ABC>
concept DefaultBuildableToABC = requires() {
    std::derived_from<T, ABC>;
    std::is_default_constructible_v<T>;
};

template <typename T>
concept Buildable =
    requires(const typename T::Def& def, AnythingFactory& factory) {
        typename T::Def;
        std::constructible_from<T, typename T::Def&&, AnythingFactory>;
    };

template <typename T>
class DefaultABCFactory
{
    using id_type = oge_id_type;

   public:
    template <typename Derived>
        requires DefaultBuildableToABC<Derived, T> || BuildableToABC<Derived, T>
    void Register(oge_id_type id)
    {
        if constexpr (std::derived_from<Derived, T> &&
                      std::is_default_constructible_v<Derived>)
        {
            static_assert(std::is_default_constructible_v<Derived>);
            default_builders.emplace(id, []() -> std::unique_ptr<T>
                                     { return std::make_unique<Derived>(); });
        }
        else if constexpr (std::constructible_from<Derived,
                                                typename Derived::Def&&>)
        {
            def_builders.emplace(
                id,
                [](entt::any& data) -> std::unique_ptr<T>
                {
                    assert(data);
                    return std::make_unique<Derived>(
                        std::move(entt::any_cast<typename Derived::Def>(data)));
                });
        }
        else
        {
            static_assert(BuildableToABC<Derived, T>);
            builders.emplace(
                id,
                [](entt::any& data, AnythingFactory& af) -> std::unique_ptr<T>
                {
                    return std::make_unique<Derived>(
                        std::move(entt::any_cast<typename Derived::Def>(data)), af);
                });
        }
    }

    std::unique_ptr<T> Build(id_type id, entt::any data, AnythingFactory& af)
    {
        {
            auto it = default_builders.find(id);
            if (it != default_builders.end()) return it->second();
        }
        {
            auto it = def_builders.find(id);
            if (it != def_builders.end()) return it->second(data);
        }
        {
            auto it = builders.find(id);
            if (it != builders.end()) return it->second(data, af);
        }
        return nullptr;
    }

   private:
    std::unordered_map<id_type, std::function<std::unique_ptr<T>()>>
        default_builders;
    std::unordered_map<id_type, std::function<std::unique_ptr<T>(entt::any&)>>
        def_builders;
    std::unordered_map<
        id_type, std::function<std::unique_ptr<T>(entt::any&, AnythingFactory)>>
        builders;
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
        registry.Emplace<DefaultABCFactory<T>>();
    }

    template <typename TBase, typename TDrived>
        requires IsABC<TBase> && BuildableToABC<TDrived, TBase> ||
                     DefaultBuildableToABC<TDrived, TBase>
                 void RegisterDrived()
    {
        oge_id_type id =
            entt::type_hash<TDrived>::value();  // !!!! this copy is very
                                                // important for Sys<>, I don't
                                                // know why
        LOG_INFO("[AF] registering {} as {}", TDrived::name(), id);
        registry.Get<DefaultABCFactory<TBase>>()->template Register<TDrived>(
            id);
    }

    template <typename T>
    std::unique_ptr<T> BuildABC(oge_id_type name, entt::any def = {})
    {
        LOG_DEBUG("adding ABC with id {}", name);
        auto factory = registry.Get<DefaultABCFactory<T>>();
        assert(factory);
        return factory->Build(name, def, *this);
    }

    template <typename T>
        requires IsABC<T>
    std::vector<std::unique_ptr<T>> BuildABCVec(
        const std::vector<oge_id_type>& defs)
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
    std::vector<std::unique_ptr<T>> BuildABCVec(
        const std::vector<std::tuple<oge_id_type, const typename T::Def>>& defs)
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
