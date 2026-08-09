#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include "oge/log.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/runtime/type_name.hpp"

namespace oge::runtime
{

class OGEContextReadOnly
{
   protected:
    OgeRegistryRef m_registry;

   public:
    OGEContextReadOnly(OgeRegistryRef registry) : m_registry(registry)
    {
    }

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
    OGEContext(OgeRegistryRef ref) : OGEContextReadOnly(ref)
    {
    }

    template <typename T, typename... Args>
    T& Emplace(Args... args)
    {
        return m_registry.ctx().emplace<T>(args...);
    }

    template <typename T, typename... Args>
    T& GetOrEmplace(Args... args)
    {
        if (m_registry.ctx().contains<T>())
        {
            return m_registry.ctx().get<T>();
        }
        return m_registry.ctx().emplace<T>(args...);
    }

    template <typename T>
    bool Erase()
    {
        return m_registry.ctx().erase<T>();
    }
};

struct ICapability
{
    virtual ~ICapability() = default;
};

class CapabilitySet
{
    std::unordered_map<std::type_index, std::unique_ptr<ICapability>> caps;

   public:
    CapabilitySet()
    {
    }
    CapabilitySet(const CapabilitySet&) = delete;
    CapabilitySet& operator=(const CapabilitySet&) = delete;

    CapabilitySet(CapabilitySet&&) noexcept = default;
    CapabilitySet& operator=(CapabilitySet&&) noexcept = default;

    template <typename T, typename... Args>
        requires std::constructible_from<T, Args...>
    T& Add(Args&&... args)
    {
        static_assert(std::is_base_of_v<ICapability, T>);
        auto [it, inserted] = caps.try_emplace(
            typeid(T), std::make_unique<T>(std::forward<Args>(args)...));
        return *static_cast<T*>(it->second.get());
    }

    template <typename T>
    T* Get()
    {
        auto it = caps.find(typeid(T));
        if (it == caps.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }
};

using FamilyId = oge_id_type;

class TypeRegistry;
struct FactoryCapability : ICapability
{
    using BuildFn = entt::any (*)(entt::any, TypeRegistry&);

    FamilyId family;
    BuildFn build = nullptr;

    FactoryCapability(FamilyId f, BuildFn b) : family(f), build(b)
    {
    }
};

struct TypeDescriptor
{
    std::string name;
    oge_id_type localId;

    CapabilitySet capabilities = {};
};

class TypeRegistry
{
    OGEContext& ctx;

    std::deque<TypeDescriptor> descs;
    std::unordered_map<std::string, size_t> byName;
    std::unordered_map<oge_id_type, size_t> byId;

    std::unordered_map<std::string, FamilyId> familyLookup;

   public:
    TypeRegistry(OGEContext& c) : ctx(c)
    {
    }

    std::deque<TypeDescriptor>& GetAll()
    {
        return descs;
    }

    // ----------------------------
    // Family registration
    // ----------------------------

    FamilyId RegisterFamily(std::string_view name)
    {
        oge_id_type id = entt::hashed_string{name.data()}.value();
        familyLookup[std::string(name)] = id;

        // record the name so name-based lookup works
        std::string key{name};
        if (!byName.contains(key))
        {
            descs.emplace_back(key, id);
            byName.emplace(std::move(key), descs.size() - 1);
            byId.emplace(id, descs.size() - 1);
        }
        return id;
    }

    FamilyId GetFamily(std::string_view name) const
    {
        return familyLookup.at(std::string(name));
    }

    // ----------------------------
    // Type identity
    // ----------------------------

    template <typename T>
    oge_id_type Id() const
    {
        return entt::type_hash<T>::value();
    }

    oge_id_type Id(std::string_view name) const
    {
        return descs[byName.at(std::string(name))].localId;
    }

    // ----------------------------
    // Type registration
    // ----------------------------

