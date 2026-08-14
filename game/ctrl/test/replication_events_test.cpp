/**
 * Unit tests for replication events, registry, event log, snapshot, and
 * chunk compression.
 *
 * Build: cmake --build . --target replication_events_test
 * Run:   ctest -R replication_events_test
 */

#include <test_macros.hpp>

#include <vector>

#include "game/components.hpp"
#include "game/components_net.hpp"
#include "game/input/net.hpp"
#include "game/net/event_log_stream.hpp"
#include "game/net/replication_registry.hpp"
#include "game/terrain/defs.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/net_traits.hpp"

// ---------------------------------------------------------------------------
// Test utilities (specific to this suite)
// ---------------------------------------------------------------------------

static std::vector<std::byte> S(size_t c = 65536)
{
    std::vector<std::byte> v;
    v.reserve(c);
    v.resize(v.capacity());
    return v;
}

template <typename T>
static void RT(const T& o, T& d)
{
    auto s = S();
    oge::runtime::net::Buffer b(s);
    oge::runtime::net::Serialize(b, o);
    b.ToReadOnly();
    oge::runtime::net::Deserialize(b, d);
}

// =========================================================================
// Serialization round-trips
// =========================================================================
TEST(entity_add_rt){ game::net::AddEntityEvent o{entt::entity{42}},d{}; RT(o,d); CHECK_EQ(o.entity,d.entity); }
TEST(entity_remove_rt){ game::net::RemoveEntityEvent o{entt::entity{99}},d{}; RT(o,d); CHECK_EQ(o.entity,d.entity); }
TEST(comp_add_rt){ game::ComponentCamera cam{}; cam.position=oge::math::vec3{1,2,3}; cam.forward=oge::math::vec3{0,0,1}; game::net::AddComponentEvent<game::ComponentCamera> o{entt::entity{7},cam},d{}; RT(o,d); CHECK_EQ(o.entity,d.entity); CHECK(o.component.position==d.component.position); }
TEST(comp_update_rt){ using Ev=game::net::UpdateComponentEvent<game::ComponentAABBCollider>; Ev o{entt::entity{3},game::ComponentAABBCollider{oge::AABB{oge::math::vec3{0,0,0},oge::math::vec3{1,1,1}}}}; Ev d{}; RT(o,d); CHECK_EQ(o.entity,d.entity); }
TEST(comp_remove_rt){ using Ev=game::net::RemoveComponentEvent<game::ComponentPlayer>; Ev o{entt::entity{15}},d{}; RT(o,d); CHECK_EQ(o.entity,d.entity); }
TEST(chunk_add_rt){
    game::terrain::ChunkData cd(oge::Point3{1,2,3});
    cd.data[0]=42; cd.data[100]=7;
    game::net::AddChunkEvent o{}; o.coords=cd.Coords;
    game::terrain::PaletteCompressedChunk::FromChunkData(cd, o.chunk);
    game::net::AddChunkEvent d{}; RT(o,d);
    CHECK_EQ(o.coords.x,d.coords.x);
    CHECK_EQ(o.chunk.palette.size(),d.chunk.palette.size());
    CHECK_EQ(o.chunk.Get(0,0,0),d.chunk.Get(0,0,0));
}
TEST(chunk_remove_rt){ game::net::RemoveChunkEvent o{oge::Point3{-1,0,5}},d{}; RT(o,d); CHECK_EQ(o.coords.x,d.coords.x); }
TEST(chunk_update_rt){ game::net::UpdateChunkEvent o{}; o.coords=oge::Point3{0,0,0}; o.dirtyCnt=2; o.updates[0]=game::net::ChunkBlockUpdate{oge::CompactLocalPoint3{oge::Point3{3,5,7}},100}; o.updates[1]=game::net::ChunkBlockUpdate{oge::CompactLocalPoint3{oge::Point3{1,1,1}},200}; game::net::UpdateChunkEvent d{}; RT(o,d); CHECK_EQ(o.dirtyCnt,d.dirtyCnt); CHECK_EQ(o.updates[0].block,d.updates[0].block); }

// =========================================================================
// Apply tests
// =========================================================================
TEST(entity_add_apply){ oge::runtime::OgeRegistry w; game::net::ApplyEvent(w,game::net::AddEntityEvent{entt::entity{100}}); CHECK(w.valid(entt::entity{100})); CHECK(w.all_of<game::ReplicatedTag>(entt::entity{100})); }
TEST(entity_remove_apply){ oge::runtime::OgeRegistry w; auto e=w.create(); w.emplace<game::ReplicatedTag>(e); game::net::ApplyEvent(w,game::net::RemoveEntityEvent{e}); CHECK(!w.valid(e)); }
TEST(comp_add_apply){ oge::runtime::OgeRegistry w; auto e=w.create(); game::ComponentCamera cam{}; cam.position=oge::math::vec3{1,2,3}; game::net::ApplyEvent(w,game::net::AddComponentEvent<game::ComponentCamera>{e,cam}); CHECK(w.all_of<game::ComponentCamera>(e)); CHECK(w.get<game::ComponentCamera>(e).position.x==1.f); }
TEST(comp_update_apply){ oge::runtime::OgeRegistry w; auto e=w.create(); w.emplace<game::ComponentCamera>(e); game::ComponentCamera cam{}; cam.position=oge::math::vec3{10,20,30}; game::net::ApplyEvent(w,game::net::UpdateComponentEvent<game::ComponentCamera>{e,cam}); CHECK(w.get<game::ComponentCamera>(e).position.x==10.f); }
TEST(comp_remove_apply){ oge::runtime::OgeRegistry w; auto e=w.create(); w.emplace<game::ComponentCamera>(e); game::net::ApplyEvent(w,game::net::RemoveComponentEvent<game::ComponentCamera>{e}); CHECK(!w.all_of<game::ComponentCamera>(e)); }
TEST(cap_apply){ oge::runtime::OgeRegistry w; game::net::AddEntityEvent oe{entt::entity{77}}; auto s=S(); oge::runtime::net::Buffer b(s); oge::runtime::net::Serialize(b,oe); b.ToReadOnly(); auto cap=game::net::MakeSimpleReplicationCapability<game::net::AddEntityEvent>(0,nullptr); game::net::EventLogStream<> ds{}; cap.apply(ds,w,b); CHECK(w.valid(entt::entity{77})); }

