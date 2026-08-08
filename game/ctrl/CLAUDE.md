# game/ctrl — Network Replication Module

## Overview

The `game/ctrl` module provides a client-server networking layer built on ENet, using an
event-log-based replication system.  The core idea: game state changes (entity creation,
component updates, terrain changes, player input) are recorded as typed events in an
`EventLogStream`, then serialized and sent to peers via `ReplicationRegistry`.

On the client side, `RollbackEventLogStream` extends the basic event log with prediction
and rollback support, enabling lag-compensated gameplay.

## Directory layout

```
game/ctrl/
├── include/game/net/
│   ├── replication_registry.hpp        — ReplicationRegistry, PacketScheduler, ReplicationCapability
│   ├── replication_events.hpp          — Event structs, hooks, apply functions, NetTraits, state types
│   ├── event_log_stream.hpp            — EventLogStream (ring-buffer-backed log with per-peer masks)
│   ├── rollback_capability.hpp         — RollbackCapability + built-in snapshot/rollback/compare fns
│   └── rollback_event_log_stream.hpp   — RollbackEventLogStream (client-side prediction + rollback)
├── src/net/
│   ├── replication_registry.cpp        — Registration of all event types
│   └── rollback_capability.cpp         — Entity & chunk snapshot/rollback implementations
├── test/
│   └── replication_events_test.cpp     — 43 unit tests
└── CLAUDE.md                           — This file
```

## Wire protocol (high-level)

Each packet carries:
```
┌─────────────────────────────────────────────────────┐
│ FamilyId      (4 bytes)   — event type hash          │
│ LogCursor     (8 bytes)   — position in event log    │
│ Payload bytes (variable)  — serialized event         │
└─────────────────────────────────────────────────────┘
```

The `FamilyId` is `entt::type_hash<T>::value()` for the event type `T`.  On the receiver side,
`ReplicationRegistry::HandleIncoming` deserializes the event and calls the matching
`ReplicationCapability::apply` function.

Events do **not** contain a discriminator byte — the type hash in the header identifies
the event.  Each event type registers its own `ReplicationCapability`.

---

## Event types

### Entity lifecycle
| Event | Payload | Trigger |
|---|---|---|
| `AddEntityEvent` | `entt::entity` | `on_construct<ReplicatedTag>` |
| `RemoveEntityEvent` | `entt::entity` | `on_destroy<ReplicatedTag>` |

### Component lifecycle (one set per component type T)
| Event | Payload | Trigger |
|---|---|---|
| `AddComponentEvent<T>` | `entity` + `T` | `on_construct<T>` or `on_construct<ReplicatedTag>` |
| `UpdateComponentEvent<T>` | `entity` + `T` | `on_update<T>` |
| `RemoveComponentEvent<T>` | `entity` | `on_destroy<T>` |

Registered component types: `ComponentAABBCollider`, `ComponentCamera`,
`ComponentPerspectiveCamera`, `ComponentPhysicBody`, `ComponentCreature`, `ComponentPlayer`.

### Terrain chunks
| Event | Payload |
|---|---|
| `AddChunkEvent` | `Point3 coords` + `vector<uint32_t> blocks` (+ `bool compressed`) |
| `RemoveChunkEvent` | `Point3 coords` |
| `UpdateChunkEvent` | `Point3 coords` + dirty blocks (up to 29) |

`AddChunkEvent` supports RLE compression (`CompressChunk`/`DecompressChunk`).  Set
`compressed = true` to enable.  Serialization handles both paths; tests cover both.

### Player input
| Event | Payload |
|---|---|
| `PlayerInputReplicationEvent` | `entt::entity player` + `PackedPlayerInputFrame` |

---

## Two snapshot systems

This module contains **two distinct** snapshot mechanisms.  They serve different purposes
and should not be confused.

### 1. Initialisation Snapshot (`ReplicationRegistry::GenerateSnapshot`)

**Purpose**:  Bring a newly-joined peer up to date with the current world state.

**Trigger**:  Called automatically from `ReplicationRegistry::AddPeer(id, peer, world)`.

**What it does**:
- Iterates all entities with `ReplicatedTag` → emits `AddEntityEvent`
- Iterates all registered component types on those entities → emits `AddComponentEvent<T>`
- Iterates all persistent terrain chunks → emits `AddChunkEvent`
- Each event is enqueued with a `peerMask` targeting **only** the new peer, so existing
  peers do not receive duplicate data.

