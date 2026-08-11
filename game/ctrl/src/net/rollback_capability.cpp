#include "game/net/rollback_capability.hpp"

#include "game/input/net.hpp"
#include "game/net/replication_events.hpp"
#include "oge/math.hpp"
#include "oge/runtime/net_serializer.hpp"

using namespace game::net;

// =========================================================================
// Entity snapshot / rollback
// =========================================================================

std::pmr::vector<std::byte> game::net::EntitySnapshotFn(
    const oge::runtime::OgeRegistryRef world)
{
    std::pmr::vector<std::byte> out;
    out.reserve(4096);

    auto view = world.view<ReplicatedTag>();
    uint32_t count = static_cast<uint32_t>(view.size());

    // Grow to exact size needed: header (4 bytes) + N * sizeof(entity)
    size_t needed = sizeof(uint32_t) + count * sizeof(entt::entity);
    out.resize(needed);

    net::Buffer buf(out);
    buf.Write(count);
    for (entt::entity e : view)
    {
        buf.Write(e);
    }

    return out;
}

void game::net::EntityRollbackFn(oge::runtime::OgeRegistryRef world, net::Buffer& payload)
{
    // Read the snapshot: count then entity list.
    uint32_t count = 0;
    payload.Read(count);

    std::pmr::vector<entt::entity> snapEntities(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        snapEntities[i] = payload.Read<entt::entity>();
    }

    // Build set of currently replicated entities.
    auto existingView = world.view<ReplicatedTag>();
    std::pmr::unordered_set<entt::entity> existing(
        existingView.begin(), existingView.end());

    // Destroy entities that are in the world but not in the snapshot.
    for (entt::entity e : existing)
    {
        bool found = false;
        for (auto se : snapEntities)
        {
            if (se == e) { found = true; break; }
        }
        if (!found && world.valid(e))
        {
            world.destroy(e);
        }
    }

    // Create entities that are in the snapshot but not in the world.
    for (auto se : snapEntities)
    {
        if (!world.valid(se))
        {
            auto _ = world.create(se);
            (void)_;
        }
        if (!world.all_of<ReplicatedTag>(se))
        {
            world.emplace<ReplicatedTag>(se);
        }
    }
}

bool game::net::EntityCompareFn(net::Buffer& a, net::Buffer& b)
{
    auto sa = a.Data();
    auto sb = b.Data();
    if (sa.size() != sb.size()) return false;
    return std::memcmp(sa.data(), sb.data(), sa.size()) == 0;
}

// =========================================================================
// Terrain chunk snapshot / rollback
// =========================================================================

std::pmr::vector<std::byte> game::net::ChunkSnapshotFn(
    const oge::runtime::OgeRegistryRef world)
{
    std::pmr::vector<std::byte> out;
    out.reserve(65536);

    if (!world.ctx().contains<terrain::TerrainView>())
    {
        // No terrain — empty snapshot.
        uint32_t zero = 0;
        out.resize(sizeof(uint32_t));
        net::Buffer buf(out);
        buf.Write(zero);
        return out;
    }

    auto& terrain = world.ctx().get<terrain::TerrainView>();

    // Count persistent chunks first.
    uint32_t count = 0;
    terrain::ChunkHandle cursor{};
    while (const terrain::ChunkData* c = terrain.PollChunk(cursor))
    {
        if (c->state == terrain::ChunkState::Persistent) ++count;
    }

    // Header: count, then per-chunk: coords (Point3) + blocks (4096
    // uint32_t).
    size_t needed = sizeof(uint32_t) +
                    count * (sizeof(oge::Point3) +
                             terrain::CHUNK_SIZE_TOTAL * sizeof(uint32_t));
    out.resize(needed);
    net::Buffer buf(out);
    buf.Write(count);

    // Second pass: write chunk data.
    terrain::ChunkHandle cursor2{};
    while (const terrain::ChunkData* c = terrain.PollChunk(cursor2))
    {
        if (c->state != terrain::ChunkState::Persistent) continue;

        buf.Write(c->Coords);
        buf.WriteRaw(c->data,
                     terrain::CHUNK_SIZE_TOTAL * sizeof(uint32_t));
    }

    return out;
}

void game::net::ChunkRollbackFn(oge::runtime::OgeRegistryRef world, net::Buffer& payload)
{
    if (!world.ctx().contains<terrain::TerrainView>()) return;

    auto& terrain = world.ctx().get<terrain::TerrainView>();

    // Discard all existing persistent chunks by downgrading them.
    terrain::ChunkHandle cursor{};
    while (const terrain::ChunkData* c = terrain.PollChunk(cursor))
    {
        if (c->state != terrain::ChunkState::Persistent) continue;
        auto [h, _] = terrain.GetChunk(c->Coords);
        terrain.DowngradeChunk(h, terrain::ChunkState::InvalidLighting);
    }

    // Restore from snapshot.
    uint32_t count = 0;
    payload.Read(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        oge::Point3 coords{};
        payload.Read(coords);

        auto [handle, chunk] = terrain.GetChunk(coords);
        if (chunk == nullptr)
        {
            handle = terrain.CreateChunk(coords);
            chunk = terrain.GetChunk(handle);
        }
        if (chunk == nullptr) continue;

        payload.ReadRaw(chunk->data,
                        terrain::CHUNK_SIZE_TOTAL * sizeof(uint32_t));
        chunk->Coords = coords;

        terrain.DowngradeChunk(handle, terrain::ChunkState::InvalidLighting);
        terrain.UpgradeChunk(handle, terrain::ChunkState::Persistent);
    }
}

bool game::net::ChunkCompareFn(net::Buffer& a, net::Buffer& b)
{
    auto sa = a.Data();
    auto sb = b.Data();
    if (sa.size() != sb.size()) return false;
    return std::memcmp(sa.data(), sb.data(), sa.size()) == 0;
}

// =========================================================================
// Physics body compare — tolerance-based (client and server diverge)
// =========================================================================

bool game::net::PhysicsBodyCompareFn(net::Buffer& a, net::Buffer& b)
{
    UpdateComponentEvent<ComponentPhysicBody> evtA{};
    UpdateComponentEvent<ComponentPhysicBody> evtB{};
    net::Deserialize(a, evtA);
    net::Deserialize(b, evtB);

    if (evtA.entity != evtB.entity) return false;

    // Must cover the prediction lead (client tick - server tick) in
    // position units.  With a 6-tick lead, 20 Hz, and 5 u/s max player
    // speed: 6 * 0.05 s * 5 u/s = 1.5 u.  Use 2.0 u for margin.
    // TODO: match predictions to server events by tick so the epsilon
    // can be tightened.
    constexpr float kPositionEpsilon = 2.0f;
    auto res = oge::math::len(evtA.component.pos - evtB.component.pos) <=
           kPositionEpsilon;
    return res;
}