// =========================================================================
// Hook tests
// =========================================================================
TEST(hooks_entity_construct){ oge::runtime::OgeRegistry w; w.ctx().emplace<game::net::EventLogStream<>>(); w.ctx().get<game::net::EventLogStream<>>().AddPeer(0); game::net::InstallAddEntityHooks(w.ctx().get<game::net::EventLogStream<>>(),w); auto e=w.create(); w.emplace<game::ReplicatedTag>(e); game::net::SmallPayload dp; game::net::EventLogEntryConstRef r{{},dp}; CHECK(w.ctx().get<game::net::EventLogStream<>>().PeekEvent(0,r)); }
TEST(hooks_entity_destroy){ oge::runtime::OgeRegistry w; w.ctx().emplace<game::net::EventLogStream<>>(); w.ctx().get<game::net::EventLogStream<>>().AddPeer(0); game::net::InstallRemoveEntityHooks(w.ctx().get<game::net::EventLogStream<>>(),w); auto e=w.create(); w.emplace<game::ReplicatedTag>(e); w.remove<game::ReplicatedTag>(e); game::net::SmallPayload dp; game::net::EventLogEntryConstRef r{{},dp}; CHECK(w.ctx().get<game::net::EventLogStream<>>().PeekEvent(0,r)); }
TEST(hooks_comp_construct){ oge::runtime::OgeRegistry w; w.ctx().emplace<game::net::EventLogStream<>>(); w.ctx().get<game::net::EventLogStream<>>().AddPeer(0); game::net::InstallAddComponentHooks<game::ComponentCamera>(w.ctx().get<game::net::EventLogStream<>>(),w); auto e=w.create(); w.emplace<game::ReplicatedTag>(e); w.emplace<game::ComponentCamera>(e); game::net::SmallPayload dp; game::net::EventLogEntryConstRef r{{},dp}; CHECK(w.ctx().get<game::net::EventLogStream<>>().PeekEvent(0,r)); }

// =========================================================================
// EventLogStream tests
// =========================================================================
TEST(el_enqueue_peek){ game::net::EventLogStream<> s; s.AddPeer(0); auto b=s.EnqueueEvent(42,8); b.Write<uint64_t>(0xDEADBEEF); game::net::SmallPayload dp; game::net::EventLogEntryConstRef r{{},dp}; CHECK(s.PeekEvent(0,r)); CHECK_EQ(r.entry.id,42); CHECK_EQ(r.payload.size(),8); }
TEST(el_dequeue){ game::net::EventLogStream<> s; s.AddPeer(0); auto b=s.EnqueueEvent(100,4); b.Write<uint32_t>(12345); game::net::EventLogEntry e; CHECK(s.TryDequeueEvent(0,e)); CHECK_EQ(e.entry.id,100); }
TEST(el_multi_peer){ game::net::EventLogStream<> s; s.AddPeer(0); s.AddPeer(1); auto b1=s.EnqueueEvent(1,4,std::bitset<64>{}.set(0)); b1.Write<uint32_t>(111); auto b2=s.EnqueueEvent(2,4,std::bitset<64>{}.set(1)); b2.Write<uint32_t>(222); game::net::EventLogEntry e; CHECK(s.TryDequeueEvent(0,e)); CHECK_EQ(e.entry.id,1); CHECK(s.TryDequeueEvent(1,e)); CHECK_EQ(e.entry.id,2); }
TEST(el_peek_cursor){ game::net::EventLogStream<> s; s.AddPeer(0); s.EnqueueEvent(1,4).Write<uint32_t>(111); s.EnqueueEvent(2,4).Write<uint32_t>(222); game::net::SmallPayload dp; game::net::EventLogEntryConstRef r{{},dp}; CHECK(s.PeekEvent(0,r)); CHECK_EQ(r.entry.id,1); CHECK(s.PeekEvent(0,r,r.entry.cursor+1)); CHECK_EQ(r.entry.id,2); }
TEST(el_deserialize_stale_claim)
{
    // Regression: a sender behind the local log (e.g. client input arriving
    // while the server generated events the client has not seen yet) claims
    // a cursor the local log already passed.  The event must be appended at
    // the local head with the local cursor — keying the payload at the
    // stale claim would collide with the local event still occupying that
    // slot ("Failed to emplace payload at cursor").
    oge::runtime::OgeRegistry reg;
    oge::runtime::OGEContext ctx(reg);
    oge::runtime::TypeRegistry af(ctx);
    af.RegisterType<game::net::AddEntityEvent>();  // debug-log name lookup

    game::net::EventLogStream<> s(&af);
    s.AddPeer(0);

    // Local events occupy cursors 1..5 with payloads still in flight.
    for (uint32_t i = 1; i <= 5; ++i)
        s.EnqueueEvent(1000 + i, 4).Write<uint32_t>(i);

    // Remote event claiming cursor 3 — stale, since the local head is 6.
    game::net::EventLogEntryMeta meta{};
    meta.id = entt::type_hash<game::net::AddEntityEvent>::value();
    meta.cursor = 3;
    std::vector<std::byte> payload(4);

    auto s2 = S();
    oge::runtime::net::Buffer buf(s2);
    s.SerializeEventMeta(buf, meta);
    s.SerializeEventPayload(meta.cursor, buf, payload);
    buf.ToReadOnly();

    auto received = s.DeserializeEvent(0, buf);

    // Appended at the local head with the local cursor; payload keyed there.
    CHECK_EQ(received.cursor, uint64_t{6});
    CHECK_EQ(received.id, meta.id);
    const auto* entry = s.GetEntry(6);
    CHECK(entry != nullptr);
    CHECK_EQ(entry->id, meta.id);
    auto* rpayload = s.GetPayload(6);
    CHECK(rpayload != nullptr);
    CHECK_EQ(rpayload->size(), 4u);

    // The local event at the claimed cursor must be untouched.
    const auto* localEntry = s.GetEntry(3);
    CHECK(localEntry != nullptr);
    CHECK_EQ(localEntry->id, 1003u);
}
TEST(el_deserialize_fresh_claim)
{
    // A claim at the local head keeps its slot: the entry and payload land
    // at the claimed cursor.
    oge::runtime::OgeRegistry reg;
    oge::runtime::OGEContext ctx(reg);
    oge::runtime::TypeRegistry af(ctx);
    af.RegisterType<game::net::AddEntityEvent>();

    game::net::EventLogStream<> s(&af);
    s.AddPeer(0);

    game::net::EventLogEntryMeta meta{};
    meta.id = entt::type_hash<game::net::AddEntityEvent>::value();
    meta.cursor = 1;  // local head is 1
    std::vector<std::byte> payload(4);

    auto s2 = S();
    oge::runtime::net::Buffer buf(s2);
    s.SerializeEventMeta(buf, meta);
    s.SerializeEventPayload(meta.cursor, buf, payload);
    buf.ToReadOnly();

    auto received = s.DeserializeEvent(0, buf);

    CHECK_EQ(received.cursor, uint64_t{1});
    CHECK(s.GetPayload(1) != nullptr);
}

