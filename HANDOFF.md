# HANDOFF — deterministic player sync (branch `dev/cleanup`, PR #7)

**Status: PLANNING COMPLETE, implementation not started.**  All code is committed,
working tree clean.  Read `PLAYER_SYNC_IMPL_PLAN.md` (963 lines, `36778f2`) for the
implementation plan and `PLAYER_SYNC_PLAN.md` for the PR-level motivation.  This file
is the missing context: the live behavior that was measured, the bug history that
motivated the design, and the open problems.

## What is on this branch (ahead of `main` at c3af288)

| Commit | What |
|---|---|
| fbbeba9 | CI uploads `game_desktop` artifact (Linux/macOS/Windows) |
| bc95dbc | **Wire fixes**: `moveZ` now serialized; `UnpackFrame` restores `hasAim`; `AdvanceTick` only commits dirty frames (no more empty-frame ring pollution — server ring was wrapping every 16 ticks and overwriting real frames); drain-all frames in Realtime player; move averaging moved to commit (`normalize()`); UpdateTag replication |
| 1374a36 | **e2e**: `e2e_player_input_jump_move_aim_sync` + harness fix (add `SubsystemPlayer<FixedStep>` to prediction config — the missing stage made the prediction copy never jump) |
| a842879 | Realtime player: average move across drained frames instead of last-wins |
| 019a02c | **Jump stamp**: client-stamped jump decision (`HasJump` flag + `jumpPos`), server validates (drift ≤ 1 m + near-ground raycast) and snaps; server jump is stamp-only |
| 020ab32 | PR plan doc |
| 36778f2 | **Implementation plan doc** (963 lines) — dual-cadence client sim, ray-encoded actions, shared tick space |

CI is green on all four platforms at `019a02c`; docs commits since.

## Current player-input data flow (post-bc95dbc, post-019a02c)

**Client** (`DebugVoxelView`/`input_source.cpp` → `PlayerInputStream`):
- `KeyMouseInput::onUpdate` (60 Hz realtime pipeline) pushes a `PlayerInputFrameDelta`
  per frame: unit moveDelta (camera-agnostic, e.g. W = {0,1}), panDelta, and an
  action event whose mask tracks held actions (jump/dig/place as long as held).
- `PlayerInputFrame::apply()` converts moveDelta to **world space** at the frame's
  aim (`dummyCamera.right()/forward`), accumulates pan into absolute `aim`
  (0..2π wrapped).  So a committed `PlayerInputFrame` carries world-space `move`
  (unit-magnitude per-tick order) + absolute `aim` + held-action events.
- `SubsystemPlayer<Realtime>` calls `AdvanceTick()` which commits the accumulated
  frame (only if dirty), then drains **all** pending frames → sets
  `camera` (SetYawPitch), `creature.moveOrder = avg(move)`, chase-camera position,
  target-block raycast.  `SubsystemPlayer<FixedStep>` (also in the realtime pipeline
  on the client) drains action events: jump sets `creature.jumpOrder`; dig/place
  raycast and `terrain.SetBlock`.
- `SubsystemCreature<Realtime>`: `velocity = lerp(velocity, maxSpeed*moveOrder, friction)`;
  jump impulse if `isGrounded && jumpOrder`.  `SubsystemPhysics<Realtime>`: gravity,
  terrain AABB collision → `isGrounded`, `pos`.
- `PollPlayerInputs` (before sim each `Update`) ships each **dirty** frame as one
  `PlayerInputReplicationEvent` (SingleReliable).

**Server** (`DebugServerScene`):
- `ApplyEvent(PlayerInputReplicationEvent)` → `PushTick` into the server player's
  `PlayerInputStream` (reliable, ordered).
- `SubsystemPlayer<Realtime>`: `AdvanceTick()` (commits nothing — no local deltas),
  drains frames → sets server camera aim + `moveOrder`.  `SubsystemPlayer<FixedStep>`
  (20 Hz): drains frames, applies jump stamps (validated snap + impulse), dig/place.
- Realtime creature/physics (60 Hz) integrate the body; fixed creature/physics (20 Hz)
  integrate it **again** → gravity double-integration (measured apex skew 1.51 vs 1.55 m).
- `on_update<ComponentPhysicBody>` → `UpdateComponentEvent<ComponentPhysicBody>` →
  client mirror world.

**Client prediction vs authoritative** (both live on the client):
- Default world (`m_world`): local prediction — runs the same realtime stages on the
  local player, sends predicted `ComponentPhysicBody` into `RollbackEventLogStream`
  (`InsertPredicted`).
- Mirror world (variant 1, `m_authoritativeWorld`): receives server truth (physics
  body, entity/AABB adds, `AdvanceTick`), owns the `RollbackEventLogStream`, takes
  rollback snapshots every 3 server ticks.
- `ValidateLatest`: compares aligned prediction vs last server payload per family
  (`PhysicsBodyCompareFn`); on mismatch → `RollbackToLatest` (restore snapshot into
  `m_world`) + ping server for tick/cursor alignment; validation suspended until pong.

## Key correctness properties (don't regress these)

