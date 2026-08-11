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

// worldMask bit 0 mirrors an event family into the client's authoritative
// world (ClientScene2's variant 0).  That world owns the rollback snapshot
// stream, so it must track server truth for the families snapshots capture —
// entities and components.  Chunk families are excluded: the mirror has no
// TerrainView (apply would throw) and chunks are not part of rollback
// snapshots anyway.
static void SetMirrorMask(ReplicationCapability& cap)
{
    cap.worldMask.set(0);
}

static void RegisterEntityEvents(AnythingFactory& af)
{
    // AddEntityEvent
    {
        auto& desc = af.RegisterType<AddEntityEvent>();
        auto cap = MakeSimpleReplicationCapability<AddEntityEvent>(
            desc.localId, InstallAddEntityHooks,
            ReplicationMethod::SingleReliable);
        SetMirrorMask(cap);
        desc.capabilities.Add<ReplicationCapability>(cap);
    }

    // RemoveEntityEvent
    {
        auto& desc = af.RegisterType<RemoveEntityEvent>();
        auto cap = MakeSimpleReplicationCapability<RemoveEntityEvent>(
            desc.localId, InstallRemoveEntityHooks,
            ReplicationMethod::SingleReliable);
        SetMirrorMask(cap);
        desc.capabilities.Add<ReplicationCapability>(cap);
    }
}

template <typename TComponent>
static void RegisterComponentEvents(AnythingFactory& af)
{
    // AddComponentEvent<T>
    {
        auto& desc = af.template RegisterType<AddComponentEvent<TComponent>>();
        auto cap = MakeSimpleReplicationCapability<AddComponentEvent<TComponent>>(
            desc.localId, InstallAddComponentHooks<TComponent>,
            ReplicationMethod::SingleSequenced);
        SetMirrorMask(cap);
        desc.capabilities.template Add<ReplicationCapability>(cap);
    }

    // UpdateComponentEvent<T>
    {
        auto& desc =
            af.template RegisterType<UpdateComponentEvent<TComponent>>();
        auto cap =
            MakeSimpleReplicationCapability<UpdateComponentEvent<TComponent>>(
                desc.localId, InstallUpdateComponentHooks<TComponent>,
                ReplicationMethod::SingleSequenced);
        SetMirrorMask(cap);
        desc.capabilities.template Add<ReplicationCapability>(cap);
    }

    // RemoveComponentEvent<T>
    {
        auto& desc =
            af.template RegisterType<RemoveComponentEvent<TComponent>>();
        auto cap =
            MakeSimpleReplicationCapability<RemoveComponentEvent<TComponent>>(
                desc.localId, InstallRemoveComponentHooks<TComponent>,
                ReplicationMethod::SingleSequenced);
        SetMirrorMask(cap);
        desc.capabilities.template Add<ReplicationCapability>(cap);
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

    RegisterComponentEvents<UpdateTag<UpdateType::FixedStep>>(af);
    RegisterComponentEvents<UpdateTag<UpdateType::Realtime>>(af);

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

    // AdvanceTick — server→client tick sync; drives rollback snapshots.
    // Mirrored into the client's authoritative world: that world owns the
    // RollbackEventLogStream, so the routed apply snapshots clean server
    // truth instead of the prediction world (whose state includes client
    // predictions).
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
        SetMirrorMask(tickCap);
        desc.capabilities.Add<ReplicationCapability>(tickCap);
    }

    rf.RegisterFrom(af);
}