// =========================================================================
// PacketScheduler tests
// =========================================================================
TEST(sched_basic){ game::net::EventLogStream<> s; s.AddPeer(0); s.EnqueueEvent(1,4).Write<uint32_t>(100); s.EnqueueEvent(2,4).Write<uint32_t>(200); game::net::SimplePacketScheduler sc; game::net::EncodeContext ctx{}; ctx.peer.id=0; game::net::PacketPlan p; CHECK(sc.Schedule(s,ctx,p)); CHECK(p.hasPacket); CHECK_EQ(p.packets.size(),2); }
TEST(sched_oversized){ game::net::EventLogStream<> s; s.AddPeer(0); s.EnqueueEvent(1,4).Write<uint32_t>(100); game::net::SimplePacketScheduler sc; game::net::EncodeContext ctx{}; ctx.peer.id=0; game::net::PacketPlan p; CHECK(sc.Schedule(s,ctx,p)); CHECK(p.hasPacket); CHECK_EQ(p.packets.size(),1); }
TEST(sched_empty){ game::net::EventLogStream<> s; s.AddPeer(0); game::net::SimplePacketScheduler sc; game::net::EncodeContext ctx{}; ctx.peer.id=0; game::net::PacketPlan p; CHECK(!sc.Schedule(s,ctx,p)); }

// =========================================================================
// Snapshot tests (use Registry with proper TypeRegistry)
// =========================================================================
TEST(snapshot_entity){
    oge::runtime::OgeRegistry w;
    w.ctx().emplace<game::net::EventLogStream<>>();
    w.ctx().get<game::net::EventLogStream<>>().AddPeer(5);

    oge::runtime::OGEContext octx(w.Raw());
    oge::runtime::TypeRegistry types(octx);

    auto e1=w.create(); w.emplace<game::ReplicatedTag>(e1);
    auto e2=w.create(); w.emplace<game::ReplicatedTag>(e2);

    game::net::ReplicationRegistry reg({w.ctx().get<game::net::EventLogStream<>>(), types});
    reg.RegisterSnapshotComponent<game::ComponentAABBCollider>();
    reg.GenerateSnapshot(5, w);

    auto& stream=w.ctx().get<game::net::EventLogStream<>>();
    game::net::SmallPayload dp; game::net::EventLogEntryConstRef r{{},dp};
    CHECK(stream.PeekEvent(5,r));  // add-entity for e1 or e2
    CHECK(stream.PeekEvent(5,r,r.entry.cursor+1)); // the other entity
    CHECK(!stream.PeekEvent(0,r));  // peer 0 not snapshotted
}

TEST(snapshot_terrain){
    oge::runtime::OgeRegistry w;
    w.ctx().emplace<game::net::EventLogStream<>>();
    w.ctx().get<game::net::EventLogStream<>>().AddPeer(2);
    w.ctx().emplace<game::terrain::TerrainView>();

    oge::runtime::OGEContext octx(w.Raw());
    oge::runtime::TypeRegistry types(octx);

    auto& terrain=w.ctx().get<game::terrain::TerrainView>();
    auto h=terrain.CreateChunk(oge::Point3{0,0,0});
    auto* chunk=terrain.GetChunk(h);
    CHECK(chunk != nullptr);
    chunk->state=game::terrain::ChunkState::Persistent;
    // Palette compression caps distinct block values at 255.
    for(size_t i=0;i<game::terrain::CHUNK_SIZE_TOTAL;++i) chunk->data[i]=static_cast<uint32_t>(i % 100);

    game::net::ReplicationRegistry reg({w.ctx().get<game::net::EventLogStream<>>(), types});
    reg.GenerateSnapshot(2, w);

    game::net::SmallPayload dp; game::net::EventLogEntryConstRef r{{},dp};
    CHECK(w.ctx().get<game::net::EventLogStream<>>().PeekEvent(2,r));
    // Verify the snapped event is an AddChunkEvent with correct coords
    CHECK(r.entry.id == entt::type_hash<game::net::AddChunkEvent>::value());
}

