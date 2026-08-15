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

## Test Suites (7 suites, 97 tests)
| Suite | Count | Module | Covers |
|---|---|---|---|
| datastruct_test | 11 | core | RingBuffer, DiscreteEventStream |
| oge_registry_test | 20 | runtime | OgeRegistry CRUD, views, ctx, signals, stage-dup guard |
| replication_events_test | 43 | ctrl | Events, hooks, EventLog, scheduler, snapshot, compression, rollback |
| registry_bug_recreate_test | 4 | ctrl | OgeRegistry vs raw entt parity (regression) |
| scene_load_test | 4 | ctrl | Scene construction + config, standalone sim smoke + dig + jump |
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
- **`TerrainReplicationState::chunkEventCursor`** — the current code
  initialises it at **1** (a 0 cursor is the `DiscreteEventStream::PollOne`
  snap-to-frontier sentinel, which skips queued events).  Note: c4e8d59's
  intended behavior was **0** (skip the pre-poll backlog; the join snapshot
  covers current state) — see "Intended chunk replication behavior" below.
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

### Intended chunk replication behavior (from c4e8d59 "fix terrain bug")

The terrain-bug fix (commit `c4e8d59`) established the intended contract
between the server's chunk state machine and the replication events.  The
current code diverges in two places (see "Known divergence" below) — when in
doubt, treat this section as the spec.

1. **One replication event per state transition.**  `PollTerrainChunkEvents`
   classifies each `ChunkStateUpdateEvent` and pushes **exactly one** event:
   - Chunk **entering `Persistent`** (`prevState != Persistent &&
     state == Persistent`):
     - `IsAllDirty()` (dirtyCnt == 255, set by `MarkAllDirty()` in terrain
       generation) → full **`AddChunkEvent`** (palette-compressed).
     - else `dirtyCnt > 0` (block edit via `SetBlock`) → **`UpdateChunkEvent`**
       (≤29 dirty blocks).
     - else (`dirtyCnt == 0` — pure state change, e.g. neighbor
       revalidation) → nothing.
   - Chunk **leaving `PendingDestroy`** (`prevState == PendingDestroy &&
     state != PendingDestroy`) → **`RemoveChunkEvent`**.
   - Before the fix, one block edit (Persistent → InvalidLighting →
     Persistent) emitted *three* events: a spurious `RemoveChunkEvent`, a full
     `AddChunkEvent`, and an `UpdateChunkEvent` — deleting the client chunk
     and re-sending 4 KB per edit.
2. **`SetBlock` marks every face-touching neighbor** for revalidation via
   `ChunkDir::ForEachDirtyChunkNeighbor*` (`defs.hpp`: FACE_IDX_*/FACE_MASK_*).
   The edited chunk gets the dirty-block event; neighbors get a pure state
   change (dirtyCnt == 0 → nothing replicated).
3. **Client `ApplyEvent(UpdateChunkEvent)` gate:** the chunk must exist **and**
   be `state == Persistent`; otherwise the update is dropped.  Updates must
   never be written into a chunk whose full data has not been validated yet
   (a chunk awaiting neighbor upgrades has `state < Persistent`).  The apply
   path mirrors server `SetBlock`: write blocks, `DowngradeChunk
   (InvalidLighting)`, `UpgradeChunk(Persistent)`.
4. **`TerrainReplicationState::chunkEventCursor` starts at 0.**  A zero cursor
   is the `PollOne` snap-to-frontier sentinel: the pre-poll backlog (events
   queued before hooks ran, e.g. chunks generated before a peer joined) is
   skipped, and current state is covered by the peer-join snapshot
   (`GenerateSnapshot`).  Replaying the backlog can resurrect stale
   "ghost" chunks (AddChunk for a chunk already destroyed server-side).

