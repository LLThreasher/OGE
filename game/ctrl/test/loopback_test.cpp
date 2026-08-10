// =============================================================================
// End-to-end loopback integration tests — server + client through ENet
//
// Tests entity replication over a real ENet loopback connection in a single
// process.  Mirrors the DebugServerScene usage path:
//   hooks → EventLogStream → ProduceAll → ENet → HandleIncoming → no echo.
//
// Wire format under test (new protocol):
//   SingleReliable/SingleSequenced packet:
//     [eventId][cursor][sendPayloadTypeId][cursor][uint32 size][payload]
//   StreamReliable: meta packet [eventId][cursor] on channel 0 plus a
//     payload-only packet [sendPayloadTypeId][cursor][size][payload] on
//     channel 1.
//
// Deserialized events carry an empty receive mask (recieveMask = {}), so no
// peer — including the receiving client — can PeekEvent/TryDequeueEvent them.
// This is what prevents the client from echoing server events back through
// ProduceAll.
// =============================================================================

#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/net/replication_events.hpp"
#include "game/net/replication_registry.hpp"
#include "oge/runtime/net_client.hpp"
#include "oge/runtime/net_server.hpp"
#include "test_macros.hpp"

namespace net = oge::runtime::net;

namespace
{
constexpr uint16_t TEST_PORT = 23402;
constexpr float TEST_TIMEOUT_SEC = 5.0f;
constexpr float POLL_DT = 0.016f;
}  // namespace

// =============================================================================
// Minimal loopback test harness
// =============================================================================
struct LoopbackHarness
{
    oge::runtime::OgeRegistry metaWorld;
    oge::runtime::OGEContext octx{metaWorld.Raw()};
    oge::runtime::TypeRegistry types{octx};

    // Server — EventLogStream must be given the TypeRegistry so it can
    // resolve event ids for logging (mirrors server_scene.hpp, which passes
    // &m_ctx.any_factory).
    oge::runtime::OgeRegistry serverWorld;
    entt::dispatcher serverDispatcher;
    oge::runtime::NetServer server;
    game::net::ReplicationRegistry serverReg{
        game::net::ReplicationRegistry::Def{
            serverWorld.ctx().emplace<game::net::EventLogStream<>>(&types),
            types}};

    // Client
    oge::runtime::OgeRegistry clientWorld;
    entt::dispatcher clientDispatcher;
    oge::runtime::NetClient client;
    game::net::ReplicationRegistry clientReg{
        game::net::ReplicationRegistry::Def{
            clientWorld.ctx().emplace<game::net::EventLogStream<>>(&types),
            types}};

    bool serverReady = false;
    bool clientConnected = false;
    bool handshakeDone = false;

    // Traffic counters.  clientFamilies counts packets received by family id
    // (peeked before HandleIncoming consumes them), so tests can verify
    // delivery without PeekEvent — deserialized events are invisible to all
    // peers.
    int serverPackets = 0;
    int clientPackets = 0;
    std::unordered_map<game::net::oge_id_type, int> clientFamilies;

    LoopbackHarness()
    {
        game::net::RegisterReplications(types, serverReg);
        game::net::RegisterReplications(types, clientReg);

        // Install hooks on server world so entity/component creation triggers
        // replication (mirrors server_scene.hpp).
        game::net::InstallEntityReplicationHooks(serverWorld);
        game::net::InstallComponentReplicationHooks<game::ComponentCreature>(
            serverWorld);

        serverDispatcher
            .sink<oge::runtime::OnServerReceiveConnect>()
            .connect<&LoopbackHarness::onServerConnect>(this);
        serverDispatcher
            .sink<oge::runtime::OnServerReceivePacket>()
            .connect<&LoopbackHarness::onServerPacket>(this);

        clientDispatcher
            .sink<oge::runtime::OnClientConnected>()
            .connect<&LoopbackHarness::onClientConnected>(this);
        clientDispatcher
            .sink<oge::runtime::OnClientReceivePacket>()
            .connect<&LoopbackHarness::onClientPacket>(this);
    }

    ~LoopbackHarness()
    {
        serverDispatcher
            .sink<oge::runtime::OnServerReceiveConnect>()
            .disconnect<&LoopbackHarness::onServerConnect>(this);
        serverDispatcher
            .sink<oge::runtime::OnServerReceivePacket>()
            .disconnect<&LoopbackHarness::onServerPacket>(this);
        clientDispatcher
            .sink<oge::runtime::OnClientConnected>()
            .disconnect<&LoopbackHarness::onClientConnected>(this);
        clientDispatcher
            .sink<oge::runtime::OnClientReceivePacket>()
            .disconnect<&LoopbackHarness::onClientPacket>(this);
    }

