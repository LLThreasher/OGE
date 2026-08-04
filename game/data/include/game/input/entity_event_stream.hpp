#pragma once

#include <string>
#include "oge/event_stream.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game::input
{

enum class EntityEventType : uint8_t
{
    Create,
    Destroy,
};

struct EntityEvent
{
    EntityEventType type;
    entt::entity entity;

    EntityEvent(EntityEventType ty = {}, entt::entity e = entt::null)
        : type(ty), entity(e)
    {
    }
};

class EntityEventStream : public oge::DiscreteEventStream<EntityEvent, 1024>
{
};

enum class ComponentDeltaType : uint8_t
{
    Add,
    Update,
    Remove
};

template <typename T>
struct ComponentDeltaEvent
{
    ComponentDeltaType type;
    entt::entity entity;
};

template <typename T, size_t Capacity = 1024>
struct ComponentDeltaStream
    : public oge::DiscreteEventStream<ComponentDeltaEvent<T>, Capacity>
{
};
}  // namespace game::input

namespace oge::runtime
{
template <>
struct TypeName<game::input::EntityEventStream>
{
    static constexpr std::string Get()
    {
        return "core::EntityEventStream";
    }
};

template <typename T, size_t capacity>
struct TypeName<game::input::ComponentDeltaStream<T, capacity>>
{
    static constexpr std::string Get()
    {
        return "core::ComponentDeltaStream<" + TypeName<T>::Get() + "," + std::to_string(capacity) + ">";
    }
};
}  // namespace oge::runtime
