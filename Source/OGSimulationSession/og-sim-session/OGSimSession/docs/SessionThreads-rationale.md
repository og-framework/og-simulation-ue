<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: beginStoppedDrain -- replaced by og-session-runtime task 31 with StoppedDrainScope (section 8); kept in the history -->
<!-- lint-external-ref: endStoppedDrain -- replaced by og-session-runtime task 31 with StoppedDrainScope (section 8); kept in the history -->
# `SessionThreads.h` — rationale

`SessionThreads.h` holds the session runtime's per-role thread table and its checks (og-session-runtime design §3.4, the
SessionThreads component; lead ruling L8: one table per role, here and nowhere else). It is the docs pointer target of
`SessionThreads.h`. The header's rules are `static_assert`s and one `OG_CHECK`, so there is no guards file.

**If this file and `SessionThreads.h` disagree, the header is authoritative and this file is stale.**

---

## §1 What it is for

Until og-session-runtime task 22 the Unreal session marked each function's thread with a suffix (`_GameThread`, `_Step`,
`_Runner`, `_Owner`) and checked some of them with `checkf(IsInGameThread())`; the table of who calls what on which thread
was prose (§7, moved here from OGSimulationUnreal's docs). The runtime replaces both with data: one row per operation and
role, and a check each entry point calls with its operation, which fails with an `OG_CHECK` on any other thread. Task 22
added the table and the check with no caller; tasks 23 to 31 moved the session's entry points into og-sim-session, and each
component's doors call `checkOp` with their operation (each component's rationale names its doors and rows). The Unreal
adapter that remains (`UESimulationSession.h`) keeps the suffixes on its own game-thread members.

## §2 The threads: `SessionThread`

| thread | prediction (client) | authority |
|---|---|---|
| `Frame` | the engine's frame thread (the Unreal game thread) | the same |
| `Step` | the thread running a step: the worker task while it runs a batch (`StepThreading::FrameBounded`), inside the tick driver's `StepScope` | the runner (`StepThreading::FreeRunning`), the thread that configured the world host's thread context |
| `NetIo` | og-net's own I/O thread, which receives and pushes into the inbox | the runner, which services og-net in its idle (`SimNetIdle.h`) |

Design §3.4: "Step" is the role's owner thread while it steps. On the authority the runner is both `Step` and `NetIo`; on a
client they are different threads.

## §3 The operations and the tables

`SessionOp` has one enumerator per operation named in the rows of design §3.4's table, in the rows' order, and a sentinel
`Count`:

| design row | operations |
|---|---|
| sample input; the rate door; register, unregister, collect outcomes; acquire the display; present | `SampleInput`, `RateDoor`, `TryRegister`, `RequestUnregister`, `PollOutcomes`, `AcquireDisplay`, `Present` |
| drain the inbox; apply lifecycle requests; capture; step; send; publish the display or the presentation | `DrainInbox`, `ApplyLifecycleRequests`, `Capture`, `Step`, `Send`, `Publish` |
| service og-net | `ServiceNet` |
| the link's frame service | `LinkFrameService` |
| the client's stopped drain at its teardown (og-session-runtime task 24) | `DrainStopped` |
| the frame probe after the pump (og-session-runtime task 26) | `FrameProbe` |
| the latency diagnostics' doors, one row per door (og-session-runtime task 37) | `LatencyWrap`, `LatencyCapture`, `LatencyClientReceived`, `LatencyClientWireSample`, `LatencyServerWireSample`, `LatencyHanded`, `LatencyServerReceived`, `LatencyCorrectionHanded`, `LatencyAfterStep`, `LatencyStepEnd`, `LatencyForget`, `LatencyPostTickFlush`, `LatencyWindow` |

`kPredictionThreads` and `kAuthorityThreads` give each operation its thread, or none. They follow the design's table, with
three readings taken from the code, through task 16's table (§7.1), and two rows the design's table does not have:

- **The stopped drain** (`DrainStopped`, after `LinkFrameService`): a client's teardown drains og-sim-net's inbox once on the
  frame thread after its steps stopped (§7.1, its row), and a message in that drain reaches the session clock's receipt doors
  there; its row is the client's `Frame`, and the authority has none (it has no such drain). The session clock checks it only
  between the teardown's two markers (`SessionClock-rationale.md` §3); `DrainInbox` stays the step's (lead ruling under task 24,
  2026-11-25 02:25). A fourth `static_assert` holds the row's shape.