    void onServerConnect(oge::runtime::OnServerReceiveConnect c)
    {
        serverReady = true;
        serverReg.AddPeer(c.peerId, c.peer, &serverWorld);
        handshakeDone = true;
    }

    void onServerPacket(oge::runtime::OnServerReceivePacket p)
    {
        ++serverPackets;
        serverReg.HandleIncoming(p.peerId, serverWorld, *p.data);
    }

    void onClientConnected(oge::runtime::OnClientConnected)
    {
        clientConnected = true;
        clientReg.AddPeer(0, client.Host());
    }

    void onClientPacket(oge::runtime::OnClientReceivePacket p)
    {
        ++clientPackets;

        // Peek the event family id with a separate buffer so it doesn't
        // disturb the packet for HandleIncoming.
        net::Buffer peek(p.data->Data());
        peek.ToReadOnly();
        game::net::oge_id_type id;
        peek.Read(id);
        clientFamilies[id]++;

        clientReg.HandleIncoming(0, clientWorld, *p.data);
    }

    bool start()
    {
        if (!server.Initialize(TEST_PORT, 2, 2)) return false;
        if (!client.Initialize(2)) return false;
        return client.Connect("127.0.0.1", TEST_PORT, 3000);
    }

    void poll()
    {
        server.Poll(serverDispatcher, POLL_DT, 0);
        serverReg.ProduceAll(server, serverWorld);
        client.Poll(clientDispatcher, POLL_DT, 0);
        // Mirror ClientScene2::Update — the client also runs ProduceAll.
        clientReg.ProduceAll(client, clientWorld);
    }

    bool waitForHandshake(float timeout = TEST_TIMEOUT_SEC)
    {
        float elapsed = 0.f;
        while (!handshakeDone)
        {
            poll();
            elapsed += POLL_DT;
            if (elapsed > timeout) return false;
        }
        return true;
    }

    // Poll both sides until `done` returns true, bounded to maxPolls so a
    // missed event fails the test instead of hanging the binary.
    template <typename Fn>
    bool pumpUntil(Fn&& done, int maxPolls = 200)
    {
        for (int i = 0; i < maxPolls; ++i)
        {
            poll();
            if (done()) return true;
        }
        return false;
    }
};

// =============================================================================
// Tests
// =============================================================================

TEST(loopback_connect_and_handshake)
{
    LoopbackHarness h;
    CHECK(h.start());
    CHECK(h.waitForHandshake());
    CHECK(h.serverReady);
    CHECK(h.clientConnected);
    CHECK(h.handshakeDone);
}

TEST(loopback_hooks_fire_on_entity_create)
{
    LoopbackHarness h;
    CHECK(h.start());
    CHECK(h.waitForHandshake());

    // Create a replicated entity on the server.
    auto e = h.serverWorld.create();
    h.serverWorld.emplace<game::ReplicatedTag>(e);

    // The hook should have enqueued an AddEntityEvent in the server's stream.
    auto& stream =
        h.serverWorld.ctx().get<game::net::EventLogStream<>>();
    game::net::SmallPayload dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    bool found = stream.PeekEvent(0, r);
    CHECK(found);
    CHECK(r.entry.id == entt::type_hash<game::net::AddEntityEvent>::value());
}

TEST(loopback_hooks_fire_multiple_entities)
{
    LoopbackHarness h;
    CHECK(h.start());
    CHECK(h.waitForHandshake());

    constexpr int N = 3;
    for (int i = 0; i < N; ++i)
    {
        auto e = h.serverWorld.create();
        h.serverWorld.emplace<game::ReplicatedTag>(e);
    }

    // PeekEvent(at=0) always restarts at the stream tail, so advance the
    // cursor explicitly (same idiom as el_peek_cursor).
    auto& stream =
        h.serverWorld.ctx().get<game::net::EventLogStream<>>();
    game::net::SmallPayload dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    int count = 0;
    for (game::net::LogCursor cursor = 0; stream.PeekEvent(0, r, cursor);)
    {
        if (r.entry.id == entt::type_hash<game::net::AddEntityEvent>::value())
            ++count;
        cursor = r.entry.cursor + 1;
    }
    CHECK(count >= N);
}

