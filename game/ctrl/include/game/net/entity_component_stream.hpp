#pragma once

#include <cstdint>
#include "game/input/entity_event_stream.hpp"
#include "game/net/reliable_streams.hpp"
#include "game/net/latest_streams.hpp"
#include "oge/runtime/net_traits.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game::net
{
using oge::runtime::OGEContext;

using EntityEventNetOutputStream =
    ReliableEventNetOutputStream<input::EntityEventStream>;

using EntityEventNetInputStream =
    ReliableEventNetInputStream<input::EntityEventStream>;

template <typename T, size_t Capacity = 256>
using ComponentEventNetOutputStream =
    LatestEventNetOutputStream<SimpleEventAdapter<input::ComponentEventStream<T, Capacity>>>;

template <typename T, size_t Capacity = 256>
using ComponentEventNetInputStream =
    LatestEventNetOutputStream<SimpleEventAdapter<input::ComponentEventStream<T, Capacity>>>;

}  // namespace game::net

DECL_NET_OBJ(game::input::EntityEvent, {
    uint8_t tyVal = static_cast<uint8_t>(self.type);
    visit(tyVal);
    uint32_t entity = static_cast<uint32_t>(self.entity);
    visit(entity);
})
