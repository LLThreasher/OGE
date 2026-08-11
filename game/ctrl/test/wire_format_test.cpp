// =============================================================================
// Wire-format unit tests — event serialisation / deserialisation round-trips
//
// Tests the exact byte-level protocol without ENet involved:
//   SingleReliable/SingleSequenced packet:
//     [eventId][cursor][sendPayloadTypeId][cursor][uint32 size][payload]
//   StreamReliable: meta packet [eventId][cursor] on channel 0 plus a
//     payload-only packet [sendPayloadTypeId][cursor][size][payload] on
//     channel 1.  Either packet may arrive first.
//
// Deserialized events carry an empty receive mask (recieveMask = {}), so no
// peer — including the receiving client — can PeekEvent/TryDequeueEvent them.
// This is what prevents the client from echoing server events back through
// ProduceAll.
// =============================================================================

#include <cstdlib>
#include <vector>

#include "game/components.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/net/replication_events.hpp"
#include "game/net/replication_registry.hpp"
#include "test_macros.hpp"

namespace net = oge::runtime::net;

// =============================================================================
// TestStream exposes protected stream internals so tests can verify the
// exact wire layout and stream state.
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
    const game::net::SmallPayload& PayloadAt(game::net::LogCursor c) const
    {
        return m_payloads.at(c);
    }
    bool ValidAt(game::net::LogCursor c) const
    {
        return m_validSet.test(c % m_entries.MCapacity);
    }
};

// TypeRegistry with all replication event types registered, shared by the
// wire-format tests (mirrors RegisterReplications in the scenes).
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

// =============================================================================
// Tests
// =============================================================================

TEST(wire_single_packet_roundtrip)
{
    // The full server→client single-packet path round-trips the event meta
    // and the exact payload bytes.
    TypesFixture fx;
    TestStream srv{&fx.types};
    srv.AddPeer(0);

    game::net::UpdateComponentEvent<game::ComponentPhysicBody> evt;
    evt.entity = entt::entity{42};
    evt.component.pos = {1.f, 2.f, 3.f};
    evt.component.velocity = {0.5f, -1.f, 2.f};
    evt.component.isGrounded = true;

    auto buf = srv.EnqueueEvent(
        entt::type_hash<
            game::net::UpdateComponentEvent<game::ComponentPhysicBody>>::value(),
        net::Size(evt));
    net::Serialize(buf, evt);

    game::net::EventLogEntry entry;
    CHECK(srv.TryDequeueEvent(0, entry));
    CHECK(entry.entry.id ==
          entt::type_hash<
              game::net::UpdateComponentEvent<game::ComponentPhysicBody>>::
              value());

    // Build the exact packet ProduceAll sends: [id][cursor] then
    // [sendPayloadId][cursor][size][payload].
    auto raw = std::vector<std::byte>(128, std::byte{0});
    net::Buffer wire(raw);
    srv.SerializeEventMeta(wire, entry.entry);
    srv.SerializeEventPayload(entry.entry.cursor, wire, entry.payload);

    TestStream cli{&fx.types};
    cli.AddPeer(0);
    cli.DeserializeEvent(0, wire);

    CHECK_EQ(cli.Head(), 2u);  // one real entry at cursor 1
    CHECK(cli.Contains(1));
    CHECK_EQ(cli.EntryAt(1).cursor, 1u);
    CHECK(cli.EntryAt(1).id == entry.entry.id);
    CHECK(cli.HasPayload(1));
    CHECK(cli.PayloadAt(1) == entry.payload);

    // The stored payload deserializes back to the original event.
    game::net::UpdateComponentEvent<game::ComponentPhysicBody> back;
    auto sp = cli.PayloadAt(1).span();
    std::vector<std::byte> payloadCopy(sp.begin(), sp.end());
    // Use the span constructor (non-owning) so that ToReadOnly() correctly
    // sets writePos to the payload size.  The owning vector& constructor
    // starts with an empty span (grows on write), leaving writePos = 0 after
    // ToReadOnly and failing every read.
    net::Buffer payloadBuf(payloadCopy.data(), payloadCopy.size());
    payloadBuf.ToReadOnly();
    net::Deserialize(payloadBuf, back);
    CHECK_EQ(back.entity, evt.entity);
    // Only the fields declared in DECL_NET_OBJ(ComponentPhysicBody) travel
    // (components_net.hpp serializes just `pos`).
    CHECK(back.component.pos == evt.component.pos);
}