**Registration**:  Component types must be registered with
`ReplicationRegistry::RegisterSnapshotComponent<T>()`.  This is done in
`RegisterReplications()` (see `replication_registry.cpp`).

**Key APIs**:
```cpp
// In ReplicationRegistry:
void AddPeer(PeerId id, ENetPeer* peer, entt::registry* world = nullptr);
void GenerateSnapshot(PeerId peerId, entt::registry& world);
template <typename T> void RegisterSnapshotComponent();
```

### 2. Rollback Snapshot (`RollbackEventLogStream`)

**Purpose**:  Enable client-side prediction with rollback on misprediction.  Used by the
client to optimistically predict game state, then validate against server-authoritative
events and roll back if they diverge.

**Trigger**:  Periodically via `RollbackEventLogStream::AdvanceTick(world)` (every
`m_snapshotInterval` ticks).  Validation happens explicitly via `Validate(world)`.

**Architecture**:
```
┌──────────────────────────────────────────────────────────────┐
│ RollbackEventLogStream (inherits EventLogStream)             │
│                                                              │
│  Base EventLogStream (authoritative server events)           │
│  ├── m_entries, m_payloads, m_currentTail                   │
│  └── PeekEvent / TryDequeueEvent / EnqueueEvent              │
│                                                              │
│  Rollback additions:                                         │
│  ├── m_predictedEvents  — deque<PredictedEntry>              │
│  │     Client-side predictions, stored separately from       │
│  │     the authoritative stream.  Only event types with a    │
│  │     registered RollbackCapability are accepted.           │
│  │                                                          │
│  ├── m_snapshots        — deque<SnapshotPoint>               │
│  │     Periodic snapshots of world state.  Each entry has:   │
│  │       - streamCursor: position in base stream             │
│  │       - payloads: per-family serialised world state       │
│  │                                                          │
│  ├── m_rollbackCaps     — map<FamilyId, RollbackCapability>  │
│  │     Registered capabilities that define how to snapshot,  │
│  │     rollback, compare, and extract region keys.           │
│  │                                                          │
│  └── m_currentTick, m_snapshotInterval                      │
│         Tick counter and snapshot frequency.                 │
└──────────────────────────────────────────────────────────────┘
```

**Lifecycle**:
1. Server events arrive via the normal `HandleIncoming` path → stored in the base
   `EventLogStream`.
2. The client *predicts* events locally by calling `InsertPredicted()`.  Only event types
   with a registered `RollbackCapability` are accepted (gating).
3. Every `m_snapshotInterval` ticks, `AdvanceTick()` takes a snapshot of the world state
   using the registered `RollbackCapability::takeSnapshot` functions.
4. `Validate()` compares predicted events against server-authoritative events.  If they
   diverge, the latest snapshot is restored via `RollbackCapability::rollback`, predicted
   events are discarded, and the client can re-simulate from the restored state.
5. Old snapshots beyond `m_maxSnapshots` are pruned automatically.

**Key APIs**:
```cpp
// In RollbackEventLogStream:
void RegisterRollbackCapability(const RollbackCapability& cap);
bool InsertPredicted(FamilyId family, const std::vector<std::byte>& payload);
template <typename TEvent> bool InsertPredicted(const TEvent& event);
void AdvanceTick(entt::registry& world);
bool Validate(entt::registry& world);
void RollbackToLatest(entt::registry& world);
```

**Configuration**:
```cpp
stream.m_snapshotInterval = 20;   // take snapshot every N ticks
stream.m_maxSnapshots = 32;       // keep at most this many
```

**Comparison table**:

| Aspect | Init Snapshot | Rollback Snapshot |
|---|---|---|
| Purpose | New peer catch-up | Client prediction validation |
| When | On peer join | Every N ticks |
| Scope | All entities + chunks | Per-family (registered caps) |
| Target | Single peer (peerMask) | All peers (state restore) |
| Data stored | In EventLogStream entries | In RollbackEventLogStream::m_snapshots |
| Restore action | Apply events from stream | Call RollbackCapability::rollback |

---

## Regional Rollback

The rollback system operates **regionally** rather than globally.  Each payload carries a
region key, extracted by `RollbackCapability::getRegionKey`:

| Event family | Region key | Extracted from |
|---|---|---|
| Entity events (`AddEntity`, `RemoveEntity`) | entity ID (`uint32_t` → `uint64_t`) | first field of payload |
| Component events (`AddComponent<T>`, `UpdateComponent<T>`, `RemoveComponent<T>`) | entity ID | first field of payload |
| Chunk events (`AddChunk`, `RemoveChunk`, `UpdateChunk`) | FNV-1a hash of `Point3` coords | first 12 bytes of payload |

