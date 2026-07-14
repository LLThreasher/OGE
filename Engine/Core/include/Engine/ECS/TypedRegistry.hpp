#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace OneGame::Engine
{
template <typename TBase>
class TypedRegistry
{
   public:
    template <typename T>
        requires std::derived_from<T, TBase> && std::default_initializable<T>
    void Register()
    {
        m_constructors.emplace(T::Name, []() { return std::make_unique<T>(); });
    }

    std::unique_ptr<TBase> Get(std::string_view name)
    {
        auto it = m_constructors.find(std::string(name));
        if (it == m_constructors.end()) return nullptr;
        return it->second();
    }

   private:
    std::unordered_map<std::string, std::function<std::unique_ptr<TBase>()>> m_constructors;
};

struct AppContext;

namespace DefBuilder
{
template <typename T>
std::vector<std::unique_ptr<T>> BuildABCVec(const std::vector<std::string>& defs, TypedRegistry<T>& reg)
{
    std::vector<std::unique_ptr<T>> res;
    res.reserve(defs.size());
    for (const auto& def : defs)
    {
        res.push_back(reg.Get(def));
    }
    return res;
}
template <typename T, typename TCtx>
std::vector<T> BuildVec(const std::vector<typename T::Def>& defs, TCtx& ctx)
{
    std::vector<T> res;
    res.reserve(defs.size());
    for (const auto& def : defs)
    {
        res.push_back(T::Build(def, ctx));
    }
    return res;
}
}  // namespace DefBuilder

template <typename TBase>
class ValueRegistry
{
   public:
    void Register(std::string name, TBase value) { m_values.emplace(name, value); }

    TBase& Get(std::string name) { return m_values.at(name); }

   private:
    std::unordered_map<std::string, TBase> m_values;
};
}  // namespace OneGame::Engine