TEST(wire_payload_only_packet)
{
    // StreamReliable sends the payload as a standalone packet prefixed with
    // [sendPayloadId][cursor][size].  The receiver must store the payload
    // against the cursor without pushing an entry.
    TypesFixture fx;
    TestStream cli{&fx.types};
    cli.AddPeer(0);

    auto raw = std::vector<std::byte>(64, std::byte{0});
    net::Buffer wire(raw);
    game::net::LogCursor cursor = 7;
    std::vector<std::byte> payload = {std::byte{0xAB}, std::byte{0xCD},
                                      std::byte{0xEF}};
    cli.SerializeEventPayload(cursor, wire, payload);

    cli.DeserializeEvent(0, wire);
    CHECK_EQ(cli.Head(), 1u);  // no entry pushed
    CHECK(cli.HasPayload(7));
    CHECK(cli.PayloadAt(7) == payload);
}

TEST(wire_stream_reliable_meta_then_payload)
{
    TypesFixture fx;
    TestStream srv{&fx.types};
    srv.AddPeer(0);

    // One event, produced the way ProduceAll schedules StreamReliable: a
    // meta packet on channel 0 and a payload-only packet on channel 1.
    game::net::AddEntityEvent evt{entt::entity{7}};
    auto buf = srv.EnqueueEvent(
        entt::type_hash<game::net::AddEntityEvent>::value(), net::Size(evt));
    net::Serialize(buf, evt);
    game::net::EventLogEntry entry;
    CHECK(srv.TryDequeueEvent(0, entry));

    auto rawMeta = std::vector<std::byte>(32, std::byte{0});
    net::Buffer metaWire(rawMeta);
    srv.SerializeEventMeta(metaWire, entry.entry);

    auto rawPl = std::vector<std::byte>(64, std::byte{0});
    net::Buffer plWire(rawPl);
    srv.SerializeEventPayload(entry.entry.cursor, plWire, entry.payload);

    TestStream cli{&fx.types};
    cli.DeserializeEvent(0, metaWire);
    // Meta first: entry in place, payload not yet.
    CHECK(cli.Contains(1));
    CHECK_EQ(cli.EntryAt(1).id, entry.entry.id);
    CHECK(!cli.HasPayload(1));

    cli.DeserializeEvent(0, plWire);
    CHECK(cli.HasPayload(1));
    CHECK(cli.PayloadAt(1) == entry.payload);
}

TEST(wire_stream_reliable_payload_then_meta)
{
    TypesFixture fx;
    TestStream srv{&fx.types};
    srv.AddPeer(0);

    game::net::AddEntityEvent evt{entt::entity{9}};
    auto buf = srv.EnqueueEvent(
        entt::type_hash<game::net::AddEntityEvent>::value(), net::Size(evt));
    net::Serialize(buf, evt);
    game::net::EventLogEntry entry;
    CHECK(srv.TryDequeueEvent(0, entry));

    auto rawMeta = std::vector<std::byte>(32, std::byte{0});
    net::Buffer metaWire(rawMeta);
    srv.SerializeEventMeta(metaWire, entry.entry);

    auto rawPl = std::vector<std::byte>(64, std::byte{0});
    net::Buffer plWire(rawPl);
    srv.SerializeEventPayload(entry.entry.cursor, plWire, entry.payload);

    TestStream cli{&fx.types};
    cli.DeserializeEvent(0, plWire);
    // Payload first: stored against the cursor, no entry yet.
    CHECK_EQ(cli.Head(), 1u);
    CHECK(cli.HasPayload(1));

    cli.DeserializeEvent(0, metaWire);
    CHECK(cli.Contains(1));
    CHECK_EQ(cli.EntryAt(1).id, entry.entry.id);
    CHECK(cli.PayloadAt(1) == entry.payload);
}

