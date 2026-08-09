#pragma once

#include <string>

#include "oge/event_stream.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "game/input/net.hpp"

namespace game::input
{

inline void EnsureEntity(
    entt::registry& world,
    entt::entity entity)
{
    if (entity == entt::null)
    {
        return;
    }

    if (!world.valid(entity))
    {
        auto _ = world.create(entity);
        (void)_;
    }
}

inline void EnsureReplicatedEntity(
    entt::registry& world,
    entt::entity entity)
{
    EnsureEntity(world, entity);

    if (entity == entt::null)
    {
        return;
    }

    if (!world.all_of<ReplicatedTag>(entity))
    {
        world.emplace<ReplicatedTag>(entity);
    }
}

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

class EntityEventStream
    : public oge::DiscreteEventStream<EntityEvent, 256>
{
   public:
    using Event = EntityEvent;
    using Base = oge::DiscreteEventStream<Event, 256>;
    using Cursor = typename Base::Cursor;

    using Base::Base;

    static void RegisterHooks(entt::registry& world)
    {
        if (!world.ctx().contains<EntityEventStream>())
        {
            world.ctx().emplace<EntityEventStream>();
        }

        /*
            Entity becomes visible to the remote.
        */
        world.on_construct<ReplicatedTag>()
            .template connect<
                +[](entt::registry& world, entt::entity entity)
                {
                    auto& stream =
                        world.ctx()
                            .template get<EntityEventStream>();

                    stream.Push(Event{
                        EntityEventType::Create,
                        entity,
                    });
                }>();

        /*
            Entity stops being visible to the remote.
        */
        world.on_destroy<ReplicatedTag>()
            .template connect<
                +[](entt::registry& world, entt::entity entity)
                {
                    auto& stream =
                        world.ctx()
                            .template get<EntityEventStream>();

                    stream.Push(Event{
                        EntityEventType::Destroy,
                        entity,
                    });
                }>();
    }

    static void Apply(
        entt::registry& world,
        Cursor& cursor)
    {
        auto& stream =
            world.ctx().template get<EntityEventStream>();

        Event event{};

        while (stream.PollOne(cursor, event))
        {
            Apply(world, event);
        }
    }

    static void Apply(
        entt::registry& world,
        const Event& event)
    {
        if (event.entity == entt::null)
        {
            return;
        }

        switch (event.type)
        {
            case EntityEventType::Create:
            {
                EnsureReplicatedEntity(world, event.entity);
                break;
            }

            case EntityEventType::Destroy:
            {
                if (world.valid(event.entity))
                {
                    world.destroy(event.entity);
                }

                break;
            }
        }
    }
};

enum class ComponentEventType : std::uint8_t
{
    Add,
    Update,
    Remove,
};

template <typename T>
struct ComponentEvent
{
    ComponentEventType type = ComponentEventType::Update;
    entt::entity entity = entt::null;

    /*
        Only meaningful for Add/Update.
        net::Serialize can skip this for Remove.
    */
    T component{};

    ComponentEvent(
        ComponentEventType ty = ComponentEventType::Update,
        entt::entity e = entt::null,
        const T& value = {})
        : type(ty)
        , entity(e)
        , component(value)
    {
    }
};

template <typename T, std::size_t Capacity = 256>
class ComponentEventStream
    : public oge::DiscreteEventStream<ComponentEvent<T>, Capacity>
{
   public:
    using Event = ComponentEvent<T>;
    using Base = oge::DiscreteEventStream<Event, Capacity>;
    using Cursor = typename Base::Cursor;

    using Base::Base;

    static void RegisterHooks(entt::registry& world)
    {
        if (!world.ctx().contains<ComponentEventStream<T, Capacity>>())
        {
            world.ctx().emplace<ComponentEventStream<T, Capacity>>();
        }

        /*
            Entity becomes replicated while already having T.
            Send initial component state.
        */
        world.on_construct<ReplicatedTag>()
            .template connect<
                +[](entt::registry& world, entt::entity entity)
                {
                    if (!world.template all_of<T>(entity))
                    {
                        return;
                    }

                    auto& stream =
                        world.ctx()
                            .template get<ComponentEventStream<T, Capacity>>();

                    const auto& component = world.template get<T>(entity);

                    stream.Push(Event{
                        ComponentEventType::Add,
                        entity,
                        component,
                    });
                }>();

        /*
            Component added to an already-replicated entity.
        */
        world.on_construct<T>()
            .template connect<
                +[](entt::registry& world, entt::entity entity)
                {
                    if (!world.template all_of<ReplicatedTag>(entity))
                    {
                        return;
                    }

                    auto& stream =
                        world.ctx()
                            .template get<ComponentEventStream<T, Capacity>>();

                    const auto& component = world.template get<T>(entity);

                    stream.Push(Event{
                        ComponentEventType::Add,
                        entity,
                        component,
                    });
                }>();

        /*
            Component changed on an already-replicated entity.
        */
        world.on_update<T>()
            .template connect<
                +[](entt::registry& world, entt::entity entity)
                {
                    if (!world.template all_of<ReplicatedTag>(entity))
                    {
                        return;
                    }

                    auto& stream =
                        world.ctx()
                            .template get<ComponentEventStream<T, Capacity>>();

                    const auto& component = world.template get<T>(entity);

                    stream.Push(Event{
                        ComponentEventType::Update,
                        entity,
                        component,
                    });
                }>();

        /*
            Component removed from an already-replicated entity.
        */
        world.on_destroy<T>()
            .template connect<
                +[](entt::registry& world, entt::entity entity)
                {
                    if (!world.template all_of<ReplicatedTag>(entity))
                    {
                        return;
                    }

                    auto& stream =
                        world.ctx()
                            .template get<ComponentEventStream<T, Capacity>>();

                    stream.Push(Event{
                        ComponentEventType::Remove,
                        entity,
                    });
                }>();
    }

    static void Apply(
        entt::registry& world,
        Cursor& cursor)
    {
        auto& stream =
            world.ctx().template get<ComponentEventStream<T, Capacity>>();

        Event event{};

        while (stream.PollOne(cursor, event))
        {
            Apply(world, event);
        }
    }

    static void Apply(
        entt::registry& world,
        const Event& event)
    {
        if (event.entity == entt::null)
        {
            return;
        }

        switch (event.type)
        {
            case ComponentEventType::Add:
            case ComponentEventType::Update:
            {
                EnsureEntity(world, event.entity);

                world.template emplace_or_replace<T>(
                    event.entity,
                    event.component);

                break;
            }

            case ComponentEventType::Remove:
            {
                if (world.valid(event.entity) &&
                    world.template all_of<T>(event.entity))
                {
                    world.template remove<T>(event.entity);
                }

                break;
            }
        }
    }
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

DECL_TYPE_NAME(game::input::EntityEventStream, "core::EntityEventStream")

namespace oge::runtime
{
template <typename T, size_t capacity>
struct TypeName<game::input::ComponentDeltaStream<T, capacity>>
{
    static constexpr std::string Get()
    {
        return "core::ComponentDeltaStream<" + TypeName<T>::Get() + "," + std::to_string(capacity) + ">";
    }
};
}  // namespace oge::runtime
