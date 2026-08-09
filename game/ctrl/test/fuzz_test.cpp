// =============================================================================
// Fuzz tests — random payloads with mixed SingleSequenced / StreamReliable
// packets, including stream packets in the 1000-4096 byte range.
//
// Single-peer and multi-peer variants exercise the full event-log pipeline:
//   EnqueueEvent → TryDequeueEvent → SerializeEventMeta/SerializeEventPayload
//   → DeserializeEvent → stream-internals verification.
// =============================================================================

#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "game/components.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/net/replication_events.hpp"
#include "game/net/replication_registry.hpp"
#include "test_macros.hpp"

namespace net = oge::runtime::net;

// =============================================================================
// Shared helpers (identical to wire_format_test.cpp)
// =============================================================================
struct TestStream : game::net::EventLogStream<>
{
    using game::net::EventLogStream<>::EventLogStream;

    uint64_t Head() const
    {
        return m_entries.HeadCursor();
    }
    bool Contains(game::net::LogCursor c) const
    {
        return m_entries.Contains(c);
    }
    const game::net::EventLogEntryMeta& EntryAt(game::net::LogCursor c) const
    {
        return m_entries.Get(c);
    }
    bool HasPayload(game::net::LogCursor c) const
    {
        return m_payloads.contains(c);
    }
    const std::vector<std::byte>& PayloadAt(game::net::LogCursor c) const
    {
        return m_payloads.at(c);
    }
    bool ValidAt(game::net::LogCursor c) const
    {
        return m_validSet.test(c % m_entries.MCapacity);
    }
};

struct TypesFixture
{
    oge::runtime::OgeRegistry metaWorld;
    oge::runtime::OGEContext octx{metaWorld.Raw()};
    oge::runtime::TypeRegistry types{octx};
    game::net::EventLogStream<> stream{&types};
    game::net::ReplicationRegistry reg{
        game::net::ReplicationRegistry::Def{stream, types}};

    TypesFixture()
    {
        game::net::RegisterReplications(types, reg);
    }
};

// Only use type IDs that are registered on the TypeRegistry so that
// LOG_DEBUG name lookups inside SerializeEventMeta / DeserializeEvent never
// null-deref GetDescriptor(id) in debug builds.  AdvanceTick is
// intentionally excluded — nothing calls RegisterType<AdvanceTick>().
constexpr oge::runtime::oge_id_type KNOWN_IDS[] = {
    entt::type_hash<game::net::AddEntityEvent>::value(),
    entt::type_hash<game::net::RemoveEntityEvent>::value(),
    entt::type_hash<
        game::net::AddComponentEvent<game::ComponentPhysicBody>>::value(),
    entt::type_hash<
        game::net::UpdateComponentEvent<game::ComponentPhysicBody>>::value(),
    entt::type_hash<
        game::net::AddComponentEvent<game::ComponentCreature>>::value(),
    entt::type_hash<
        game::net::UpdateComponentEvent<game::ComponentCreature>>::value(),
    entt::type_hash<
        game::net::AddComponentEvent<game::ComponentAABBCollider>>::value(),
    entt::type_hash<
        game::net::AddComponentEvent<game::ComponentPlayer>>::value(),
    entt::type_hash<game::net::AddChunkEvent>::value(),
};
constexpr int NUM_IDS = sizeof(KNOWN_IDS) / sizeof(KNOWN_IDS[0]);

// =============================================================================
// Single-peer fuzz test
// =============================================================================

