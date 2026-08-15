# Implementation Plan — Deterministic Player Sync

Executable, code-level plan for the tick-stamped input + single-cadence player sim
redesign.  Supersedes the sketch in `PLAYER_SYNC_PLAN.md` where they disagree; the
decision record below pins every choice that sketch left open.

Working branch: `worktree-player-sync-tickspace-impl` (PR #8 targets `main`;
`dev/player-sync-tickspace` carried this plan's drafts and the superseded
jump-stamp experiments).

> **Implementation status (2026-08-14):** Phases 1–4 implemented and verified on
> `worktree-player-sync-tickspace-impl` (PR #8) — commits 89054f5 … eb6e31e.
> Full ctest 145/145 twice; e2e 18/18 three times.  The jump stamp is deleted
> (Phase 3) and the interpolation buffer is in (Phase 4).  Two planned numeric
> bounds were adjusted during implementation, both documented in the test
> comments: T4's absolute per-tick bounds (the Phase 3 mechanism fix removed the
> per-tick re-anchor they assumed) and T6's 0.3 m/frame bound (unattainable
> alongside k ≈ 12/s for a 100+ m snap).  Phase 5 stays out of scope.  See
> HANDOFF.md for the implemented state.
>
> **Follow-up fix (2026-08-15):** dig/place release resets the action cooldown
> (main parity, user-reported regression) — commit 88017fc.  A mask-0 release
> action rides the replicated action stream for every dig/place-unset event and
> `ApplyRayAction` zeroes `lastActionTime` before the cooldown gate.  ctest
> 151/151; e2e 19/19; the new e2e was red-checked against the exact pre-fix
> semantics (dig 2 dropped inside the cooldown window).
>
> **Rebase (2026-08-15):** branch rebased onto `origin/main` (a5f5b30 — the cursor
> fix from PR #10); commit hashes throughout this file and HANDOFF.md were
> updated to the post-rebase values.
>
> **Protocol version check (2026-08-15):** `net::kProtocolVersion` (2) rides
> the handshake in both directions; mismatched/truncated handshakes are
> rejected on both sides so stale binaries fail loudly (commit 68be42a).
> ctest 152/152; e2e 20/20.
> - The client runs the player sim at **both** cadences in parallel — the 20 tps
>   fixed pipeline (parity sim) and a 60 Hz realtime sim (aim + action authority).
> - Input flows through **non-destructive, cursor-based streams**: the fixed
>   update reads off its cursor and aggregates at read time; the realtime update
>   polls per frame; **nothing is consumed except by ring wrap**.
> - Actions split into their own stream: the realtime sim emits **ray-encoded
>   dig/place actions** (origin + direction), replicated per tick; the **action
>   section is dropped from the player input (movement) frame**.
> - **Jump is movement input**: it is aggregated into the movement frame as a
>   `jump` flag (not ray-encoded into the action stream); the action stream
>   carries dig/place only.
>
> **Revision 4** (per design review — see §9 for the resolved flags):
> - The client's realtime pipeline is **input processing**: it produces
>   sophisticated input (aim, ray-encoded actions, smooth render-side
>   movement) on top of the raw input.  The **actual simulation rate is
>   20 Hz** per fixed frame.
> - **Terrain edits are fixed-pipeline-only on both sides** (disabled in the
>   realtime pipeline); the client's realtime pipeline **writes movement to
>   the prediction world** (`m_world`) and the 20 tps sim runs in the
>   **authoritative world** — the existing two-world split, no new registry;
>   the prediction world is re-anchored to authority each tick.
> - Actions are **tick-stamped and ordered**, ≤3 per tick in a
>   `std::array<PlayerAction, 3>`.
> - Smoothing: **exponential attractor**, with **extrapolation for remote
>   players only** (the local player renders the realtime side effect
>   zero-delay).  Chase camera + server head-tracking: out of scope.

---

## 1. Context (unchanged from PLAYER_SYNC_PLAN.md)

Both copies run **different sims on different tick spaces**: the server applies
input frames at receive time and double-integrates (FixedStep + Realtime), the
client simulates only realtime; the jump stamp (`019a02c`) is a workaround
anchoring the arc instead of fixing the divergence.  Goal: **both sides run the
same deterministic player sim from the same inputs at the same tick**, so
grounded/jump agree by construction, the stamp becomes unnecessary, corrections
become rare and tiny, and the remote copy renders smoothly.

**Success criteria (measurable):**
1. `e2e_player_input_jump_move_aim_sync` passes with the stamp **deleted** —
   apex skew < 0.05 m, lift drift < 0.05 m.
2. Inputs delivered with gaps / stale duplicates are applied at their stamped
   tick, not at receive time (new e2e).
3. The rendered remote copy never teleports: all position changes flow through
   the interpolation buffer (new e2e).
4. All 124 existing tests stay green (with the explicit adjustments listed in
   §5), plus the new tests.

---

## 2. Decision record — pinned interpretations

The target design leaves several points open.  Each is pinned here with
rationale; **A-items are the ones to re-confirm** if you disagree before
implementation starts.

### D1 — The shared tick space is 20 Hz (one tick = one fixed frame = 3 sub-steps of dt = 1/60)

The target says `AdvanceLocalTick` and `CurrentTick` are "the same space" but
does not fix the base rate.  Pinned: **both counters advance once per fixed
frame (20 Hz)**.

- Matches "one input frame per fixed tick (20 Hz)" — one frame per tick, 1:1,
  the stamp is the frame's own tick.
- Matches the existing intent comment in `client_scene2.hpp:154`
  ("Snapshot every 3 server ticks (~150ms at 20Hz)") — today that comment is
  wrong because `AdvancePeerTick` runs at 60 Hz; this plan makes it true.
- 3 render frames per tick.  Server fixed step = 1 per 3rd poll = 3 sub-steps
  of `dt = 1/60` in one burst.  Both sides execute the **identical ordered
  sequence of (player → creature → physics) sub-steps** — determinism by
  construction, not by matching "roughly equal" integration.
- Costs of the alternative (60 Hz tick space, frames stamped every 3rd tick):
  tick↔frame mapping bookkeeping in every consumer, `m_snapshotInterval`
  re-tune, muddier "one frame per fixed tick" semantics.  Rejected.

> **Resolved (design review):** "the actual simulation rate is 20 Hz per
> frame" — D1's 20 Hz tick space is confirmed.

### D2 — Sub-step mechanism: explicit scene-driven sub-step loop (both sides identical)

The sub-step loop needs to be **visible to the scene**: the physics stage
patches only on the last sub-step of a tick and the client inserts one
prediction per tick (D7).  The `TickScheduler` catch-up loop
(staged_scheduler.hpp:151+) hides sub-step boundaries inside the pipeline, so
it cannot serve here.

Mechanism:
- `Scene` (`game/ctrl/include/game/scene.hpp`, `game/ctrl/src/scene.cpp:31-36`)
  gains a fixed-frame accumulator: `Update()` accumulates `f.dt` into
  `m_fixedAccum`; when `m_fixedAccum >= m_fixedFrameDuration` it runs
  `for (s = 0; s < kSubStepsPerTick; ++s) { ctx<SimTickContext>.subStepIdx = s;
  m_subsystems.Update(kSubStepDt); }` and resets (realtime pipeline keeps
  running per frame with `f.dt`).  New accessor
  `void SetFixedFrameDuration(float)`, default `1/30` (preserves today's
  behavior exactly for every other scene: default interval is 1/30, fed
  1/60 per frame → same tick pattern).
- **Both scenes configure identically:** `SetUpdateInterval(1/60)` +
  `SetFixedFrameDuration(1/20)` — the fixed pipeline fires once per 3rd poll
  and runs 3 sub-steps of dt = 1/60 through the full stage chain.  This
  satisfies "FixedStep pipeline (20 Hz) on the server" **and** the harness
  constraint "server fixed step runs every 3rd poll", and makes the client's
  parity pipeline byte-for-byte the same shape as the server's.
- `SimTickContext` (`game/sim/include/game/sim/player_sim_config.hpp`, world
  ctx) carries `{ uint32_t currentTick; uint8_t subStepIdx; }` — scenes write
  `currentTick` (from `CurrentTick()` / the rollback stream) before
  `Scene::Update`; `Scene::Update` writes `subStepIdx` per sub-step.

### D3 — Non-destructive cursor-based input streams (one stream, many readers)

- `PlayerInputStream` holds **per-render-frame** input (move delta, pan, action
  events) pushed by the view at 60 Hz (accumulated per frame by `PushFrame`,
  committed by `AdvanceTick` once per render frame — today's mechanics, minus
  the tick stamp).
- **Readers never consume.**  Each reader owns an independent cursor: the
  20 tps fixed stage (movement), the 60 Hz realtime stage (aim + actions), and
  the replication poller.  Frame data leaves the stream **only when it falls
  out of the ring window** (16 slots at 60 Hz ≈ 0.27 s — comfortably > 1 tick
  window (3 frames) + jitter margin; sizing comment + `static_assert` added).
  A fresh cursor (value 0) snaps to the frontier, so late readers start at
  "now" — the intended semantics for the fixed stage (skip pre-registration
  backlog).
- **Aggregation happens once per tick, in the replication poller.**  One
  shared helper `input::AggregateTickInput(stream, cursor, tick, frame&)`
  reads the window since the cursor (the frames of the tick being
  aggregated) and produces the synthetic per-tick movement frame: `move` =
  the window's average (identical to today's `normalize()` semantics —
  player_input_stream.hpp:137-143), `jump` = OR of the window's jump events,
  `tick` = the producing tick.  `PollPlayerInputs` runs it once per tick;
  the aggregated frame goes to **both** the wire and the client's
  authoritative-world stream — the local 20 tps sim and the server receive
  bit-identical frames by construction.  There is no separate "accumulator
  commit step": the accumulator *is* the stream, applied when the 20 tps
  pipeline reads it.
