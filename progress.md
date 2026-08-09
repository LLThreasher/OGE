# Code Review: OgeRegistry Migration

**Branch:** `dev/network3`  
**Scope:** 39 files, +369/−297 lines  
**Status:** All 87 tests pass, Arterium + game_server link

---

## Summary

This change introduces `oge::runtime::OgeRegistry` / `OgeRegistryRef` as a safe wrapper around `entt::registry` and replaces all direct `entt::registry` usage throughout game code.  The goal is to keep `entt::registry` confined to the `oge::runtime` layer so that debug assertions, validation, and instrumentation can be added centrally.

## Architectural Changes

### `oge::runtime::OgeRegistry` (196 lines, substantial rewrite)

| Component | Before | After |
|---|---|---|
| Base class | `OgeRegistry` owning a stack `entt::registry` | `OgeRegistryRef` (non-owning pointer) + `OgeRegistry` (owns via `unique_ptr`) |
| `ctx()` return | `auto` (stripped reference → copy of context) | `decltype(auto)` (preserves reference) |
| `emplace()` return | `auto` (stripped reference) | `decltype(auto)` (preserves reference) |
| `try_get` | Single-type only | Added multi-type (`try_get<T1, T2, ...>`) for tuple returns |
| Move semantics | Deleted (no heap allocation to transfer) | Kept deleted (simplicity) |

**Review:** The `auto` → `decltype(auto)` fix for `ctx()` was critical — the old code returned a copy of the context, so `reg.ctx().emplace<T>(...)` emplaced into a temporary that was immediately destroyed.  This was the root cause of the `__next_prime overflow` bug in `BlockRegistry`.  Good catch.

**Concern:** `OgeRegistryRef` accepts `nullptr` (the constructor takes `entt::registry* registry = nullptr`).  Several methods call `Raw()` which dereferences the pointer without a null check.  A `CHECK_NULL_REGISTRY` macro was added in some methods but not consistently applied to all.  Consider adding it to `view()`, `valid()`, `on_construct()`, etc.

### `oge::runtime::ui::objects.hpp` (+27 lines)

Added 8 missing `DECL_TYPE_NAME` declarations for UI types (`UIRect`, `UISprite`, `UIZLevel`, `UIRaycastTarget`, `ScreenRect`, `UITerminal`, `UIText`, `UIParent`, `UIRoot`).  These were previously missing and caused linker errors when `OgeRegistry::emplace<T>` called `TypeName<T>::Get()` in its `OGE_ASSERT`.

### `game/data/include/game/components.hpp` (+7 lines)