During `Validate()`, payloads are **grouped by region key** before comparison.  Only
payloads with matching region keys are compared against each other.  This means:
- A component update for entity A is only compared against server updates for entity A
- A chunk update for chunk (1,2,3) is only compared against server updates for that chunk
- Events targeting different regions are independent

When a mismatch is detected, **only the diverged regions are rolled back** (the rollback
function restores state from the snapshot for all regions, but the comparison is
region-aware).

### Adding rollback for a new event type

```cpp
// 1. Implement the four functions (or reuse existing helpers):
RollbackCapability cap;
cap.family   = entt::type_hash<MyEvent>::value();
cap.getRegionKey     = &MyRegionKeyFn;      // extract region from payload
cap.takeSnapshot     = &MySnapshotFn;       // dump world state
cap.rollback         = &MyRollbackFn;       // restore from snapshot
cap.compare          = &ByteCompareFn;      // or a custom comparator

// 2. Register in the RollbackEventLogStream:
stream.RegisterRollbackCapability(cap);

// 3. Now InsertPredicted<MyEvent>(...) will be accepted.
```

---

## Key types

### `EventLogStream<Capacity=4096>`
Ring-buffer-backed log with per-peer receive masks.  Protected members allow inheritance
by `RollbackEventLogStream`.

### `ReplicationRegistry`
Central coordinator.  Holds `EventLogStream*`, `PacketScheduler`, and per-peer state.

### `SimplePacketScheduler`
Concrete scheduler with configurable `m_maxBytesPerFrame` (default 1200 for MTU-safe).

### `ReplicationCapability`
Per-event-type metadata: `family`, `sendType`, `installHooks`, `apply`.

### `RollbackCapability`
Per-event-type rollback metadata:
- `getRegionKey` — `uint64_t(*)(Buffer&)` — extract region from payload
- `takeSnapshot` — `pmr::vector<byte>(*)(const registry&)` — dump state
- `rollback` — `void(*)(registry&, Buffer&)` — restore state
- `compare` — `bool(*)(Buffer&, Buffer&)` — compare two payloads

### `RollbackEventLogStream<Capacity>`
Inherits `EventLogStream`.  Adds prediction buffer, periodic snapshots, and
regional validation with rollback support.

---

## State in `world.ctx()`

| Type | Purpose |
|---|---|
| `EventLogStream<>` | Central event log |
| `TerrainReplicationState` | Cursor into `ChunkEventStream` for terrain polling |
| `PlayerInputReplicationState` | Per-player cursors and stream pointers |

---

## Serialization

Events use `DECL_NET_OBJ` or custom `NetTraits` specializations in `oge::runtime::net`.
Notable additions:
- `NetTraits<entt::entity>` — entity is a distinct enum type
- `NetTraits<oge::IntTriple<T>>` — covers `Point3`, `LocalPoint3`, `LocalUPoint3`
- Component events use generic partial `NetTraits` specializations inheriting `ObjectTraits`
- `AddChunkEvent` has custom `NetTraits` supporting both raw and RLE-compressed paths

---

## Adding a new component type

1. In `replication_registry.cpp`: Add `RegisterComponentEvents<T>(af)` and
   `rf.RegisterSnapshotComponent<T>()` in `RegisterReplications`.

---

## Building and testing

```sh
# Build the library
cmake --build . --target game_ctrl

# Build and run tests (43 tests)
cmake --build . --target replication_events_test
./game/ctrl/replication_events_test

# Via CTest
ctest -R replication_events_test
```

### Test categories
| Category | Count | Covers |
|---|---|---|
| Serialization round-trips | 8 | entity, component, chunk events |
| Apply functions | 6 | entity add/remove, component add/update/remove |
| Hooks | 3 | entity construct/destroy, component construct |
| EventLogStream | 4 | enqueue/peek/dequeue, multi-peer, cursor |
| PacketScheduler | 3 | basic, byte limit, empty stream |
| Init snapshot | 2 | entity snapshot, terrain snapshot |
| Compression | 4 | non-compressed, compressed, RLE basic, empty |
| ReplicationRegistry | 1 | add/remove peer |
| Rollback | 12 | register, gating, entity/chunk/component snapshot/rollback, stream tick/validate, payload/chunk compare |