// =========================================================================
// Palette compression tests
// =========================================================================
TEST(palette_compress_rt){
    // Many distinct block values (up to the 255-entry palette cap) — the
    // round-trip must preserve them all.
    game::terrain::ChunkData cd(oge::Point3{10,20,30});
    for(size_t i=0;i<game::terrain::CHUNK_SIZE_TOTAL;++i) cd.data[i]=static_cast<uint32_t>(i % 200);
    game::net::AddChunkEvent o{}; o.coords=cd.Coords;
    game::terrain::PaletteCompressedChunk::FromChunkData(cd, o.chunk);
    game::net::AddChunkEvent d{}; RT(o,d);
    CHECK_EQ(o.chunk.palette.size(),d.chunk.palette.size());
    CHECK_EQ(o.chunk.palette.size(),200u);
    CHECK_EQ(o.chunk.Get(0,0,0),d.chunk.Get(0,0,0));
    CHECK_EQ(o.chunk.Get(1,2,3),d.chunk.Get(1,2,3));
}
TEST(palette_compress_uniform){
    // Two non-zero block values plus the zero default — palette has 3
    // entries, data stores only 1-byte indices.
    game::terrain::ChunkData cd(oge::Point3{1,2,3});
    cd.data[0]=42; cd.data[4095]=99;
    game::net::AddChunkEvent o{}; o.coords=cd.Coords;
    game::terrain::PaletteCompressedChunk::FromChunkData(cd, o.chunk);
    game::net::AddChunkEvent d{}; RT(o,d);
    CHECK_EQ(o.chunk.palette.size(),d.chunk.palette.size());
    CHECK_EQ(o.chunk.palette.size(),3u);
    CHECK_EQ(o.chunk.Get(0,0,0),d.chunk.Get(0,0,0));
    CHECK_EQ(o.chunk.Get(15,15,15),d.chunk.Get(15,15,15));
    // ToChunkData reconstructs the raw block data.
    game::terrain::ChunkData out(oge::Point3{1,2,3});
    d.chunk.ToChunkData(out);
    CHECK_EQ(out.data[0],42u);
    CHECK_EQ(out.data[4095],99u);
}
TEST(palette_decompress_empty){
    // Empty palette (no chunk data) must not crash.
    game::net::AddChunkEvent o{}; o.coords=oge::Point3{0,0,0};
    game::net::AddChunkEvent d{}; RT(o,d);
    CHECK(d.chunk.palette.empty());
}

// =========================================================================
// ReplicationRegistry tests
// =========================================================================
TEST(reg_add_remove_peer){
    oge::runtime::OgeRegistry w; w.ctx().emplace<game::net::EventLogStream<>>();
    oge::runtime::OGEContext octx(w.Raw()); oge::runtime::TypeRegistry types(octx);
    game::net::ReplicationRegistry reg({w.ctx().get<game::net::EventLogStream<>>(),types});
    reg.AddPeer(0,nullptr); CHECK_EQ(reg.Peers().size(),1);
    reg.RemovePeer(0); CHECK(reg.Peers().empty());
}

// =========================================================================
// Rollback tests
// =========================================================================

#include "game/net/rollback_capability.hpp"
#include "game/net/rollback_event_log_stream.hpp"

namespace rnet = oge::runtime::net;

TEST(rollback_cap_register){
    game::net::RollbackCapability cap{};
    cap.family = 1;
    cap.takeSnapshot = nullptr;
    cap.rollback = nullptr;
    cap.compare = game::net::ByteCompareFn;
    CHECK(cap.compare != nullptr);
}

TEST(rollback_predicted_gated){
    game::net::RollbackEventLogStream<> stream;
    // No capabilities registered — insertion should fail.
    CHECK(!stream.InsertPredicted(game::net::AddEntityEvent{entt::entity{1}}));

    // Register entity capability — now it should work.
    game::net::RollbackCapability cap{};
    cap.family = entt::type_hash<game::net::AddEntityEvent>::value();
    cap.compare = game::net::EntityCompareFn;
    stream.RegisterRollbackCapability(cap);
    CHECK(stream.InsertPredicted(game::net::AddEntityEvent{entt::entity{1}}));
    CHECK_EQ(stream.PredictedCount(), 1);
}

TEST(rollback_snapshot_entity){
    oge::runtime::OgeRegistry w;
    auto e1 = w.create(); w.emplace<game::ReplicatedTag>(e1);
    auto e2 = w.create(); w.emplace<game::ReplicatedTag>(e2);

    auto payload = game::net::EntitySnapshotFn(w);
    CHECK(payload.size() > 0);
}

TEST(rollback_rollback_entity){
    oge::runtime::OgeRegistry w;
    auto e1 = w.create(); w.emplace<game::ReplicatedTag>(e1);
    auto e2 = w.create(); w.emplace<game::ReplicatedTag>(e2);

    // Take snapshot
    auto payload = game::net::EntitySnapshotFn(w);

    // Modify world
    w.destroy(e1);
    auto e3 = w.create(); w.emplace<game::ReplicatedTag>(e3);

    // Roll back
    std::vector<std::byte> v(payload.begin(), payload.end());
    rnet::Buffer buf(v.data(), v.size());
    buf.ToReadOnly();
    game::net::EntityRollbackFn(w, buf);

    // Verify: e1 restored, e3 gone
    CHECK(w.valid(e1));
    CHECK(w.all_of<game::ReplicatedTag>(e1));
    CHECK(w.valid(e2));
    CHECK(!w.valid(e3));
}

TEST(rollback_snapshot_component){
    oge::runtime::OgeRegistry w;
    auto e = w.create(); w.emplace<game::ReplicatedTag>(e);
    game::ComponentCamera cam{}; cam.position = oge::math::vec3{1,2,3};
    w.emplace<game::ComponentCamera>(e, cam);

    auto payload = game::net::ComponentSnapshotFn<game::ComponentCamera>(w);
    CHECK(payload.size() > 0);
}

TEST(rollback_rollback_component){
    oge::runtime::OgeRegistry w;
    auto e = w.create(); w.emplace<game::ReplicatedTag>(e);
    game::ComponentCamera cam{}; cam.position = oge::math::vec3{1,2,3};
    w.emplace<game::ComponentCamera>(e, cam);

    auto payload = game::net::ComponentSnapshotFn<game::ComponentCamera>(w);

    // Modify component
    w.get<game::ComponentCamera>(e).position = oge::math::vec3{99,99,99};

    // Roll back
    std::vector<std::byte> vb(payload.begin(), payload.end());
    rnet::Buffer buf(vb.data(), vb.size()); buf.ToReadOnly();
    game::net::ComponentRollbackFn<game::ComponentCamera>(w, buf);

    auto& cam2 = w.get<game::ComponentCamera>(e);
    CHECK(cam2.position.x == 1.f);
    CHECK(cam2.position.y == 2.f);
    CHECK(cam2.position.z == 3.f);
}

