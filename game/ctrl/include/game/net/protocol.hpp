#pragma once

#include "game/net/replication_registry.hpp"
#include "oge/json.hpp"

namespace game::net
{
    namespace json = oge::json;

    json::Object SerializeType(const TypeRegistry& reg, oge_id_type typeId);
    json::Object SerializeReplicationCapacity(const ReplicationCapability& desc);
    json::Object SerializeAllReplications(const TypeRegistry& reg);
}
