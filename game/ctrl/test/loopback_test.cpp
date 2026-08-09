// =============================================================================
// Networking integration tests — server + client loopback
//
// Tests end-to-end entity replication through ENet in a single process.
// =============================================================================

#include <cstdio>
#include <cstdlib>

#include "game/components.hpp"
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

    // Server
    oge::runtime::OgeRegistry serverWorld;
    entt::dispatcher serverDispatcher;
    oge::runtime::NetServer server;
    game::net::ReplicationRegistry serverReg{
        game::net::ReplicationRegistry::Def{
            serverWorld.ctx().emplace<game::net::EventLogStream<>>(), types}};

    // Client
    oge::runtime::OgeRegistry clientWorld;
    entt::dispatcher clientDispatcher;
    oge::runtime::NetClient client;
    game::net::ReplicationRegistry clientReg{
        game::net::ReplicationRegistry::Def{
            clientWorld.ctx().emplace<game::net::EventLogStream<>>(), types}};

    bool serverReady = false;
    bool clientConnected = false;
    bool handshakeDone = false;

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
        serverReg.HandleIncoming(p.peerId, serverWorld, *p.data);
    }

    void onClientConnected(oge::runtime::OnClientConnected)
    {
        clientConnected = true;
        clientReg.AddPeer(0, client.Host());
    }

    void onClientPacket(oge::runtime::OnClientReceivePacket p)
    {
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
    std::vector<std::byte> dp;
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
    std::vector<std::byte> dp;
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

    auto& clientStream =
        h.clientWorld.ctx().get<game::net::EventLogStream<>>();
    std::vector<std::byte> dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    CHECK(h.pumpUntil([&] { return clientStream.PeekEvent(0, r); }));

    CHECK(r.entry.id == entt::type_hash<game::net::AddEntityEvent>::value());
    CHECK_EQ(r.entry.cursor, 1u);  // first event of the stream

    // The payload must round-trip the entity id.
    game::net::AddEntityEvent evt;
    net::Buffer payloadBuf{r.payload};
    payloadBuf.ToReadOnly();
    net::Deserialize(payloadBuf, evt);
    CHECK_EQ(evt.entity, e);
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

    auto& clientStream =
        h.clientWorld.ctx().get<game::net::EventLogStream<>>();
    std::vector<std::byte> dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    // PeekEvent(at=0) restarts at the tail, so advance the cursor explicitly.
    game::net::LogCursor cursor = 0;
    CHECK(h.pumpUntil([&] {
        while (clientStream.PeekEvent(0, r, cursor))
        {
            cursor = r.entry.cursor + 1;
            if (r.entry.id ==
                entt::type_hash<
                    game::net::AddComponentEvent<game::ComponentCreature>>::
                    value())
                return true;
        }
        return false;
    }));

    game::net::AddComponentEvent<game::ComponentCreature> evt;
    net::Buffer payloadBuf{r.payload};
    payloadBuf.ToReadOnly();
    net::Deserialize(payloadBuf, evt);
    CHECK_EQ(evt.entity, e);
    // Only the fields declared in DECL_NET_OBJ(ComponentCreature) travel.
    CHECK_EQ(evt.component.maxSpeed, creature.maxSpeed);
    CHECK_EQ(evt.component.initJumpSpeed, creature.initJumpSpeed);
}

RUN_TESTS("Loopback Integration Tests")
