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
