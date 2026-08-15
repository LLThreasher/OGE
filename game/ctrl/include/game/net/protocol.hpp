#pragma once

#include "game/net/replication_registry.hpp"
#include "oge/json.hpp"

namespace game::net
{
    namespace json = oge::json;

    // Handshake protocol version.  The first client handshake packet starts
    // with this value and the server's reply echoes it, so a stale binary on
    // either side fails loudly instead of misreading the other side's packet
    // layout (see DebugServerScene::onServerReceivePacket and
    // ClientConnScene::onConnected).  Bump on every wire-format change.
    // v1 = legacy unversioned handshake; v2 = tick-stamped movement frames +
    // ray-encoded actions (PR #8).
    constexpr uint32_t kProtocolVersion = 2;

    json::Object SerializeType(const TypeRegistry& reg, oge_id_type typeId);
    json::Object SerializeReplicationCapacity(const ReplicationCapability& desc);
    json::Object SerializeAllReplications(const TypeRegistry& reg);
}
