# PR Plan — Deterministic Player Sync

Tick-stamped inputs, single-cadence player sim, interpolation rendering.

See also `NEXT_STEPS.md`, `progress.md`, and the jump-stamp fix (`019a02c`).

## Context

The jump stamp (`019a02c`) made both copies agree on the jump arc, but it is a
workaround for a deeper problem: the client and server run **different sims on
different tick spaces**, so every state-dependent decision can diverge.

- **No shared tick space.** The server applies input frames at receive time,
  not at the tick the client produced them.  The same input means different
  things on each side.
- **Double integration on the server.** The player entity is simulated by both
  the FixedStep (20 Hz) and Realtime (60 Hz) creature/physics stages; the
  client simulates only realtime.  Measured apex skew: 1.51 m vs 1.55 m.
- **Corrections snap.** `RollbackEventLogStream::Validate` restores a snapshot;
  there is no unacknowledged-input re-simulation, so corrections pop.
- **The remote copy renders live state.** `RenderStrategy::Interpolation` /
  `ComponentInterpolatedTransform` exist but nothing feeds a delayed,
  lerped buffer — every correction is visible.

**Goal:** both sides run the same deterministic player sim from the same inputs
at the same tick.  Then the grounded check agrees by construction, the jump
stamp becomes unnecessary, corrections become rare and tiny, and the remote
copy renders smoothly.

**Success criteria (measurable):**
1. `e2e_player_input_jump_move_aim_sync` passes with the stamp **disabled**
   (parity instead of anchoring), apex skew < 0.05 m, lift drift < 0.05 m.
2. Inputs delivered out of order / coalesced are applied at their stamped
   tick, not at receive time (new e2e).
3. The rendered remote copy never teleports: all position changes flow
   through the interpolation buffer (visual + snapshot-velocity assertion).

## Changes (phased — land 1+2 together, 3–5 as follow-ups)

### Phase 1 — The shared simulation tick space

One counter, owned by the sim, consumed everywhere: input commit, input
apply, rollback snapshots, and ping/pong all reference the same tick.

- `game/data/include/game/input/net.hpp` — add a `uint32_t tick` to
  `PackedPlayerInputFrame` (always serialized, not flag-gated; 4 bytes).
- `game/data/include/game/input/player_input_stream.hpp` — `PlayerInputStream`
  gets the current sim tick when committing (`AdvanceTick(uint32_t tick)`;
  the client's tick source is the existing
  `RollbackEventLogStream::AdvanceLocalTick` counter — that becomes *the*
  shared space; `ReplicationRegistry::CurrentTick` on the server is the same
  space mirrored).
- Server apply path (`replication_events.hpp` `ApplyEvent(PlayerInputReplicationEvent)`
  → `PushTick`): buffer frames in a small per-player tick-ordered queue in
  `PlayerInputReplicationState`; `SubsystemPlayer` drains by tick —
  frames with `tick <= lastAppliedTick` are dropped (duplicates from the
  reliable channel are impossible, but late/out-of-order delivery across
  ticks is), frames with gaps wait (bounded, e.g. 8 ticks) so a missing
  frame degrades rather than stalls.
- The player sim consumes the input stamped for the *previous* tick — one
  fixed-tick pipeline delay on both sides, so producer and consumer agree
  on which tick each frame belongs to.

### Phase 2 — Single-cadence deterministic player sim, one shared config

- **Decision: players simulate in the FixedStep pipeline on both sides at
  20 Hz.**  The client predicts at render rate (60 Hz) by sub-stepping the
  same fixed `dt = 1/60` three times per frame — same code, same order, same
  dt → same result.  (20 Hz raw sim looks choppy only until Phase 4 adds
  interpolation.)
- **Same simulation by loading the same config — no more hand-assembled
  client config.**  Extract the player-sim stage list into a single shared
  definition (e.g. `game/sim/player_sim_config.hpp`:
  `std::vector<entt::id_type> PlayerSimStages(...)` returning
  FixedStep player/creature/physics + realtime player/creature/physics in
  one canonical order).  `DebugServerScene`, the ClientConnScene default
  config, `ClientScene2`, and the e2e harness's `enableClientPrediction()`
  all build their `SceneConfig` from it.  This is the class of drift that
  produced the missing-`SubsystemPlayer<FixedStep>` bug — the harness
  hand-copied the client config and diverged; a single source removes the
  failure mode.
- `game/sim/src/subsystem_player.cpp` / `subsystem_creature.cpp` /
  `subsystem_physics.cpp` — stop the Realtime stages from simulating the
  player entity on the **server** (the realtime player stage on the client
  remains the input commit point).  Mechanism: a `SimTag<UpdateType>` filter
  on the views, or remove the realtime creature/physics stages from the
  server's copy of the shared config (a server/client variant of the shared
  definition — same stages, one filtered).
