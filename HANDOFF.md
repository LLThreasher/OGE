# HANDOFF — deterministic player sync (branch `worktree-player-sync-tickspace-impl`, PR #8)

**Status: Phases 1–4 IMPLEMENTED and verified.  Working tree clean, all commits pushed.**
Target branch: `main` (PR #8 was retargeted from `dev/player-sync-tickspace` on
2026-08-14; on 2026-08-15 the branch was rebased onto `origin/main` at a5f5b30 —
the cursor fix from PR #10 — so it remains a linear, conflict-free extension.
The old target's jump-stamp experiments are superseded.)
Read `PLAYER_SYNC_IMPL_PLAN.md` for the
full spec; this file is the implemented state, the two documented plan deviations,
and what remains.

## What landed (ahead of the base)

| Commit | What |
|---|---|
| 89054f5 | **Phase 1** — shared 20 Hz tick space (`SimTickContext`, `kSubStepDt=1/60`, 3 sub-steps/tick), tick-stamped movement frames, ray-encoded action stream |
| a6a314f | **Phase 2** — single-cadence deterministic player sim from one shared config (`player_sim_config.hpp` builders; no hand-assembled stage pushes) |
| 14f5e9b | **Phase 3** — parity: final component values on the wire, exact harness dt |
| eced2bf | **Phase 3** — T4 rewritten to post-stamp aligned-arc parity bounds |
| 8f042ee | **Phase 3** — the jump stamp deleted (276 lines), diagnostics removed |
| eb6e31e | **Phase 4** — interpolation buffer for the remote copy (two-world attractor) |
| 1085b8a | docs: PR #8 retargeted to `main` (conflict-free — linear extension of origin/main) |
| 88017fc | **fix** — dig/place release resets the action cooldown (main parity, user-reported regression) |
| 68be42a | **feat** — handshake protocol version check: `net::kProtocolVersion` (2) starts the first handshake packet and the server reply echoes it; mismatched/truncated handshakes are rejected on both sides |

## The implemented design (what the code does now)

**Tick space.**  Server fixed stage and the client's authoritative mirror run the
identical 20 tps pipeline over the same `SimTickContext` (3 sub-steps of 1/60 each).
The client's prediction world runs the same fixed trio plus the 60 Hz realtime trio
(LocalPrediction-filtered); the fixed pipeline simulates every player, like the
server.

**Input flow (D3/D4/D6/D8).**  Raw 60 Hz input accumulates in `PlayerInputStream`;
per tick, `PollPlayerInputs` aggregates it into one `PlayerInputFrame`
(tick-stamped, SNorm8 move + jump flag) and ships it reliable — the server unpacks
it and the client's own authoritative mirror receives the quantized round-trip
value, so both sides read bit-identical frames.  The fixed stage applies the frame
stamped `simTick - 1` (`TryReadTickFrame`: exact-tick apply, stale drop, bounded
8-tick gap wait).  Actions ride a parallel `PlayerActionStream` with ray-encoded
dig/place.

**Jump parity (Phase 3).**  There is NO client-decided jump stamp anymore — every
stream re-derives the jump from its own physics over the shared frame, and the
shared config makes the arcs bit-identical (verified vy/y bit-exact across the
wire).  The impulse defect that motivated the stamp (client 1.55 m vs server
1.65 m arcs) was fixed at its root: `CreatePlayer` mutated
`ComponentPhysicBody`/`ComponentCreature` AFTER emplace, so the `on_construct`
replication payload carried the struct defaults — now it mutates before emplace.

**Interpolation (Phase 4).**  `InterpolationLayer` snapshots the authoritative
mirror in `PreUpdate(authoritativeWorld)` and writes
`ComponentInterpolatedTransform` into the render world in
`PostUpdate(renderWorld, alpha, dt)` via an exponential attractor (k = 12/s)
toward the extrapolated state (latest snapshot + velocity × alpha ×
kFixedFrameDuration).  `LocalPrediction` entities are exempt — the prediction body
IS the local render state.  `SceneView` passes the two worlds (null-mirror scenes
fall back to their own world).

## Correctness properties (don't regress these)

1. **Emplace-order rule:** any component whose value is tweaked after `emplace`
   on a replicated entity ships its struct defaults on the wire (on_construct
   serializes at emplace time).  Mutate before emplace.
2. **Harness `POLL_DT = 1/60` exactly** (`scene_test_harness.hpp`) — the client's
   realtime integrator uses `f.dt` and the server's sub-steps use `kSubStepDt`;
   any other poll dt diverges trajectories by dt mismatch alone.
3. **Cursor 0 = snap-to-frontier sentinel** on every `DiscreteEventStream` poll:
   a zero cursor skips the whole backlog.  `TerrainReplicationState::
   chunkEventCursor` starts at 1 for the chunk stream (see CLAUDE.md chunk notes —
   that divergence is intentional for chunks), while input reads start at 0.
4. **Empty-tick contract:** `AggregateTickInput` returns false for empty windows
   and nothing ships; `AdvanceTick` commits dirty frames only (empty frames
   pollute the 16-slot ring).
5. **Input rate pins one frame per tick:** over a 90-poll window the server
   applies 30 ± 2 movement frames (T4 assertion).
6. **Interpolation is remote-only:** LocalPrediction entities must never get a
   `ComponentInterpolatedTransform` (T6/exemption assertion).
7. **Entity ids are shared between the client's worlds** — the interpolation
   layer correlates mirror↔render entities by id.
8. **Dig/place release resets the cooldown (main parity):** every event whose
   non-jump mask is 0 emits a mask-0 release action into the action stream,
   and `ApplyRayAction` zeroes `lastActionTime` before the cooldown gate —
   releasing unthrottles the next press; held dig/place still repeats at the
   0.3 s gap.  The reset rides the replicated stream (never the realtime
   stage) so both fixed pipelines stay deterministic.
9. **Handshake version check:** the first client packet starts with
   `net::kProtocolVersion` (2) and the server reply echoes it.  A stale
   binary on either side fails loudly (the server disconnects before
   CreatePlayer; the client aborts on a mismatched/short reply) instead of
   misreading the other side's packet layout.  Overridable per scene via
   the `protocol_version` scene arg (mismatch tests).

## Documented plan deviations (in the test comments, don't "fix" without reading)

- **T4 absolute bounds:** the plan's per-tick `|predPos − authPos| < 0.05` assumed
  a per-tick mirror re-anchor the Phase 3 mechanism fix removed.  The pred's 60 Hz
  raw-drain aim leads the server's tick-averaged frames while panning, leaving a
  constant ~0.28 m horizontal offset (accepted feel tradeoff), and the mirror's
  sampling grid sits one sub-step behind the pred's (constant 0.0163 m vertical).
  The test asserts shape-relative parity (apex skew < 0.05, per-tick shape
  divergence < 0.05) + a loose absolute sanity (< 0.5 m).
- **T6 per-frame bound:** the plan pins `|interpDelta| < 0.3 m` against a 100+ m
  snap while also pinning k ≈ 12/s — the first-frame attractor response is
  (1 − e^(−12/60)) ≈ 0.18 of the gap (~18 m), so both cannot hold.  The test
  asserts the attractor contract: the transform never moves more than its
  exponential response to the largest mirror jump.

## Remaining work

1. **Merge + PR #8** — the branch is green locally; CI/manual smoke pending.
2. **Manual smoke (Arterium, macOS)** — 60 Hz local feel with zero added latency,
   remote copy renders without 20 Hz stepping or teleport-on-correction.  Not
   run in this environment (no display).
3. **Phase 5 (stretch, explicitly out of scope)** — reconciliation by
   re-simulation (ring of predicted states + acked cursor) instead of
   snapshot-restore rollback.
4. **Interpolation buffer delay:** the layer currently drives the target from the
   latest snapshot (extrapolated); the per-entity history (`kMaxHistory = 16`)
   is in place if a real buffer delay (~2 ticks) is wanted later.

## Verification numbers

- Full build (`--target all`): green, incl. `Arterium.app` + `game_server`.
- Full ctest: **152/152** (151 through the release-reset fix + the
  handshake version-check test; 70–80 s each).
- e2e binary: **20/20**; with the old single-world lerp swapped in,
  the Phase-4 tests fail (16/2) — the new tests have teeth.  The release-reset
  e2e was red-checked against the exact pre-fix semantics (dig 2 dropped
  inside the cooldown window); the version-mismatch e2e fails if either
  side's check is removed (same wire format, different version numbers).
- Grep gates: no `HasJumpStamp|jumpPos|MarkJumpPerformed|kUseJumpStamp|stampActive`
  hits; no hand-assembled `push_back(Id<sim::Subsystem…)` stage pushes outside
  `player_sim_config.hpp`.

## Test & build cheat-sheet

```sh
cmake --preset apple-debug                    # always the preset, not raw -D
cmake --build out/build/apple-debug --target e2e_scene_test
out/build/apple-debug/game/ctrl/e2e_scene_test                 # 20 tests
out/build/apple-debug/game/ctrl/e2e_scene_test --run-test=<name>
ctest --test-dir out/build/apple-debug                         # 152 tests
gh pr checks 8                                                 # PR CI
```
