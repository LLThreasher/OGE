# OGE — One Game Engine

## Project Summary
Vulkan-based game engine + block-world game ("Arterium").  Client-server multiplayer
with ENet networking, entity replication, client-side prediction with rollback, and
voxel terrain.

**Active branches:** `dev/network4` (current), `main` (stable)

## Tech Stack
- **Language:** C++20 (clang++, Apple Clang on macOS)
- **Build:** CMake 3.28+, Ninja
- **Libraries:** EnTT (ECS), Vulkan (graphics), ENet (networking), SDL3 (windowing),
  GLM (math), spdlog/fmt (logging), stduuid (player IDs)
- **Testing:** Custom inline macros via `oge::test_support` interface library
  (`TEST`, `CHECK`, `CHECK_EQ`, `RUN_TESTS` in `<test_macros.hpp>`)

## Layer Architecture (engine/modules/)
```
core         — RingBuffer, AABB, EventStream, OGE_ASSERT, log (no deps)
platform     — Stack traces, IO, memory (depends on core)
runtime      — EnTT wrapper (OgeRegistry), net serialisation, TypeRegistry
               (depends on core + platform)
test_support — Test macros interface library (no deps)
graphics*    — Vulkan/Metal backends (pre-built, source not in repo)
```

## Code Style
- **Namespaces:** `oge::runtime`, `game::net`, `game::view::gfx`, etc.
- **Naming:** PascalCase classes, camelCase functions, `m_` member prefix
- **Macros:** `DECL_TYPE_NAME(Type, "Name")` for type registry; `DECL_NET_OBJ` for
  network serialisation; `DECL_JSON_OBJ` for JSON; `NO_COPY` to delete copy
- **Assertions:** `OGE_ASSERT(cond, fmt, ...)` — compiled out in release.  Stack
  trace hook set by platform init.
- **Type names:** Always use `DECL_TYPE_NAME` right after the struct definition,
  never in a separate file.  Include `oge/runtime/type_name.hpp` for the macro.
- **Registries:** Use `oge::runtime::OgeRegistry` (owning) or `OgeRegistryRef`
  (non-owning) instead of raw `entt::registry`.  Never include `entt::registry`
  outside `oge::runtime`.

## Key Systems
- **Replication:** Event-log-based (`EventLogStream` → `ReplicationRegistry`).
  Each event type has its own `ReplicationCapability`.  Server generates
  initialisation snapshots on peer join.  See `game/ctrl/CLAUDE.md`.
- **Rollback:** `RollbackEventLogStream` extends EventLogStream with client-side
  prediction + regional rollback.  `RollbackCapability` provides
  snapshot/rollback/compare functions per event family.  See `game/ctrl/CLAUDE.md`.
- **Rendering:** Command-buffer / render-pass architecture.  Passes consume typed
  commands from `SubmissionQueue`.  `PanelSprite` for 9-slice UI.  See
  `game/view/CLAUDE.md`.
- **Terrain:** Voxel chunks with LOD, RLE-compressed network transfer,
  `BlockRegistry` for block type definitions.

## Build & Test
```sh
# Configure (macOS, no Metal)
cmake -S . -B out/build/apple-debug -DBUILD_METAL=OFF

# Build
cmake --build out/build/apple-debug --target Arterium

# Build & run all tests
cmake --build out/build/apple-debug --target all
ctest --test-dir out/build/apple-debug

# Add a test: use add_test_target helper in cmake/utils.cmake
add_test_target(name SOURCES test/foo.cpp LIBRARIES game::ctrl oge::platform::native)
```

## Test Suites (7 suites, 93 tests)
| Suite | Count | Module | Covers |
|---|---|---|---|
| datastruct_test | 11 | core | RingBuffer, DiscreteEventStream |
| oge_registry_test | 19 | runtime | OgeRegistry CRUD, views, ctx, signals |
| replication_events_test | 43 | ctrl | Events, hooks, EventLog, scheduler, snapshot, compression, rollback |
| registry_bug_recreate_test | 4 | ctrl | OgeRegistry vs raw entt parity (regression) |
| scene_load_test | 1 | ctrl | Scene construction + config |
| sim_physics_test | 12 | sim | AABB collision, PhysBody defaults |
| debug_scene3_test | 3 | ctrl_ext | Type registration, inheritance |

