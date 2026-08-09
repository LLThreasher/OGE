# OGE — One Game Engine

## Project Summary
Vulkan-based game engine + block-world game ("Arterium").  Client-server multiplayer
with ENet networking, entity replication, client-side prediction with rollback, and
voxel terrain.

**Active branches:** `dev/network3` (current), `main` (stable)

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
1. **New component type:** Add `DECL_TYPE_NAME` in the struct's header.  If it
   needs network replication, add `RegisterComponentEvents<T>` in
   `replication_registry.cpp` + `RegisterSnapshotComponent<T>`.
2. **New event type:** Define struct + `DECL_NET_OBJ` in `replication_events.hpp`.
   Add `DECL_TYPE_NAME` + register in `replication_registry.cpp`.  Add rollback
   capability functions in `rollback_capability.hpp/.cpp` if needed.
3. **New test:** `add_test_target(name SOURCES test/x.cpp LIBRARIES ...)` in
   CMakeLists.txt.  Include `<test_macros.hpp>`.
