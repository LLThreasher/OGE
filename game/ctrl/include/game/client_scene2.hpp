#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>

#include "game/components.hpp"
#include "game/json.hpp"
#include "game/net/replication_events.hpp"
#include "game/net/replication_registry.hpp"
#include "game/net/rollback_capability.hpp"
#include "game/net/rollback_event_log_stream.hpp"
#include "game/scene.hpp"
#include "oge/log.hpp"
#include "oge/runtime/net_client.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/net_serializer.hpp"

namespace game
{
using oge::runtime::NetClient;
using oge::runtime::OnClientConnected;
using oge::runtime::OnClientConnectionTimeout;
using oge::runtime::OnClientDisconnected;
using oge::runtime::OnClientReceivePacket;

class ClientScene2 : public Scene
{
    NetClient& m_client;
    entt::dispatcher m_clientDispatcher;

    oge::runtime::TickScheduler m_serverTickScheduler{1 / 20.f};

    // Authoritative mirror world (WorldRouter variant 1): receives the
    // replication families flagged in their worldMask, mirrors server truth
    // without local prediction, and owns the RollbackEventLogStream.  The
    // rollback snapshots are taken from this world so they never contain
    // predicted state.
    GameWorld m_authoritativeWorld;
    net::RollbackEventLogStream<>& m_rollbackStream;
    net::WorldRouter m_worldRouter;
    net::ReplicationRegistry m_replicationRegistry;

    bool m_readyToQuit = false;

    void onDisconnected(OnClientDisconnected ctx)
    {
        LOG_ERROR("client disconnected");
        m_readyToQuit = true;
    }

    void onRecievePacket(OnClientReceivePacket ctx)
    {
        m_replicationRegistry.HandleIncoming(0, m_worldRouter, *ctx.data);
    }

   public:
    ClientScene2(const Def& def)
        : Scene(def),
          m_client(*m_ctx.any_ctx.Get<NetClient>()),
          m_rollbackStream(m_authoritativeWorld.ctx()
                               .emplace<::game::net::RollbackEventLogStream<>>(
                                   &m_ctx.any_factory)),
          m_worldRouter(m_world),
          m_replicationRegistry(::game::net::ReplicationRegistry::Def{
              m_rollbackStream, m_ctx.any_factory})
    {
        LOG_INFO("client scene loaded");

        // Route incoming events via the WorldRouter: the default world
        // (m_world) and the authoritative mirror each receive the families
        // flagged in their worldMask.  See the mask block below for which
        // families go where.
        m_worldRouter.AddWorldVariant(1, m_authoritativeWorld);

        // Store a base-class pointer in ctx so GetReplicationStream()
        // (used by PollPlayerInputs, etc.) finds the stream via exact-type
        // lookup without needing to include the rollback header.
        m_world.ctx().emplace<net::EventLogStream<>*>(&m_rollbackStream);

        RegisterReplications(m_ctx.any_factory, m_replicationRegistry);

        // Register rollback capability for physics bodies so predicted
        // updates are accepted by InsertPredicted.
        {
            std::bitset<net::MAX_WORLD_VARIANTS> bothMask{};
            bothMask.set(0);  // default (prediction) world
            bothMask.set(1);  // authoritative mirror

            // Authoritative-only: physics updates and AdvanceTick would
            // clobber locally-predicted state (or no-op) in the default
            // world — they belong on the mirror.
            std::bitset<net::MAX_WORLD_VARIANTS> authoritativeMask{};
            authoritativeMask.set(1);

            auto setMask = [&](entt::id_type typeId,
                               std::bitset<net::MAX_WORLD_VARIANTS> mask)
            {
                m_ctx.any_factory.GetDescriptor(typeId)
                    ->capabilities.Get<net::ReplicationCapability>()
                    ->worldMask = mask;
            };

            setMask(Id<net::AddEntityEvent>(), bothMask);
            setMask(Id<net::RemoveEntityEvent>(), bothMask);

            setMask(Id<net::AddComponentEvent<ComponentAABBCollider>>(),
                    bothMask);
            setMask(Id<net::UpdateComponentEvent<ComponentAABBCollider>>(),
                    bothMask);
            setMask(Id<net::RemoveComponentEvent<ComponentAABBCollider>>(),
                    bothMask);

            setMask(Id<net::AddComponentEvent<ComponentPhysicBody>>(),
                    bothMask);
            setMask(Id<net::UpdateComponentEvent<ComponentPhysicBody>>(),
                    authoritativeMask);
            setMask(Id<net::RemoveComponentEvent<ComponentPhysicBody>>(),
                    bothMask);

            // AdvanceTick snapshots the authoritative world — the only world
            // holding the RollbackEventLogStream in its ctx.  Without this
            // the client never takes rollback snapshots.
            setMask(Id<net::AdvanceTick>(), authoritativeMask);

            // The pong must reach the authoritative world: its apply calls
            // HandlePong on the RollbackEventLogStream that lives there.
            setMask(Id<net::RollbackPong>(), authoritativeMask);

            net::RollbackCapability physCap;
            physCap.family = entt::type_hash<
                net::UpdateComponentEvent<ComponentPhysicBody>>::value();
            physCap.getRegionKey = net::ComponentRegionKey<ComponentPhysicBody>;
            physCap.takeSnapshot =
                net::ComponentSnapshotFn<ComponentPhysicBody>;
            physCap.rollback = net::ComponentRollbackFn<ComponentPhysicBody>;
            physCap.compare = net::PhysicsBodyCompareFn;
            m_rollbackStream.RegisterRollbackCapability(physCap);
        }

        m_clientDispatcher.sink<OnClientReceivePacket>()
            .connect<&ClientScene2::onRecievePacket>(this);
        m_clientDispatcher.sink<OnClientDisconnected>()
            .connect<&ClientScene2::onDisconnected>(this);

        // Track the local player's input stream so PollPlayerInputs can flush
        // it to the server.  The stream component appears after the player
        // entity replicates (DebugVoxelView emplaces it on construct).
        net::InstallPlayerInputReplicationHooks(m_world);

        m_replicationRegistry.AddPeer(0, m_client.Host(), &m_world);

        // Snapshot every 3 server ticks (~150ms at 20Hz) for rollback.
        m_rollbackStream.m_snapshotInterval = 3;

        // send ready package
        auto packet = m_client.StartPacket(sizeof(uint32_t));
        packet.Write<uint32_t>(0);
        m_client.Send(packet, oge::runtime::SendType::Reliable);
    }