TEST(fuzz_mixed_single_stream)
{
    TypesFixture fx;
    TestStream srv{&fx.types};
    srv.AddPeer(0);
    TestStream cli{&fx.types};
    cli.AddPeer(0);

    constexpr int NUM_EVENTS = 96;
    std::srand(42);

    struct FuzzEvent
    {
        oge::runtime::oge_id_type id;
        std::vector<std::byte> payload;
        game::net::ReplicationMethod sendType;
    };
    std::vector<FuzzEvent> events;

    // --- Generate random events on the server ---
    for (int i = 0; i < NUM_EVENTS; ++i)
    {
        FuzzEvent evt;
        evt.id = KNOWN_IDS[std::rand() % NUM_IDS];
        evt.sendType = (std::rand() % 2)
                           ? game::net::ReplicationMethod::SingleSequenced
                           : game::net::ReplicationMethod::StreamReliable;

        size_t payloadSize;
        if (evt.sendType == game::net::ReplicationMethod::StreamReliable)
            payloadSize = 1000 + (static_cast<size_t>(std::rand()) % 3097);
        else
            payloadSize = 1 + (static_cast<size_t>(std::rand()) % 197);

        evt.payload.resize(payloadSize);
        for (auto& b : evt.payload)
            b = static_cast<std::byte>(std::rand() & 0xFF);

        auto buf = srv.EnqueueEvent(evt.id, payloadSize);
        buf.WriteRaw(evt.payload.data(), payloadSize);

        events.push_back(std::move(evt));
    }

    // --- Transmit: dequeue from server, serialise to wire, feed client ---
    game::net::LogCursor expectedCursor = 1;
    for (auto& evt : events)
    {
        game::net::EventLogEntry entry;
        CHECK(srv.TryDequeueEvent(0, entry));
        CHECK_EQ(entry.entry.cursor, expectedCursor);
        CHECK_EQ(entry.entry.id, evt.id);

        if (evt.sendType == game::net::ReplicationMethod::SingleSequenced)
        {
            // Single packet combining meta + payload header + payload.
            size_t totalSize =
                game::net::EventLogStream<>::MetaSize() +
                game::net::EventLogStream<>::PayloadHeaderBytes() +
                evt.payload.size();
            auto raw = std::vector<std::byte>(totalSize);
            net::Buffer wire(raw);
            srv.SerializeEventMeta(wire, entry.entry);
            srv.SerializeEventPayload(entry.entry.cursor, wire, entry.payload);
            cli.DeserializeEvent(0, wire);
        }
        else
        {
            // StreamReliable: two independent packets; arrival order is
            // not guaranteed by the network.
            auto rawMeta =
                std::vector<std::byte>(game::net::EventLogStream<>::MetaSize());
            net::Buffer metaWire(rawMeta);
            srv.SerializeEventMeta(metaWire, entry.entry);

            auto rawPl = std::vector<std::byte>(
                game::net::EventLogStream<>::PayloadHeaderBytes() +
                evt.payload.size());
            net::Buffer plWire(rawPl);
            srv.SerializeEventPayload(entry.entry.cursor, plWire,
                                       entry.payload);

            bool payloadFirst = (std::rand() % 2) != 0;
            if (payloadFirst)
            {
                cli.DeserializeEvent(0, plWire);
                cli.DeserializeEvent(0, metaWire);
            }
            else
            {
                cli.DeserializeEvent(0, metaWire);
                cli.DeserializeEvent(0, plWire);
            }
        }
        ++expectedCursor;
    }

    // --- Verify every event round-trips ---
    expectedCursor = 1;
    for (auto& evt : events)
    {
        CHECK(cli.Contains(expectedCursor));
        CHECK(cli.ValidAt(expectedCursor));
        CHECK_EQ(cli.EntryAt(expectedCursor).id, evt.id);
        CHECK_EQ(cli.EntryAt(expectedCursor).cursor, expectedCursor);
        CHECK(cli.HasPayload(expectedCursor));
        CHECK(cli.PayloadAt(expectedCursor) == evt.payload);
        ++expectedCursor;
    }
}

// =============================================================================
// Multi-peer fuzzer
//
// Events are enqueued with per-peer masks.  Peers dequeue independently,
// simulating concurrent replication streams.  All events then arrive on a
// single client stream (as they would after traveling the network).
// =============================================================================

