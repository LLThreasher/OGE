#pragma once

#include "oge/event_stream.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game::input
{
using oge::runtime::net::SimpleNetValue;

enum class EntityEventType : uint32_t
{
    Create,
    Destroy,
};

NET_OBJ(EntityEvent)
{
    SimpleNetValue<EntityEventType> type;
    SimpleNetValue<entt::entity> entity;

    NET_OBJ_FN
    {
        visit(self.type);
        visit(self.entity);
    }
};

class EntityEventStream : public oge::DiscreteEventStream<EntityEvent, 1024>
{
};
}  // namespace game::input

namespace oge::runtime {
template<>
struct TypeName<game::input::EntityEventStream>
{
    static consteval std::string_view Get()
    {
        return "core::PlayerInputStream";
    }
};
}
