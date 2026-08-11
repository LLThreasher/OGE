#include "game/net/protocol.hpp"
#include <cstdint>

namespace game::net
{

json::Object SerializeType(const TypeRegistry& reg, oge_id_type typeId)
{
    return {
        {"id", typeId},
        {"name", reg.GetDescriptor(typeId)->name},
    };
}

static json::Str ReplicationMethodToString(ReplicationMethod method)
{
    switch (method)
    {
        case ReplicationMethod::SingleReliable:
            return "reliable";
        case ReplicationMethod::SingleSequenced:
            return "sequenced";
        case ReplicationMethod::StreamReliable:
            return "stream";
        default:
            return "unknown";
    }
}

json::Object SerializeReplicationCapacity(const ReplicationCapability& desc)
{
    return {
        {"family", desc.family},
        {"type", ReplicationMethodToString(desc.sendType)},
    };
}

json::Object SerializeAllReplications(const TypeRegistry& reg)
{
    return {
        {"replications",
         [&reg]() -> json::Array
         {
             json::Array arr;
             for (const auto& desc : reg.GetAll())
             {
                if (auto cap = desc.capabilities.Get<ReplicationCapability>())
                {
                    arr.push_back(SerializeReplicationCapacity(*cap));
                }
             }
             return arr;
         }()},
    };
}
}