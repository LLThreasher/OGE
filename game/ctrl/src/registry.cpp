#include "game/input/entity_event_stream.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/replication_registry.hpp"

void game::RegisterReplications(oge::runtime::AnythingFactory& af,
                                ReplicationRegistry& rf)
{
    {
        auto& desc = af.RegisterType<input::PlayerInputStream>();
        desc.capabilities.Add<ReplicationCapability>(
            af.Id<input::PlayerInputStream>(),
            &EntityEventStreamReplication<input::PlayerInputStream>::Encode,
            &EntityEventStreamReplication<input::PlayerInputStream>::Decode);
    }
    {
        auto& desc = af.RegisterType<input::EntityEventStream>();
        desc.capabilities.Add<ReplicationCapability>(
            af.Id<input::EntityEventStream>(),
            &EntityEventStreamReplication<input::EntityEventStream>::Encode,
            &EntityEventStreamReplication<input::EntityEventStream>::Decode);
    }
}