- **The frame probe** (`FrameProbe`, after `DrainStopped`): `SessionTickGame::onFrameStepsDue`, PROBE A after the pump, runs
  where the pump runs: a client's frame thread (`startFrame`), an authority's runner (each loop wake); its rows are `Frame` and
  `Step`. The design's call order names it, but no row did: `RateDoor` is the rate door's, which runs at the same place but is
  another operation, so the probe got its own row rather than a wider one (og-session-runtime task 26). A fifth `static_assert`
  holds its shape.
- **The latency diagnostics' doors** (the thirteen `Latency…` rows, after `FrameProbe`; og-session-runtime task 37): each door
  of og-sim-session's `SessionLatencyDiagnostics` asks its own row first. A client's capture stamp, receipt, wire sample,
  hand-over and step stamps are `Step`, its wrap, forget, post-tick flush and window `Frame`; the authority's server-host wire
  sample is `Frame` and its hand-over, receipt, correction hand-over and step stamps `Step` (the runner); a door the role never
  calls has no thread (`std::nullopt`): three on a client, four on the authority. The runtime routes the checks
  (`SessionRuntime::checkDiagnosticsOp`): nothing before `start`, `DrainStopped` during a client's stopped drain. A sixth
  `static_assert`, `latencyDoorsKeepTheirSide`, holds the step doors off the frame thread and the frame doors on it. Each row's
  door and caller are `SessionLatencyDiagnostics-rationale.md` §4.

The three readings:

- **The authority samples no input** (`std::nullopt`): its characters' inputs arrive over the network, and the tick
  driver's `startFrame` returns at once in `FreeRunning`.
- **The authority's rate door is on the runner** (`Step`), where the design's row says "Frame (no rate change)": the
  driver builds the door before each pump, on the thread that pumps, and on the authority that is the runner
  (`noteAuthorityRateDoor_Owner` in §7.1; the session clock's `noteAuthorityRateDoor` since og-session-runtime task 24, which
  checks this row). The door refuses every rate change there (`TickRateDoor`,
  `TickDriverGameTypes-rationale.md` §2).
- **The authority's `AcquireDisplay` is applying the presentation slices** on the frame thread
  (`presentAuthority_GameThread`), where a client acquires its display snapshot. og-sim-session's `SessionDisplay` checks
  this row in `presentLatest`, and a client's in `acquireForFrame` and its readers; both roles' `Publish` row in
  `publishAfterBatch` and `publishPresentation` (og-session-runtime task 25, `SessionDisplay-rationale.md` §2).

Three `static_assert`s hold the tables to their shape:

- **a row's position is its operation** (`isIndexedByOp`), so a row cannot be read for another operation;
- **the step's work never runs on the frame thread, on either role** (`stepWorkIsOffTheFrame`: `DrainInbox` to
  `ServiceNet`);
- **the authority samples no input.**

## §4 `SessionThreadChecks`