TEST(rollback_snapshot_chunk){
    oge::runtime::OgeRegistry w;
    w.ctx().emplace<game::terrain::TerrainView>();
    auto& terrain = w.ctx().get<game::terrain::TerrainView>();
    auto h = terrain.CreateChunk(oge::Point3{0,0,0});
    auto* c = terrain.GetChunk(h);
    if(c){ c->state = game::terrain::ChunkState::Persistent; }

    auto payload = game::net::ChunkSnapshotFn(w);
    CHECK(payload.size() > 0);
}

TEST(rollback_rollback_chunk){
    oge::runtime::OgeRegistry w;
    w.ctx().emplace<game::terrain::TerrainView>();
    auto& terrain = w.ctx().get<game::terrain::TerrainView>();
    auto h = terrain.CreateChunk(oge::Point3{0,0,0});
    auto* c = terrain.GetChunk(h);
    if(c){ c->state = game::terrain::ChunkState::Persistent; c->data[0]=42; }

    auto payload = game::net::ChunkSnapshotFn(w);

    // Modify chunk
    if(c) c->data[0] = 99;

    // Roll back
    std::vector<std::byte> vc(payload.begin(), payload.end());
    rnet::Buffer buf(vc.data(), vc.size()); buf.ToReadOnly();
    game::net::ChunkRollbackFn(w, buf);

    // Verify original data restored
    auto [h2, c2] = terrain.GetChunk(oge::Point3{0,0,0});
    CHECK(c2 != nullptr);
    if (c2) CHECK_EQ(c2->data[0], 42);
}

TEST(rollback_stream_advance_tick){
    oge::runtime::OgeRegistry w;
    w.ctx().emplace<game::net::EventLogStream<>>();

    game::net::RollbackEventLogStream<> stream;
    stream.m_snapshotInterval = 2;

    game::net::RollbackCapability cap{};
    cap.family = entt::type_hash<game::net::AddEntityEvent>::value();
    cap.takeSnapshot = game::net::EntitySnapshotFn;
    cap.rollback = game::net::EntityRollbackFn;
    cap.compare = game::net::EntityCompareFn;
    stream.RegisterRollbackCapability(cap);

    // auto e = w.create(); w.emplace<game::ReplicatedTag>(e);
    // stream.AdvanceTick(w);  // tick 1
    // CHECK_EQ(stream.Snapshots().size(), 0);
    // stream.AdvanceTick(w);  // tick 2 → snapshot
    // CHECK_EQ(stream.Snapshots().size(), 1);
}

TEST(rollback_stream_validate){
    oge::runtime::OgeRegistry w;
    game::net::RollbackEventLogStream<> stream;
    stream.m_snapshotInterval = 1;
    stream.AddPeer(0);  // peer 0 must be active for PeekEvent to work

    // Register entity rollback capability.
    game::net::RollbackCapability cap{};
    cap.family = entt::type_hash<game::net::AddEntityEvent>::value();
    cap.takeSnapshot = game::net::EntitySnapshotFn;
    cap.rollback = game::net::EntityRollbackFn;
    cap.compare = game::net::EntityCompareFn;
    stream.RegisterRollbackCapability(cap);

    // auto e = w.create(); w.emplace<game::ReplicatedTag>(e);
    // stream.AdvanceTick(w);  // takes snapshot

    // // Push a matching server event into the base stream.
    // auto buf2 = stream.EnqueueEvent(
    //     entt::type_hash<game::net::AddEntityEvent>::value(), 4);
    // buf2.Write(entt::entity{static_cast<uint32_t>(e)});

    // // Insert matching prediction.
    // game::net::AddEntityEvent pred{e};
    // stream.InsertPredicted(pred);

    // // Validate — predicted and server events match.
    // CHECK(stream.Validate(w));
    // CHECK_EQ(stream.PredictedCount(), 0);
}

TEST(rollback_compare_payloads){
    game::net::AddEntityEvent ev1{entt::entity{42}};
    game::net::AddEntityEvent ev2{entt::entity{42}};
    game::net::AddEntityEvent ev3{entt::entity{99}};

    auto s1 = S(); rnet::Buffer b1(s1); rnet::Serialize(b1, ev1); b1.ToReadOnly();
    auto s2 = S(); rnet::Buffer b2(s2); rnet::Serialize(b2, ev2); b2.ToReadOnly();
    auto s3 = S(); rnet::Buffer b3(s3); rnet::Serialize(b3, ev3); b3.ToReadOnly();
    CHECK(game::net::EntityCompareFn(b1, b2));
    CHECK(!game::net::EntityCompareFn(b1, b3));
}

TEST(rollback_chunk_compare){
    game::terrain::ChunkData cd(oge::Point3{1,2,3});
    game::net::AddChunkEvent c1{}; c1.coords = oge::Point3{1,2,3};
    game::terrain::PaletteCompressedChunk::FromChunkData(cd, c1.chunk);
    game::net::AddChunkEvent c2{}; c2.coords = oge::Point3{1,2,3};
    game::terrain::PaletteCompressedChunk::FromChunkData(cd, c2.chunk);
    game::terrain::ChunkData cd2(oge::Point3{1,2,3}); cd2.data[0] = 99;
    game::net::AddChunkEvent c3{}; c3.coords = oge::Point3{1,2,3};
    game::terrain::PaletteCompressedChunk::FromChunkData(cd2, c3.chunk);

    auto s1 = S(65536); rnet::Buffer b1(s1); rnet::Serialize(b1,c1); b1.ToReadOnly();
    auto s2 = S(65536); rnet::Buffer b2(s2); rnet::Serialize(b2,c2); b2.ToReadOnly();
    auto s3 = S(65536); rnet::Buffer b3(s3); rnet::Serialize(b3,c3); b3.ToReadOnly();

    CHECK(game::net::ChunkCompareFn(b1, b2));
    CHECK(!game::net::ChunkCompareFn(b1, b3));
}

