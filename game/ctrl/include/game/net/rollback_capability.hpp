#pragma once

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "entt/entity/fwd.hpp"
#include "game/net/replication_events.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game::net
{

namespace net = oge::runtime::net;
using oge::runtime::FamilyId;
using oge::runtime::ICapability;

// =========================================================================
// RollbackCapability
//
// Attached to event types that support client-side prediction with
// rollback.  Three operations:
//
//   takeSnapshot  — dump relevant world state into a byte vector
//   rollback      — restore world state from a snapshot payload
//   compare       — compare two serialised delta payloads for equivalence
//
// Only event types that have this capability may be inserted into a
// RollbackEventLogStream as predicted events.
// =========================================================================

struct RollbackCapability : ICapability
{
    FamilyId family{};

    // Extract a region key from a serialised delta event payload.
    // Used to group events by region for comparison.
    // Entity events: returns entity id (uint32_t cast to uint64_t).
    // Component events: returns entity id.
    // Chunk events: returns hash of chunk coords.
    // Returns 0 for global / non-regional events.
    using RegionKeyFn = uint64_t (*)(net::Buffer& payload);
    RegionKeyFn getRegionKey = nullptr;

    // Serialise the portion of world state relevant to this event family.
    using SnapshotFn = std::pmr::vector<std::byte> (*)(
        const entt::registry& world);
    SnapshotFn takeSnapshot = nullptr;

    // Restore world state from a previously taken snapshot.
    using RollbackFn = void (*)(entt::registry& world, net::Buffer& payload);
    RollbackFn rollback = nullptr;

    // Compare two delta payloads.  The caller guarantees that getRegionKey
    // returns the same value for both (regional comparison).
    using CompareFn = bool (*)(net::Buffer& a, net::Buffer& b);
    CompareFn compare = nullptr;
};

// =========================================================================
// Built-in RollbackCapability implementations for core event types.
// =========================================================================

// --- Entity snapshot / rollback / compare / region key ---
std::pmr::vector<std::byte> EntitySnapshotFn(const entt::registry& world);
void EntityRollbackFn(entt::registry& world, net::Buffer& payload);
bool EntityCompareFn(net::Buffer& a, net::Buffer& b);

// Region key for entity events: entity id is the first field.
inline uint64_t EntityRegionKey(net::Buffer& p)
{
    auto save = p.Data();  // peek without consuming
    entt::entity e = p.Read<entt::entity>();
    p = net::Buffer(const_cast<std::byte*>(save.data()), save.size());
    p.ToReadOnly();
    return static_cast<uint64_t>(static_cast<uint32_t>(e));
}

// --- Component snapshot / rollback (per T) ---
template <typename T>
std::pmr::vector<std::byte> ComponentSnapshotFn(const entt::registry& world)
{
    std::pmr::vector<std::byte> out;
    auto view = world.view<ReplicatedTag, const T>();
    size_t count = 0;
    for (entt::entity _ : view) { (void)_; ++count; }
    size_t needed = sizeof(uint32_t) +
                    count * (sizeof(entt::entity) + net::Size(T{}));
    out.reserve(needed);
    out.resize(out.capacity());
    net::Buffer buf(out);
    buf.Write(static_cast<uint32_t>(count));
    for (entt::entity e : view)
    {
        buf.Write(e);
        const auto& comp = world.template get<T>(e);
        net::Serialize(buf, comp);
    }
    return out;
}

template <typename T>
void ComponentRollbackFn(entt::registry& world, net::Buffer& payload)
{
    // Remove existing T components from replicated entities.
    auto existing = world.view<ReplicatedTag, T>();
    for (entt::entity e : existing)
    {
        world.template remove<T>(e);
    }

    // Restore from snapshot.
    uint32_t count = 0;
    payload.Read(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        entt::entity e = payload.Read<entt::entity>();
        T comp{};
        net::Deserialize(payload, comp);
        if (world.valid(e))
        {
            world.template emplace_or_replace<T>(e, comp);
        }
    }
}

template <typename T>
bool ComponentCompareFn(net::Buffer& a, net::Buffer& b)
{
    auto sa = a.Data();
    auto sb = b.Data();
    if (sa.size() != sb.size()) return false;
    return std::memcmp(sa.data(), sb.data(), sa.size()) == 0;
}

// Region key for component events: entity id is the first serialised field.
template <typename T>
uint64_t ComponentRegionKey(net::Buffer& p)
{
    auto save = p.Data();
    entt::entity e = p.Read<entt::entity>();
    p = net::Buffer(const_cast<std::byte*>(save.data()), save.size());
    p.ToReadOnly();
    return static_cast<uint64_t>(static_cast<uint32_t>(e));
}

// --- Terrain chunk snapshot / rollback ---
std::pmr::vector<std::byte> ChunkSnapshotFn(const entt::registry& world);
void ChunkRollbackFn(entt::registry& world, net::Buffer& payload);
bool ChunkCompareFn(net::Buffer& a, net::Buffer& b);

// Region key for chunk events: hash of coords (first 3 int32_t fields).
inline uint64_t ChunkRegionKey(net::Buffer& p)
{
    auto save = p.Data();
    oge::Point3 coords = p.Read<oge::Point3>();
    p = net::Buffer(const_cast<std::byte*>(save.data()), save.size());
    p.ToReadOnly();
    // Simple FNV-like hash.
    uint64_t h = 14695981039346656037ull;
    h ^= static_cast<uint64_t>(coords.x); h *= 1099511628211ull;
    h ^= static_cast<uint64_t>(coords.y); h *= 1099511628211ull;
    h ^= static_cast<uint64_t>(coords.z); h *= 1099511628211ull;
    return h;
}

// --- Generic helpers ---
inline bool ByteCompareFn(net::Buffer& a, net::Buffer& b)
{
    auto sa = a.Data(); auto sb = b.Data();
    if (sa.size() != sb.size()) return false;
    return std::memcmp(sa.data(), sb.data(), sa.size()) == 0;
}

}  // namespace game::net
