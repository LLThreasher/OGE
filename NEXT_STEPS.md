# OGE — Next-Step Engineering Plan

Focus criteria: **modernity, testability, structural simplicity, readability,
explicit control, data oriented design**.  Items are ranked by impact/effort;
each is a direction, not a final design.  See also `progress.md` and
`CLAUDE.md` (Known Issues) for context.

## 1. Fix the TypeName / registry landmine (correctness first)

Two copy-paste `TypeName` strings collide with other types
(`SubsystemCreature<T>` returns `"core::SubsystemTerrain<...>"` at
`game/sim/include/game/sim/subsystem.hpp:146-154`; `UpdateTag<T>` returns
`"core::SubsystemPlayer<...>"` at `game/data/include/game/components.hpp:185-194`).
`TypeRegistry::RegisterType` then **silently replaces the older type's id**
(`engine/modules/runtime/include/oge/runtime/typed_registry.hpp:224-234`) —
name collisions corrupt the registry instead of failing.  JSON scene config
looks types up by these strings, so a collision means a scene builds the
wrong stage.

**Do:** correct the two strings; make `RegisterType` abort (or fail loudly)
on a name collision; derive names from `entt::type_name<T>` and keep
`DECL_TYPE_NAME` for user-facing labels only.  ~1 day.

## 2. Close the view/data test gap (pure functions, zero harness)

`game/view` and `game/data` have **no tests** at all, yet they hold the
purest, most testable code: `PanelSprite::EmitCommands` 9-slice math,
`GizmoPass::EmitWireCube/EmitWireRect`, `TerrainView::CastRay` (note the
shadowed `size_t dim` at `terrain_view.cpp:218/222`), `SetBlock` neighbor/
dirty logic, palette/RLE round-trips, `BlockRegistry`.

**Do:** add `view_test` + `data_test` via `add_test_target`.  De-risks the
pending gizmo renderer work (CLAUDE.md TODO) before it starts.  ~1-2 days.

## 3. Finish (or kill) the OgeRegistry wrapper

