#include "game/net/replication_registry.hpp"

#include "game/app_context.hpp"
#include "game/input/net.hpp"
#include "game/net/entity_component_stream.hpp"
#include "game/net/latest_streams.hpp"
#include "game/net/player_net_input_stream.hpp"
#include "game/net/terrain_net_stream.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/typed_registry.hpp"

using game::AnythingFactory;
using game::ReplicatedTag;
using namespace game::net;

static void RegisterTerrainReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<game::terrain::TerrainView>();
    desc.capabilities.Add<ReplicationCapability>(
        MakeReplicationCapability<TerrainChunkNetOutStream,
                                  TerrainChunkNetInStream>(desc.localId, SendType::Reliable, 0));
}

static void RegisterEntityReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<ReplicatedTag>();
    desc.capabilities.Add<ReplicationCapability>(
        MakeReplicationCapability<EntityEventNetOutputStream, EntityEventNetInputStream>(desc.localId, SendType::Reliable, 0)
    );
}

template <typename TAdapter>
static void RegisterLatestReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<ReplicatedTag>();
    desc.capabilities.Add<ReplicationCapability>(
        MakeReplicationCapability<LatestEventNetOutputStream<TAdapter>, LatestEventNetInputStream<TAdapter>>(desc.localId, SendType::Reliable, 0)
    );
}

template <typename TComponent, size_t Capacity = 128>
static void RegisterComponentReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<TComponent>();
    desc.capabilities.template Add<ReplicationCapability>(
        MakeReplicationCapability<EntityEventNetOutputStream, EntityEventNetInputStream>(desc.localId, SendType::Sequenced, 1)
    );
}

void game::net::RegisterReplications(AnythingFactory& af,
                                     ReplicationRegistry& rf)
{
    RegisterTerrainReplication(af);
    RegisterEntityReplication(af);
    RegisterComponentReplication<ComponentAABBCollider>(af);
    RegisterComponentReplication<ComponentCamera>(af);
    RegisterComponentReplication<ComponentPerspectiveCamera>(af);
    RegisterComponentReplication<ComponentPhysicBody>(af);
    RegisterComponentReplication<ComponentCreature>(af);
    RegisterComponentReplication<ComponentPlayer>(af);
    RegisterLatestReplication<PlayerInputLatestEventAdapter>(af);
    rf.RegisterFrom(af);
}
