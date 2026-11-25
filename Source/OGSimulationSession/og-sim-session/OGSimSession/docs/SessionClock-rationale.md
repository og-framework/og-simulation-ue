<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: shouldStallForTierTransition -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
<!-- lint-external-ref: hadAnyTier -- retired by og-input-latency P6-2; kept in the history -->
# `SessionClock.h` — rationale

`SessionClock.h` holds og-sim-session's `ogsession::SessionClock`: the session's clock routing (the authority tick and the
SimClock sample into the estimator, the `[ClockSource.Window]` line, the dilation stage on a client, the authority's rate line,
its step start and the server tick's publish with that time) and its schedule (the connection tier, the relay-delay floor, the
effective input delay and its payment as stalls or skips). It is the docs pointer target of `SessionClock.h`; the prohibitions
are in `SessionClock-guards.md`.

**If this file and `SessionClock.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Where it came from (og-session-runtime task 24)

Design §3.4 names the component: "clock routing (authority tick, SimClock sample → estimator; the window line; the dilation
stage on the client; the server tick's publish with its step start time), and the schedule (tier, floor, effective delay,
stalls)". Until task 24 it was eighteen member functions and eleven data members of og-simulation-unreal's `UESimulationSession`.
The task moved them in two steps (TeamDescription W6):

- **(a), the move** (OGSimulation `dae3018`): the eighteen definitions cut from `UESimulationSession.h` and pasted unchanged
  into this header, which the Unreal session then included in place of the code; 538 of 565 changed lines moved.
- **(b), the adaptation:** the class `SessionClock<ManagerT, InputResolutionT, EngineT>` with the data members, doors named by
  operation, each door's first statement its thread check, the lines through the engine's log routes, the Unreal session holding
  one `std::optional<SessionClock<…>>` after its manager.

What stayed in the Unreal session: the wire-sample note and the test callback at the head of `onTimingInfoReceived`
(`UESimulationSession-rationale.md` §9.6), `getClientEffectiveInputDelayTicks` and the public accessor of the tier consumer
(forwarded), `beforePump_Owner` (task 26's: the tick driver's hook, which calls one of the two rate doors), the lanes and the
SimClock server, and the brawler binding's two step calls (`noteAuthorityStepTime_Step`, `afterStep_Step`), now one-line
forwarders.

## §2 The class, its ports and its doors

**Template parameters, each the narrowest thing the moved code used:**

| parameter | what the clock calls on it | the Unreal session's |
|---|---|---|
| `ManagerT` | `runsPrediction`, `getTimeConfig`, `setRelayDelayFloorTicks`, `editNetworkEstimator` / `getNetworkEstimator`, `editClientClock().requestInputDelayTransition`, `getServerClock().getTick`, `noteAuthorityStepSeconds`, and what `clockDilation::beforeClientPump` reads | og-simulation's `SimulationManager` |
| `InputResolutionT` | `setClientEffectiveInputDelayTicks` | the session's `SimulationInputResolution` peer |
| `EngineT`, a `SessionClockEngine` | `logRouted(const char*)` (`SessionLogSink`) and a const `nowSeconds()` | `UESessionClockEngine` (`RouteOGLevelLine`; `FPlatformTime::Seconds()`) |

Two more ports are per call: a door that may log the window line takes a `SessionClockLink` (`ackSrttSeconds()`,
`clockStats()`: og-net's ack round trip and the SimClock client's counts; the Unreal session's `ClockLink` over its lanes), and
`publishServerTick` takes a `SessionClockServer*` (`publishServerTick(tick, tickStartSeconds)`: og-sim-net's
`SimNetClockServer`; `nullptr` on a client). A full `SessionEngine` models `SessionClockEngine` (it refines `TickDriverEngine`,
whose `nowSeconds` is const, and `SessionLogSink`). Each concept has a reference shape and one negative per requirement
(`sessionClockSelfCheck`); the engine's `nowSeconds` must be og-net's receive-stamp clock (`⛔G-05`).

**The doors, named by operation.** The first statement of each is `SessionThreadChecks::checkOp` with its row of
`SessionThreads.h`'s table (the task's description calls it `check`, an engine macro in the Unreal builds;
`SessionThreads-rationale.md` §4):

| door | was (Unreal session) | op | client | authority | caller |
|---|---|---|---|---|---|
| `onAuthorityTick(tick, rtt, link)` | `onTimingInfoReceived`'s clock half | `DrainInbox`, or `DrainStopped` in the stopped drain | step | — | og-sim-net's client drain, through the client host's `onTimingInfo` |
| `onClockSample(sample, link)` | `onSimClockSample` | the same | step | — | the same, through the lanes host's `onClockSample` |
| `onConnectionTier(old, new)` | `onConnectionTierReceived` | the same | step | — | the same, through `onConnectionTier` |
| `onRelayFloor(floor)` | `onRelayDelayFloorReceived` | the same | step | — | the same, through `onRelayFloor` |
| `beforePredictionPump(host, rate)` | `runClockDilation_GameThread` | `RateDoor` | frame | — | `beforePump_Owner` on a predicting manager |
| `noteAuthorityRateDoor(host, rate)` | `noteAuthorityRateDoor_Owner` | `RateDoor` | — | runner | `beforePump_Owner` otherwise |
| `noteAuthorityStepStart()` | `noteAuthorityStepTime_Step` | `Step` | — | runner | the binding's `releaseInputsForAuthorityTick` |
| `takeReleaseStepSeconds()` | the first statement of `releaseDelayedInputsForStep` | `Step` | — | runner | `releaseDelayedInputsForStep` |
| `publishServerTick(server)` | `afterStep_Step` | `Publish` | step (returns) | runner | the binding's `onStepEnded`, both roles |

Not doors, with no check: `beginTierConsumer` and `logRelayDelayFloorAdvisory` run at start-up (a client's on the frame, an
authority's on the runner that configures the session) and the advisory again inside `onRelayFloor`; `endTierConsumer` runs in
`endPlay` on the frame; `tierConsumer()`, `dilationController()`, `dilationWindow()` and `displayTier()` are accessors (the
display snapshot's read of the tier reading moves with task 25's display component).

**Every guard on a manager that may be absent went with the move:** the clock holds a reference to its manager, and the Unreal
session emplaces it right after `m_manager.emplace` on both roles, so each door's old `m_manager.has_value()` test is the
session's `m_clock.has_value()` test now, at the call site. A door the session reaches with no clock returns there, as the old
function returned without a manager; no caller reaches one before `beginPlay` configures the session (the client host is bound
in `beginClientSimNet`, after the manager, and the tick driver's hooks run only once stepping starts).

## §3 The stopped drain

A bound client drains og-sim-net's inbox at the start of each step, on the step's thread (`UESimNetLanes::drainClient_Step`,
inside the driver's `StepScope`), and once more on the frame thread at its teardown: `teardownImmediateClient_GameThread` stops
the steps (`onEndPlayBegin`), queues the unregistrations and calls `drainClientWhileStopped_GameThread` while the session is still
bound, so a timing info, a clock sample, a tier or a floor in that drain reaches the four receipt doors on the frame. Their row
is `DrainInbox`, the step's; so the Unreal session marks that one drain with `beginStoppedDrain()` and `endStoppedDrain()`
around the call, and between them each receipt door checks `SessionOp::DrainStopped`, the client's frame row (the authority has
none: it has no such drain). `DrainInbox` itself is not widened: a receipt on the frame outside that window still fails (lead
ruling under task 24, 02:25). The two markers check `DrainStopped` themselves. The flag is written and read on the frame thread;
the step thread reads it only while no teardown runs (the steps are stopped before the marker is set).

## §4 The effective input delay

*Moved from `UESimulationSession-rationale.md` §9.4 by og-session-runtime task 24, with the code; R0 notes below.*

Two independent channels feed one number:

```
effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
                                 : rttTierInputDelays[kMaxConnectionTierIndex])
