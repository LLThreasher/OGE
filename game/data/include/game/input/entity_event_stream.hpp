#pragma once

#include "entt/entity/fwd.hpp"
#include "oge/event_stream.hpp"

namespace game
{
    enum class EntityEventType : uint32_t
    {
        Create,
        Destroy,
    };

    struct EntityEvent
    {
        EntityEventType type;
        entt::entity entity;
    };

    using EntityEventStream = oge::DiscreteEventStream<EntityEvent, 1024>;
}