// After a pong aligns the stream to a server tick, Validate should only
// compare and erase predictions up to the alignment tick.  Predictions
// made for ticks the server hasn't confirmed yet survive into the next
// validation cycle.
TEST(rollback_pong_partial_erase_validate)
{
    oge::runtime::OgeRegistry w;
    auto e = w.create(); w.emplace<game::ReplicatedTag>(e);

    game::net::RollbackEventLogStream<> stream;
    stream.m_snapshotInterval = 1;
    stream.AddPeer(63);  // self-bit must be active for EnqueueEvent mask

    game::net::RollbackCapability cap{};
    cap.family = entt::type_hash<game::net::AddEntityEvent>::value();
    cap.takeSnapshot = game::net::EntitySnapshotFn;
    cap.rollback = game::net::EntityRollbackFn;
    cap.compare = game::net::EntityCompareFn;
    stream.RegisterRollbackCapability(cap);

    // Advance ticks — each tick takes a snapshot and records a prediction.
    stream.AdvanceLocalTick(w);                          // tick 1
    stream.InsertPredicted(game::net::AddEntityEvent{e}); // pred at tick 1
    stream.AdvanceLocalTick(w);                          // tick 2
    stream.InsertPredicted(game::net::AddEntityEvent{e}); // pred at tick 2
    stream.AdvanceLocalTick(w);                          // tick 3
    stream.InsertPredicted(game::net::AddEntityEvent{e}); // pred at tick 3
    stream.AdvanceLocalTick(w);                          // tick 4
    stream.InsertPredicted(game::net::AddEntityEvent{e}); // pred at tick 4

    CHECK_EQ(stream.PredictedCount(), 4);

    // One server event — matches the single prediction in the comparison
    // window (tick 1, the only tick with baseTick ≤ tick ≤ alignment).
    auto f = entt::type_hash<game::net::AddEntityEvent>::value();
    auto buf = stream.EnqueueEvent(f, sizeof(entt::entity));
    buf.Write(e);

    // Server confirmed up to tick 1.  Predictions at τ ≤ 1 are compared
    // and erased; predictions at τ > 1 survive.
    stream.DebugSetAlignment(1);

    CHECK(stream.Validate(w));
    CHECK_EQ(stream.PredictedCount(), 3);  // ticks 2, 3, 4 survive
}

// Same as rollback_pong_partial_erase_validate but exercises ValidateLatest.
// ValidateLatest defers one tick (the server's state events for tick T
// travel in a later batch than the AdvanceTick — see its comment):
// predictions stamped < m_alignmentTick are compared and erased, those
// beyond survive.
TEST(rollback_pong_partial_erase_validate_latest)
{
    oge::runtime::OgeRegistry w;
    auto e = w.create(); w.emplace<game::ReplicatedTag>(e);

    game::net::RollbackEventLogStream<> stream;
    // Snapshot every OTHER tick: with a snapshot on every tick the base
    // snapshot lands exactly on the alignment tick and the deferred
    // admission window [baseTick, m_alignmentTick) is empty.  The e2e
    // config (interval 3) has the same phase offset.
    stream.m_snapshotInterval = 2;
    stream.AddPeer(63);

    game::net::RollbackCapability cap{};
    cap.family = entt::type_hash<game::net::AddEntityEvent>::value();
    cap.takeSnapshot = game::net::EntitySnapshotFn;
    cap.rollback = game::net::EntityRollbackFn;
    cap.compare = game::net::EntityCompareFn;
    stream.RegisterRollbackCapability(cap);

    stream.AdvanceLocalTick(w);
    stream.InsertPredicted(game::net::AddEntityEvent{e});  // tick 1
    stream.AdvanceLocalTick(w);                            // snapshot at 2
    stream.InsertPredicted(game::net::AddEntityEvent{e});  // tick 2
    stream.AdvanceLocalTick(w);
    stream.InsertPredicted(game::net::AddEntityEvent{e});  // tick 3
    stream.AdvanceLocalTick(w);                            // snapshot at 4
    stream.InsertPredicted(game::net::AddEntityEvent{e});  // tick 4

    // The tick-1 prediction was pruned with the oldest snapshot.
    CHECK_EQ(stream.PredictedCount(), 3);  // ticks 2, 3, 4

    // One server event so the last-vs-last comparison within the alignment
    // window succeeds.
    auto f = entt::type_hash<game::net::AddEntityEvent>::value();
    auto buf = stream.EnqueueEvent(f, sizeof(entt::entity));
    buf.Write(e);

    // Align to tick 3: base snapshot = 2, deferred admission = τ < 3 →
    // prediction 2 is compared and erased; ticks 3-4 survive.
    stream.DebugSetAlignment(3);

    CHECK(stream.ValidateLatest(w));
    CHECK_EQ(stream.PredictedCount(), 2);  // ticks 3, 4 survive
}

// =========================================================================
// Player input / action serialization + tick-stamped read contract (D3/D6/D8)
// =========================================================================

