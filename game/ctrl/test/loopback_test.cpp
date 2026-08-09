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

        // Install hooks on server world so entity creation triggers replication.
        game::net::InstallEntityReplicationHooks(serverWorld);

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

    auto& stream =
        h.serverWorld.ctx().get<game::net::EventLogStream<>>();
    std::vector<std::byte> dp;
    game::net::EventLogEntryConstRef r{{}, dp};
    int count = 0;
    while (stream.PeekEvent(0, r))
    {
        if (r.entry.id == entt::type_hash<game::net::AddEntityEvent>::value())
            ++count;
        r.entry.cursor++;
    }
    CHECK(count >= N);
}

RUN_TESTS("Loopback Integration Tests")