- **One-tick pipeline delay.**  Input produced during tick T is aggregated by
  the fixed stage at tick T+1 (window read at the tick boundary; `Scene::Update`
  runs fixed before realtime, so on frame 3 of tick T the commit lands after
  the last sub-step).  The frame is stamped T; both sides apply the frame
  stamped T during tick T+1.
- **Server-side reads (stamp-anchored).**  `ApplyEvent(PlayerInputReplication-
  Event)` inserts the arrived frame into the server player's stream via
  `PushTick` (as today — the stream *is* the per-tick buffer; no separate
  deque).  The server's fixed stage reads per tick with a shared helper
  `input::TryReadTickFrame(stream, cursor, simTick, lastAppliedTick, frame&)`:
  - frame tick ≤ `lastAppliedTick` → stale/duplicate: skip;
  - frame tick > `simTick - kInputPipelineDelayTicks` → not due yet: wait;
  - `simTick - lastAppliedTick > kMaxInputWaitTicks` (8 ticks = 400 ms) →
    **degrade**: advance `lastAppliedTick` past the gap, apply nothing this
    tick (a missing frame stalls at most 400 ms);
  - frame tick == `simTick - 1` → apply, `lastAppliedTick = tick`.
  - **Empty-tick contract:** idle ticks commit no frame and put **nothing on
    the wire** (today's `AdvanceTick` only commits dirty frames — an empty
    frame on the wire is undefined).  Absence of a frame for a tick means
    "no input", not loss: the sim just applies nothing that tick, and the
    degrade rule merely keeps `lastAppliedTick` fresh so a frame arriving
    late for an already-elapsed tick is treated as stale rather than applied
    8 ticks late.  (See flag A11.)
  The client feeds its own 20 tps sim the same way: `PollPlayerInputs`
  aggregates once per tick (the shared helper above) and pushes the
  aggregated frame into **both** the wire and the client's
  authoritative-world stream (`ClientScene2` owns both worlds).  The
  client's fixed stage is therefore the **same code path** as the server's —
  `TryReadTickFrame` on its own world's stream; no client-specific
  aggregation branch.

> **Flag A3.** Frames can neither be lost nor reordered on the ENet
> **ordered**-reliable channel, and the server stream is FIFO — so the
> "out-of-order/coalesced" case is reduced to its reachable forms: **gaps**
> (wait → degrade) and **stale duplicates** (skip).  The new e2e injects
> directly via `ApplyEvent` with crafted stamps to exercise exactly those two
> paths (contract test; the stamp anchoring is the point).

### D4 — One aggregated frame drives all 3 sub-steps; decisions once per tick

The tick frame's `move` is re-applied as `creature.moveOrder` in **each** of
the tick's 3 sub-steps (SubsystemCreature resets `moveOrder` every update —
subsystem_creature.cpp:51-52); jump/action decisions are taken **once per
tick**, on sub-step 0, so `lastActionTime` decrements 3 × 1/60 = 1/20 per tick
(wall-clock correct — subsystem_player.cpp:272).

Mechanism: the fixed stage caches the current tick's frame in a new
**non-replicated** per-entity component (`PlayerSimInputState`, §3 Phase 1)
and gates aggregation + decision processing on the `SimTickContext.currentTick`
change.  With D2's explicit sub-step loop, the stage reads `subStepIdx`
directly (sub-step 0 = aggregate + decisions, sub-steps 1–2 = move
re-application).

### D5 — Cursors leave ComponentPlayer entirely

`ComponentPlayer::inputCursor/actionCursor` (game/data/include/game/
components.hpp:163-164) are not serialized today (`DECL_NET_OBJ(game::
ComponentPlayer, ...)` in components_net.hpp visits only `id`), but they live
inside a **replicated** component: any `ApplyEvent(AddComponentEvent<
ComponentPlayer>)` does `emplace_or_replace` (replication_events.hpp:745-763)
and resets them mid-play, and any future serialization change leaks them.
They move to `PlayerSimInputState` (non-replicated, never patched by apply).
`ComponentPlayer` keeps `id` + `lastActionTime` (`lastActionTime` is not on
the wire; see risk R8).

### D6 — Realtime input processing + fixed-tick actions (ray-encoded, tick-stamped, ordered)

The client's realtime pipeline is **part of the input-processing chain**: it
consumes raw input per frame and produces *sophisticated input* on top of it
(aim, ray-encoded actions, smooth render-side movement).  Nothing it produces
edits terrain or drives authority.

- **Aim (60 Hz).**  Pan deltas are applied to the camera **per render frame**
  (immediate, smooth).  Aim is a **secondary input to the fixed pipeline**:
  it enters via the action rays (below) — the ray baked at action time is the
  exact aim the player had, so the fixed-stage raycasts need no camera
  reconstruction.
- **Ray-encoded actions, tick-stamped and ordered.**  The realtime stage polls
  the input stream per frame for dig/place action events (its own cursor, D3)
  and emits `PlayerAction { actionMask, origin, direction }` — the ray
  computed from the live 60 Hz camera + `actionPos` (`ViewToRay`).  Actions
  accumulate into the **`PlayerActionStream`** (second per-entity
  `DiscreteEventStream`), aggregated per tick as
  `PlayerActionFrame { uint32_t tick; uint8_t actionCnt;
  std::array<PlayerAction, 3> actions; }` — **≤3 actions per tick**, ordered
  by emission (same fixed-array pattern as `UpdateChunkEvent`'s
  `dirtyCnt`/`updates`).  Jump is not here — jump is movement input (below).
