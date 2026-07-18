#pragma once

#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "oge/runtime/entt.hpp"

namespace oge::runtime
{
using oge_id_type = entt::id_type;

class AnythingFactory;

template<typename T>
concept IsABC =
requires {
    typename T::Def;
};

template<typename T, typename ABC>
concept BuildableToABC =
requires(const typename ABC::Def& def, AnythingFactory& factory)
{
    std::derived_from<T, ABC>;

    // must have static Id of correct type
    { T::Id } -> std::convertible_to<oge_id_type>;

    // must have static Build function
    { T::Build(def, factory) } -> std::same_as<std::unique_ptr<ABC>>;
};

template<typename T>
concept Buildable =
requires(const typename T::Def& def, AnythingFactory& factory)
{
    typename T::Def;

    // must have static Build function
    { T::Build(def, factory) } -> std::same_as<T>;
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
        return &m_registry.ctx().get<T>();
    }

    template <typename... Args>
    std::tuple<Args*...> GetMultiple()
    {
        return {(&m_registry.ctx().get<Args>())...};
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
        requires IsABC<T>
    void RegisterABC()
    {
        registry.Emplace<ABCFactory<T>>();
    }

    template <typename TBase, typename TDrived>
        requires IsABC<TBase> && BuildableToABC<TDrived, TBase>
    void RegisterDrived()
    {
        registry.Get<ABCFactory<TBase>>().template Register<TDrived>();
    }

    template <typename T>
        requires IsABC<T>
    std::unique_ptr<T> BuildABC(oge_id_type name, const T::Def& def)
    {
        auto factory = registry.Get<ABCFactory<T>>();
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
}  // namespace OneGame::Engine