1. **Wire**: `PackedPlayerInputFrame` serializes `flags | moveX/Y/Z | panX/Y |
   jumpX/Y/Z | events[]`, each flag-gated.  `move` is world-space (unit per tick),
   `aim` is absolute (pan0 = UNorm16 over 0..2π, pan1 = SNorm16 over ±π), `jumpPos`
   is raw floats.  Round-trip is lossy (SNorm8 move, 16-bit pan) — comparisons use
   tolerances.
2. **Frames commit only when dirty** (`inputEventCnt || move != 0 || hasAim ||
   jumped`).  Empty frames must never enter the 16-slot ring (server wraps every
   16 ticks otherwise).
3. **Jump is stamp-only on the server** — the held Jump bit must not feed the
   server's `jumpOrder` (double-impulse).  The client stamps via
   `MarkJumpPerformed` (latched in `AdvanceTick`, gated on `IsLocalInput()` —
   true iff `PushFrame` was used, i.e. not a server stream).
4. **`isGrounded` needs replicated terrain + collision** — the prediction copy
   must be grounded via its own world (client `TerrainView` + chunks) before jump
   tests.  Tests wait for `isGrounded` on both copies.
5. **e2e harness `enableClientPrediction()` must include `SubsystemPlayer<FixedStep>`
   in the client's realtime pipeline** — that's the only place action events
   (jump/dig/place) are processed on the client.
6. **Rollback rollback target**: `RollbackToLatest(world)` is called with `m_world`
   (the prediction world), snapshots come from the mirror world.

## Measured numbers (loopback, 16 ms poll dt, 20 Hz server, 60 Hz client)

- Input → server apply latency: ~4–6 polls.  Mirror position trails prediction by
  ~0.2 m at walk speed; jump lift-off trails by 4 polls (~0.25–0.4 m drift).
- Jump: `initJumpSpeed = sqrt(2·1.65·9.8)`; apex ≈ 1.51 m server, 1.55 m client
  (double-integration skew), lift-off drift ~0.01 m after the stamp fix.
- Friction: air 0.01, ground 0.5; run velocity converges to 4 m/s in ~5 landed
  ticks; friction applied before `moveOrder` assignment so the first landed frame
  keeps the old velocity for one more tick.

## Open problems / design decisions still pending

### 1. The config-unification flake (investigated, NOT fixed — reverted)

Request: "Make server/client both load the default config."  `GetDefaultSceneConfig`
(`game/ctrl/src/scene.cpp:80`) already contains exactly the server's stage list
(terrain + fixed/realtime player/creature/physics).  Making `DebugServerScene`,
`ClientConnScene` default, `ClientScene2` (+`SetUpdateInterval(1/20)`), and the e2e
harness all load it **caused `e2e_rollback_recovery_no_relapse` to fail ~2/5 full-suite
runs** (`!IsWaitingPong()` after the pong — a *second* rollback re-triggered).
Baseline without the change: 6/6 clean; isolated test: always passes.

**Root-cause hypothesis**: the only *behavioral* delta was the harness config —
adding the real client config puts `SubsystemTerrain` into the client's **fixed**
pipeline (it was realtime-only via `LOAD_MASK_TERRAIN` before).  Client now
generates terrain locally; `SubsystemTerrain` is not deterministic across
processes (client chunk set/positions differ from the server's replicated chunks),
so the client physics collides against divergent terrain → `isGrounded`/`pos`
differences → false rollback mismatches.  **Tentative fix prepared but unbuilt:
split terrain generation out of the shared default; server adds
`SubsystemTerrain` explicitly** (see `scene.cpp` comment drafted in
`GetDefaultSceneConfig`).  This needs building + a 6-run stress comparison before
committing.  The relevant diff is described in the git stash-attempt history above;
recreate it, don't trust the current working tree (it was reverted).

### 2. Server double-integration

The player body is integrated by both the FixedStep (20 Hz) and Realtime (60 Hz)
creature/physics stages on the server; the client integrates only realtime.  The
implementation plan's "dual-cadence client sim" (Phase: client sub-steps fixed sim
at render rate) is the intended resolution — read `PLAYER_SYNC_IMPL_PLAN.md` first.

### 3. RenderStrategy tag duplication

`CreatePlayer` emplaces `RenderStrategyTag<Interpolation>`; prediction tests then
`emplace_or_replace` `RenderStrategyTag<LocalPrediction>`.  Two template
instantiations — fine mechanically but confusing; `CreatePlayer` should take the
strategy (or be told local vs remote).

### 4. jumpOrder consumed by both pipelines' creature stages

`jumpOrder` set by FixedStep player is consumed+cleared by *both* realtime and
fixed creature stages on the server — with the stamp path the impulse is applied
directly (`velocity.y = initJumpSpeed`), so the creature jump branch should not
also fire server-side; verify when touching this code.

## Test & build cheat-sheet

```sh
cmake --build out/build/apple-debug --target e2e_scene_test
out/build/apple-debug/game/ctrl/e2e_scene_test                 # full suite (15 tests)
out/build/apple-debug/game/ctrl/e2e_scene_test --run-test=<name>  # single test
ctest --test-dir out/build/apple-debug                         # 124 tests total
gh pr checks 7                                                  # PR CI (4 platforms)
```

Full-suite runs exercise cross-test interactions (tests run in reverse
registration order; the rollback tests share the process).  Flakes that only
appear in full-suite runs (like the config flake above) are real signals, not
harness noise — attribute before committing.