Built from the role, a `const TickDriverThreadContext*` (the world host's, which owns the role bit and the owner count) and
the frame thread's id. `isOn(thread)` answers for the calling thread:

- `Frame`: the calling thread is the frame thread.
- On the authority, `Step` and `NetIo`: the calling thread is the context's owner thread
  (`TickDriverThreadContext::isOwnerThreadForRole`), the runner that configured it.
- On a client, `Step`: a step of this role is running on the calling thread (`isStepRunningOnThisThread`, the driver's
  `StepScope` around each step and around the publish after the batch). `NetIo`: neither the frame thread nor a step.
- With no context, or a context configured for the other role, `Step` and `NetIo` never pass: only the frame rows can.
- **The exact `NetIo` row** (og-session-runtime task 31): on a client, once `setNetIoThread(id)` has been given a thread,
  `NetIo` passes on that thread only, and the context is not asked. The runtime calls it from `start` when the engine fills
  `SessionStart::netIoThread` (`SessionRuntime-rationale.md`); an engine that does not know og-net's I/O thread leaves it
  empty and keeps the reading above ("neither the frame thread nor a step"). Unreal's adapter leaves it empty today:
  `UESimulationSession::beginPlay` sets no `netIoThread`. Test:
  `SessionThreads.WithOgNetsIoThreadKnownAPredictionNetIoOpPassesOnThatThreadAlone`.

`isAllowed(op)` is `isOn` of the operation's row, and false for an operation the role does not perform and for `Count`.
**`checkOp(op)` is `OG_CHECK(isAllowed(op))`**: on the wrong thread it fails with
`SessionThreadChecks::checkOp: this operation ran on a thread its role's row of the session thread table does not name`.
The task's description calls it `check(SessionOp)`; it is `checkOp` because `check` is an engine assertion macro in the Unreal
builds, and a member called `check` would be rewritten by it in every translation unit that includes an engine header
first (the module's own source does).

## §5 The receipt gate: the authority's first batch

og-sim-net's `SimNetServer::afterInlineBatch(serverTick)` reaps the connections, and the reap is the only feed of the
reception coordinator's receipt gate: a capture judged against tick 0 is outside the gate's domain and dropped
(`SimNetServer-rationale.md` §7). The tick driver hands `afterInlineBatch` the batch's first upcoming tick, which on the
authority loop's first batch is 0, because the driver publishes its tick offset only after a tick ran.

So the runtime, and any host of it (og-session-runtime task 32's headless host included), must keep one of two things true:

1. **the first batch at tick 0 runs with no connection attached.** `SessionNetHooks::afterBatch` checks exactly this (an
   `OG_CHECK` that `firstUpcomingSimTick` is not 0 or the server's `attachedConnections` are empty; the Unreal session's
   `checkf` in `afterAuthorityBatch` until og-session-runtime task 26): the runner services og-net only in its idle, after its
   first batch, so no connection can attach before it; or
2. **the gate is armed with a real tick first**, before any connection's capture is judged.

The check moved with `afterAuthorityBatch` into the runtime when the runtime took over the tick driver's hooks (task 26), as
an `OG_CHECK` (`SessionNetHooks-rationale.md` §5). (Lead note under task 22, 2026-11-24 13:23, from review_SR2_15's lead
item 3.)

## §6 The tests

`SessionThreadsTest.cpp` (tag `[Session]`):

- `SessionThreads.TheTablesAreTheDesignsTable`: each table's rows equal a test-local transcription of design §3.4's table
  with the three readings of §3.
- `SessionThreads.APredictionOpPassesOnItsThreadAndOnNoOther` and `SessionThreads.AnAuthorityOpPassesOnItsThreadAndOnNoOther`:
  every operation is asked on the frame thread, on a step (a worker inside a `StepScope`; on the authority, the runner that
  configured the context) and on a plain thread. Each passes on its row's thread and on no other: every row has a case on
  its thread and one on a forbidden thread (a row with no thread has every case forbidden: on the authority `SampleInput`,
  `DrainStopped` and the four client-only latency rows, on a client the three authority-only latency rows; the cases count
  them, og-session-runtime task 37).
- `SessionThreads.WithoutTheRolesContextOnlyTheFrameRowsPass`: no context, or a client's context given to the authority.
- `SessionThreads.CheckOpIsSilentOnTheRowsThreadAndTheSentinelHasNoRow`.
- `SessionThreads.TheStoppedDrainIsAClientsFrameRowAndNoStepsOrAuthoritys` (og-session-runtime task 24): `DrainStopped` passes
  on the client's frame thread and fails in a step and on both of the authority's threads, where `DrainInbox` passes in the step
  and fails on the frame.
- `SessionThreads.TheFrameProbeIsTheClientsFrameAndTheAuthoritysRunner` (og-session-runtime task 26): `FrameProbe` passes on a
  client's frame thread and on the authority's runner, and fails in a client's step and on the authority's frame and on a plain
  thread.
- **Hidden** (`[.][SessionThreadsDeath]`): `SessionThreads.DrainInboxOnTheFrameTripsTheCheck` calls `checkOp(DrainInbox)` on
  the frame thread and must never reach its `FAIL`; `SessionThreads.DrainStoppedOnTheAuthorityTripsTheCheck` (task 24) calls
  `checkOp(DrainStopped)` on an authority's frame thread, where the row has no thread;
  `SessionThreads.FrameProbeOnAnAuthoritysFrameThreadTripsTheCheck` (task 26) configures the authority's context on a runner
  thread, then calls `checkOp(FrameProbe)` on the frame thread, where its row is the runner's: the check trips on the owner
  thread, not on an unconfigured context (called on the runner, it passes and the case reaches its `FAIL`). Run them by name.
  (A client's step on a worker is the other forbidden thread; the matrix case above covers it, because an `OG_CHECK` that fails off the test's
  thread ends the runner.) In `OGSimulationTests` the failed `OG_CHECK` is reported as
  a failed case with its message (seen at task 22). It calls the check on the test's own thread.

## §7 The Unreal session's thread table (task 16), moved here

> og-session-runtime task 22 moved this section's text here verbatim from OGSimulationUnreal's
> `UESimulationSession-rationale.md` §24, where task 16 wrote it, so that the thread table is kept in one place. It
> describes today's Unreal session, read from the code after task 15; the runtime takes over its rows as tasks 23 to 31
> move the session here. Section numbers in it (§19, §23) are that file's, and "this table" is this section.
>
> *Since og-session-runtime task 31* the code it was read from is og-sim-session's (`SessionRuntime` and its components); the
> Unreal session is the thin adapter. The section stays the record of task 16's reading, under the names the code had then;
> the checked table is §3's, in `SessionThreads.h`. Where a row names a member that moved, the component's rationale names
> its successor.

The one table of who calls what on which thread, for both roles, read from the code after og-session-runtime task 15
(`UESimulationSession.h`, `BrawlerSessionHooksUImpl.h`, og-simulation's `SimulationTickDriver.h`, `TickDriverStepHooks.h`
and `SimulationStepDriver.h`, `UESimNetLanes.h`, `UESimNetHost.h`). og-simulation and og-sim-net state only their own
contracts ("the owner thread only", "one consumer at a time") and name no caller; og-simulation's `ThreadingCrossings.md`
indexes the shared data structures and points here for the callers. og-session-runtime task 22 moves this table to the
runtime (design §3.4, its SessionThreads component).

Each role has one shape. A **client** steps on a worker task, one task at a time (`StepThreading::FrameBounded`; the frame
waits for its own single step, `FrameWait::ThisFramesSingleStep`). An **authority** runs only on a dedicated server with
the og-net driver (§23) and steps on its runner, the authority loop (`StepThreading::FreeRunning`, §19). The names used
below:

- **frame**: UE's game thread. It runs the tick functions (`startFrame`, `endFrame`), the components' registration and
  `EndPlay`, the session's `beginPlay` and `endPlay`, and the og-net driver's `UESimNetLanes::afterDispatch`.
- **step**: a client's worker task while it runs a batch.
- **runner**: an authority's runner thread (`FUEAuthorityRunner`), which builds the world, pumps, steps and services og-net.
- **og-net**: og-net's own I/O thread; a client's lanes use it (`simLanesIoMode`), a dedicated server's lanes do not.

### 7.1 The table (was §24.1)

> Since og-session-runtime task 26 the tick driver's hooks are og-sim-session's `SessionTickGame` (each door checks the row of
> §3 it runs on; `SessionTickGame-rationale.md` §3, §4) and the net-hook bodies are `SessionNetHooks`. The Unreal names in the
> table and in §7.2 (`beforePump_Owner`, `receiveForClientStep_Step`, `afterClientCapture_Step`, `afterStep_Step`,
> `publishDisplaySnapshot_Step`, `noteAuthorityStepTime_Step`, `releaseDelayedInputsForStep`, `afterAuthorityBatch`,
> `stampCorrectionsHanded_Runner`, `idleUntil_Runner`) are those bodies' names until then; the threads and the order are
> unchanged (the order test, `SessionTickGame-rationale.md` §6).

| Operation | Client | Authority |
|---|---|---|
| Sample input (`sampleInput`: each character publishes its input sample to its mailbox); the rate door (`beforePump_Owner` → `runClockDilation_GameThread`); `tryRegister`, `unregisterFromNewFramework`; collect the lifecycle outcomes (`collectLifecycleOutcomes_GameThread`); acquire the display (`refreshDisplayView_GameThread`); present (`beforePresent_GameThread`) | frame | frame, except: no input sample (`startFrame` returns at once in `FreeRunning`), and the rate door runs on the runner (`noteAuthorityRateDoor_Owner`); the presentation slices are applied (`presentAuthority_GameThread`) where a client acquires its display |
| Drain the inbox; apply the lifecycle requests; capture; step; send; publish the display or the presentation | step: `receiveForClientStep_Step` (→ `applyUnregisterRequests_Step` → `UESimNetLanes::drainClient_Step` → `SimNetClient::drain`; since og-session-runtime tasks 26 and 29 `SessionNetHooks::receiveForStep` → `SessionLifecycle::applyRequests` → the drain), the send at capture (`afterClientCapture_Step` → `SimNetClient::afterCaptureTick_Step`), `publishDisplaySnapshot_Step` after the batch | runner: `beforePump_Owner` (the queued runner requests, then `SimNetServer::drain`), `releaseInputsForAuthorityTick` per step, `afterAuthorityBatch` after the batch (the send, the corrections, the clock, `publishAuthorityPresentation_Runner`) |
| Service og-net (receive, push into the inbox, answer a clock ping) | og-net | runner, in `idleUntil_Runner` (`SimNetIdle`), between batches |
| The link's frame service (clock ping, the pre-bind drain, the RTT publish) | frame, in `UESimNetLanes::afterDispatch`: og-sim-net's `SimNetLink::serviceFrame` (the ping, then `publishServerRtt_GameThread`, then the drain while no session is attached; `SimNetLink-guards.md` G-02; its own frame-thread check, `SimNetLink-rationale.md` §4) | frame, in `UESimNetLanes::afterDispatch`: `publishRunnerRtt_GameThread` (`FRunnerRttTable`, read on the runner) |
| Start | frame: `beginPlay` builds the world, configures the session and starts stepping | frame starts the runner and waits for it (`runAuthorityLoop_GameThread`, `waitForStart_GameThread`); the runner builds the world and configures the session (§19.2) |
| Stop | frame: `endPlay` → `onEndPlayBegin` (the tick functions unregistered, the steps waited for), then the teardown drain (`teardownImmediateClient_GameThread` → `drainClientWhileStopped_GameThread`; since og-session-runtime task 29 through `SessionLifecycle::teardown`, which checks `DrainStopped` and has its port call the drain) | frame: `onEndPlayBegin` joins the runner (`stopAuthorityLoop_GameThread`), or the lanes' stop has joined it first (`onAuthorityRunnerJoined_GameThread`) |
| The stopped drain's receipts (row added by og-session-runtime task 24) | frame: the teardown drain above runs with the session still bound, so a timing info, clock sample, tier or floor in it reaches the session clock's receipt doors on the frame; the lifecycle's teardown marks the drain (the runtime port's `drainWhileStopped`, inside the clock's `stoppedDrain()` scope, a `StoppedDrainScope`, since og-session-runtime task 31), and inside the marks those doors check `SessionOp::DrainStopped` (§3) | none: the authority has no teardown drain |

A body is created where its role registers it: a client creates it on the frame (`createBodies_GameThread`, the frame-thread
door, design Q12) and queues the registration, which og-sim-net applies in the next drain on the step
(`onQueuedRegisterApplied_Step`; `SessionLifecycle::onRegisterApplied` since og-session-runtime task 29); an authority builds a body request on the frame and posts it, and the runner creates the bodies and registers
(`makeBodyRequest_GameThread`, `postRunnerRequest`, `createBodies_Runner`, `onRunnerRegisterApplied_Runner` until og-session-runtime
task 30; since then `SessionAuthorityLifecycle`'s `tryRegister`, its `CreateBodies` and `Register` requests and `onRegisterApplied`,
over the bodies' `makeRequest` and `create`, `SessionLifecycle-rationale.md` §10.2).
Each posts an outcome that the frame collects.

### 7.2 The per-step order (was §24.2)

- **Client, frame, `startFrame`:** `sampleInput` → the frame time → `beforePump` (`runClockDilation_GameThread`, the dilation
  stage) → the pump → `onFrameStepsDue` (`onFrameStepsDue_Step`: PROBE A's `[ResimProbe.Frame]` lines) → the batch is
  dispatched to a worker task.
- **Client, step, per step:** the occupancy commands → `receiveForStep` (the unregister requests → `SimNetClient::drain`: the
  lifecycle queue, the inbox, the newest clock sample → the disconnect request) → resimulate if requested → capture the
  correction → collect and capture the input → `onStepStarted` (the latency stamp; the send at capture) → physics →
  `onStepEnded` (the latency stamp; `afterStep_Step`, which returns at once on a client) → the presenter's publish → the world
  host → the tick offset → the step cost. **After the batch:** `publishForDisplay` (`publishDisplaySnapshot_Step`).
- **Client, frame, `endFrame`:** wait for this frame's step → `onPostPhysics` (`onPostPhysicsStep`: present → collect the
  lifecycle outcomes → `refreshDisplayView_GameThread` → the game's `afterVisualization` → the diagnostics tick).
- **Authority, runner, per wake:** `beforePump` (`noteAuthorityRateDoor_Owner` → the queued runner requests →
  `SimNetServer::drain`: the queued registrations, then each bundle is parked and relayed at once) → the pump →
  `onFrameStepsDue` (PROBE A's `[RelayProbe.Frame]` lines) → per step: `releaseInputsForAuthorityTick`
  (`noteAuthorityStepTime_Step` → `releaseDelayedInputsForStep` → `SimNetServer::releaseForStep`) → the step → `onStepEnded`
  (`afterStep_Step`: the server tick to the SimClock server with its start time) → after the batch: `afterInlineBatch`
  (`afterAuthorityBatch`: `SimulationManager::onPostSimulationGameThread`, the manager's post-simulation send, on the runner
  despite its name → `SimNetServer::publishClock` → `SimNetServer::afterInlineBatch`: the reap, the corrections, the clock,
  the window → `stampCorrectionsHanded_Runner` → `publishAuthorityPresentation_Runner`) → `idleUntil` (`idleUntil_Runner`:
  og-net serviced to the deadline).
- **Authority, frame, `endFrame`:** `onPostPhysics` only (`onPostPhysicsStep`: present → collect the lifecycle outcomes →
  `presentAuthority_GameThread` → the diagnostics tick).

### 7.3 What crosses (was §24.3)

The session's shared state between these threads, each with its own synchronization:

| Crossing | Writer → reader | Shape |
|---|---|---|
| the lifecycle queue: since og-session-runtime task 30 the role's lifecycle class's one request vector and one outcome vector under its mutex (`SessionLifecycle`, `SessionAuthorityLifecycle`; the Unreal session's bridge, `m_runnerRequests` and `m_lifecycleOutcomes` under `m_lifecycleBridgeMutex`, until then), and og-sim-net's own lifecycle queue under it | frame → runner (the requests); runner or step → frame (the outcomes); a client's registrations go through og-sim-net's queue to the step | a mutex around the vectors (design L9 makes it one queue) |
| each character's input sample (`Mailbox.h`'s `TripleBuffer`) | frame → step | triple buffer, one writer, one reader |
| the display snapshot (`SimulationDisplayPublisher`, `SessionDisplay`'s since og-session-runtime task 25) | step → frame | snapshot channel (`ThreadingCrossings.md`, the display snapshot's crossings) |
| the tier reading (`m_displayTier`) | step → step while the session steps; the frame writes it only while no step runs | triple buffer |
| the presentation slices (`SessionDisplay`'s presentation buffer since og-session-runtime task 25; the Unreal session's own until then) | runner → frame | triple buffer |
| the inbox (og-sim-net's transport) | og-net → step on a client; runner → runner on an authority | the transport's single-consumer inbox |
| the listener-bind request (og-sim-net's client, `SimNetClient::bindListener`) | frame → step | one atomic, consumed by the drain |
| the RTT (`FRunnerRttTable`; the client host's published RTT, read by `serverRttSeconds`) | frame → runner; frame → step | atomics, one writer |

The tick driver keeps its own (the occupancy ring, the tick offset, the step-cost ring and the rest of
`ThreadingCrossings.md`'s J-rows). Each entry point's thread is checked where the code can check it: the tick driver's
`TickDriverThreadContext` `OG_CHECK`s, and the session's and lanes' `checkf(IsInGameThread())` and `checkf(!IsInGameThread())`.
Design §3.4 makes every row an `OG_CHECK` (task 22).

## §8 `StoppedDrainScope` (og-session-runtime task 31)

One scope for a stopped drain's flag, so the clock's and the lifecycle's flags are set and cleared the same way. The
constructor checks `SessionOp::DrainStopped` and stores `true` in the flag it is given; the destructor checks the same
operation again and stores `false`. Both stores are relaxed: the flag is read on the same thread, by the receipt doors
that the drain inside the scope reaches (`SessionClock-rationale.md` §3). It is neither copyable nor assignable, so a
scope cannot outlive its drain by a copy. Its users: the session clock's `stoppedDrain()` and `SessionLifecycle`'s
teardown drain (task 31 replaced the clock's `beginStoppedDrain`/`endStoppedDrain` pair and the lifecycle's own flag
handling with it).