// =============================================================================
// Wire delivery: server event → ENet packet → client event stream
//
// Delivery is verified with the client's per-family packet counters (peeked
// before HandleIncoming).  PeekEvent is never used on the client: received
// events carry an empty receive mask by design, so the client must not see
// them locally — that would let ProduceAll echo them back.
// =============================================================================

TEST(loopback_wire_delivers_add_entity)
{
    LoopbackHarness h;
    CHECK(h.start());
    CHECK(h.waitForHandshake());

    // Create a replicated entity on the server; the hook enqueues an
    // AddEntityEvent which must travel over ENet into the client stream.
    auto e = h.serverWorld.create();
    h.serverWorld.emplace<game::ReplicatedTag>(e);

    using AddEv = game::net::AddEntityEvent;
    CHECK(h.pumpUntil(
        [&] { return h.clientFamilies[entt::type_hash<AddEv>::value()] > 0; }));

    // The event is stored on the client but visible to no peer.
    auto& clientStream =
        h.clientWorld.ctx().get<game::net::EventLogStream<>>();
    game::net::SmallPayload dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    CHECK(!clientStream.PeekEvent(0, r));

    // No echo: pumping the client's ProduceAll sends nothing back — the
    // server receives only the connection's packets.
    int serverPackets = h.serverPackets;
    for (int i = 0; i < 30; ++i) h.poll();
    CHECK_EQ(h.serverPackets, serverPackets);
}

TEST(loopback_wire_delivers_add_component)
{
    LoopbackHarness h;
    CHECK(h.start());
    CHECK(h.waitForHandshake());

    // ReplicatedTag + ComponentCreature → AddEntityEvent + AddComponentEvent.
    auto e = h.serverWorld.create();
    h.serverWorld.emplace<game::ReplicatedTag>(e);
    game::ComponentCreature creature{};
    creature.maxSpeed = 12.5f;
    creature.moveOrder = {3.f, 0.f, 4.f};
    h.serverWorld.emplace<game::ComponentCreature>(e, creature);

    using AddComp = game::net::AddComponentEvent<game::ComponentCreature>;
    CHECK(h.pumpUntil(
        [&] { return h.clientFamilies[entt::type_hash<AddComp>::value()] > 0; }));

    auto& clientStream =
        h.clientWorld.ctx().get<game::net::EventLogStream<>>();
    game::net::SmallPayload dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    CHECK(!clientStream.PeekEvent(0, r));

    int serverPackets = h.serverPackets;
    for (int i = 0; i < 30; ++i) h.poll();
    CHECK_EQ(h.serverPackets, serverPackets);
}

// =============================================================================
// DebugServerScene usage path
//
// Mirrors the two-packet handshake: connect → PlayerInfo → player entity +
// entity-id reply → ready packet → AddPeer + GenerateSnapshot.  Verifies
// snapshot delivery, update delivery, and the absence of client echoes.
// =============================================================================
struct ServerSceneHarness
{
    static constexpr uint16_t Port = TEST_PORT + 1;

    oge::runtime::OgeRegistry metaWorld;
    oge::runtime::OGEContext octx{metaWorld.Raw()};
    oge::runtime::TypeRegistry types{octx};

    // Server world — GameWorld like DebugServerScene so CreatePlayer works.
    game::GameWorld serverWorld;
    entt::dispatcher serverDispatcher;
    oge::runtime::NetServer server;
    game::net::ReplicationRegistry serverReg{
        game::net::ReplicationRegistry::Def{
            serverWorld.ctx().emplace<game::net::EventLogStream<>>(&types),
            types}};

    // Client world.
    game::GameWorld clientWorld;
    entt::dispatcher clientDispatcher;
    oge::runtime::NetClient client;
    game::net::ReplicationRegistry clientReg{
        game::net::ReplicationRegistry::Def{
            clientWorld.ctx().emplace<game::net::EventLogStream<>>(&types),
            types}};

    game::PlayerInfo playerInfo{
        std::array<uint8_t, 16>{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01},
        oge::math::vec3{1.f, 2.f, 3.f}};