`CHECK_NULL_REGISTRY` is a no-op (`oge_registry.hpp:23-25`) while the header
documents null-checks everywhere; there's a copy-paste `CHECK_NULL_REGISTRY
(remove)` in `on_construct` (`:225`); the `requires (sizeof...(Rest) >= 0)`
constraints at `:142/:150` are always-true dead code; `RingBuffer::Get/Head`
bounds asserts are commented out (`ring_buffer.hpp:48/54/62`) and silently
wrap; `OGEContext` is bypassed by ~103 raw `.ctx()` uses in game code.

**Do:** make null impossible by construction (non-null reference wrapper,
delete `OgeRegistryPtr`) or actually implement the checks; fix the dead
constraints; re-enable RingBuffer debug asserts; collapse `OGEContext` into
the wrapper.  Mechanical, high confidence.  ~1 day.

## 4. Delete dead code + consolidate the three event streams

`DiscreteEventStream`/`AccumulativeEventStream`/`CompositeEventStream`
(`core/include/oge/event_stream.hpp`) coexist with the unused
`NetworkEventStream` (`event_stream2.hpp`, aliased in
`replication_registry.hpp:43` but never instantiated).  Both use the magic
"cursor 0 = snap-to-frontier" sentinel — the documented root of the
`chunkEventCursor {1} vs {0}` divergence.

**Do:** delete one stream family; replace the magic 0 with a named constant;
add cursor-semantics property tests.  Also remove commented-out classes
(`game_world.hpp`, `memory_context.hpp`) and the shadowed `dim`.  ~1 day.

## 5. Always-on invariant checks for the wire format

`OGE_ASSERT` compiles out in release, and `Buffer::ReadRaw/ReadNoCpy`
bounds checks are plain `assert` (`net_serializer.hpp:74-94`) — the recent
`uint32_t`/`uint64_t` payload-size-prefix bug shipped precisely because an
invariant was unchecked in release.  Same for EventLogStream cursor math.

**Do:** add a small always-on `OGE_CHECK` layer for serializer/cursor
invariants; keep `OGE_ASSERT` debug-only; drive `OGE_DEBUG` from the preset,
not per-target Debug checks.  ~1-2 days.

## 6. DOD pass on the replication hot path

`ReplicationRegistry::ProduceAll` allocates a `PacketPlan` with
`std::vector<PacketDesc>` **per peer per frame**
(`replication_registry.hpp:338-424`); `SimplePacketScheduler::Schedule`
copies payloads into scratch vectors; peer/family lookup is
`std::unordered_map` in the per-event loop (`:294-295, :367`) for a system
with ≤8 peers and ≤30 families.  A per-frame arena
(`MemoryContext.frameBuffer`) already exists but is only used for pmr
strings.

**Do:** allocate plan/descs from the frame arena; replace the maps with
indexed small arrays (peer id → slot, family hash → slot).  No behavior
change; measurable server tick win.  ~2 days.

## 7. Explicit replication routing (kill the shared-mask mutation)

`ClientScene2` mutates `ReplicationCapability::worldMask` on the **shared**
factory descriptors after registration (`client_scene2.hpp:97-130`) — hidden
global state with an ordering dependency; any second scene sharing the
factory fights over the same masks.  The per-component registration list in
`replication_registry.cpp:56-92` is hand-repeated 7×.

**Do:** move routing into a per-`ReplicationRegistry` table passed at
construction; generate the 3×N component registrations with a type-list
fold.  ~2 days.

## 8. Runtime-configurable scene config (data-driven composition)

Today scene composition is code: `DebugServerScene` hardcodes its subsystem
lists, `ClientConnScene` synthesizes a default `SceneConfig`, and
`game_desktop/main.cpp` builds the desktop scene config in C++.
`SceneConfig` already round-trips through JSON (`scene_config`, load masks,
subsystem type-name strings) — but there is no way to load one from a file.

**Do:** make `SceneConfig` fully data-driven: a JSON asset (e.g.
`scenes/client.json`, `scenes/server.json`) describing subsystems (by
registered type name), load masks, terrain/view params; loaded once at
startup by the scene runner; validated against the `AnythingFactory`
registry with **hard errors on unknown type names** (ties into item 1);
plus a schema-validation test.  DOD-flavored: scenes become data, and
"which stages run where" becomes visible instead of buried in constructors.
~2-3 days.

## 9. Build hygiene (best developer-hour ROI)

- `file(GLOB_RECURSE)` everywhere; no PCH (every test binary recompiles
  entt/fmt/glm through ~30 headers); no `-Wall -Wextra` policy (the
  `CastRay` shadowing would already be caught); a 90-line Android/Gradle
  branch inside the root `CMakeLists.txt`; `GAME_BUILD_TARGET` is an
  enum-in-a-string.

**Do:** PCH target for entt/fmt/glm; explicit source lists for `game/*`;
`-Wall -Wextra -Wshadow` per module; extract `cmake/android.cmake`.
~1-2 days.

## 10. Remove the last implicit-wiring pattern

`GetReplicationStream` switches on which of two ctx types exists — one of
which is a raw **pointer aliasing another ctx object**
(`replication_events.hpp:277-285`, stuffed in at `client_scene2.hpp:80`).
The codebase already moved terrain/player-input replication from signals to
explicit `Poll*` calls (CLAUDE.md) — this is the same fix for the stream
handle itself.

**Do:** own the stream handle explicitly on the scene; drop the ctx
pointer-aliasing; update CLAUDE.md's stale test table while in there.
~1 day.

## Suggested order

1 → 3 → 9 → 2 (foundations: correctness, hygiene, test baseline) →
5 → 6 (runtime invariants + hot path) → 4 (cleanup after streams stabilize)
→ 8 → 7 → 10 (data-driven config and explicit routing).