```

The pre-arrival case is therefore `max(floor, rttTierInputDelays[kMaxConnectionTierIndex])`. The published value lands in
the input-resolution peer's effective-delay atomic.

The **floor** and the **tier** are two og-net Control messages since og-input-latency task P5-7 (the session relay and the
per-connection relay carried them before). They are independent and can land in either order, which is exactly why both call
**one** recompute holding **both** cached inputs rather than each writing the atomic on its own (`⛔G-03`): two writers each
holding half the formula would answer with a stale other half whenever their arrivals interleaved. The start-up baseline
publish (`beginTierConsumer`) goes through the same site.

Both arms are evaluated inside `ReplicatedTierConsumer::effectiveInputDelayTicks`, which uses the same shared lookups
(`tierInputDelayTicks`, which floors, and the floored worst-tier fallback) as the server's
`ServerInputDelayQueue::effectiveDelay` for the same connection. That sharing is what makes the two ends agree by
construction rather than by coincidence.

`recomputeAndPublishEffectiveInputDelay` **returns the change** in published delay (new − previous) so a caller that must
pay for a change can: both receipts pay it, either sign (below). A caller publishing a baseline rather than reacting to a
transition ignores it. `m_lastPublishedEffectiveInputDelayTicks` is kept on the clock rather than read back from the atomic,
because the delta is the recompute's own bookkeeping, on the thread that runs it: the atomic exists to hand one scalar to the
input resolution's reads, not to be used as shared state. It is seeded to `0`, so the start-up publish reports its full value
as the delta, which `beginTierConsumer` deliberately ignores, since nothing has been predicted yet. The recompute also writes
the display's tier reading (`readDisplayTier`) into the clock's `m_displayTier` triple buffer, which the client's display
publish reads after each batch, and logs `[ConnectionTier] applied client input delay = …`.

**The tier is server-owned.** The server derives each connection's tier from its own per-connection ping reading
(og-simulation's `ServerReceptionCoordinator`) and sends the result to the owning client, as og-sim-net's
`SimNetServer::publishTier`. The client never computes a tier and never samples the round trip for tier purposes, which is what
makes a client/server tier disagreement impossible by construction rather than merely unlikely.

**The tier consumer is emplaced on both roles** (`UESimulationSession-guards.md` G-06, at the Unreal session's two
`m_clock->beginTierConsumer()` calls). A dedicated server never reads the value it publishes there, but a listen-server host
did: its local player's input delay came from that cache, and an empty cache would leave the atomic at `0` where the no-tier
fallback is correct. No tier ever reaches an authority world, so the authority cache stays at that fallback.

**The tier consumer is destroyed before the manager.** It borrows the manager's `TimeConfig`. It is the clock's member, the
clock is declared after the Unreal session's `m_manager`, and a `compositionContractsHold()` row there asserts that order;
`endPlay` also resets it while the manager still exists (`endTierConsumer`, `UESimulationSession-guards.md` G-21).

### 4.1 Both receipts pay the change, either sign (og-input-latency P6-2)

The floor's receipt (`applyReplicatedRelayDelayFloor`, from `onRelayFloor`) and the tier's (`onConnectionTier`) each hand the
recompute's return value to `payInputDelayTransition`, which requests `ClientPredictionClock::requestInputDelayTransition(delta)`
(on a predicting manager only, `⛔G-01`): a positive change is paid as prediction Stalls, a negative one as Skips. 0 requests
nothing. The lines:

```
[RelayDelayFloor] floor = 6 ticks, UPWARD, requesting 2-tick prediction stall      (Warning)
[ConnectionTier] floor = 3 ticks, DOWNWARD, requesting 1-tick prediction skip      (Warning)
[RelayDelayFloor] floor = 3 ticks, effective delay delta=0, no stall or skip       (Log)
[ConnectionTier] tier 0 -> 0, effective delay delta=-1                             (Log, before the payment)
```

**Why a lowered delay must skip.** The server re-times its parked inputs to the smaller delay and releases one per slot per
tick, oldest first, so at the switch two are due; with the client's gap-free capture stream its drain then stays the
difference late for the session (`[Release] late=1` on every release at a lowered delay of 1). The client's prediction target
never reads the delay, so no drift correction ever closes it. Each Skip leaves one capture tick unproduced, and that gap
drains one tick of the backlog when the release front reaches it (og-simulation's `ClientPredictionClock.h`, the input-delay
transition block; og-simulation-tests' `Network/DelayTransitionTest.cpp` pins both). Until P6-2 a lowered delay was logged as
`no stall` and did nothing.

**Why the tier pays its first arrival too** (`⛔G-07`). The quantity is the client's own effective delay before and after the
arrival, so a first arrival pays the change from the pre-arrival fallback (`max(floor, rttTierInputDelays[3])`) to the tier's
delay. That is the change the server made at its first sample of the wire, when it left the same fallback for the same tier
(`ServerInputDelayQueue::effectiveDelay`). At a floor of 4 or more both collapse to the floor and the change is 0; at the shipped
tier table (`{1, 2, 3, 4}`) floor 3 skips once at join for tiers 0-2, and floor 2 skips twice for tiers 0-1 and once for tier 2.
Until P6-2 the tier path asked og-simulation's `shouldStallForTierTransition` (retired with it), which returned zero for a first
arrival and dropped every negative change, and read `hadAnyTier` for it.

**The server sends a wire's first tier, tier 0 included** (og-input-latency P6-2; `ServerReceptionCoordinator` and og-sim-net's
`SimNetServer::publishTier`). Until P6-2 neither sent a first tier 0, so a wire that never left tier 0 never reached the tier
receipt: the client sat on the pre-arrival fallback while the server parked at tier-0 delay.

A floor **rise** on the live channel is indistinguishable from an upward tier transition to the client: the frontier must fall
back by the difference, which the clock pays down as stall ticks. A floor **fall** is the mirror: paid as Skips since P6-2.

**The bind replays the latched floor and tier.** og-net delivers the floor (and any early tier) before the client's world
exists; the client session in the driver's lanes latches them, and the Unreal session's bind (`bindClientSession`, after the
manager and the clock's tier consumer, a `checkf` in `beginClientSimNet`) has og-sim-net replay them at the next drain, through
the same receipt doors. Until og-input-latency task P5-9 the session also had two replay members, which applied and republished
but did not stall; nothing called them after P5-7.

### 4.2 The floor advisory

`logRelayDelayFloorAdvisory` reports what og-simulation's `classifyRelayDelayFloor` says about the session floor, from every
intake: the ini override at start-up (og-sim-session's `applySessionKnobs`, through the Unreal session's lambda) and the floor's
receipt. It is advisory only (`⛔G-02`, `⛔G-04`): floor `0` is the documented off mode and is never flagged, and floor `1` is a
legal configuration. Two lines: `[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline …` (Warning) and
`[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness mode active …` (Log).

*R0, og-session-runtime task 24, against the tree:* the four example lines above are the formats `kTransition`, `kNoTransition`
and `kConnectionTierChange` with those values (the task's smokes print `[RelayDelayFloor] floor = 6 ticks, UPWARD, requesting
2-tick prediction stall` and `[ConnectionTier] tier 0 -> 0, effective delay delta=0` on every client); `requestInputDelayTransition`
is og-simulation's `ClientPredictionClock` member; the shipped tier table is `{1, 2, 3, 4}` (`TimeConfig.h`). **Corrected:** the
moved text said the last-published delay "is kept on the session"; it is the clock's member now. It also said the listen-server
host reads the authority's cache: listen servers start no session since og-session-runtime task 14, so the authority's consumer
has no reader today; the guard (`UESimulationSession-guards.md` G-06) keeps the emplace, as that entry says. The "pull" paragraph
on the retired G-07 was history of the relay actors (deleted by P5-9) and was left out.

## §5 The clock feeds and the window line

*Moved from `UESimulationSession-rationale.md` §20 (its feeds and its shadow line; the knobs bullet stays there and in
`SessionKnobs-rationale.md` §3) and §9.6's estimator half, by og-session-runtime task 24.*

- **Two feeds, one estimator, one thread.** The engine's round trip arrives in `onAuthorityTick` (once per drain that delivers
  a clock tick; the Unreal session's `onTimingInfoReceived` first notes the wire sample for its latency diagnostics and returns
  early if its test callback is set) and goes to `NetworkTimeEstimator::noteEngineRttSample`, then the authority tick to
  `recordAuthorityTick`. A SimClock sample arrives in `onClockSample`, as og-simulation's `SimClockRttSample`, and goes to
  `noteSimClockSample` unchanged, the server tick and its age included (§7.1). Both run on the client session's drain thread, the
  estimator's writer: the step's thread once the session is bound, the frame in the stopped drain (§3). Which feed moves the
  estimate is the estimator's rule (og-simulation `TimeConfig-rationale.md` §15).
- **The shadow line**, `[ClockSource.Window]` at `Warning`, once per `ogsimnet::kSimNetWindowSeconds` (10 s) of the engine's
  clock, checked from either feed on the drain's thread, on a predicting client only. The first call starts the window, so a
  client that receives neither feed logs nothing; a call at 10 s or more after the start logs the line and restarts the window
  at that call (the due rule `SessionClock.TheWindowLineIsDueTenSecondsAfterTheFirstFeedAndAfterEachLine` checks). Its fields:
  - `source=` the estimator's active source (`getActiveRttSource`);
  - `engineRttMs=` the clock's own EMA of the engine readings above 0 and finite, at `rttSmoothingAlpha` and with no
    plausibility gate (`noteEngineRttForClockWindow`). While the engine is the active source this is the estimator's arithmetic
    without its gate; after the switch the estimator drops engine readings, so this is the only engine figure left to compare
    against. `-1.000` before the first reading above 0 (the engine ping reads 0 for its first second, og-input-latency task
    P5-28);
  - `simClockRttMs=` the estimator's shadow EMA of the SimClock RTT (no gate); `-1.000` before the first sample;
  - `ackSrttMs=` og-net's smoothed ack round trip to the server, through the link (the Unreal lanes publish it each frame); a
    cross-check only: og-net's ack RTT includes the peer's ack delay. `-1.000` without a reading;
  - `minRttOffsetUs=` the offset of the minimum-RTT sample among the last 8 (NTP's clock filter): the server clock minus the
    client clock, which on a link with unequal one-way delays is off by half their difference (`SimNetClock-rationale.md` §1);
  - `owdVarUs=<up>/<down>` the RFC 3550 interarrival jitter of `t1 − t0` (up) and `t3 − t2` (down), microseconds;
  - `smoothedRttMs=` the estimator's estimate, from the active source, through its gate;
  - `samples=` and `invalid=` the estimator's SimClock sample counts, and `pings=` and `pongs=` the clock client's (through the
    link), all cumulative.
- **Reading it.** SimClock leaves out the server's frame turnaround that the engine's ack round trip carries (og-input-latency
  task P5-27), so on one link `simClockRttMs` is expected at or below `engineRttMs` by about one server frame.

*R0, og-session-runtime task 24:* verified against the code and the transcription test: the window's start, its 10 s rule
(`now - start < kSimNetWindowSeconds` returns), the order of the reads (the estimator's shadow, then the link's ack round trip,
then its clock counts) and the eleven keys (the task's smokes: 11, 5 and 4 lines on client1, client2a and client2b, the same keys
in both dilation modes). **Corrected:** the moved text said the window is timed on `FPlatformTime`; it is the engine's
`nowSeconds`, which the Unreal adapter answers with `FPlatformTime::Seconds()`.

## §6 The server tick and its step's start time (og-time-dilation task 2b)

*Moved from `UESimulationSession-rationale.md` §19.8 by og-session-runtime task 24; the measurements are carried from
og-time-dilation task 2b, not re-measured.*

After every authority step `publishServerTick` publishes the server tick to og-sim-net's SimClock server
(`SimNetClockServer::publishServerTick`), and each pong carries that tick with its age, the server's time from the tick to the
pong (og-sim-net's `SimNetClock-rationale.md` §5). The publish passes the time the tick's step started, and the age is measured
from it:

- **Which tick, which time.** On Jolt the authority steps one tick per `SimulationStepDriver::runAuthorityTick`, also inside a
  batch. Its `beforeTick` releases the inputs for the upcoming tick T through the brawler's binding, which first calls the
  Unreal session's `noteAuthorityStepTime_Step` → `noteAuthorityStepStart`; the step then advances the server clock to T and
  prints `[AuthoritySimulation] tick=T stepSeconds=<s>`; its `afterTick` reaches `afterStep_Step` → `publishServerTick` through
  the binding's `onStepEnded`, where `getServerClock().getTick()` is T. So the publish carries T, and the time that belongs to T
  is the stamp of T's own `beforeTick`. Each tick of a batch has its own stamp.
- **The kept stamp.** `noteAuthorityStepStart` keeps the stamp twice, in `m_releaseStepSeconds` (the input release takes and
  clears it, `takeReleaseStepSeconds`) and in `m_clockTickStartSeconds` (`publishServerTick` takes and clears it on every call,
  then returns on a client or without a clock server). It also hands it to the manager (`noteAuthorityStepSeconds`). On an
  authority with a clock server an `OG_CHECK` states the stamp is there. All three doors run on the runner (`Step`, `Publish`).
- **Why.** A time read at the publish is a step's duration D after the start. The truth the dilation sensor is judged against
  (the lead-split tool's G3, og-time-dilation task 1) is each tick's `stepSeconds`, so with the publish time every client's `Ŝ`
  read D · f low; with the start time that part is 0 by construction (`⛔G-06`). og-simulation's
  `ClockDilationController-rationale.md` §15 is the convention on the sensor's side.
- **How large D is.** D is at most the server's `[SimHost.Window]` `preUs` + `stepUs`. On the brawler's shaped b0 smokes
  (`T8_shadow`, `T8_on`, `T2b_shadow`, 17 windows each) the median window's p50 sum was 70–75 µs (about 0.0045 tick at 60 Hz),
  its p99 sum 115–128 µs, and the largest max sum 237 µs (0.014 tick). ⚠ So og-time-dilation task 8's sensor bias (`bias sHat`
  −0.100, −0.108, −0.040 on `T8_shadow`; −0.059, −0.079, −0.057 on `T8_on`) was **not** D · f: it is 3 to 8 times the worst
  D · f. Task 2b's own smoke (`T2b_shadow`, with the start time) reads −0.048, −0.054 and −0.071. The rest of the bias is outside
  the publish (`ServerTickEstimator-rationale.md` §4); a candidate, not established, is the b0 shaper's in-order clamp
  (og-net's emulated backend releases each packet at the later of its own due time and the previous packet's), which a model
  put at −0.035 to −0.053 tick.
- **The client's t3 is not frame-bound.** The pong's receive time t3 is og-net's, taken when the datagram leaves the socket on
  og-net's own thread on a client; only the finished sample record passes the inbox drained at step start.
- **Off is unchanged.** Only the pong's `serverTickAgeUs` changes (larger by D); under `ClockDilation=Off` nothing reads the
  sensor's estimate (§7.3). The publish runs whatever the mode.

*R0, og-session-runtime task 24:* the call path and the two stamps verified on the tree; `SessionClockTest`'s transcription of
og-time-dilation task 2b's route case (a tick whose step started at 10.000000 s, published at 10.001500 s, a ping received at
10.004000 s and answered at 10.004321 s) reads an age of 4321 µs through the clock, and 2821 with the publish time (seen
failing).

## §7 The dilation stage on a client, and the authority's rate line

*Moved from `UESimulationSession-rationale.md` §22.1 (the sample's server tick), §22.4, §22.5 and §22.9 by og-session-runtime
task 24. The controller, the window and the law are og-simulation's (`ClockDilationController-rationale.md`); the knob and its
proof lines are og-sim-session's `SessionKnobs.h` (`SessionKnobs-rationale.md` §4, §5).*

### 7.1 The server tick on the SimClock sample

`onClockSample` passes og-simulation's `SimClockRttSample` to the estimator with its last two fields: `serverTick`, the server
tick the pong carried, and `serverTickAgeSeconds`, how long after that tick the server answered.
`NetworkTimeEstimator::noteSimClockSample` feeds them to its `ServerTickEstimator`, the stage's sensor. og-sim-net's clock
produces `SimClockRttSample` itself (og-session-runtime task 20) and writes `kNoServerTick` into a pong sent before the server's
first tick.

### 7.2 The stage: the thread, the two clocks and the window line

The controller and the window (`m_clockDilation`, `m_clockDilationWindow`) are the clock's members, emplaced by og-sim-session's
`applyLatencyKnobs` (through `dilationController()` and `dilationWindow()`) on every client session, `Off` included, and on no
authority. `beforePredictionPump` returns at once without them or in `Off`. In `Shadow` and `On` it calls
`clockDilation::beforeClientPump(rate, hostNowSeconds, clockNowSeconds, manager, controller, window)` once per frame; in `On` the
stage sets the door's scale itself.

- **The thread.** A client's scheduler is owned by the frame thread, which runs `startFrame` and so `beforePump`: a client steps
  on a worker, but the pump is the frame's (`RateDoor`, the client's frame row). The controller and the window are touched only
  there. What the stage reads from other threads is og-simulation's contract: the estimator's and the sensor's atomics, written
  by the drain, the clock's published correction phase, and the door's tick offset.
- **The two clocks** (`⛔G-05`). `hostNowSeconds` is the driver's host time, which places the prediction tick against the
  scheduler's deadlines; on a client the driver runs `timeBase=worldDelta`. `clockNowSeconds` is the engine's `nowSeconds`, read
  once per call: the clock og-net stamps receive times with, and so the clock the sensor's samples are on. The window is timed on
  it as well.
- **The window line.** The stage notes every call in the window. When `isDue(clockNowSeconds)` (10 s since the window's start,
  og-simulation's `clockDilation::kWindowSeconds`), the clock logs the window's own `formatLine()` at `Warning`, through the
  engine's log routes behind a `[Warning]` token (`kDilationWindow`), and calls `restart()`. Its fields are og-simulation's, and
  og-time-dilation's `lead_split.py` parses it. Measured on og-time-dilation task 8's Shadow smoke: a window every 10.00 s on every
  client, `calls=1000`, `applied=0` and `rateScale=1.00000/1.00000` in every window, and no window on the server.

### 7.3 What `Off` keeps

In `Off` the stage is never called: no estimator read for it, no window, no `[Dilation.Window]` line. The one runtime change on
an `Off` client is the two sample fields (§7.1), which feed the sensor; the stage is its only production reader. Measured at
og-session-runtime task 24: with `-ini:Engine:[OGNetcode]:ClockDilation=Off` no client logs `[Dilation.Window]` (smokes
`SR4_t24a_off_smoke`, `SR4_t24b_off_smoke`), and the server logs `[Dilation.Authority]` as in `On`.

### 7.4 The authority's refused rate changes: `[Dilation.Authority]` (og-time-dilation task 11)

og-simulation's tick driver refuses `setRateScale` on an authority's door and counts each refusal in a plain field written and
read on the scheduler's owning thread with no synchronisation: on the authority loop that thread is the runner, so a read from the
frame thread would race the runner's writes. `noteAuthorityRateDoor` reads it through the door the driver hands `beforePump`
(`TickRateDoor::refusedRateChanges()`, the same field) and logs at `Warning`:

`[Dilation.Authority] refusedRateChanges=<n> rateScale=<s>`

- **When:** at the first pump it sees, then every `clockDilation::kWindowSeconds` (10 s) of the driver's host time, and at the
  next pump whenever the count has changed since the last line (`SessionClock.TheAuthorityRateLineIsDueAtTheFirstPumpEveryWindowAndOnARefusedChange`).
  `rateScale` is the authority scheduler's scale, which a refused door leaves at 1.0.
- **The thread holds by construction:** the clock keeps no reference to the driver, and a `TickRateDoor` exists only inside the
  driver's pump, on the owning thread (the runner; `RateDoor`'s authority row).
- Nothing in the session calls `setRateScale` on the authority, so the count stays 0; the line is the evidence.

*R0, og-session-runtime task 24:* verified on the tree and in the task's smokes (the server's 13 and 15 lines, the same two keys,
in both modes). **Corrected:** the moved §22.5 said the window line is "logged directly, like the proof line, not through
`RouteOGMessage`"; since task 24 it goes through the engine's `logRouted` (the Unreal adapter's `RouteOGLevelLine`), with the same
text, category (`LogOGNet`) and level, a route row `[Dilation.Window]` added for it.

## §8 The lines: one format each, a level token, the engine's routes

Each `UE_LOG(LogOGNet, <Level>, …)` of the moved code is one format in `sessionClockLines`, formatted by og-sim-session's
`sessionKnobsLines::formatLine` (`snprintf` sized to the line, `SessionKnobs-rationale.md` §6) and handed to
`engine.logRouted(line)`. A `Warning` line carries a leading `[Warning]` token, a `Log` line none; the Unreal adapter's
`RouteOGLevelLine` reads the token for the level, routes the body by its tag through the route table and prints the body
without the token (`UESimulationLogRoutes-rationale.md` §2.1). The route table's adapter block gained `[ConnectionTier]`,
`[ClockSource.Window]`, `[Dilation.Window]` and `[Dilation.Authority]` (`LogOGNet`, plain level `Log`) with task 24;
`[RelayDelayFloor]` was task 23's. A `static_assert` in `UESimulationSession.h` checks each format's category and level.

| format | level | tag |
|---|---|---|
| `kConnectionTierChange` `[ConnectionTier] tier %u -> %u, effective delay delta=%d` | Log | its own |
| `kNoTransition` `%s floor = %d ticks, effective delay delta=0, no stall or skip` | Log | `kConnectionTierTag` or `kRelayDelayFloorTag`, the `%s` |
| `kTransition` `[Warning]%s floor = %d ticks, %s, requesting %d-tick prediction %s` | Warning | the same; `kUpward`/`kStall` or `kDownward`/`kSkip` |
| `kRelayFloorClamped` | Warning | `[RelayDelayFloor]` |
| `kRelayFloorBelowHiccupBaseline`, `kRelayFloorUniformD` | Warning, Log | `[RelayDelayFloor]` |
| `kAppliedInputDelay` | Log | `[ConnectionTier]` |
| `kClockSourceWindow` | Warning | `[ClockSource.Window]`; `kSourceSimClock` or `kSourceEngine` |
| `kDilationWindow` `[Warning]%s` | Warning | the window's own `[Dilation.Window]` |
| `kDilationAuthority` | Warning | `[Dilation.Authority]` |

The old `TEXT` literals' em dash is `\xE2\x80\x94` in the narrow formats (UTF-8; `RouteOGLevelLine` decodes it), `%s` of a
`TCHAR*` is `%s` of a `char*`, and `%hs` of the window's buffer is `%s` behind the token.

## §9 The tests

- **`SessionClockTest.cpp`** (`[Session][SessionClock]`, og-sim-session's tests): a transcription of the eighteen functions as
  task 24 (a) committed them (OGSimulation `dae3018`, `SessionClock.h`, each function's lines cited), with `UE_LOG` a recorder,
  `FPlatformTime` the fake engine's time, against `SessionClock` over the same fakes (a manager with a real `NetworkTimeEstimator`
  behind a recording wrapper, a recording input resolution, link and clock server). Each scenario compares the event sequence
  (lines with their token, estimator calls, transition requests, publishes, floor sets, step stamps, server-tick publishes, the
  display tier reading after each receipt): a tier change on both roles at five floors; a floor change up and down with and
  without a tier; a clock sample then an authority tick with each RTT source, through the window; the window's due rule on both
  roles; the authority's step start then the server tick's publish; the authority's rate line; the stopped drain; the advisory
  and the tier cache at start-up. 32 scenarios, 877 compared events, every format and event kind present (the
  non-vacuity case). The client's step doors run inside a `StepScope` of a configured client context, the authority's on the
  thread that configured its context, so every door's check is exercised. And og-time-dilation task 2b's case through the clock
  and a real `SimNetClockServer` (§6).
- **`SessionKnobsFormatParityTest.cpp`** (og-simulation-tests' root, `SessionClockFormatParity.…`): each of the ten formats
  through `FString::Printf` of the old `TEXT` literal and through `formatLine` of the new constant, byte for byte after the
  token, the token checked against the old level: 2,016 lines, the two transition formats with each tag, delta 0, +n and −n.
- **`UESessionClockLineRouteTest.cpp`** (UE automation `OGBrawler.Session.ClockLineRoute`): 14 lines logged the old way and
  through `UESessionClockEngine`, captured on `GLog`: equal text, category and level.
- **`SessionThreadsTest.cpp`**: the `DrainStopped` row (§3) allowed on the client's frame, forbidden in a step and on the
  authority; hidden `SessionThreads.DrainStoppedOnTheAuthorityTripsTheCheck`.