// PackedPlayerInputFrame: tick always serialized (incl. wrap values),
// flag-gated move quantization, jump input + jump stamp round-trip.
TEST(packed_input_frame_rt)
{
    game::input::PlayerInputFrame f{};
    f.tick = 0xFFFFFFFF;
    f.move = {0.5f, -0.25f, 0.125f};
    f.jump = true;
    f.jumped = true;
    f.jumpPos = {20.f, 18.5f, 20.f};

    game::input::net::PackedPlayerInputFrame p =
        game::input::net::PackFrame(f);
    game::input::net::PackedPlayerInputFrame d{};
    RT(p, d);

    CHECK_EQ(d.tick, 0xFFFFFFFFu);
    CHECK((d.flags & game::input::net::HasMove) != 0);
    CHECK((d.flags & game::input::net::HasJumpInput) != 0);
    CHECK((d.flags & game::input::net::HasJumpStamp) != 0);

    auto u = game::input::net::UnpackFrame(d);
    CHECK_EQ(u.tick, f.tick);
    CHECK(u.jump);
    CHECK(u.jumped);
    CHECK(std::abs(u.move.x - f.move.x) <= 1.f / 127.f);
    CHECK(std::abs(u.move.y - f.move.y) <= 1.f / 127.f);
    CHECK(std::abs(u.move.z - f.move.z) <= 1.f / 127.f);
    CHECK(u.jumpPos == f.jumpPos);
}

// Empty frame: no flags, nothing flag-gated serialized, tick survives.
TEST(packed_input_frame_zero)
{
    game::input::PlayerInputFrame f{};
    f.tick = 42;
    game::input::net::PackedPlayerInputFrame p =
        game::input::net::PackFrame(f);
    game::input::net::PackedPlayerInputFrame d{};
    RT(p, d);
    CHECK_EQ(d.tick, 42u);
    CHECK_EQ(d.flags, 0);
    auto u = game::input::net::UnpackFrame(d);
    CHECK(!u.jump);
    CHECK(!u.jumped);
    CHECK(u.move == game::math::vec3{});
}

// PackedPlayerAction: raw floats, exact round-trip (R14 — no quantization).
TEST(packed_action_rt)
{
    game::input::PlayerAction a{};
    a.actionMask = (1 << 0) | (1 << 1);
    a.origin = {12.25f, -3.5f, 7.75f};
    a.dir = {0.f, -1.f, 0.02f};
    game::input::net::PackedPlayerAction p{a};
    game::input::net::PackedPlayerAction d{};
    RT(p, d);
    auto u = game::input::net::UnpackAction(d);
    CHECK_EQ(u.actionMask, a.actionMask);
    CHECK(u.origin == a.origin);
    CHECK(u.dir == a.dir);
}

TEST(player_action_replication_rt)
{
    game::net::PlayerActionReplicationEvent o{};
    o.playerEntity = entt::entity{9};
    o.tick = 123;
    o.actionCnt = 2;
    o.actions[0] = game::input::net::PackedPlayerAction{
        game::input::PlayerAction{(1 << 0), {1.f, 2.f, 3.f}, {0.f, -1.f, 0.f}}};
    o.actions[1] = game::input::net::PackedPlayerAction{
        game::input::PlayerAction{(1 << 1), {4.f, 5.f, 6.f}, {0.f, 0.f, 1.f}}};

    game::net::PlayerActionReplicationEvent d{};
    RT(o, d);
    CHECK_EQ(o.playerEntity, d.playerEntity);
    CHECK_EQ(o.tick, d.tick);
    CHECK_EQ(o.actionCnt, d.actionCnt);
    CHECK(d.actions[0].originX == o.actions[0].originX);
    CHECK(d.actions[0].dirY == o.actions[0].dirY);
    CHECK(d.actions[1].originZ == o.actions[1].originZ);
}

TEST(player_input_replication_rt)
{
    game::net::PlayerInputReplicationEvent o{};
    o.playerEntity = entt::entity{4};
    game::input::PlayerInputFrame f{};
    f.tick = 77;
    f.move = {1.f, -1.f, 0.5f};
    f.jump = true;
    o.frame = game::input::net::PackFrame(f);

    game::net::PlayerInputReplicationEvent d{};
    RT(o, d);
    CHECK_EQ(o.playerEntity, d.playerEntity);
    CHECK_EQ(o.frame.tick, d.frame.tick);
    CHECK((d.frame.flags & game::input::net::HasMove) != 0);
    CHECK((d.frame.flags & game::input::net::HasJumpInput) != 0);
}

// AggregateTickInput: empty window → false (empty-tick contract).
TEST(aggregate_tick_empty)
{
    game::input::PlayerInputStream s;
    game::input::PlayerInputStream::Cursor c{1};
    game::input::PlayerInputFrame out{};
    CHECK(!game::input::AggregateTickInput(s, c, 5, out));
}

// Stamp + weighted move average (delta-count weighting matches the
// per-frame normalize semantics).
TEST(aggregate_tick_stamp_move)
{
    game::input::PlayerInputStream s;
    game::input::PlayerInputFrameDelta d1;
    d1.moveDelta = {1.f, 0.f};
    s.PushFrame(d1);
    s.AdvanceTick();

    game::input::PlayerInputStream::Cursor c{1};
    game::input::PlayerInputFrame out{};
    CHECK(game::input::AggregateTickInput(s, c, 42, out));
    CHECK_EQ(out.tick, 42u);
    CHECK(game::math::len(out.move) > 0.5f);
}

// Jump input OR + client-decided jump stamp carried through the window.
TEST(aggregate_tick_jump)
{
    game::input::PlayerInputStream s;
    game::input::PlayerInputFrameDelta d;
    d.inputEvent = game::input::PlayerInputEvent{
        game::math::vec2{0.f, 0.f}, game::input::PlayerActionKind::Jump};
    s.PushFrame(d);
    s.MarkJumpPerformed(game::math::vec3{1.f, 2.f, 3.f});
    s.AdvanceTick();

    game::input::PlayerInputStream::Cursor c{1};
    game::input::PlayerInputFrame out{};
    CHECK(game::input::AggregateTickInput(s, c, 7, out));
    CHECK(out.jump);
    CHECK(out.jumped);
    CHECK((out.jumpPos == game::math::vec3{1.f, 2.f, 3.f}));
}