- **Terrain edits happen only in the fixed pipeline, on both sides.**  The
  realtime stage never calls `SetBlock` (terrain edit is disabled there).  At
  sub-step 0 of tick T+1, **both** fixed stages apply the tick's dig/place
  actions with the delivered rays — `CastRay(action.origin, action.dir)` →
  `SetBlock` / place-collision check (subsystem_player.cpp:242-249) — against
  their own fixed-tick body state.  No emit-time vs tick-time asymmetry; the
  raycast is identical on both sides by construction.
- **Per tick, actions are replicated**: `PollPlayerActions(world)` packs the
  tick's action frame into a new `PlayerActionReplicationEvent { entity,
  tick, actionCnt, actions }`.  The server `ApplyEvent` inserts it into the
  server player's action stream; the server's fixed stage reads it per tick
  (stamp-anchored, same gap/stale contract as D3) and applies exactly as the
  client's fixed stage does.
- **Jump lives in the movement frame.**  `AggregateTickInput` ORs the window's
  Jump events into the frame's `jump` flag (the view's `PushFrame` path is
  unchanged — Jump still arrives as an input event, it just never leaves the
  movement window); both fixed stages set `creature.jumpOrder` from that flag
  on sub-step 0 of tick T+1 — the grounded evaluation happens at the same
  tick with the same state on both sides, which is what kills the jump stamp.
- **The movement frame drops the action section entirely** (D6 wire below):
  `PlayerInputFrame`/`PackedPlayerInputFrame` carry `tick` + `move` + `jump`
  (the jump *input* flag; the jump-*stamp* position fields exist only until
  Phase 3 deletes them) — no `inputEvents`, no aim/pan.

> **Resolved (design review):** jump in the movement frame (above); terrain
> edits fixed-pipeline-only on both sides (no emit-time client edits); aim
> reaches the server only inside action rays — server-side head/aim tracking
> ("model head movement") does not exist in the codebase yet and is
> explicitly out of scope; the e2e yaw assertion
> (`e2e_player_input_prediction_vs_authoritative`) is re-targeted to assert
> ray delivery instead (see T3); the server `ComponentCamera` aim becomes
> vestigial (kept for replication compatibility).

### D7 — Patch/prediction cadence: exactly 1 physics event per entity per tick

- `SubsystemPhysics` batches its `modified` set across the tick's 3 sub-steps
  (member set, cleared on `subStepIdx == 0`, `patch<ComponentPhysicBody>`
  fired once on `subStepIdx == kSubStepsPerTick-1` with the final state) →
  exactly **1** `UpdateComponentEvent<ComponentPhysicBody>` per entity per
  tick, payload = body state after sub-step 3.
- `ClientScene2` inserts exactly **1** predicted physics event per tick,
  sourced from the **authoritative world's** fixed-sim body (the client's own
  parity sim, after the fixed frame ran) — payload-for-payload equal to the
  server's single patch when the sims agree.
- `RollbackEventLogStream::Validate` (rollback_event_log_stream.hpp:244-262)
  then sees 1:1 counts with equal payloads when the sims agree — no count
  mismatch, no spurious rollback.
- `e2e_physics_events_bounded` **keeps its ≤1 per entity per poll bound**
  (its original purpose — catching the old 3-patches-per-*axis* bug — is
  fully preserved).

### D8 — Tick type and wrap

Wire tick is `uint32_t` (always serialized, not flag-gated — 4 bytes, per the
target).  All staleness/ordering comparisons use wrap-safe signed delta:
`static_cast<int32_t>(a - b) <= 0` ⇒ stale.  20 Hz wraps after ~6.8 years of
uptime; the comparisons, not the wrap, are what matters.

### D9 — Client world split: 20 tps authoritative sim vs 60 Hz prediction sim

The client already owns two worlds (`ClientScene2` — WorldRouter variant 1):
the **authoritative world** (`m_authoritativeWorld`, the mirror) and the
**prediction world** (`m_world`, the render world).  The two client sims map
onto them 1:1 — no new registry, no body ownership conflict:

- **Authoritative world (20 tps fixed pipeline).**  The client runs the same
  fixed player sim as the server, here: aggregated tick frames, 3 sub-steps,
  jump + dig/place (D3/D4/D6).  Server replicated events still apply into
  this world, overwriting any divergence.  Rollback snapshots and
  `InsertPredicted` (1/tick, D7) source from here.  Entities simulated:
  everything tagged `UpdateTag<FixedStep>` (local + remote players).
- **Prediction world (60 Hz realtime pipeline).**  The realtime pipeline is
  input processing: aim, ray-encoded action emission (D6) — and it
  **writes movement to the prediction world** (per design review): the 60 Hz
  movement sim (realtime creature/physics stages + the realtime player
  stage) drives the local player's body in `m_world` for zero-delay
  rendering.  Terrain edits stay disabled here (D6).  The movement is a
  render side effect: it is **re-anchored ("explained") against the
  authoritative world** each fixed tick (copy the authoritative body state
  into the prediction world after the fixed frame runs) and on rollback —
  the existing snapshot-restore path.  Only the local player is simulated
  realtime: the realtime stages filter
  `RenderStrategyTag<LocalPrediction>` (the same filter the
  prediction-insertion loop already uses, client_scene2.hpp:194-197), so
  replicated remote players are never driven locally.
- **Tags:** the player entity carries **both** `UpdateTag<FixedStep>` and
  `UpdateTag<Realtime>`; each stage variant filters on its own tag, the
  client's realtime stages additionally on `LocalPrediction`.

> **Resolved (design review):** the realtime pipeline is input processing
> plus the prediction-world movement sim (movement + aim only, no terrain
> edits); the authoritative world hosts the 20 tps parity sim — the existing
> two worlds are the two sims.

---

## 3. Phased changes (test-first, per repo TDD pattern)

Each phase lists: **Tests first** (change/extend so it fails), then
**Implementation**, then **Exit criteria**.  Phases 1+2 land together (neither
is shippable alone); 3 and 4 are follow-ups, 4 immediately after 3.

### Phase 1 — Shared 20 Hz tick space + tick-stamped movement frames + action stream

**Tests first:**
1. `game/ctrl/test/replication_events_test.cpp` — update the
   `PackedPlayerInputFrame` serialization round-trip tests to visit the new
   `tick` field (and, until Phase 3, the stamp fields); add `tick = 0` /
   `tick = 0xFFFFFFFF` round-trips; add `PackedPlayerAction` /
   `PlayerActionReplicationEvent` round-trips.  Fails: fields don't exist yet.
2. New unit block (same file): `AggregateTickInput` — a 3-frame window with
   moves (1, 0, 0) yields the averaged move; jump OR's across the window;
   the frame is stamped with the producing tick; a re-read with the same
   cursor returns nothing (cursor advanced, data intact).  Plus
   `TryReadTickFrame` stamp contract — stale skip, not-due wait, gap degrade
   after `kMaxInputWaitTicks`, exact-tick apply, wrap-safe comparisons
   (`lastAppliedTick = 0xFFFFFFFE`, incoming `tick = 2`).  Fails: helpers
   don't exist.
3. `game/ctrl/test/e2e_scene_test.cpp` — rewrite `e2e_player_input_replicates`
   (§5, T2) and add `e2e_input_tick_anchored_apply` (§5, T1).  Both fail
   (no tick on the wire; apply path semantics untested).
4. `e2e_player_input_prediction_vs_authoritative` — adapt to the action-ray
   design (§5, T3).  Fails to compile until the action events exist.

**Implementation:**

- **`game/data/include/game/input/player_input_stream.hpp`**
  - Constants move here or to `player_sim_config.hpp`:
    `kInputPipelineDelayTicks = 1`, `kMaxInputWaitTicks = 8` (400 ms at 20 Hz).
  - `PlayerInputFrame`: drop `inputEvents`/`inputEventCnt`/`deltaCnt`/`hasAim`/
    `aim` (the action + aim sections leave the movement frame — D6); keep
    `move`; add `uint32_t tick = 0;` and `bool jump = false;` (the aggregated
    jump input — OR of the window's Jump events); keep the stamp fields
    `jumped`/`jumpPos` **only until Phase 3 deletes them**.
  - The raw per-render-frame entry keeps its action events + pan internally
    (view `PushFrame` unchanged); the realtime stage polls them per frame via
    a new non-consuming `PollEvents(Cursor&)`/`PeekAim()` API (D3 — data
    stays in the window).
  - New shared helpers:
    ```cpp
    bool AggregateTickInput(PlayerInputStream&, Cursor&, uint32_t tick,
                            PlayerInputFrame& out);  // window read + average + jump OR
    bool TryReadTickFrame(PlayerInputStream&, Cursor&, uint32_t simTick,
                          uint32_t& lastAppliedTick, PlayerInputFrame& out); // D3 server contract
    ```
  - Add `PlayerSimInputState` here (avoids a components.hpp → input include
    cycle): `{ uint32_t lastAppliedTick = 0; Cursor moveCursor{}; Cursor
    actionCursor{}; bool hasFrame = false; PlayerInputFrame frame{}; }` +
    `DECL_TYPE_NAME` ("core::PlayerSimInputState").  Never registered for
    replication; emplace in `ComponentPlayer::CreatePlayer` (server) and
    `DebugVoxelView::onConstructPlayer` (client).
  - Ring-size note: 16 slots at 60 Hz raw frames ≈ 0.27 s (5+ ticks) —
    the tick window is 3 frames; add the sizing comment + `static_assert`.
- **New: `game/data/include/game/input/player_action_stream.hpp`** (data
  module, same dependency rules):
  ```cpp
  namespace game::input {
  struct PlayerAction { uint8_t actionMask; math::vec3 origin; math::vec3 dir; };
  struct PlayerActionFrame {                          // one per tick, ordered
      uint32_t tick = 0;
      uint8_t  actionCnt = 0;                         // ≤ 3
      std::array<PlayerAction, 3> actions{};          // ray-encoded, emission order
  };
  class PlayerActionStream { /* DiscreteEventStream<PlayerActionFrame, 16>:
      raw per-frame pushes + per-tick window aggregation (≤3, tick-stamped,
      ordered), cursor readers */ };
  }
  ```
- **`game/data/include/game/input/net.hpp`**
  - `PackedPlayerInputFrame`: add `uint32_t tick = 0;` — serialized always
    (not flag-gated): add `visit(self.tick)` first in its `DECL_NET_OBJ`
    (net.hpp:367); drop the `HasEvents` arm (action section gone); `HasPan`
    arm gone; `HasMove` + `HasJump` (until Phase 3) remain.
    `PackFrame`/`UnpackFrame` updated accordingly.
  - New `DECL_NET_OBJ(game::input::net::PackedPlayerAction, { mask, origin,
    dir })` + `DECL_NET_OBJ(game::net::PlayerActionReplicationEvent, {
    playerEntity, tick, actions })`.
- **New: `game/sim/include/game/sim/player_sim_config.hpp`** (sim module):
  ```cpp
  namespace game::sim {
  constexpr float   kSubStepDt        = 1.f / 60.f;
  constexpr int     kSubStepsPerTick  = 3;
  constexpr float   kFixedFrameDuration = kSubStepDt * kSubStepsPerTick; // 1/20
  struct SimTickContext { uint32_t currentTick = 0; uint8_t subStepIdx = 0; }; // world ctx
  }
  ```
  (Stage-list builders `FixedStepPlayerStages`/`RealtimePlayerStages` land
  here in Phase 2.)
- **`game/ctrl/include/game/scene.hpp` + `game/ctrl/src/scene.cpp`**
  - `Scene::Update` fixed-frame accumulator + explicit sub-step loop per D2:
    `m_fixedAccum += f.dt; if (m_fixedAccum + eps >= m_fixedFrameDuration) {
    for (s < kSubStepsPerTick) { ctx<SimTickContext>.subStepIdx = s;
    m_subsystems.Update(kSubStepDt); } m_fixedAccum = 0; }
    m_realtimeSubsystems.Update(f.dt);`
    (order preserved: fixed before realtime — scene.cpp:34-35; `eps` covers
    the harness's `POLL_DT = 0.016` vs 1/60 drift).
  - `SetFixedFrameDuration(float)`, default `1/30` (preserves current
    behavior for all other scenes).
- **`game/ctrl/include/game/server_scene.hpp` (`DebugServerScene`)**
  - Emplace `SimTickContext` in `world.ctx()` (constructor).
  - `SetUpdateInterval(1/60)` + `SetFixedFrameDuration(1/20)` (replaces the
    current `SetUpdateInterval(1/20.f)` at server_scene.hpp:191).
  - `Update`: move `AdvancePeerTick()` (server_scene.hpp:262) to the fixed
    cadence — `if (++m_fixedFrameCounter % kSubStepsPerTick == 0)` — and
    write `SimTickContext.currentTick = m_replicationRegistry.CurrentTick()`
    into `m_world.ctx()` before `Scene::Update` so the fixed stages observe
    the tick.  `PongContext.currentServerTick` unchanged (still
    `CurrentTick()`).
  - `CreatePlayer` (`game/sim/src/subsystem_player.cpp`): emplace
    `PlayerSimInputState` + `PlayerActionStream` + `UpdateTag<FixedStep>`
    (keep `UpdateTag<Realtime>` for the view).
- **`game/ctrl/include/game/client_scene2.hpp` (`ClientScene2`)**
  - The **authoritative world gets its own fixed pipeline** (D9): a member
    `sim::SubsystemPipeline m_authoritativeSim` constructed over
    `m_authoritativeWorld` with the server's fixed stage list, interval
    `1/60`, driven with the same 3-sub-step loop on the fixed frames.
    `SimTickContext` is emplaced in **both** worlds' ctx and written before
    each drive (currentTick = `m_rollbackStream.CurrentTick()`).
  - The authoritative world needs terrain for its sim: emplace
    `BlockRegistry` + `TerrainDesc` + `TerrainView` in its ctx (mirroring
    the scene `Load()`), and set the chunk families
    (`AddChunkEvent`/`RemoveChunkEvent`/`UpdateChunkEvent`) to `bothMask` in
    the constructor's mask block so replicated terrain reaches both worlds.
  - Emplace a `PlayerInputStream` + `PlayerActionStream` + `PlayerSimInputState`
    on the mirror's player entity when `ComponentPlayer` constructs there
    (`on_construct` hook) — the client's fixed stage reads them exactly like
    the server's.
  - `Update`: `AdvanceLocalTick(m_authoritativeWorld)` (client_scene2.hpp:186)
    moves to the every-3rd-frame cadence (`m_fixedFrameCounter %
    kSubStepsPerTick == 0`); on fixed frames: `PollPlayerInputs` +
    `PollPlayerActions` aggregate the tick (the aggregated movement/action
    frames are pushed into the mirror's streams **and** the wire), then
    drive `m_authoritativeSim` (3 sub-steps), then **re-anchor**: copy the
    authoritative body state into `m_world`'s local player (the "explain"
    step, D9).  `Scene::Update` runs the m_world realtime pipeline per
    frame.
  - Prediction insertion (client_scene2.hpp:191-201) moves to the fixed
    frame and sources the **authoritative world's** body — one predicted
    physics event per tick (D7).
  - Remove the unused `m_serverTickScheduler{1/20.f}` (client_scene2.hpp:33).
- **`game/ctrl/include/game/net/replication_events.hpp`**
  - `ApplyEvent(PlayerInputReplicationEvent)` (replication_events.hpp:679-700):
    unchanged shape (`PushTick` into the server player's stream) — the stream
    is the buffer (D3); the unpack restores `tick`.
  - New `ApplyEvent(PlayerActionReplicationEvent)`: push the action frame into
    the server player's `PlayerActionStream`.
  - New `PollPlayerActions(OgeRegistryRef world)`: per tick, aggregate the
    action window and `PushReplicationEvent` it (mirror of `PollPlayerInputs`).
  - `DECL_TYPE_NAME` for `PlayerActionReplicationEvent`.
  - Register `PlayerActionReplicationEvent` in
    `game/ctrl/src/net/replication_registry.cpp` (`RegisterReplications`):
    `ReplicationCapability` with `SingleReliable` send type + the apply fn
    (same registration shape as `PlayerInputReplicationEvent`).
- **`game/sim/src/subsystem_player.cpp`**
  - `SubsystemPlayer<FixedStep>::onUpdate`: per player entity, read
    `SimTickContext`; on `subStepIdx == 0` with a tick change:
    - `TryReadTickFrame(stream, simState.moveCursor, ctxTick,
      simState.lastAppliedTick, frame)` (D3 contract) → cache in `simState`;
      on this sub-step: `creature.jumpOrder = frame.jump`; then read the
      action stream frame for `ctxTick` (same stamp contract) → apply its
      dig/place actions (≤3): `CastRay(action.origin, action.dir)` →
      `SetBlock` / place-collision check (D6).  **One code path for both
      sides** — the server world and the client's authoritative world run
      the identical stage (D9).
    - store `consumedTick = ctxTick` in `simState`.
  - Every sub-step: `creature.moveOrder = cachedFrame.move` (re-applied — D4);
    stamp branch (`frame.jumped && !input.IsLocalInput()`,
    subsystem_player.cpp:166-197) stays for now (Phase 3 removes it).
  - Cursor reads `player.inputCursor`/`player.actionCursor` →
    `simState.moveCursor`/`simState.actionCursor`; `player.lastActionTime`
    decrement unchanged.
  - `SubsystemPlayer<Realtime>::onUpdate` becomes the **60 Hz input-processing
    + prediction-movement stage** (D9, prediction world only): keep the
    per-frame input drain (subsystem_player.cpp:86-131) — `moveOrder` feeds
    the realtime creature/physics stages that drive the prediction world's
    body; per frame: pan → camera (`SetYawPitch` via `PeekAim`); dig/place →
    build the ray from the live camera + `actionPos` and push the
    ray-encoded action into `PlayerActionStream` (≤3 per tick, ordered —
    **no `SetBlock` here**, terrain edits are fixed-pipeline-only, D6); Jump
    events are left in the window for the movement-frame aggregation (D6);
    camera position + target-block raycast per frame.
- **`game/data/include/game/components.hpp`** — delete
  `ComponentPlayer::inputCursor/actionCursor` (components.hpp:163-164).
- **`game/ctrl_ext/include/game/debug_voxel_view.hpp`** —
  `onConstructPlayer` (debug_voxel_view.hpp:131): emplace
  `PlayerSimInputState` + `PlayerActionStream` + `UpdateTag<FixedStep>`
  (defensive — the tags replicate, but keep parity with `CreatePlayer`).

**Exit criteria:** `replication_events_test` + new unit blocks green;
`e2e_input_tick_anchored_apply` green; `e2e_player_input_replicates` green;
existing input e2e tests green under the action-ray design; server fixed step
still fires every 3rd poll (asserted implicitly by the tick e2e).

### Phase 2 — Single-cadence deterministic player sim from one shared config

**Tests first:**
1. Add `e2e_player_config_parity` (§5, T3).  Fails: harness/scenes still
   hand-assemble configs.
2. Harness `scene_test_harness.hpp::connect()` (scene_test_harness.hpp:114-124)
   must build `cliConfig` from the shared builders (compile-fails first if
   the builders don't exist).
3. Update `UpdateTag` emplacements in e2e tests (§5, T5) to `FixedStep`.
   (`e2e_physics_events_bounded` needs **no** bound change — D7 preserves
   ≤1 per poll.)

**Implementation:**

- **`game/sim/include/game/sim/player_sim_config.hpp`** — add the stage-list
  builders (single source; ids via `entt::type_hash`, stable across the two
  runner factories):
  ```cpp
  std::vector<oge_id_type> FixedStepPlayerStages(AnythingFactory&); // player, creature, physics (FixedStep) — server + client authoritative world
  std::vector<oge_id_type> RealtimePlayerStages(AnythingFactory&);  // player, creature, physics (Realtime) — client prediction world: 60 Hz movement + aim
  void ApplyServerSimConfig(SceneConfig&, AnythingFactory&);        // fixed: +SubsystemTerrain; realtime: none
  void ApplyClientSimConfig(SceneConfig&, AnythingFactory&);        // realtime: +SubsystemDebugText
  ```
- **`game/ctrl/include/game/server_scene.hpp`** — replace the hand-pushed
  stage lists (server_scene.hpp:160-173) with `ApplyServerSimConfig`; drop
  `SubsystemPlayer<Realtime>`, `SubsystemCreature<Realtime>`,
  `SubsystemPhysics<Realtime>` (server realtime stages are gone — the server
  camera is only updated for replication compatibility, A9).
- **`game/ctrl/include/game/client_conn_scene.hpp`** — the synthesized default
  client config (client_conn_scene.hpp:160-168) becomes
  `ApplyClientSimConfig(cfg, AF())` — no more hand-assembled stage list.
- **`game/ctrl/test/scene_test_harness.hpp`** — `connect()` builds the
  prediction config from `ApplyClientSimConfig` (replacing
  scene_test_harness.hpp:114-124).  This deletes the drift class that
  produced the missing-`SubsystemPlayer<FixedStep>` divergence bug.
- **`game/sim/src/subsystem_player.cpp`** — add `UpdateTag<utype>` to the
  `SubsystemPlayer<utype>` views (subsystem_player.cpp:79-84) for uniformity
  with creature/physics (both entity tags now exist on every player entity
  via `CreatePlayer`/`DebugVoxelView`; the FixedStep tag replicates —
  hooks already installed at server_scene.hpp:197-200).
- **`game/sim/src/subsystem_creature.cpp` / `subsystem_physics.cpp`**
  - The **Realtime** variants gain a `RenderStrategyTag<RenderStrategy::
    LocalPrediction>` filter in their views — on the client they simulate
    only the local player (prediction world); remote players are never
    driven realtime (D9).  The FixedStep variants keep the existing
    `UpdateTag<FixedStep>` filter only (they simulate everyone, like the
    server).
  - physics: tick-batched patch per D7 — member
    `std::unordered_set<entt::entity> m_modifiedTick`, cleared on
    `subStepIdx == 0`, filled per sub-step, `patch<ComponentPhysicBody>`
    fired once on `subStepIdx == kSubStepsPerTick-1` (read `SimTickContext`
    from `world.ctx()`).  Applies to the FixedStep variant; the Realtime
    variant keeps the per-frame patch (prediction world, no replication
    hooks there).

**Exit criteria:** `e2e_player_config_parity` green; the client's
authoritative-world sim runs 3 sub-steps per 3rd poll (identical to the
server's); the server has **no** realtime stages; the client's realtime
creature/physics run only in the prediction world (LocalPrediction-filtered);
`e2e_player_input_prediction_vs_authoritative` +
`e2e_local_prediction_player_moves` green under the new cadence;
`e2e_physics_events_bounded` green with its original bound.

### Phase 3 — Parity gate: disable, then delete the jump stamp

**Tests first:** tighten `e2e_player_input_jump_move_aim_sync` thresholds
(§5, T4) to apex skew < 0.05 m and lift drift < 0.05 m (currently 0.5 / 0.15
— e2e_scene_test.cpp:910, 919).  Then:

1. **Gate off:** `constexpr bool kUseJumpStamp = false` in
   `subsystem_player.cpp`; wrap the stamp branch (subsystem_player.cpp:166-197),
   `MarkJumpPerformed` call (267-270) and the `HasJump` pack/unpack arms.
   Run the full e2e suite — criterion 1 must hold **without** the stamp.
2. **Delete:** `HasJump` + `jumpX/jumpY/jumpZ` and their DECL_NET_OBJ arms
   (net.hpp:90, 118-120, 275-282, 383-388); `jumped`/`jumpPos`
   (player_input_stream.hpp:117-118); `MarkJumpPerformed`/`m_pendingJump`/
   `m_jumpPos` (player_input_stream.hpp:164-166, 230-238);
   `kMaxJumpStampDelta` (subsystem_player.cpp:57) + the stamp branch +
   the local-jump `IsLocalInput()` gating (subsystem_player.cpp:208-211, 267-270);
   the wire shrinks back by the 12 float bytes.  The `jump` **input** flag
   stays — the wire's `HasJump` bit becomes the flag itself (no position
   payload).  `IsLocalInput()` itself stays (commit semantics + the
   client/server consumption branch).
3. Update the pack/unpack round-trip unit tests for the shrunk frame.

**Exit criteria:** jump e2e green at the tightened thresholds with zero stamp
code in the tree (grep `HasJump|jumpPos|MarkJumpPerformed` ⇒ no hits).

### Phase 4 — Interpolation buffer for the remote copy (+ 60 Hz local render)

**Tests first:** extend `e2e_interpolation_layer` / add
`e2e_remote_copy_no_teleport` (§5, T6).  Fails: interpolation is still a
plain lerp of the local body and never reads the mirror world.

**Implementation:**

- **Local player render:** the prediction world's body **is** the local
  render state — the realtime sim wrote it (D9), so no local
  `ComponentInterpolatedTransform`, no extrapolation, zero delay.  The body
  is re-anchored to the authoritative world each fixed tick and on rollback
  ("explains it according to the real authoritative data").
- **`game/ctrl_ext/include/game/interpolation.hpp` (`InterpolationLayer`)**
  - Source world split: `PreUpdate(const GameWorld& authoritative)` snapshots
    **authoritative-world** `ComponentPhysicBody` per Interpolation-tagged
    entity (per-entity snapshot history with arrival ticks);
    `PostUpdate(GameWorld& renderWorld, float alpha)` writes
    `ComponentInterpolatedTransform` in the **render** world (m_world) via an
    **exponential attractor** toward the **extrapolated** authoritative
    state — target = latest authoritative snapshot position + velocity ×
    render delay (`pos += (target - pos) * (1 - exp(-k * dt))`,
    `k ≈ 12/s`).  Extrapolation applies to **remote players only**; the
    local player uses the prediction-world body (above) and never passes
    through this buffer.
  - Local player exempt: `RenderStrategyTag<LocalPrediction>` entities are
    skipped here (handled by the realtime stage above —
    `DebugVoxelView::onConstructPlayer` already replaces the tag,
    debug_voxel_view.hpp:147).
  - `SceneView` call sites (game/view) pass the two worlds; the harness has
    no `SceneView`, so the e2e drives `PreUpdate`/`PostUpdate` directly
    (same pattern as `e2e_interpolation_layer` notes at e2e_scene_test.cpp:
    1013-1015).
- **Chase camera:** explicitly out of scope (design review) — the camera
  stays a per-frame view concern following the local prediction-world body;
  no special smoothed-transform rule.
- Passes already prefer `ComponentInterpolatedTransform` over
  `ComponentPhysicBody` when present (components.hpp:57-62) — no pass
  changes.

**Exit criteria:** `e2e_remote_copy_no_teleport` green; local player renders
with zero added delay (extrapolated transform from the 60 Hz sim); remote
copy path is continuous under a server correction.

### Phase 5 (stretch, not in this PR) — reconciliation by re-simulation

Ring of predicted body states keyed by input tick + acknowledged cursor;
re-run unacked inputs forward instead of snapshot-restore in
`RollbackEventLogStream::Validate`.  Explicitly out of scope here —
snapshot-restore rollback stays until parity is proven in the field.

---

## 4. Files touched

| File | Phase |
|---|---|
| `game/data/include/game/input/player_input_stream.hpp` | 1, 3 |
| `game/data/include/game/input/player_action_stream.hpp` **(new)** | 1 |
| `game/data/include/game/input/net.hpp` | 1, 3 |
| `game/data/include/game/components.hpp` (cursor removal) | 1 |
| `game/sim/include/game/sim/player_sim_config.hpp` **(new)** | 1, 2 |
| `game/sim/src/subsystem_player.cpp` | 1, 2, 3 |
| `game/sim/src/subsystem_creature.cpp`, `subsystem_physics.cpp` | 2 |
| `game/ctrl/include/game/scene.hpp`, `game/ctrl/src/scene.cpp` | 1 |
| `game/ctrl/include/game/server_scene.hpp` | 1, 2 |
| `game/ctrl/include/game/client_scene2.hpp` | 1 |
| `game/ctrl/include/game/client_conn_scene.hpp` | 2 |
| `game/ctrl/include/game/net/replication_events.hpp` | 1 |
| `game/ctrl/src/net/replication_registry.cpp` (register `PlayerActionReplicationEvent`) | 1 |
| `game/ctrl/test/scene_test_harness.hpp` | 1, 2 |
| `game/ctrl/test/replication_events_test.cpp` | 1, 3 |
| `game/ctrl/test/e2e_scene_test.cpp` | 1–4 |
| `game/ctrl_ext/include/game/debug_voxel_view.hpp` | 1 |
| `game/ctrl_ext/include/game/interpolation.hpp` (+ view call sites) | 4 |
| `game/view/…` (SceneView interpolation call sites) | 4 |
| `game/ctrl/include/game/net/rollback_event_log_stream.hpp` | — (verify only: tick cadence, snapshotInterval comment now true) |

---

## 5. Tests — new and adjusted (concrete assertions)

All e2e tests live in `game/ctrl/test/e2e_scene_test.cpp` and use
`NetSceneHarness` (scene_test_harness.hpp).  Harness polling: 60 Hz, server
fixed step every 3rd poll.

### T1 (new) `e2e_input_tick_anchored_apply` — Phase 1

Setup: harness + handshake + prediction; find the server player; read
`serverWorld().ctx().get<sim::SimTickContext>()`; let both worlds settle
grounded.

Inject movement frames by calling `net::ApplyEvent(serverWorld,
PlayerInputReplicationEvent{serverPlayer, packed})` directly (flag A3 — the
real channel is ordered, so this is a contract test of the stamp-anchored
read path) with distinct move signatures per stamp:

- **Assert A (anchored application):** with server tick T, inject frames
  stamped T+1 (move +Z), T+2 (move −Z), T+3 (move +X) — in order.  At ticks
  T+2..T+4 the server body's per-tick displacement direction sequence must be
  +Z, −Z, +X (record `body.pos` per tick from `SimTickContext` changes) —
  each frame applied at its stamped tick +1, not at receive time.
- **Assert B (stale drop):** after the above, inject frame stamped T+2 again
  (move −X).  Poll through tick T+5; body trajectory must equal the
  no-injection baseline (stale frame has no effect).
- **Assert C (bounded gap wait):** inject only frame stamped T+8 (move +Z),
  leaving T+7 unproduced.  Through tick T+9 no +Z displacement (waiting for
  T+7); after `kMaxInputWaitTicks` without it, the read degrades —
  `lastAppliedTick` advances past the gap and T+8 applies at its own stamped
  tick (assert exactly this per the D3 contract, pinned in the test comment).
- **Assert D (client stamps):** with the client driving input normally, the
  frames arriving in the server stream carry `tick` equal to the client's
  producing tick (`clientRollbackStream().CurrentTick()` observed at
  aggregation time, ±1).

### T2 (rewritten) `e2e_player_input_replicates` — Phase 1

As today (e2e_scene_test.cpp:538-624), but: push a move delta + a Jump action
+ a Dig action; poll; assert the server's `PlayerInputStream` receives the
aggregated movement frame (tick = producing tick, move magnitude > 0.4 —
SNorm8 rounding, `jump == true`, **no** inputEvents section) and the server's
`PlayerActionStream` receives the dig action frame (same tick, ray origin/dir
equal to the client camera's origin/forward at emit time ±1e-3); then poll
until the server fixed stage applied them — `lastAppliedTick == stamp`,
`consumedTick` advanced, and the dig ray was cast (terrain block changed at
the hit position).

### T3 (new) `e2e_player_config_parity` — Phase 2

After handshake: `clientScene()->GetConfig().subsystems` ==
`sim::FixedStepPlayerStages(clientAF)` and `.realtimeSubsystems` ==
`sim::RealtimePlayerStages(clientAF)` (+ `SubsystemDebugText`); same for the
server scene vs the server variant (fixed + terrain, empty realtime).
Guards the config-drift bug class.

### T4 (tightened) `e2e_player_input_jump_move_aim_sync` — Phases 2/3

Same flow as today (e2e_scene_test.cpp:766-920), adjusted:
- trajectories recorded per tick (every 3rd poll) for the prediction world
  body and the authoritative mirror body, aligned by lift-off tick (flag
  A6 — mirror arrives with latency; aligning by lift-off removes it);
- assertions: `predLift > 0.5`, `authLift > 0.5`, `predLiftPoll <
  authLiftPoll` (prediction leads — unchanged);
- **apex skew < 0.05 m** and **lift drift < 0.05 m** (from 0.5/0.15);
- per-tick `|predPos − authPos| < 0.05 m` for the aligned arc
  (tick-for-tick cadence parity);
- input rate: over a 90-poll window, server-side applied movement frames
  ≈ 30 (±2) — pins "one frame per fixed tick".

### T5 (adjusted, existing) — Phases 1/2

- `e2e_player_input_prediction_vs_authoritative`: the movement/action flow
  adapts to the new design; the **yaw assertion is re-targeted** (A9): drive
  forward + pan + a dig action, and assert the server's action stream
  received rays whose direction tracks the pan (angle between the delivered
  ray dir and the client camera forward at emit < 1e-3) — aim parity now
  proven through the rays.  Replace the `UpdateTag<Realtime>` emplacements
  with `UpdateTag<FixedStep>` + `PlayerSimInputState` (defensive, mirroring
  `CreatePlayer`).
- `e2e_local_prediction_player_moves`: same tag/commit adjustments.
- `e2e_physics_events_bounded`: **unchanged** (≤1 per entity per poll holds
  under D7 — 1 patch per tick).
- `e2e_interpolation_layer`: unchanged for now (it only asserts the tag +
  local movement); extended in Phase 4.
- `e2e_rollback_on_server_correction` / `e2e_rollback_recovery_no_relapse` /
  `e2e_prediction_stability` / `e2e_scenes_stability` / handshake + chunk
  tests: no assertion changes expected; must stay green at the new cadence
  (snapshots now every 3 ticks = 150 ms — the pumpUntil snapshots>0 waits
  still succeed within their bounds).

### T6 (new) `e2e_remote_copy_no_teleport` — Phase 4

Setup: handshake + prediction; tag the client player
`RenderStrategyTag<Interpolation>` **in the render world** as a stand-in
remote copy (the harness has no SceneView, so drive
`InterpolationLayer::PreUpdate(clientAuthoritativeWorld())` +
`PostUpdate(clientWorld(), alpha)` inside the pump loop with
`alpha = h.clientScene()->GetFixedStepAlpha()`).  Trigger a server-side
teleport correction (as in `e2e_rollback_on_server_correction`,
e2e_scene_test.cpp:1156-1159).  Assert:
- the mirror body snaps to `distantPos` within the window, **while**
- `ComponentInterpolatedTransform.pos` never jumps: max per-frame
  `|interpDelta|` over the window < 0.3 m (body snap is 100+ m),
- and the interpolated transform converges to `distantPos` within the
  window (attractor still tracks the target).
This pins criterion 3 (no teleport) without a real renderer.

### Unit-test deltas

- `replication_events_test.cpp`: tick + action round-trips (Phase 1),
  shrunk-frame round-trips (Phase 3), `AggregateTickInput` +
  `TryReadTickFrame` contract blocks (Phase 1).
- No changes expected in datastruct/registry/sim/scene suites (verify via
  full ctest).

---

## 6. Verification checklist

1. **Per phase:** `cmake --build out/build/apple-debug --target <test>` for
   the touched suites before the implementation lands (test-first red), then
   green.
2. **Full build:** `cmake --build out/build/apple-debug --target all` (0
   errors; no new warnings in touched files).
3. **Full ctest:** `ctest --test-dir out/build/apple-debug` — all 124
   existing + new tests green, run **twice** (the snapshot_terrain flake is
   the known exception — record its pass/fail separately).
4. **E2E binary twice:** run `e2e_scene_test` directly twice (per repo
   requirement "full e2e suite twice").
5. **Grep gates:**
   - Phase 3 exit: `grep -rn "HasJump\|jumpPos\|MarkJumpPerformed"` ⇒ no
     hits in `game/`.
   - Phase 2 exit: no hand-assembled `SubsystemPlayer<FixedStep>`/
     `SubsystemCreature`/`SubsystemPhysics` stage pushes outside
     `player_sim_config.hpp` (`grep -rn "push_back(Id<sim::Subsystem" game/`).
6. **Manual smoke (Arterium, macOS):** local player moves/jumps at 60 fps
   feel with zero added latency (60 Hz aim + extrapolated local transform +
   immediate dig/place); camera aim smooth and immediate; second client (if
   available) renders smoothly (no 20 Hz stepping, no teleport on corrections).
7. **Cadence spot-check:** server `AdvancePeerTick` fires every 3rd poll in
   the harness (log or counter assertion in T1's pump); client fixed
   pipeline fires on the same cadence.

---

## 7. Risks

- **R1 — `Scene::Update` accumulator change touches all scenes.**  Default
  duration 1/30 reproduces current fixed-pipeline behavior exactly
  (accumulator math equivalent); debug_scene3_test + scene_load_test must
  stay green before Phase 1 is considered done.
- **R2 — `SubsystemTerrain` now runs 3× per fixed frame (dt=1/60 ×3 instead
  of 1/20 ×1).**  Verify `game/sim/terrain/subsystem_terrain`'s use of
  `ctx.dt` (chunk-gen budget): if the budget is per-update rather than
  per-second, generation triples; clamp or move the budget to per-second
  math.
- **R3 — Rollback validation cadence parity.**  D7 pins 1 patch : 1
  prediction per tick; if any change breaks that (e.g. physics patching per
  sub-step again, or prediction insertion per render frame), `Validate`'s
  count comparison (rollback_event_log_stream.hpp:250) rollback-loops.
  The unchanged ≤1 bound in `e2e_physics_events_bounded` encodes this.
- **R4 — unordered_set event order** (subsystem_physics.cpp:170) is
  implementation-defined but stable per build; with multiple players the
  server's patch order across entities could differ from the client's
  prediction insertion order within the same validation window (client
  iterates views, server iterates the unordered_set).  Single-player e2e is
  immune; for multi-player, replace with a small deterministic vector
  (insertion order) as a hardening follow-up — not required for this PR's
  per-build determinism bar.
- **R5 — Float drift / tick accumulation.**  Harness `POLL_DT = 0.016` ≠
  1/60 exactly (and 3 × 0.016 = 0.048 ≠ 1/20); the fixed accumulator uses an
  epsilon comparison, so the 3rd-poll cadence is preserved with occasional
  late fire.  Both sides drift identically in a single binary (loopback),
  which is the asserted scope — cross-platform bit-parity is out of scope
  (per constraint).
- **R6 — Client tick catch-up after join.**  `AdvanceTick(avt)` jumps
  `m_currentTick` forward (rollback_event_log_stream.hpp:142-149); frames
  produced before the jump become stale on the server and are dropped —
  acceptable (per-tick frames, not cumulative), but the parity e2e must
  settle/align before measuring (T4 aligns by lift-off tick).
- **R7 — 20 Hz choppiness before Phase 4.**  The client's 60 Hz aim sim +
  extrapolated local transform keep the local player smooth from Phase 1;
  remote copies step at 20 Hz until Phase 4 lands — schedule 4 immediately
  after 3.
- **R8 — `lastActionTime` remains in the replicated `ComponentPlayer`.**
  Not on the wire, but a re-applied `AddComponentEvent<ComponentPlayer>`
  resets it (same hazard as the cursors).  Move it to `PlayerSimInputState`
  if re-snapshots ever occur mid-play; not required now.
- **R9 — Tick wrap** (uint32_t, 20 Hz ⇒ ~6.8 years).  Wrap-safe `int32_t`
  delta comparisons everywhere (D8); covered by the `TryReadTickFrame` unit
  test (`lastAppliedTick = 0xFFFFFFFE`, incoming `tick = 2`).
- **R10 — Pong/alignment semantics at the new cadence.**
  `m_snapshotInterval = 3` (client_scene2.hpp:155) now means 150 ms —
  matches the comment; `m_alignmentTick` comparisons all work in the same
  tick unit.  Verify `HandlePong`'s tick scan (rollback_event_log_stream.
  hpp:397-440) against 20 Hz `AdvanceTick` entries — no code change
  expected, but the rollback e2e tests are the gate.
- **R11 — Two client sims, one entity (D9).**  Ownership rules must hold
  exactly: the realtime stage never writes `moveOrder`/body, the fixed stage
  never writes camera aim.  Add an assert in each stage during development;
  the e2e suite is the regression net after.
- **R12 — Ring wrap under reader lag (D3).**  The movement ring holds 16
  raw 60 Hz frames (~0.27 s ≈ 5 ticks); readers poll at most once per tick
  (window 3 frames).  If the fixed reader falls more than ~4 frames behind
  (only possible under a stall), input is silently lost — acceptable and by
  design (age-out), but keep the `static_assert` + comment so future rate
  changes re-verify the size.
- **R13 — Action stream bursts.**  Dig/place are event-driven; a burst of
  clicks within one tick exceeds the per-tick action frame capacity if the
  vector is unbounded it's fine, but cap the packed actions (e.g. 32/tick)
  and assert the cap is unreachable in practice.
- **R14 — Raw-float action rays.**  ~25 bytes per action at action rate is
  negligible; quantization (snorm16 direction) is a later optimization and
  risks action-ray parity — do not quantize in v1.
- **R15 — Snapshot purity in the authoritative world (D9).**  The mirror now
  contains the client's own fixed-sim state, not only server-applied truth.
  Rollback snapshots are taken from it (client_scene2.hpp:185-186 — before
  the sim runs), so a snapshot could capture client-sim state that a later
  server apply overwrites; the validation comparison and rollback still
  converge because server applies land in the same world.  The rollback e2e
  tests are the gate; if snapshot purity ever matters again, take snapshots
  immediately after the poll instead of after the sim.
- **R16 — Mirror world terrain (D9).**  The authoritative world's fixed sim
  needs `BlockRegistry` + `TerrainView` + chunk data: chunk events must be
  masked to both worlds, doubling chunk replication traffic (chunk events
  only; physics events stay mirror-only).  Verify the mask change doesn't
  break `e2e_add_chunk_replicates` / `e2e_update_chunk_replicates`.

---

## 8. Out of scope / follow-ups

- Phase 5 reconciliation by re-simulation (stretch — see §3).
- Multi-player determinism hardening (R4 vector swap).
- Cross-platform bit-parity.
- Action-ray quantization (R14).
- Server `ComponentCamera` aim is vestigial under A9 — remove once nothing
  consumes it.
- `PlayerInputStream`/`PlayerActionStream` on the server player entity
  become the apply targets only — fine as-is, revisit if hook bookkeeping
  changes.

## 9. Ambiguities flagged

**Resolved in design review (no longer open):**
- **A1** — the actual simulation rate is 20 Hz per fixed frame (D1).
- **A4** — chase camera: out of scope ("ignore the chase camera part").
- **A5** — exponential attractor (confirmed).
- **A7** — extrapolation for **remote players only**; the local player
  renders the prediction-world body zero-delay.
- **A8** — the realtime pipeline is input processing + the prediction-world
  movement sim; the 20 tps sim lives in the authoritative world (D9).
- **A9** — server head/aim tracking ("model head movement") does not exist
  in the codebase yet: out of scope; aim reaches the server only via action
  rays (e2e assertion re-targeted to ray delivery, T3).
- **A10** — terrain edits are fixed-pipeline-only on both sides; no
  emit-time vs tick-time asymmetry (D6).

**Still open (methodological / implementation-time):**
- **A3** — no reordering is possible end-to-end (ordered reliable channel +
  FIFO streams); the e2e contracts the stamp-anchored **read** path (gaps,
  stale) via direct `ApplyEvent` injection (D3).
- **A6** — tick-for-tick parity measurement must align trajectories by
  lift-off tick (mirror latency), not wall time — T4 asserts the aligned
  quantities.
- **A11** — empty-tick contract: idle ticks send no frame; absence = "no
  input" (not loss); degrade keeps `lastAppliedTick` fresh so late frames
  stay stale (D3).  Also pinned here: the `lastActionTime` action cooldown
  (subsystem_player.cpp:253, 272) moves with the action path — the client
  realtime stage gates emission, the server fixed stage gates application —
  per-side local state as today, not on the wire.