    // Handshake state (mirrors DebugServerScene).
    ENetPeer* pending = nullptr;
    ENetPeer* pending2 = nullptr;
    bool replicationReady = false;
    bool clientReady = false;
    entt::entity playerEntity = entt::null;
    int serverPackets = 0;
    int clientPackets = 0;
    std::unordered_map<game::net::oge_id_type, int> clientFamilies;

    ServerSceneHarness()
    {
        game::net::RegisterReplications(types, serverReg);
        game::net::RegisterReplications(types, clientReg);

        // Same hook set as DebugServerScene's ctor.
        game::net::InstallEntityReplicationHooks(serverWorld);
        game::net::InstallComponentReplicationHooks<game::ComponentAABBCollider>(
            serverWorld);
        game::net::InstallComponentReplicationHooks<game::ComponentPhysicBody>(
            serverWorld);
        game::net::InstallComponentReplicationHooks<game::ComponentCreature>(
            serverWorld);
        game::net::InstallComponentReplicationHooks<game::ComponentCamera>(
            serverWorld);
        game::net::InstallComponentReplicationHooks<
            game::ComponentPerspectiveCamera>(serverWorld);
        game::net::InstallComponentReplicationHooks<game::ComponentPlayer>(
            serverWorld);

        serverDispatcher
            .sink<oge::runtime::OnServerReceiveConnect>()
            .connect<&ServerSceneHarness::onServerConnect>(this);
        serverDispatcher
            .sink<oge::runtime::OnServerReceivePacket>()
            .connect<&ServerSceneHarness::onServerPacket>(this);

        clientDispatcher
            .sink<oge::runtime::OnClientConnected>()
            .connect<&ServerSceneHarness::onClientConnected>(this);
        clientDispatcher
            .sink<oge::runtime::OnClientReceivePacket>()
            .connect<&ServerSceneHarness::onClientPacket>(this);
    }

    ~ServerSceneHarness()
    {
        serverDispatcher
            .sink<oge::runtime::OnServerReceiveConnect>()
            .disconnect<&ServerSceneHarness::onServerConnect>(this);
        serverDispatcher
            .sink<oge::runtime::OnServerReceivePacket>()
            .disconnect<&ServerSceneHarness::onServerPacket>(this);
        clientDispatcher
            .sink<oge::runtime::OnClientConnected>()
            .disconnect<&ServerSceneHarness::onClientConnected>(this);
        clientDispatcher
            .sink<oge::runtime::OnClientReceivePacket>()
            .disconnect<&ServerSceneHarness::onClientPacket>(this);
    }

    void onServerConnect(oge::runtime::OnServerReceiveConnect c)
    {
        if (pending != nullptr)
        {
            // Flow control, mirror of the scene.
            server.Disconnect(c.peer);
            return;
        }
        pending = c.peer;
    }

    void onServerPacket(oge::runtime::OnServerReceivePacket p)
    {
        ++serverPackets;
        if (p.peer == pending)
        {
            // Packet #1: PlayerInfo → create the player entity and reply
            // with its id (mirrors DebugServerScene::onServerReceivePacket).
            pending = nullptr;
            pending2 = p.peer;
            auto info = p.data->Read<game::PlayerInfo>();
            playerEntity = game::ComponentPlayer::CreatePlayer(serverWorld,
                                                               info);
            serverWorld.emplace<game::ReplicatedTag>(playerEntity);
            auto packet = server.StartPacket(sizeof(entt::entity));
            packet.Write(playerEntity);
            server.Send(p.peer, packet);
        }
        else if (p.peer == pending2)
        {
            // Packet #2: ready → replication begins with a snapshot.
            pending2 = nullptr;
            serverReg.AddPeer(p.peerId, p.peer, &serverWorld);
            replicationReady = true;
        }
        else
        {
            serverReg.HandleIncoming(p.peerId, serverWorld, *p.data);
        }
    }

    void onClientConnected(oge::runtime::OnClientConnected)
    {
        // Mirror ClientConnScene::onConnected.
        auto packet = client.StartPacket(sizeof(game::PlayerInfo));
        packet.Write(playerInfo);
        client.Send(packet, oge::runtime::SendType::Reliable);
    }