    ~ClientScene2()
    {
        m_client.Disconnect();
        m_ctx.any_ctx.Erase<NetClient>();
    }

    void Update(Frame f, SceneContext sctx) override
    {
        // (1) Receive server events (AdvanceTick → snapshot via apply fn)
        m_client.Poll(m_clientDispatcher, f.dt);
        if (m_readyToQuit)
        {
            sctx.nextScene = Id<Scene>();
            sctx.nextSceneArgs = {};
        }

        // (2) Flush terrain + input into replication stream
        // net::PollTerrainChunkEvents(m_world);
        net::PollPlayerInputs(m_world);

        // Advance the client tick (snapshot cadence).  Snapshots are taken
        // from the authoritative mirror — clean server truth, never
        // predicted state.
        m_rollbackStream.AdvanceLocalTick(m_authoritativeWorld);

        // (3) Run local simulation (prediction) — moves the player locally
        Scene::Update(f, sctx);

        // (4) Insert predicted physics for LocalPrediction-tagged entities
        for (auto [e, body] :
                m_world
                    .view<
                        RenderStrategyTag<RenderStrategy::LocalPrediction>,
                        ComponentPhysicBody>()
                    .each())
        {
            net::UpdateComponentEvent<ComponentPhysicBody> evt{e, body};
            m_rollbackStream.InsertPredicted(evt);
        }

        // (5) Validate predictions against server events.  Snapshots were
        // taken from the authoritative world (clean server truth), so a
        // rollback restores m_world to that state; the mirror itself never
        // diverges and is never rolled back.  After a rollback the stream
        // is waiting for a pong (validation suspended): keep pinging until
        // the server answers with its tick/cursor alignment.
        m_rollbackStream.ValidateLatest(m_world);
        if (m_rollbackStream.IsWaitingPong())
        {
            ::game::net::RollbackPing ping{
                m_rollbackStream.CurrentTick(),
                m_rollbackStream.PingCursor()};
            net::PushReplicationEvent(m_world, ping);
        }

        // (6) Prune old entries + send input to server
        m_rollbackStream.Update();

        m_replicationRegistry.ProduceAll(m_client, m_world);
    }

    // The authoritative mirror world (variant 1 of the WorldRouter).
    // Exposed for tests (see scene_test_harness.hpp).
    GameWorld& GetAuthoritativeWorld()
    {
        return m_authoritativeWorld;
    }
};
}  // namespace game

DECL_TYPE_NAME(game::ClientScene2, "core::ClientScene2")