TEST(fuzz_multi_peer)
{
    TypesFixture fx;
    TestStream srv{&fx.types};

    constexpr int NUM_PEERS = 5;
    // Start at a non-zero peer ID to stress the bitset index path.
    constexpr uint32_t PEER_BASE = 11;
    for (int p = 0; p < NUM_PEERS; ++p)
        srv.AddPeer(PEER_BASE + p);

    TestStream cli{&fx.types};

    constexpr int NUM_EVENTS = 80;
    std::srand(20250809);

    struct FuzzEvent
    {
        oge::runtime::oge_id_type id;
        std::vector<std::byte> payload;
        game::net::ReplicationMethod sendType;
        uint32_t targetPeer;
        game::net::LogCursor cursor = 0;
        bool dequeued = false;
    };
    std::vector<FuzzEvent> events;

    // --- Enqueue with per-peer masks ---
    for (int i = 0; i < NUM_EVENTS; ++i)
    {
        FuzzEvent evt;
        evt.id = KNOWN_IDS[std::rand() % NUM_IDS];
        evt.sendType = (std::rand() % 2)
                           ? game::net::ReplicationMethod::SingleSequenced
                           : game::net::ReplicationMethod::StreamReliable;
        evt.targetPeer = PEER_BASE + (std::rand() % NUM_PEERS);

        std::bitset<64> mask{};
        mask.set(evt.targetPeer);

        size_t payloadSize;
        if (evt.sendType == game::net::ReplicationMethod::StreamReliable)
            payloadSize = 1000 + (static_cast<size_t>(std::rand()) % 3097);
        else
            payloadSize = 1 + (static_cast<size_t>(std::rand()) % 197);

        evt.payload.resize(payloadSize);
        for (auto& b : evt.payload)
            b = static_cast<std::byte>(std::rand() & 0xFF);

        auto buf = srv.EnqueueEvent(evt.id, payloadSize, mask);
        buf.WriteRaw(evt.payload.data(), payloadSize);

        evt.cursor = srv.EntryAt(srv.Head() - 1).cursor;
        events.push_back(std::move(evt));
    }

    // --- Each peer independently dequeues its events ---
    // Map peer → next cursor to scan from.
    std::unordered_map<uint32_t, game::net::LogCursor> peerNext;
    for (int p = 0; p < NUM_PEERS; ++p)
        peerNext[PEER_BASE + p] = 1;

    struct DequeueResult
    {
        game::net::LogCursor cursor;
        game::net::oge_id_type id;
        std::vector<std::byte> payload;
    };
    std::vector<DequeueResult> dequeued;

    int remaining = NUM_EVENTS;
    while (remaining > 0)
    {
        bool any = false;
        for (auto& evt : events)
        {
            if (evt.dequeued) continue;

            uint32_t peer = evt.targetPeer;
            game::net::EventLogEntry entry;
            if (srv.TryDequeueEvent(peer, entry, peerNext[peer]))
            {
                if (entry.entry.cursor != evt.cursor) continue;

                CHECK_EQ(entry.entry.id, evt.id);
                CHECK(entry.payload == evt.payload);

                evt.dequeued = true;
                peerNext[peer] = evt.cursor + 1;
                --remaining;
                any = true;

                dequeued.push_back({evt.cursor, evt.id,
                                    std::move(entry.payload)});
            }
        }
        if (!any) break;
    }
    CHECK_EQ(remaining, 0);

    // --- Transmit dequeued events to the client (cursor order) ---
    for (auto& dq : dequeued)
    {
        game::net::EventLogEntryMeta meta{};
        meta.id = dq.id;
        meta.cursor = dq.cursor;

        bool streamReliable = (std::rand() % 2) != 0;
        if (!streamReliable)
        {
            size_t totalSize =
                game::net::EventLogStream<>::MetaSize() +
                game::net::EventLogStream<>::PayloadHeaderBytes() +
                dq.payload.size();
            auto raw = std::vector<std::byte>(totalSize);
            net::Buffer wire(raw);
            srv.SerializeEventMeta(wire, meta);
            srv.SerializeEventPayload(dq.cursor, wire, dq.payload);
            cli.DeserializeEvent(0, wire);
        }
        else
        {
            auto rawMeta =
                std::vector<std::byte>(game::net::EventLogStream<>::MetaSize());
            net::Buffer metaWire(rawMeta);
            srv.SerializeEventMeta(metaWire, meta);

            auto rawPl = std::vector<std::byte>(
                game::net::EventLogStream<>::PayloadHeaderBytes() +
                dq.payload.size());
            net::Buffer plWire(rawPl);
            srv.SerializeEventPayload(dq.cursor, plWire, dq.payload);

            if (std::rand() % 2)
            {
                cli.DeserializeEvent(0, plWire);
                cli.DeserializeEvent(0, metaWire);
            }
            else
            {
                cli.DeserializeEvent(0, metaWire);
                cli.DeserializeEvent(0, plWire);
            }
        }
    }

    // --- Verify every event on the client ---
    game::net::LogCursor expectedCursor = 1;
    for (auto& evt : events)
    {
        CHECK(cli.Contains(expectedCursor));
        CHECK(cli.ValidAt(expectedCursor));
        CHECK_EQ(cli.EntryAt(expectedCursor).id, evt.id);
        CHECK_EQ(cli.EntryAt(expectedCursor).cursor, expectedCursor);
        CHECK(cli.HasPayload(expectedCursor));
        CHECK(cli.PayloadAt(expectedCursor) == evt.payload);
        ++expectedCursor;
    }
}

RUN_TESTS("Fuzz Tests")