    void onClientPacket(oge::runtime::OnClientReceivePacket p)
    {
        ++clientPackets;
        if (!clientReady)
        {
            // First server packet: the raw entity-id reply.  Mirror
            // ClientScene2's ctor: AddPeer + ready packet.
            clientReady = true;
            clientReg.AddPeer(0, client.Host(), &clientWorld);
            auto packet = client.StartPacket(sizeof(uint32_t));
            packet.Write<uint32_t>(0);
            client.Send(packet, oge::runtime::SendType::Reliable);
            return;
        }
        // Replication events: count by family (peek without consuming), then
        // hand to the registry like ClientScene2::onRecievePacket.
        net::Buffer peek(p.data->Data());
        peek.ToReadOnly();
        game::net::oge_id_type id;
        peek.Read(id);
        clientFamilies[id]++;
        clientReg.HandleIncoming(0, clientWorld, *p.data);
    }

    bool start()
    {
        if (!server.Initialize(Port, 2, 3)) return false;
        if (!client.Initialize(3)) return false;
        return client.Connect("127.0.0.1", Port, 3000);
    }

    void poll()
    {
        server.Poll(serverDispatcher, POLL_DT, 0);
        serverReg.ProduceAll(server, serverWorld);
        client.Poll(clientDispatcher, POLL_DT, 0);
        // Mirror ClientScene2::Update.
        clientReg.ProduceAll(client, clientWorld);
    }

    bool waitForReplication(float timeout = TEST_TIMEOUT_SEC)
    {
        float elapsed = 0.f;
        while (!replicationReady)
        {
            poll();
            elapsed += POLL_DT;
            if (elapsed > timeout) return false;
        }
        return true;
    }

    template <typename Fn>
    bool pumpUntil(Fn&& done, int maxPolls = 200)
    {
        for (int i = 0; i < maxPolls; ++i)
        {
            poll();
            if (done()) return true;
        }
        return false;
    }
};

TEST(server_scene_replicates_player)
{
    ServerSceneHarness h;
    CHECK(h.start());
    CHECK(h.waitForReplication());

    using AddEntity = game::net::AddEntityEvent;
    using AddPlayer = game::net::AddComponentEvent<game::ComponentPlayer>;
    using AddPhysic = game::net::AddComponentEvent<game::ComponentPhysicBody>;
    using AddCreature = game::net::AddComponentEvent<game::ComponentCreature>;
    using AddCamera = game::net::AddComponentEvent<game::ComponentCamera>;
    using AddPersp =
        game::net::AddComponentEvent<game::ComponentPerspectiveCamera>;
    using AddAABB =
        game::net::AddComponentEvent<game::ComponentAABBCollider>;
    using UpdatePhysic =
        game::net::UpdateComponentEvent<game::ComponentPhysicBody>;

    // The snapshot must deliver the player entity + all its components.
    // ComponentPlayer is snapshotted last, so wait for it.
    CHECK(h.pumpUntil(
        [&] { return h.clientFamilies[entt::type_hash<AddPlayer>::value()] >= 1; }));
    CHECK(h.clientFamilies[entt::type_hash<AddEntity>::value()] >= 1);
    CHECK(h.clientFamilies[entt::type_hash<AddPhysic>::value()] >= 1);
    CHECK(h.clientFamilies[entt::type_hash<AddCreature>::value()] >= 1);
    CHECK(h.clientFamilies[entt::type_hash<AddCamera>::value()] >= 1);
    CHECK(h.clientFamilies[entt::type_hash<AddPersp>::value()] >= 1);
    CHECK(h.clientFamilies[entt::type_hash<AddAABB>::value()] >= 1);

    // Deserialized events must not be locally deliverable (no echo).
    auto& stream = h.clientWorld.ctx().get<game::net::EventLogStream<>>();
    game::net::SmallPayload dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    CHECK(!stream.PeekEvent(0, r));

    // Mutate the player's physics body on the server → an
    // UpdateComponentEvent must travel to the client.
    h.serverWorld.patch<game::ComponentPhysicBody>(
        h.playerEntity,
        [](game::ComponentPhysicBody& body) { body.pos.x += 5.f; });
    CHECK(h.pumpUntil(
        [&] { return h.clientFamilies[entt::type_hash<UpdatePhysic>::value()] >= 1; }));

    // With everything delivered, neither side may keep generating traffic:
    // the client must not echo events back to the server, and the server
    // must not retransmit anything.
    int serverBefore = h.serverPackets;
    int clientBefore = h.clientPackets;
    for (int i = 0; i < 60; ++i) h.poll();
    CHECK_EQ(h.serverPackets, serverBefore);
    CHECK_EQ(h.clientPackets, clientBefore);
}

RUN_TESTS("Loopback Integration Tests")