**Known divergence (068d422 "add interpolation layer..."):** commit `068d422`
changed both:
- `ApplyEvent(UpdateChunkEvent)` now gates on `chunk->weakState < Persistent`
  instead of `state != Persistent` — updates are applied while the chunk
  awaits neighbor validation (prevents dropping updates, but contradicts
  intent #3).
- `chunkEventCursor` back to `{1}` (contradicts intent #4).

**Inconsistent test:** `e2e_update_chunk_replicates`
(`game/ctrl/test/e2e_scene_test.cpp`) asserts the client receives a block
update even when its chunk copy is still waiting on neighbors — the server
sets the block as soon as its chunk is persistent, the client's chunk may not
be `Persistent` yet, and the state gate drops the update.  Under the intended
behavior this test fails (`clientValue != stoneId`); it only passes because
of the 068d422 weakState gate.  If intent #3 is restored, the test must
synchronize first (e.g. wait until the client chunk is `Persistent` before
`SetBlock` on the server).

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
- **Standalone scenes have no poller**: `PollPlayerInputs`/`PollPlayerActions`
  live in the transport layer (`ClientScene2`).  A bare `game::Scene` (no
  SimTickContext in ctx) skips them, so `SubsystemPlayer<FixedStep>`'s
  bare-world fallback drains the `PlayerActionStream` accumulation directly
  (`AggregateTick` + `ApplyRayAction`) — without the drain the 3-slot
  accumulator fills and every further action warns "player action overflow"
  while never applying.  Gated by `scene_standalone_player_dig_action`
  (scene_load_test).
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
- **Handshake protocol version**: `net::kProtocolVersion` (2, in
  `game/ctrl/include/game/net/protocol.hpp`) starts the client's first
  handshake packet (`[version, PlayerInfo]`) and the server's reply echoes
  it (`[version, playerEntity]`); both sides reject mismatched/truncated
  handshakes so stale binaries fail loudly instead of misreading the packet
  layout.  Bump on every wire-format change.  Overridable per scene via the
  `protocol_version` scene arg (the harness exposes it for
  `e2e_handshake_version_mismatch_rejected`).

## Standalone Scene Notes (transport-agnostic sim)

The sim must never branch on the transport layer's existence — `game::Scene`
runs the full default config with or without server/client scenes.  The
contract:

- **`SimTickContext` is guaranteed in every scene world at Scene
  construction.**  The `Scene` ctor emplaces it (entt ctx `emplace` is
  `try_emplace`-idempotent, so the transport scenes' own ctor emplaces are
  harmless).  This must live in the ctor, not `Load()`: `SceneRunner`
  constructs a scene and calls `Update` directly without ever loading it
  (e.g. `ClientConnScene`, the bare `game::Scene` placeholder) — a
  Load-only guarantee left those worlds without a tick ctx and the fixed
  block aborted on its first fire.
- **`Scene::Update` owns the fixed-step loop** and drives the pipeline in
  `kSubStepDt` sub-steps, writing `SimTickContext.subStepIdx` per sub-step.
  The fixed pipeline interval is `kSubStepDt` (Scene ctor default; transport
  scenes call `SetUpdateInterval(sim::kSubStepDt)` explicitly) — a longer
  interval lets the pipeline's internal scheduler collapse the sub-steps
  into one stage update at the last sub-step index, and the
  `subStepIdx == 0` decision gate never opens.  A bare scene's fixed-frame
  duration is also `kSubStepDt` (one sub-step, 60 Hz) so fixed physics
  integrates every frame — smooth standalone movement without a realtime
  body sim; transport scenes override to `sim::kFixedFrameDuration`
  (1/20, 3 sub-steps).
- **Tick arbitration:** transport scenes overwrite `currentTick` from their
  replication tick before `Scene::Update` (every fixed frame — their 1/20
  frame aligns with `kSubStepsPerTick`, so arbitration never fires there).
  When `currentTick` is unchanged, the scene owns its tick space: it
  advances the tick and runs `sim::AggregateLocalInputs` with stamp
  `tick - input::kInputPipelineDelayTicks` — the same one-tick pipeline
  delay and the same empty-window contract as the transport pollers.
- **`SubsystemPlayer<FixedStep>` has one code path.**  It reads
  `SimTickContext` + `PlayerSimInputState` unconditionally; every input
  configuration stamps tick frames into the rings (transport pollers, or
  `AggregateLocalInputs` for transport-less scenes).  `AggregateLocalInputs`
  only aggregates streams with `IsLocalInput()` — replicated players fill
  their rings via `ApplyEvent(PushTick)`.
- A fresh aggregate cursor snaps to the raw-ring frontier (shared cold-start
  contract — also true on the networked path), so the very first frame
  pushed by a stream is sacrificed; tests prime with a no-op frame before
  injecting the frame under test.
- **The default standalone config registers one body sim.**
  `GetDefaultSceneConfig` puts the fixed trio (terrain + fixed
  player/creature/physics) in the fixed pipeline and only the realtime
  player stage (camera chase, raw-frame drain, ray baking) in the realtime
  pipeline.  Registering the realtime creature/physics too would integrate
  the same body twice (fixed 30 Hz + realtime 60 Hz) and accelerate
  movement.  Networked scenes use `ApplyServerSimConfig` /
  `ApplyClientSimConfig` instead (the client's realtime trio is its
  prediction sim).  `BasePipeline::AddStage` also guards against the same
  stage type being added to one pipeline twice.

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
