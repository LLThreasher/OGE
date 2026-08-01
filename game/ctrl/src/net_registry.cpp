#include "game/components.hpp"
#include "game/input/entity_event_stream.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/replication_registry.hpp"
#include "oge/runtime/typed_registry.hpp"

static void RegisterEntityReplication(oge::runtime::AnythingFactory& af)
{

    auto& desc = af.RegisterType<game::ReplicatedTag>();
    desc.capabilities.template Add<game::ReplicationCapability>(
        af.Id<game::ReplicatedTag>(),
        &game::EntityReplication::Encode,
        &game::EntityReplication::Decode,
        &game::EntityReplication::CreateState
    );
}

template<typename T>
static void RegisterEventReplication(oge::runtime::AnythingFactory& af)
{
    auto& desc = af.RegisterType<T>();
    desc.capabilities.template Add<game::ReplicationCapability>(
        af.Id<T>(),
        &game::EntityEventStreamReplication<T>::Encode,
        &game::EntityEventStreamReplication<T>::Decode,
        &game::EntityEventStreamReplication<T>::CreateState
    );
}

template<typename T>
static void RegisterComponentReplication(oge::runtime::AnythingFactory& af)
{
    auto& desc = af.RegisterType<T>();
    desc.capabilities.template Add<game::ReplicationCapability>(
        af.Id<T>(),
        &game::ComponentReplication<T>::Encode,
        &game::ComponentReplication<T>::Decode,
        &game::ComponentReplication<T>::CreateState
    );
}

void game::RegisterReplications(oge::runtime::AnythingFactory& af,
                                ReplicationRegistry& rf)
{
    RegisterEntityReplication(af);
    RegisterComponentReplication<ComponentPlayer>(af);
    RegisterEventReplication<input::PlayerInputStream>(af);
    rf.RegisterFrom(af);
}