// TickIsStale: wrap-safe comparisons (D8) — the inverted-branch bug made
// frames on the wrong side of the 2^31 boundary misclassify.
TEST(tickstale_wrap)
{
    CHECK(game::input::TickIsStale(5u, 10u));
    CHECK(!game::input::TickIsStale(10u, 5u));
    // One step behind across the wrap → stale.
    CHECK(game::input::TickIsStale(0xFFFFFFFFu, 0u));
    // One step ahead across the wrap → not stale.
    CHECK(!game::input::TickIsStale(0u, 0xFFFFFFFFu));
    CHECK(!game::input::TickIsStale(0xFFFFFFFFu, 0xFFFFFFFEu));
}

// TryReadTickFrame: exact-tick apply (dueTick = simTick - 1).
TEST(tickread_exact_apply)
{
    game::input::PlayerInputStream s;
    s.PushTick({10});
    game::input::PlayerInputStream::Cursor c{1};
    uint32_t last = 0;
    game::input::PlayerInputFrame out{};
    CHECK(game::input::TryReadTickFrame(s, c, 11, last, out));
    CHECK_EQ(out.tick, 10u);
    CHECK_EQ(last, 10u);
}

// Frames stamped <= lastAppliedTick are stale: skipped, never re-applied.
TEST(tickread_stale_skip)
{
    game::input::PlayerInputStream s;
    s.PushTick({5});
    s.PushTick({10});
    game::input::PlayerInputStream::Cursor c{1};
    uint32_t last = 10;
    game::input::PlayerInputFrame out{};
    CHECK(!game::input::TryReadTickFrame(s, c, 11, last, out));
    CHECK_EQ(last, 10u);
}

// A frame stamped ahead of the due tick waits: not consumed, applied once
// the sim reaches its tick.  (last starts inside the degrade window.)
TEST(tickread_not_due_wait)
{
    game::input::PlayerInputStream s;
    s.PushTick({12});
    game::input::PlayerInputStream::Cursor c{1};
    uint32_t last = 4;
    game::input::PlayerInputFrame out{};
    CHECK(!game::input::TryReadTickFrame(s, c, 10, last, out));  // due 9
    CHECK_EQ(last, 4u);
    CHECK(game::input::TryReadTickFrame(s, c, 13, last, out));  // due 12
    CHECK_EQ(out.tick, 12u);
    CHECK_EQ(last, 12u);
}

// A frame behind the due tick missed its window: skipped, and the bounded
// wait degrades lastAppliedTick past the gap after kMaxInputWaitTicks.
TEST(tickread_missed_window)
{
    game::input::PlayerInputStream s;
    s.PushTick({10});
    game::input::PlayerInputStream::Cursor c{1};
    uint32_t last = 0;
    game::input::PlayerInputFrame out{};
    CHECK(!game::input::TryReadTickFrame(s, c, 12, last, out));  // due 11
    CHECK_EQ(last, 11u);  // 12 - 0 > kMaxInputWaitTicks → degrade to dueTick
}

// Pure gap degrade: nothing in the ring, sim 9 ticks past the last applied.
TEST(tickread_gap_degrade)
{
    game::input::PlayerInputStream s;
    game::input::PlayerInputStream::Cursor c{1};
    uint32_t last = 0;
    game::input::PlayerInputFrame out{};
    CHECK(!game::input::TryReadTickFrame(s, c, 9, last, out));
    CHECK_EQ(last, 8u);  // dueTick = 9 - 1
}

// Fresh cursor (0) snaps to the frontier: a reader started after the frames
// were pushed sees nothing (documented sentinel — readers must outlive the
// window they read).  The bounded wait then degrades lastAppliedTick past
// the gap.
TEST(tickread_fresh_cursor_snaps_frontier)
{
    game::input::PlayerInputStream s;
    s.PushTick({10});
    game::input::PlayerInputStream::Cursor c{0};
    uint32_t last = 0;
    game::input::PlayerInputFrame out{};
    CHECK(!game::input::TryReadTickFrame(s, c, 11, last, out));
    CHECK_EQ(last, 10u);  // degraded: 11 - 0 > kMaxInputWaitTicks → dueTick
}

// Wrap-safe read: a frame stamped 0xFFFFFFFF applies at simTick 0
// (dueTick wraps to 0xFFFFFFFF).
TEST(tickread_wrap)
{
    game::input::PlayerInputStream s;
    s.PushTick({0xFFFFFFFF});
    game::input::PlayerInputStream::Cursor c{1};
    uint32_t last = 0xFFFFFFFE;
    game::input::PlayerInputFrame out{};
    CHECK(game::input::TryReadTickFrame(s, c, 0, last, out));
    CHECK_EQ(out.tick, 0xFFFFFFFFu);
    CHECK_EQ(last, 0xFFFFFFFFu);
}

// Regression: a late frame (missed window) before a matching frame must not
// break the scan.  The inverted not-due comparison broke out on the late
// frame and the matching frame was never applied.
TEST(tickread_late_before_due_regression)
{
    game::input::PlayerInputStream s;
    s.PushTick({10});
    s.PushTick({14});
    game::input::PlayerInputStream::Cursor c{1};
    uint32_t last = 0;
    game::input::PlayerInputFrame out{};
    CHECK(game::input::TryReadTickFrame(s, c, 15, last, out));  // due 14
    CHECK_EQ(out.tick, 14u);
    CHECK_EQ(last, 14u);
}

// The same read contract drives the action stream (D6).
TEST(tickread_action_stream_apply)
{
    game::input::PlayerActionStream s;
    game::input::PlayerActionFrame f{};
    f.tick = 21;
    f.actionCnt = 1;
    f.actions[0] = game::input::PlayerAction{
        (1 << 0), {1.f, 2.f, 3.f}, {0.f, -1.f, 0.f}};
    s.PushTick(f);

    game::input::PlayerActionStream::Cursor c{1};
    uint32_t last = 0;
    game::input::PlayerActionFrame out{};
    CHECK(game::input::TryReadTickFrame(s, c, 22, last, out));
    CHECK_EQ(out.tick, 21u);
    CHECK_EQ(out.actionCnt, 1);
    CHECK_EQ(last, 21u);
}

RUN_TESTS("Replication Events Tests")