## E2E Network Tests

For end-to-end integration tests over a real ENet loopback connection, **use
`game/ctrl/test/scene_test_harness.hpp`** — do **not** build a custom loopback
harness from scratch.  The harness drives `DebugServerScene` + `ClientConnScene`
→ `ClientScene2` through a full scene lifecycle and exposes `serverWorld()` /
`clientWorld()` / `poll()` / `pumpUntil()`.

**Pattern** — follow `game/ctrl/test/e2e_scene_test.cpp`:
```cpp
#include <test_macros.hpp>
#include "scene_test_harness.hpp"   // NetSceneHarness

TEST(my_e2e_test)
{
    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.waitForHandshake());   // polls until server has ComponentPlayer

    // Poll some frames, then assert client state
    CHECK(h.pumpUntil([&] {
        auto& cw = h.clientWorld();
        // check client has expected replicated entity/component/chunk
        ...
        return true;
    }, 400));
}
```

**Key harness API:**
| Method | Description |
|---|---|
| `h.start()` | Init ENet + register scenes + switch to them |
| `h.waitForHandshake(maxPolls)` | Poll until server world has ComponentPlayer |
| `h.poll()` | One tick: server + client `Scene::Update` |
| `h.pumpUntil(fn, maxPolls)` | Poll until `fn()` returns true |
| `h.serverWorld()` / `h.clientWorld()` | Mutable `GameWorld&` for assertions |

**Register in CMakeLists.txt:**
```cmake
add_test_target(my_test
    SOURCES test/my_test.cpp
    LIBRARIES game::ctrl oge::platform::native
)
```

## Chunk Replication Notes

- **`PollTerrainChunkEvents(world)`** — call after subsystems run (now in
  `Scene::Update`) to flush `ChunkStateUpdateEvent`s from `TerrainView`'s
  `ChunkEventStream` into the replication `EventLogStream`.
- **`InstallTerrainReplicationHooks(world)`** — call once in the server scene
  constructor to emplace `TerrainReplicationState` in ctx.
- **`TerrainReplicationState::chunkEventCursor`** must start at **1**, NOT 0 —
  `DiscreteEventStream::PollOne(cursor=0)` snaps to frontier (head), skipping
  the first event.  Default-initialised `Cursor{}` (0) causes all events to be
  missed silently.
- **`AddChunkEvent`** carries a `terrain::PaletteCompressedChunk` (≤255-block
  palette + 4096 one-byte indices).  The wire format is coords + palette +
  **RLE-compressed indices** (custom `NetTraits` in `replication_events.hpp`).
  `SimplePacketScheduler` has **no byte limit** — ENet fragments reliable
  packets, so oversized payloads are safe.
- **Client must have `TerrainView` + `BlockRegistry`** in its world for
  `ApplyEvent(AddChunkEvent)` / `ApplyEvent(UpdateChunkEvent)` to write chunks.
  `ClientScene2` self-loads its `scene_config` (terrain load mask) in its
  constructor.  `ClientConnScene` forwards a caller-provided `scene_config` to
  the next scene; otherwise it synthesizes the default client config
  (SubsystemDebugText + realtime SubsystemPlayer).
- **UpdateChunk** (block updates): `TerrainView::SetBlock` emits a dirty
  `ChunkStateUpdateEvent` (1–29 dirty blocks) → `PollTerrainChunkEvents` pushes
  `UpdateChunkEvent`.  E2E coverage: `e2e_add_chunk_replicates` +
  `e2e_update_chunk_replicates` in `game/ctrl/test/e2e_scene_test.cpp`.

## Player Input Replication Notes