TEST(wire_cursor_gap_pads)
{
    // An event with cursor 5 on a fresh stream pads slots 1-4 with invalid
    // (not valid) entries; only slot 5 carries the event.
    TypesFixture fx;
    TestStream cli{&fx.types};
    cli.AddPeer(0);

    auto raw = std::vector<std::byte>(128, std::byte{0});
    net::Buffer wire(raw);
    wire.Write(entt::type_hash<game::net::AddEntityEvent>::value());
    wire.Write<game::net::LogCursor>(5);
    std::vector<std::byte> payload = {std::byte{0x01}, std::byte{0x02}};
    cli.SerializeEventPayload(5, wire, payload);

    cli.DeserializeEvent(0, wire);
    CHECK_EQ(cli.Head(), 6u);
    for (game::net::LogCursor c = 1; c <= 4; ++c)
    {
        CHECK(!cli.ValidAt(c));
    }
    CHECK(cli.ValidAt(5));
    CHECK(cli.EntryAt(5).id ==
          entt::type_hash<game::net::AddEntityEvent>::value());
    CHECK(cli.HasPayload(5));
    CHECK(cli.PayloadAt(5) == payload);
}

TEST(wire_no_echo_deserialized)
{
    // Deserialized events carry an empty receive mask: no peer — including
    // the receiving client — may PeekEvent/TryDequeueEvent them.  This is
    // what stops the client from echoing server events back (the original
    // bidirectional serialize/deserialize bug).
    TypesFixture fx;
    TestStream srv{&fx.types};
    srv.AddPeer(0);

    game::net::AddComponentEvent<game::ComponentCreature> evt;
    evt.entity = entt::entity{5};
    evt.component.maxSpeed = 3.f;
    auto buf = srv.EnqueueEvent(
        entt::type_hash<
            game::net::AddComponentEvent<game::ComponentCreature>>::value(),
        net::Size(evt));
    net::Serialize(buf, evt);
    game::net::EventLogEntry entry;
    CHECK(srv.TryDequeueEvent(0, entry));

    auto raw = std::vector<std::byte>(128, std::byte{0});
    net::Buffer wire(raw);
    srv.SerializeEventMeta(wire, entry.entry);
    srv.SerializeEventPayload(entry.entry.cursor, wire, entry.payload);

    TestStream cli{&fx.types};
    cli.AddPeer(0);
    cli.DeserializeEvent(0, wire);

    // Stored, but not visible to any peer.
    CHECK(cli.Contains(1));
    CHECK(cli.HasPayload(1));
    game::net::EventLogEntry out;
    game::net::SmallPayload dp;
    game::net::EventLogEntryConstRef ref{{}, dp};
    CHECK(!cli.PeekEvent(0, ref));
    CHECK(!cli.TryDequeueEvent(0, out));
    CHECK(!cli.PeekEvent(63, ref));  // MyPeerId is not visible either
    // Nothing was consumed — the payload is still intact.
    CHECK(cli.PayloadAt(1) == entry.payload);
}

TEST(wire_ring_wrap_small_capacity)
{
    // Cursors beyond Capacity wrap into the ring: TryDequeueEvent must
    // index m_validSet modulo Capacity.  Regression: it used to write
    // `cursor` directly, throwing std::out_of_range from std::bitset::set
    // once the cursor reached Capacity.
    //
    // The ring can only hold Capacity events at once — enqueue, dequeue,
    // then enqueue more to force cursor wrap while staying within bounds.
    TypesFixture fx;
    game::net::EventLogStream<8> srv{&fx.types};
    srv.AddPeer(0);

    // Batch 1: fill the ring (8 events at cursors 1-8).
    for (int i = 0; i < 8; ++i)
    {
        auto b = srv.EnqueueEvent(1000 + i, sizeof(uint32_t));
        b.Write<uint32_t>(static_cast<uint32_t>(i));
    }
    game::net::EventLogEntry e;
    for (int i = 0; i < 8; ++i)
    {
        CHECK(srv.TryDequeueEvent(0, e));
        CHECK(e.entry.id == static_cast<game::net::oge_id_type>(1000 + i));
    }

    // Advance the tail.  TryDequeueEvent only clears the receive-mask bit;
    // the tail moves in Update() (called every tick in production), which is
    // what makes room in the ring for the next batch.
    srv.Update();

    // Batch 2: 4 more events at cursors 9-12, forcing indices 1-4 which
    // exercise the same `curosr % Capacity` path that previously threw.
    for (int i = 8; i < 12; ++i)
    {
        auto b = srv.EnqueueEvent(2000 + i, sizeof(uint32_t));
        b.Write<uint32_t>(static_cast<uint32_t>(i));
    }
    for (int i = 8; i < 12; ++i)
    {
        CHECK(srv.TryDequeueEvent(0, e));
        CHECK(e.entry.id == static_cast<game::net::oge_id_type>(2000 + i));
    }
}

RUN_TESTS("Wire-Format Tests")