- The client's FixedStep player already runs in the realtime pipeline —
  after this phase it becomes the *only* player sim on the client, executed
  as 3 sub-steps; `ctx.dt` per sub-step must be `1/60`, not the frame dt.
- Keep the server's realtime `SubsystemPlayer` for nothing player-movement
  related (it currently also updates the chase camera + target-block raycast
  — either move those to the view layer per the earlier camera discussion,
  or leave them on a minimal realtime pass; they do not affect the body).

### Phase 3 — Parity gate: delete the jump stamp

- Flip the stamp off behind a constant (e.g. `kUseJumpStamp = false`) and
  run the full e2e suite — success criteria 1 must hold without it.
- Then remove the stamp path: `HasJump`/`jumpX/Y/Z` in `net.hpp`,
  `MarkJumpPerformed`/`m_pendingJump` in `player_input_stream.hpp`, and the
  stamp-apply branch in `subsystem_player.cpp`.  Wire format shrinks back.
- Keep the extended e2e test as the permanent parity regression.

### Phase 4 — Interpolation buffer for the remote copy

- Feed `ComponentInterpolatedTransform` from the last two authoritative
  `ComponentPhysicBody` snapshots per player entity (buffer keyed by entity
  in the mirror world; render ~100 ms behind: at 20 Hz, render
  `snapshot[n-2] → snapshot[n-1]`).
- The renderer (or a small interpolation system in `game/view`) writes the
  lerped transform; passes already prefer `ComponentInterpolatedTransform`
  over `ComponentPhysicBody` when present.
- The local player keeps zero-delay prediction (`RenderStrategy::LocalPrediction`).

### Phase 5 (stretch) — Reconciliation by re-simulation

- Client keeps a ring of predicted body states keyed by input tick plus the
  server's acknowledged cursor.  On authoritative update: if the server
  state for tick T differs beyond epsilon, restore server state and re-run
  the unacked inputs forward.  Replaces the current snapshot-restore in
  `RollbackEventLogStream::Validate` for physics events.

## Files touched

| File | Phase |
|---|---|
| `game/data/include/game/input/net.hpp` | 1, 3 |
| `game/data/include/game/input/player_input_stream.hpp` | 1, 3 |
| `game/ctrl/include/game/net/replication_events.hpp` (apply/state) | 1 |
| `game/sim/…/player_sim_config.hpp` (new — single source of the sim stage list) | 2 |
| `game/sim/src/subsystem_player.cpp`, `subsystem_creature.cpp`, `subsystem_physics.cpp` | 2, 3 |
| `game/ctrl/include/game/server_scene.hpp`, `client_conn_scene.hpp`, `client_scene2.hpp` | 2 |
| `game/ctrl/test/scene_test_harness.hpp` (load the shared config instead of hand-pushing stages) | 2 |
| `game/view/…` (interpolation buffer system) + `game/data/include/game/components.hpp` | 4 |
| `game/ctrl/include/game/net/rollback_event_log_stream.hpp` | 5 |
| `game/ctrl/test/e2e_scene_test.cpp` (+ new tests) | 1–3 |

## Verification

- **New e2e — tick-anchored application:** inject input frames with explicit
  tick gaps/order via the harness, assert the server applied each at its
  stamped tick (body state matches a reference sub-step replay).
- **New e2e — cadence parity:** same input sequence; assert the client's
  predicted state and the server's authoritative state match tick-for-tick
  (epsilon), apex skew < 0.05 m.
- **New e2e — config parity:** assert the client scene's stage list equals
  the shared `PlayerSimStages` definition (guards the config-drift bug that
  caused the missing-`SubsystemPlayer<FixedStep>` divergence).
- **Existing suites:** full `ctest` (124 tests) stays green; the extended
  jump test must pass with the stamp disabled.
- Local: `cmake --build out/build/apple-debug && ctest --test-dir out/build/apple-debug`
  plus the e2e binary twice.  CI: PR checks on all 4 platforms.

## Risks

- **Float determinism across platforms** (macOS client vs Linux server): out
  of scope for this PR — parity is asserted per-build (loopback e2e runs both
  sides in one binary).  Cross-platform bit-parity is a later, harder goal.
- **Physics event emission order:** `subsystem_physics.cpp` uses an
  `unordered_set<entt::entity> modified` — iteration order is
  implementation-defined.  Sim *state* is deterministic (view iteration
  order is stable per build); only event *order* varies.  Watch for tests
  asserting event order; switch to a small vector if it bites.
- **Tick-rate mapping:** client commits at 60 Hz, server sims at 20 Hz —
  the buffer in Phase 1 must group by fixed tick (`tick / 3`), not raw
  client tick, or the server must sub-step.  Decide at implementation time;
  the e2e from Phase 1 pins the behavior.
- **20 Hz choppiness** until Phase 4 lands — schedule 4 immediately after 3.
