#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "oge/log.hpp"
#include "oge/runtime/entt.hpp"

#define DECL_ID(Name)

namespace oge::runtime
{
using oge_id_type = entt::id_type;

template <typename T>
struct TypeName
{
    static constexpr std::string_view Get();
};

class OGEContextReadOnly
{
   protected:
    entt::registry& m_registry;

   public:
    OGEContextReadOnly(entt::registry& registry) : m_registry(registry)
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
};

struct ICapability
{
    virtual ~ICapability() = default;
};

class CapabilitySet
{
    std::unordered_map<std::type_index, std::unique_ptr<ICapability>> caps;

   public:
    template <typename T, typename... Args>
    T& Add(Args&&... args)
    {
        static_assert(std::is_base_of_v<ICapability, T>);
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *ptr;
        caps[typeid(T)] = std::move(ptr);
        return ref;
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

    FactoryCapability(FamilyId f, BuildFn b)
        : family(f), build(b) {}
};

struct TypeDescriptor
{
    std::string name;
    oge_id_type localId;

    CapabilitySet capabilities;
};

class TypeRegistry
{
    OGEContext& ctx;

    std::vector<TypeDescriptor> descs;
    std::unordered_map<std::string, TypeDescriptor> byName;
    std::unordered_map<oge_id_type, TypeDescriptor*> byId;

    std::unordered_map<std::string, FamilyId> familyLookup;

   public:
    TypeRegistry(OGEContext& c) : ctx(c)
    {
    }

    std::vector<TypeDescriptor>& GetAll()
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
        return byName.at(std::string(name)).localId;
    }

    // ----------------------------
    // Type registration
    // ----------------------------

    template <typename T>
    TypeDescriptor& RegisterType()
    {
        std::string name = std::string(TypeName<T>::Get());
        oge_id_type id = Id<T>();

        TypeDescriptor desc;
        desc.name = name;
        desc.localId = id;

        auto [it, inserted] = byName.emplace(name, std::move(desc));
        byId[id] = &it->second;

        LOG_INFO("[TR] Registered type {} as {}", name, id);

        return it->second;
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
        else if constexpr (std::is_constructible_v<TDerived, typename TDerived::Def>) {
            ptr = std::make_unique<TDerived>(entt::any_cast<typename TDerived::Def>(def));
        }
        else
        {
            ptr = std::make_unique<TDerived>(entt::any_cast<typename TDerived::Def>(def), af);
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
        if (it == byId.end()) return nullptr;

        auto* factory = it->second->capabilities.Get<FactoryCapability>();

        if (!factory || factory->family != family) return nullptr;

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
        return it->second;
    }
};

using AnythingFactory = TypeRegistry;
}  // namespace oge::runtime