Added `DECL_TYPE_NAME` for `UpdateTag<FixedStep>` and `UpdateTag<Realtime>`.  Moved `ReplicatedTag` specialization to `game/input/net.hpp` (the struct's definition site) to avoid "specialization after instantiation" errors caused by include ordering between `oge_registry.hpp` and `components.hpp`.

**Review:** This is the correct fix — TypeName specializations should live next to the type definition to prevent ordering issues.  Consider applying this pattern to all component types (move DECL_TYPE_NAME into the same header as the struct).

### `game/data/include/game/game_world.hpp` (−63 lines)

Simplified significantly: removed the external-registry constructor (`GameWorld(entt::registry&)`) since `OgeRegistry` no longer supports non-owning mode.  `GameWorld` now inherits `OgeRegistry` directly and always owns its own registry.  Added `#include "game/components.hpp"` before `oge_registry.hpp` to fix include ordering.

**Review:** Clean simplification.  The external-registry path was only used in `Scene::Scene(const Def&)` which now uses raw `entt::registry` (linter change), so this is dead code.

### `game/data/include/game/input/entity_event_stream.hpp` (−29 lines)

All `entt::registry&` parameters replaced with `oge::runtime::OgeRegistryRef`.  Added `#include "oge/runtime/oge_registry.hpp"`.

### `game/data/include/game/input/net.hpp` (+3 lines)

Added `#include "oge/runtime/typed_registry.hpp"` and `DECL_TYPE_NAME(game::ReplicatedTag, ...)`.

### `game/ctrl/` networking code (~15 files)

All hook installers (`InstallAddEntityHooks`, `InstallComponentReplicationHooks`, etc.), apply functions, snapshot functions, and rollback functions changed signatures from `entt::registry&` to `oge::runtime::OgeRegistryRef`.  This is a mechanical change with no logic modification.

### `game/view/` rendering code (~10 files)

Renderer state structs (`RendererState`, `FRendererState`) changed from storing `entt::registry&` to `OgeRegistryRef`.  This is correct since renderers only need non-owning access.  All `.cpp` files updated to match.

### `game/sim/` subsystems (~4 files)

`SubsystemTerrain::onPlayerCreated/Destroyed`, `ComponentPlayer::CreatePlayer/DestroyPlayer` changed to take `OgeRegistryRef` or `GameWorld&` as appropriate.

### Test files (~6 files)

All `entt::registry` local variables changed to `oge::runtime::OgeRegistry`.  `OGEContext octx(w)` calls fixed to `OGEContext octx(w.Raw())` because implicit conversion through base class + conversion operator requires two steps (disallowed in C++).

## Test Results

```
100% tests passed, 0 tests failed out of 7
  1 - datastruct_test (11 tests)
  2 - oge_registry_test (19 tests)
  3 - replication_events_test (43 tests)
  4 - registry_bug_recreate_test (4 tests)
  5 - scene_load_test (1 test)
  6 - sim_physics_test (12 tests)
  7 - debug_scene3_test (3 tests)
Total: 93 tests
```

## Recommendations

1. **Consistent null-check:** `CHECK_NULL_REGISTRY` is applied to `destroy`, `get`, `emplace`, `emplace_or_replace`, `remove`, `try_get` but NOT to `view`, `valid`, `all_of`, `any_of`, `on_construct/update/destroy`, `clear`.  For defensive programming, add it to all methods that dereference `m_registry`.

2. **Move all DECL_TYPE_NAME to type definition sites:**  The "specialization after instantiation" error with `ReplicatedTag` will recur for any new type used with `OgeRegistry` before its DECL_TYPE_NAME is seen.  Policy: put DECL_TYPE_NAME in the same header as the struct definition.

3. **Test-only workaround:** Tests use `OgeContext octx(w.Raw())` which exposes the raw registry.  Consider adding a constructor `OGEContext(OgeRegistryRef)` to avoid the two-step conversion issue.

4. **Dead code:** `game_world.hpp` lost the external-registry constructor.  If this path is ever needed again, consider a `GameWorldRef` analogous to `OgeRegistryRef`.

---

# Handoff — TODO cleanup (session 2 complete)

**Branch:** `worktree-todos-cleanup` (worktree at `.claude/worktrees/todos-cleanup`, based on `dev/network3` @ b0c7007)
**Date:** 2026-08-09

## State: 3 of 3 TODOs done, all compiled & verified

### ✅ TODO 1: snapshot_terrain flakiness — FIXED & VERIFIED
- `game/ctrl/test/replication_events_test.cpp` (snapshot_terrain):
  - Replaced silent `if(chunk)` guard with `CHECK(chunk != nullptr)`
  - Added `CHECK(r.entry.id == entt::type_hash<game::net::AddChunkEvent>::value())`
- All 43 tests pass.

### ✅ TODO 2: Networking integration tests — DONE (wire delivery verified)
- New: `game/ctrl/test/loopback_test.cpp` + registered `loopback_test` in `game/ctrl/CMakeLists.txt`. **5/5 tests pass** (real ENet round-trip in one process, ctest takes ~1s).
- Tests: connect+handshake, server hook enqueue (1 and N entities), and **wire delivery** of AddEntityEvent and AddComponentEvent<ComponentCreature> (client stream + payload round-trip).

**Root cause of the wire-delivery failure (production bug, fixed):**
`EventLogStream::SerializeEventPayload` wrote a `uint64_t` (8-byte) payload-size prefix, but
`DeserializeEventPayload` reads a `uint32_t` (4-byte) — and `ReplicationRegistry::ProduceAll`
allocated packets with **zero headroom for the prefix**, so every non-empty event hit
`net_serializer.hpp:158` ("Attempting to grow non-owning Buffer") in `StartPacket`'s fixed buffer.
- Fix: `SerializeEventPayload` now writes `uint32_t` (consistent with reader + scheduler + BufferOutputArchive convention); added `EventLogStream::PayloadSizePrefixBytes()`; all three ProduceAll packet allocations (+ scheduler byte estimate) reserve it.
- ⚠️ Wire-format change: server & client must be rebuilt together (header-only change).

**Other findings:**
- The reported test "hang" was NOT ENet blocking — it was an infinite loop in
  `loopback_hooks_fire_multiple_entities`: `PeekEvent(0, r)` restarts at the stream tail whenever
  `at=0`, so `r.entry.cursor++` had no effect. Fixed by advancing the cursor explicitly
  (`PeekEvent(0, r, cursor)` with `cursor = r.entry.cursor + 1`) — same idiom as `el_peek_cursor`.
- `ComponentCreature` wire schema only carries `maxSpeed` + `initJumpSpeed` (`DECL_NET_OBJ` omits
  moveOrder/lookOrder/jumpOrder) — the component wire test asserts only the serialized fields.
- Harness now installs `InstallComponentReplicationHooks<ComponentCreature>` (mirrors `server_scene.hpp`);
  `InstallEntityReplicationHooks` alone doesn't hook components.
- Loopback is a real UDP loopback (localhost) — runs fine in this environment.

### ✅ TODO 3: Gizmo renderer — COMPILED & VERIFIED
- Fixed 3 compile errors (full build green, `game_view` + `game_ctrl_ext` link clean):
  - `gizmo_renderer.cpp`: `Point3` is `IntTriple<int32_t>` → AABB center (float) rounded via `Point3::FromVec3`; `colors::BLUE/GREEN` → unqualified `BLUE`/`GREEN` (visible via `using namespace ui;` chain); removed bogus `using oge::runtime::ui::UIDrag;` (UIDrag lives in `game::ui`).
  - `ui_renderer.cpp`: `UIRenderer::onUpdate` used undeclared `game` → `auto& game = f.uiWorld;`.
- Wired: `registry.cpp` registers `GizmoRenderer`; `scene_ext.hpp` includes `GizmoPass` in `ViewExecutor`; `gizmo_pass.cpp` compiled via GLOB (CMakeLists uses `GLOB_RECURSE CONFIGURE_DEPENDS`).

## Verification
- `ctest` in `out/build/apple-debug-worktree`: **8/8 test binaries pass** (incl. loopback 1.07s, replication_events 43 tests, scene_load, sim_physics, debug_scene3).
- Full build (`cmake --build out/build/apple-debug-worktree`): green.

## Environment notes
- Worktree needs `engine/3rdparty/enet` — it's a gitlink submodule but `.gitmodules` is absent; was copied in from the main checkout (`cp -a /Users/lijiuru/Documents/OGE/engine/3rdparty/enet ...`). This copy is untracked.
- Build dir: `out/build/apple-debug-worktree` (configured with graphics ON).
- Nothing pushed; worktree branch is local only.
- Changes not yet committed since the `3865ff9` wip commit: serializer fix (event_log_stream.hpp, replication_registry.hpp), gizmo compile fixes, loopback_test additions, progress.md.