    template <typename T>
    TypeDescriptor& RegisterType()
    {
        std::string name = std::string(TypeName<T>::Get());
        oge_id_type id = Id<T>();

        auto idIt = byId.find(id);  // exact duplicate
        if (idIt != byId.end())
        {
            return descs[idIt->second];
        }

        auto nameIt = byName.find(name);  // same name, different id
        if (nameIt != byName.end())
        {
            // Replace the old entry's id (e.g. from RegisterFamily) with the
            // correct type hash.
            auto oldId = descs[nameIt->second].localId;
            descs[nameIt->second].localId = id;
            byId.erase(oldId);
            byId.emplace(id, nameIt->second);
            return descs[nameIt->second];
        }

        descs.emplace_back(name, id);
        TypeDescriptor& desc = descs.back();

        byName.emplace(name, descs.size() - 1);
        byId.emplace(id, descs.size() - 1);

        LOG_INFO("[TR] Registered type {} as {}", name, id);

        return descs.back();
    }

    // ----------------------------
    // ABC Base registration
    // ----------------------------

    template <typename TBase>
    void RegisterABC()
    {
        RegisterFamily(TypeName<TBase>::Get());
        RegisterType<TBase>();  // optional
    }

    // ----------------------------
    // Derived registration
    // ----------------------------

    template <typename TBase, typename TDerived>
    static entt::any BuildImpl(entt::any def, TypeRegistry& af)
    {
        std::unique_ptr<TBase> ptr;
        if constexpr (std::is_default_constructible_v<TDerived>)
        {
            ptr = std::make_unique<TDerived>();
        }
        else if constexpr (std::is_constructible_v<TDerived,
                                                   typename TDerived::Def>)
        {
            ptr = std::make_unique<TDerived>(
                entt::any_cast<typename TDerived::Def>(def));
        }
        else
        {
            ptr = std::make_unique<TDerived>(
                entt::any_cast<typename TDerived::Def>(def), af);
        }
        return ptr;
    }

    template <typename TBase, typename TDerived>
    void RegisterDerived()
    {
        auto& desc = RegisterType<TDerived>();

        FamilyId family = GetFamily(TypeName<TBase>::Get());

        desc.capabilities.template Add<FactoryCapability>(
            FactoryCapability{family, &BuildImpl<TBase, TDerived>});

        LOG_INFO("[TR] Registered derived {} for family {}",
                 TypeName<TDerived>::Get(), TypeName<TBase>::Get());
    }

    // ----------------------------
    // Runtime build (non-template)
    // ----------------------------

    entt::any Build(FamilyId family, oge_id_type typeId, entt::any def = {})
    {
        auto it = byId.find(typeId);
        if (it == byId.end()) return {};

        auto* factory = descs[it->second].capabilities.Get<FactoryCapability>();

        if (!factory || factory->family != family) return {};

        return factory->build(def, *this);
    }

    // Name-based build: one name has exactly one factory (enforced by the
    // by-name reuse logic in RegisterType).
    entt::any Build(std::string_view typeName, entt::any def = {})
    {
        auto it = byName.find(std::string(typeName));
        if (it == byName.end()) return {};

        auto* factory = descs[it->second].capabilities.Get<FactoryCapability>();
        if (!factory) return {};

        return factory->build(def, *this);
    }

    // ----------------------------
    // Typed wrapper (safe usage)
    // ----------------------------

    template <typename TBase>
    std::unique_ptr<TBase> BuildABC(oge_id_type typeId, entt::any def = {})
    {
        FamilyId family = GetFamily(TypeName<TBase>::Get());

        auto raw = Build(family, typeId, def);
        if (!raw) return nullptr;

        return entt::any_cast<std::unique_ptr<TBase>>(std::move(raw));
    }

    // ----------------------------
    // Descriptor access
    // ----------------------------

    TypeDescriptor* GetDescriptor(oge_id_type id)
    {
        auto it = byId.find(id);
        if (it == byId.end()) return nullptr;
        return &descs[it->second];
    }
};

using AnythingFactory = TypeRegistry;
}  // namespace oge::runtime
