#include "game/net/replication_registry.hpp"

#include "game/app_context.hpp"
#include "game/components.hpp"
#include "game/components_net.hpp"
#include "game/input/net.hpp"
#include "game/net/replication_events.hpp"
#include "game/net/rollback_event_log_stream.hpp"
#include "oge/runtime/typed_registry.hpp"

using game::AnythingFactory;
using game::ReplicatedTag;
using namespace game::net;

// ---------------------------------------------------------------------------
// Per-event-type capability registration.
// Each event type is registered separately with its own family id
// (= type_hash of the event struct), so that ReplicationRegistry
// dispatches to the correct apply function without a discriminator byte.
// ---------------------------------------------------------------------------

static void RegisterEntityEvents(AnythingFactory& af)
{
    // AddEntityEvent
    {
        auto& desc = af.RegisterType<AddEntityEvent>();
        desc.capabilities.Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<AddEntityEvent>(
                desc.localId, InstallAddEntityHooks,
                ReplicationMethod::SingleReliable));
    }

    // RemoveEntityEvent
    {
        auto& desc = af.RegisterType<RemoveEntityEvent>();
        desc.capabilities.Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<RemoveEntityEvent>(
                desc.localId, InstallRemoveEntityHooks,
                ReplicationMethod::SingleReliable));
    }
}

template <typename TComponent>
static void RegisterComponentEvents(AnythingFactory& af)
{
    // AddComponentEvent<T>
    {
        auto& desc = af.template RegisterType<AddComponentEvent<TComponent>>();
        desc.capabilities.template Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<AddComponentEvent<TComponent>>(
                desc.localId, InstallAddComponentHooks<TComponent>,
                ReplicationMethod::SingleSequenced));
    }

    // UpdateComponentEvent<T>
    {
        auto& desc =
            af.template RegisterType<UpdateComponentEvent<TComponent>>();
        desc.capabilities.template Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<UpdateComponentEvent<TComponent>>(
                desc.localId, InstallUpdateComponentHooks<TComponent>,
                ReplicationMethod::SingleSequenced));
    }

    // RemoveComponentEvent<T>
    {
        auto& desc =
            af.template RegisterType<RemoveComponentEvent<TComponent>>();
        desc.capabilities.template Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<RemoveComponentEvent<TComponent>>(
                desc.localId, InstallRemoveComponentHooks<TComponent>,
                ReplicationMethod::SingleSequenced));
    }
}

static void RegisterTerrainEvents(AnythingFactory& af)
{
    // AddChunkEvent
    {
        auto& desc = af.RegisterType<AddChunkEvent>();
        desc.capabilities.Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<AddChunkEvent>(
                desc.localId, InstallAddChunkHooks,
                ReplicationMethod::SingleReliable));
    }

    // RemoveChunkEvent
    {
        auto& desc = af.RegisterType<RemoveChunkEvent>();
        desc.capabilities.Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<RemoveChunkEvent>(
                desc.localId, InstallRemoveChunkHooks,
                ReplicationMethod::SingleReliable));
    }

    // UpdateChunkEvent
    {
        auto& desc = af.RegisterType<UpdateChunkEvent>();
        desc.capabilities.Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<UpdateChunkEvent>(
                desc.localId, InstallUpdateChunkHooks,
                ReplicationMethod::SingleReliable));
    }
}

void game::net::RegisterReplications(AnythingFactory& af,
                                     ReplicationRegistry& rf)
{
    RegisterEntityEvents(af);

    RegisterComponentEvents<ComponentAABBCollider>(af);
    rf.RegisterSnapshotComponent<ComponentAABBCollider>();

    RegisterComponentEvents<ComponentCamera>(af);
    rf.RegisterSnapshotComponent<ComponentCamera>();

    RegisterComponentEvents<ComponentPerspectiveCamera>(af);
    rf.RegisterSnapshotComponent<ComponentPerspectiveCamera>();

    RegisterComponentEvents<ComponentPhysicBody>(af);
    rf.RegisterSnapshotComponent<ComponentPhysicBody>();

    RegisterComponentEvents<ComponentCreature>(af);
    rf.RegisterSnapshotComponent<ComponentCreature>();

    RegisterComponentEvents<ComponentPlayer>(af);
    rf.RegisterSnapshotComponent<ComponentPlayer>();

    RegisterTerrainEvents(af);

    // Player input replication
    {
        auto& desc = af.RegisterType<PlayerInputReplicationEvent>();
        desc.capabilities.Add<ReplicationCapability>(
            MakeSimpleReplicationCapability<PlayerInputReplicationEvent>(
                desc.localId, nullptr,
                ReplicationMethod::SingleReliable));
    }

    // AdvanceTick — server→client tick sync; drives rollback snapshots
    {
        auto& desc = af.RegisterType<AdvanceTick>();
        ReplicationCapability tickCap;
        tickCap.family = desc.localId;
        tickCap.sendType = ReplicationMethod::SingleReliable;
        tickCap.installHooks = nullptr;
        tickCap.apply =
            [](EventLogStream<>&, oge::runtime::OgeRegistryRef world,
               net::Buffer& buffer)
        {
            AdvanceTick evt{};
            net::Deserialize(buffer, evt);
            if (world.ctx().contains<RollbackEventLogStream<>>())
            {
                auto& rbs = world.ctx().get<RollbackEventLogStream<>>();
                rbs.AdvanceTick(world);
            }
        };
        desc.capabilities.Add<ReplicationCapability>(tickCap);
    }

    rf.RegisterFrom(af);
}
