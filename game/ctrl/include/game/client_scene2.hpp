#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>

#include "game/components.hpp"
#include "oge/json.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/net/replication_events.hpp"
#include "game/net/replication_registry.hpp"
#include "game/net/rollback_capability.hpp"
#include "game/net/rollback_event_log_stream.hpp"
#include "game/scene.hpp"
#include "game/sim/player_sim_config.hpp"
#include "game/sim/subsystem_physics.hpp"
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

    // Authoritative mirror world (WorldRouter variant 1): receives the
    // replication families flagged in their worldMask, mirrors server truth
    // without local prediction, and owns the RollbackEventLogStream.  The
    // rollback snapshots are taken from this world so they never contain
    // predicted state.
    GameWorld m_authoritativeWorld;

    // The client's 20 tps parity sim (D9): the server's fixed stage list
    // over the authoritative world, driven with the same 3-sub-step loop on
    // the fixed frames.  The prediction world re-anchors to its state each
    // tick.
    sim::SubsystemPipeline m_authoritativeSim;
    net::RollbackEventLogStream<>& m_rollbackStream;
    net::WorldRouter m_worldRouter;
    net::ReplicationRegistry m_replicationRegistry;

    // Update-frame counter for the fixed cadence (D1): every
    // sim::kSubStepsPerTick updates are one fixed frame = one tick.
    uint32_t m_fixedFrameCounter = 0;

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

    // Phase 1: copy the authoritative mirror's bodies into the prediction
    // world.  The mirror holds server truth (replication + parity sim), so
    // this hard-corrects the prediction world — predictions made from it
    // then match the server until the next correction arrives.
    void ReanchorPredictionToMirror()
    {
        for (auto [e, body] :
                m_world
                    .view<
                        RenderStrategyTag<RenderStrategy::LocalPrediction>,
                        ComponentPhysicBody>()
                    .each())
        {
            auto* authBody =
                m_authoritativeWorld.try_get<ComponentPhysicBody>(e);
            if (authBody == nullptr)
            {
                continue;
            }
            body = *authBody;
        }
    }

   public:
    ClientScene2(const Def& def)
        : Scene(def),
          m_client(*m_ctx.any_ctx.Get<NetClient>()),
          m_authoritativeSim(
              {m_authoritativeWorld, m_ctx.events, m_ctx.memory},
              sim::kSubStepDt),
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

            // Replicated terrain must reach the authoritative world too —
            // its fixed pipeline (parity sim) collides against the same
            // chunks as the server (D9).  Note: doubles chunk traffic.
            setMask(Id<net::AddChunkEvent>(), bothMask);
            setMask(Id<net::RemoveChunkEvent>(), bothMask);
            setMask(Id<net::UpdateChunkEvent>(), bothMask);

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
        net::InstallPlayerActionReplicationHooks(m_world);

        // Both worlds' fixed stages observe the shared tick space (D1).
        m_world.ctx().emplace<sim::SimTickContext>();
        m_authoritativeWorld.ctx().emplace<sim::SimTickContext>();

        // The client's fixed pipeline ticks at 20 Hz aligned with the shared
        // tick space (D1/D2): 3 sub-steps of kSubStepDt per fixed frame —
        // the same cadence the server scene sets, so fixed-stage decisions
        // fire once per tick on both sides.
        m_subsystems.SetUpdateInterval(sim::kSubStepDt);
        SetFixedFrameDuration(sim::kFixedFrameDuration);

        // The authoritative world runs the fixed sim — it needs terrain like
        // the server world does (mirrors the scene Load() terrain section).
        // Chunks arrive via the replication bothMask above; the descriptor
        // matches the server's chunkViewDistance.
        {
            auto& blocks = m_authoritativeWorld.ctx()
                               .emplace<::game::terrain::BlockRegistry>();
            blocks.RegisterBlock("dirt", {"Dirt", "dirt.png", 1});
            blocks.RegisterBlock("wood", {"Wood", "wood_plank.png", 1});
            blocks.RegisterBlock("stone", {"Stone", "green_stone.png", 1});
            auto& desc = m_authoritativeWorld.ctx()
                             .emplace<::game::terrain::TerrainDesc>();
            desc.chunkViewDistance = 8;
            m_authoritativeWorld.ctx().emplace<::game::terrain::TerrainView>();
        }

        // The parity sim: the server's fixed stage list over the
        // authoritative world (no terrain stage — chunks replicate in).
        m_authoritativeSim.AddStage(
            m_ctx.any_factory,
            Id<sim::SubsystemPlayer<UpdateType::FixedStep>>());
        m_authoritativeSim.AddStage(
            m_ctx.any_factory,
            Id<sim::SubsystemCreature<UpdateType::FixedStep>>());
        m_authoritativeSim.AddStage(
            m_ctx.any_factory,
            Id<sim::SubsystemPhysics<UpdateType::FixedStep>>());

        // The mirror's player needs the input streams + sim state for the
        // parity sim — they are not replicated components.  Emplace them
        // when the physics body constructs in the authoritative world
        // (Phase 1: the mirror only receives the entity + AABBCollider +
        // PhysicBody families — ComponentPlayer is masked to the prediction
        // world.  Phase 2 extends the masks and can anchor this hook on
        // ComponentPlayer instead.)
        m_authoritativeWorld.on_construct<ComponentPhysicBody>()
            .template connect<+[](oge::runtime::OgeRegistryRef world,
                                  entt::entity entity)
                              {
                                  if (!world.all_of<input::PlayerInputStream>(
                                          entity))
                                  {
                                      world.emplace<input::PlayerInputStream>(
                                          entity);
                                      world.emplace<input::PlayerActionStream>(
                                          entity);
                                      world.emplace<input::PlayerSimInputState>(
                                          entity);
                                  }
                              }>();

        // PollPlayerInputs/PollPlayerActions push the aggregated frames into
        // the authoritative world's streams — the local 20 tps sim and the
        // server apply bit-identical frames (D3/D9).
        m_world.ctx().get<net::PlayerInputReplicationState>().mirrorWorld =
            oge::runtime::OgeRegistryPtr{&m_authoritativeWorld};
        m_world.ctx().get<net::PlayerActionReplicationState>().mirrorWorld =
            oge::runtime::OgeRegistryPtr{&m_authoritativeWorld};

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
        // (1) Fixed frame (D1): every sim::kSubStepsPerTick updates is one
        // tick in the shared tick space.  Runs BEFORE Poll so the
        // re-anchor lands ahead of this poll's server events: a server
        // correction arriving now reaches the mirror after the prediction
        // world was re-anchored to the pre-correction state, so the
        // prediction inserted post-sim (step 3) diverges from the
        // correction and rollback fires.  (Poll-first would apply the
        // correction to the mirror before the re-anchor — the prediction
        // would then match the post-correction state and rollback would
        // never trigger.)
        if (++m_fixedFrameCounter % sim::kSubStepsPerTick == 0)
        {
            // Advance the client tick (snapshot cadence).  Snapshots are
            // taken from the authoritative mirror — clean server truth,
            // never predicted state.
            m_rollbackStream.AdvanceLocalTick(m_authoritativeWorld);
            const uint32_t tick = m_rollbackStream.CurrentTick();

            // Both worlds' fixed stages observe the shared tick space.
            m_world.ctx().get<sim::SimTickContext>().currentTick = tick;
            m_authoritativeWorld.ctx().get<sim::SimTickContext>().currentTick =
                tick;

            // Aggregate this tick's input (D3): frames are stamped
            // tick - kInputPipelineDelayTicks and applied by the parity sim
            // this tick (dueTick = simTick - 1).  Pushes to the wire and the
            // mirror's streams; the prediction world's own ring receives the
            // local copy for its fixed stage.
            net::PollPlayerInputs(m_world,
                                  tick - input::kInputPipelineDelayTicks);
            net::PollPlayerActions(m_world,
                                   tick - input::kInputPipelineDelayTicks);

            // Drive the 20 tps parity sim (D9): the server's fixed stage
            // list over the authoritative world, 3 sub-steps per tick.
            auto& authTickCtx =
                m_authoritativeWorld.ctx().get<sim::SimTickContext>();
            for (uint8_t s = 0; s < sim::kSubStepsPerTick; ++s)
            {
                authTickCtx.subStepIdx = s;
                m_authoritativeSim.Update(sim::kSubStepDt);
            }

            // Re-anchor the prediction world to the parity sim's result.
            // Phase 1: the mirror sim is inert (component families are
            // prediction-world only), so this copies server truth — the
            // realtime prediction below then simulates from it.
            ReanchorPredictionToMirror();
        }

        // (2) Receive server events (AdvanceTick → snapshot via apply fn)
        m_client.Poll(m_clientDispatcher, f.dt);
        if (m_readyToQuit)
        {
            sctx.nextScene = Id<Scene>();
            sctx.nextSceneArgs = {};
        }

        // (3) Run local simulation (prediction) — moves the player locally
        Scene::Update(f, sctx);

        // Insert the predicted physics AFTER the sim: the payload must be
        // the client's genuine prediction (the locally-simulated body), not
        // a copy of mirror truth — otherwise validation always compares
        // server state against itself and rollback can never fire.  Inserted
        // every poll (3 per tick, all stamped m_currentTick); the per-family
        // map in ValidateLatest takes the last one, i.e. the fullest
        // prediction of the tick.
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

        // (4) Validate predictions against server events.  Snapshots were
        // taken from the authoritative world (clean server truth), so a
        // rollback restores m_world to that state; the mirror itself never
        // diverges and is never rolled back.  After a rollback the stream
        // is waiting for a pong (validation suspended): keep pinging until
        // the server answers with its tick/cursor alignment.
        m_rollbackStream.ValidateLatest(m_world);
        if (m_rollbackStream.IsWaitingPong())
        {
            // A rollback just restored the prediction world from the
            // snapshot (pre-correction state).  Re-anchor to the mirror
            // NOW: the mirror already holds the server's correction, and
            // without this the polls before the next fixed frame would
            // insert stale predictions from the restored snapshot — the
            // next validation would see the same divergence and roll back
            // again, forever.
            ReanchorPredictionToMirror();

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
    GameWorld* GetAuthoritativeWorld() override
    {
        return &m_authoritativeWorld;
    }
};
}  // namespace game

DECL_TYPE_NAME(game::ClientScene2, "core::ClientScene2")