- **`PlayerInputStream` is a component** on the player entity.  The server's
  `ComponentPlayer::CreatePlayer` emplaces it (before `ComponentPlayer`);
  on the client, `DebugVoxelView::onConstructPlayer` emplaces it manually
  (PlayerInputStream is **not** a replicated component — the snapshot only
  covers physics/camera/creature/player components).
- **`InstallPlayerInputReplicationHooks(world)`** — emplaces
  `PlayerInputReplicationState` in ctx and auto-registers streams via
  `on_construct<PlayerInputStream>` / `on_construct<ComponentPlayer>` (both
  orders of component creation covered) + `on_destroy` unregister.  Called by
  **both** `DebugServerScene` (so `ApplyEvent` can write into the server
  player's stream) and `ClientScene2` (so local input gets polled).
- **`PollPlayerInputs(world)`** — called from `ClientScene2::Update` before
  `ProduceAll`.  Packs each stream's new input into a
  `PlayerInputReplicationEvent` (`PackedPlayerInputFrame`, quantized via
  `game::input::net`).  Reliable channel (`SingleReliable`).  The server
  never calls it — input only flows client → server.
- **`ApplyEvent(PlayerInputReplicationEvent)`** — inserts the unpacked frame
  into the server player's stream; `SubsystemPlayer` consumes it with
  `ComponentPlayer::inputCursor`.  Entity ids are shared between client and
  server (client mirrors server ids), so `playerEntity` matches.
- **Wire format**: `PlayerInputReplicationEvent` = entity + packed frame
  (flags + moveX/Y SNorm8 + panX/Y + up to 255 packed action events).
  `DECL_NET_OBJ` for the packed types lives in `game/data/include/game/input/
  net.hpp`.
- **E2E coverage**: `e2e_player_input_replicates` in
  `game/ctrl/test/e2e_scene_test.cpp` — test emplaces a `PlayerInputStream`
  on the client player (the harness has no DebugVoxelView), injects a jump
  action + move delta, and pumps until the server's stream has the same
  content.  Note: a zero `Cursor{}` snaps to the frontier and skips all
  events — the test reads from cursor 1 to see the first event.

## Known Issues
1. **`scene_load_test`**: `Scene::Load()` with JSON config triggers `__next_prime
   overflow` in `BlockRegistry::RegisterBlock` when linked with certain .a order.
   Root cause: static init order fiasco in `DECL_JSON_OBJ` templates.  Only
   manifests in test binaries; Arterium itself works.
2. **`replication_events_test::snapshot_terrain`**: Flaky — `PeekEvent` sometimes
   returns false.  Likely timing/initialisation order in test setup.
3. **`OgeRegistryRef` null safety**: `CHECK_NULL_REGISTRY` macro not applied
   consistently to all methods (missing on `view`, `valid`, `on_construct`, etc.).

## TODOs
- [x] Move remaining DECL_TYPE_NAME to struct definition sites (component types)
- [ ] Investigate snapshot_terrain test flakiness
- [ ] Write integration tests for networking (server+client loopback)
- [ ] a gizmo renderer (live under rendering pipeline) that draw uiraycast target + screen rect with the draw gizmo rect cmd, it should behave similarly to the current implementation in ui renderer. remove the related logic in ui renderer once done. 
- [ ] the gizmo renderer should also draw aabb + component creature with draw gizmo cube cmd. 

## Adding New Features
For step-by-step guides on adding **blocks, components, subsystems, renderers,
and passes**, see [CONTRIBUTION.md](CONTRIBUTION.md).

1. **New component type:** Add `DECL_TYPE_NAME` in the struct's header.  If it
   needs network replication, add `RegisterComponentEvents<T>` in
   `replication_registry.cpp` + `RegisterSnapshotComponent<T>`.
2. **New event type:** Define struct + `DECL_NET_OBJ` in `replication_events.hpp`.
   Add `DECL_TYPE_NAME` + register in `replication_registry.cpp`.  Add rollback
   capability functions in `rollback_capability.hpp/.cpp` if needed.
3. **New test:** `add_test_target(name SOURCES test/x.cpp LIBRARIES ...)` in
   CMakeLists.txt.  Include `<test_macros.hpp>`.
