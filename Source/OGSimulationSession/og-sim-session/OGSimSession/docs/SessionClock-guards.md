<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: onRelayDelayFloorReplayed -- deleted by og-input-latency task P5-9 (the relay actors, their listeners and the session's two replay members); kept in G-02's history -->
<!-- lint-external-ref: OnRep -- Unreal Engine symbol, outside every scan root; kept in G-02's history -->
# `SessionClock.h` — guards

Prohibitions with a tagged site in og-sim-session's `OGSimSession/SessionClock.h`. Ids are opaque, stable, and retired rather
than reused. The reasoning lives in `SessionClock-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

Every entry here moved from `UESimulationSession-guards.md` (og-simulation-unreal) with the code it marks, by og-session-runtime
task 24: the clock routing and the schedule left the Unreal session for this engine-free header. Each entry names its old id;
those ids are retired there with this file's id as their successor. What the compiler or a check holds is not here: each door's
thread (its first statement, `SessionThreadChecks::checkOp`, an `OG_CHECK` against `SessionThreads.h`'s table), the published
server tick's start time being there on an authority (an `OG_CHECK` in `publishServerTick`), the ports (three concepts with
their negatives), and, in the Unreal adapter, each line's category and level (a `static_assert` in `UESimulationSession.h`) and
the clock's destruction before the manager (a `compositionContractsHold()` row there).

---

## G-01 — `requestInputDelayTransition` keeps its `runsPrediction()` guard

**Tag site:** `SessionClock.h`, directly above the `if (!m_manager.runsPrediction())` early return in
`requestInputDelayTransition`.

**The prohibition.** ⛔ Do not remove or weaken the early return: the transition request reaches the manager's client clock
only on a manager that predicts.

**What breaks if the edit is made.** `SimulationManager::editClientClock()` calls `std::terminate()` when the manager holds
no client clock, which is every authority and standalone manager. Remove the guard and the first call that reaches a
manager that does not predict ends the process: not a wrong number, a dead server.

*R0, og-ue-simulationcomponentandmanagersplit task 25:* verified against `SimulationManager.h` (`editClientClock` and
`getClientClock` both terminate on an empty client clock). The fence the entry was converted from named `getClientClock()`;
the call this guard protects is `editClientClock()`, as the brawler's task 11 already noted.

*R0, og-session-runtime task 24:* verified on the tree. `SimulationManager` emplaces its client clock only when it runs
prediction (`SimulationManager.h`, the constructor's `shouldRunPrediction` branch), and `editClientClock` terminates on an empty
one. The clock holds a reference to its manager, which exists for the clock's whole life, so the old guard's first operand
(`!m_manager.has_value()`) went with the move; the guard is the `runsPrediction()` test alone. The authority's
`payInputDelayTransition` reaches it on every non-zero change of a floor it receives (none in production: the authority's floor
comes from its ini through `applySessionKnobs`, which does not pay), and the tests drive the authority through it.

*Was `UESimulationSession-guards.md` G-03 (moved by og-session-runtime task 24); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-03 (og-netcode-v2-field-defects task 11), moved by
og-ue-simulationcomponentandmanagersplit task 25.*

---

## G-02 — the relay-delay-floor advisory is ADVISORY: never an assert

**Tag site:** `SessionClock.h`, directly above `void logRelayDelayFloorAdvisory(int32_t floorTicks)`.

**The prohibition.** ⛔ The advisory reports; it never asserts, `ensure`s or `check`s. Its contract is a log line.

**What breaks if the edit is made.** Floor `0` is the documented "scheduled regime OFF" mode, and the function is called
from every intake point of the floor: the ini override at start-up (og-sim-session's `applySessionKnobs`, which the Unreal
session's `beginPlay` hands a lambda that calls this function) and the floor's receipt (`onRelayFloor`). An assert here would
fire on the shipped default configuration. `classifyRelayDelayFloor` (og-simulation's `Network/ConnectionTierTable.h`) never
flags `0`; this function only reports what that classification says. The same prohibition is guarded inside the body, at the
classification `switch`, by **G-04**: the two tags sit where the two different wrong edits would be typed (one would change the
function's contract, the other would assert in its body).

*R0, task 25:* the moved text said "both intake points — the ini override and the `OnRep`". The floor listener had two
entry points then, the change and the replay (`onRelayDelayFloorReplayed`), and both reached this function through
`applyReplicatedRelayDelayFloor`, so there were three call paths over two intake sources.

*R0, og-input-latency tasks P5-7 and P5-9:* the client's floor intake is og-net's since P5-7, and P5-9 deleted the replay entry
point. Two intake sources and two call paths since.

*R0, og-session-runtime task 23:* the ini override's intake is og-sim-session's `ogsession::applySessionKnobs`.

*R0, og-session-runtime task 24:* verified on the tree. The callers are `applySessionKnobs` (through the Unreal session's
lambda, after the floor's ini line) and `applyReplicatedRelayDelayFloor` (after the clamp line and the manager's
`setRelayDelayFloorTicks`), reached from the `onRelayFloor` door. The old body's first statement, an early return without a
manager, went with the move (the clock always has one).

*Was `UESimulationSession-guards.md` G-04 (moved by og-session-runtime task 24); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-15, moved by og-ue-simulationcomponentandmanagersplit task 25.*

---

## G-03 — ONE recompute publishes the effective input delay, over BOTH inputs

**Tag site:** `SessionClock.h`, directly above `int32_t recomputeAndPublishEffectiveInputDelay()`.

**The prohibition.** ⛔ One site, both channels: no other code writes the effective-input-delay atomic (the input-resolution
peer's `setClientEffectiveInputDelayTicks`), and no caller derives half of the formula itself.

```
effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
                                 : rttTierInputDelays[kMaxConnectionTierIndex])
```

**What breaks if the edit is made.** The floor and the tier are two independent og-net Control messages, and they land in
either order. Two writers each holding half the formula answer with a stale other half whenever they interleave. Every
publish (the tier change, the floor change and the start-up baseline in `beginTierConsumer`) goes through this one function,
which reads both cached inputs through `ReplicatedTierConsumer::effectiveInputDelayTicks`, the same lookups
(`tierInputDelayTicks`, which floors, and the floored worst-tier fallback) that the server's
`ServerInputDelayQueue::effectiveDelay` uses for the same connection. Its return value is the change in published delay, which
is how a caller that must pay for a change can (both receipts do, either sign: `⛔G-07`).

*R0, task 25:* verified against `ReplicatedTierConsumer.h` and `ServerInputDelayQueue.h`; "the same derivation the
server's queue mirrors" is exact in the sense above (both ends call the same shared lookups), not a shared function.

*R0, og-session-runtime task 24:* verified on the tree. The clock writes the atomic only here, through the input-resolution
peer it holds (`SimulationInputResolution::setClientEffectiveInputDelayTicks`, an atomic store); the Unreal session's
`publishClientEffectiveInputDelayTicks`, the only other writer's name, was the clock's own and went with the move. The callers
are `beginTierConsumer`, `applyReplicatedConnectionTier` and `applyReplicatedRelayDelayFloor`, as before.

*Was `UESimulationSession-guards.md` G-05 (moved by og-session-runtime task 24); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-16, moved by og-ue-simulationcomponentandmanagersplit task 25.*

---

## G-04 — the relay-delay-floor advisory is a log line, never an assert

**Tag site:** `SessionClock.h`, the `switch (classifyRelayDelayFloor(...))` in `logRelayDelayFloorAdvisory`.

**The prohibition.** ⛔ ADVISORY ONLY, never an assert: floor `0` is scheduled-regime-OFF, and `classifyRelayDelayFloor`
never flags it.

**What breaks if the edit is made.** An assert on `BelowHiccupBaseline` takes a server down on a legal configured floor of
`1` (the classification's one value below the hiccup baseline that is not off, unless the worst tier's delay is itself
`1` or less, when `1` is uniform-D).

**The same prohibition, at the declaration:** G-02 carries it at the declaration of `logRelayDelayFloorAdvisory`.

*R0, task 25:* verified against `classifyRelayDelayFloor` (`<= 0` is `None`, uniform-D is tested first, then `== 1`).
*R0, og-session-runtime task 24:* re-read on the tree (`ConnectionTierTable.h`): the same three tests in the same order.

*Was `UESimulationSession-guards.md` G-09 (moved by og-session-runtime task 24); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-66, moved by og-ue-simulationcomponentandmanagersplit task 25.*

---

## G-05 — the dilation stage's sensor clock is the engine's clock, never the driver's `hostNowSeconds`

**Tag site:** `SessionClock.h`, `beforePredictionPump`, directly above `const double clockNowSeconds = m_engine.nowSeconds();`.

**The prohibition.** ⛔ Do not pass `hostNowSeconds` as the stage's `clockNowSeconds` (the third argument of
`clockDilation::beforeClientPump`), and do not replace this read with any clock other than the one og-net stamps receive times
with. The stage takes two times on purpose: `hostNowSeconds` places the prediction tick against the scheduler's deadlines, and
`clockNowSeconds` is the time at which the server-tick sensor is read and the window is timed. **The engine's `nowSeconds` is a
port requirement:** it must be og-net's receive-stamp clock (the Unreal adapter's `UESessionClockEngine` returns
`FPlatformTime::Seconds()`).

**The consequence.** The sensor's samples are stamped with og-net's clock: og-net's Unreal host clock is
`FPlatformTime::Seconds()`, and the SimClock client reads `t0` from the clock that stamps `t3` (a `static_assert` in og-sim-net's
`SimNetClock.h` says so). A client's `hostNowSeconds` is a different clock: its driver runs `timeBase=worldDelta`, so it is the
sum of the world's frame deltas since the driver was built (og-simulation's `SimulationTickDriver.h`), with its own origin, and
og-time-dilation task 9's `og.Sim.TestClientClockRatePpm` scales those deltas. Read at that time, the sensor's estimate of the
server tick is wrong by the difference of the two origins (on og-time-dilation task 8's smoke host `sHatClock` read 16,833,176 s
against a host time of under 200 s), so every lead and error on `[Dilation.Window]` is meaningless, and under `On` the law
steers the rate with it. The drift gate is lost as well: the emulated clock-rate offset moves `hostNowSeconds` and the engine
clock apart, which is exactly what the sensor has to see.

**What breaks if the tag moves.** Nothing mechanical: both arguments are `double` seconds. A Shadow run still prints windows,
with nonsense in `lead`, `err` and `sHat`.

*R0, og-time-dilation task 8:* written with the code. Verified: `FOGNetUnrealHost::nowSeconds` and `FUEPlatformClock::nowSeconds`
both return `FPlatformTime::Seconds()`; the client's `[SimHost.Frame]` line reads `timeBase=worldDelta` on the task's Shadow
smoke (`T8_shadow`), the server's `timeBase=monotonic`.

*R0, og-session-runtime task 24:* verified on the tree: `FOGNetUnrealHost::nowSeconds` (OGNetUnreal's `OGNetUnrealHost.h`) and
`UESessionClockEngine::nowSeconds` (`UESimulationSession.h`) both return `FPlatformTime::Seconds()`, which was the line this tag
marked before the move.

*Was `UESimulationSession-guards.md` G-38 (moved by og-session-runtime task 24; og-time-dilation task 8).*

---

## G-06 — the SimClock server's tick is published with its step's start time, never a time read at the publish

**Tag site:** `SessionClock.h`, `publishServerTick`, directly above
`clockServer->publishServerTick(static_cast<uint32_t>(m_manager.getServerClock().getTick()), *tickStartSeconds);`.

**The prohibition.** ⛔ Do not replace the second argument, `*tickStartSeconds`, with `m_engine.nowSeconds()` or any other
time read in `publishServerTick`. It is the stamp `noteAuthorityStepStart` took at the start of the same authority step, the
value that step's `[AuthoritySimulation] tick=<t> stepSeconds=<s>` line prints for the same tick (rationale §6).

**The consequence.** og-sim-net's SimClock server measures each pong's `serverTickAgeUs` from this time, and a client's
server-tick estimator places the tick at `t2 − age`. The door runs after the step, so a time read here is later than the step's
start by the step's duration D, and every client's `Ŝ` then reads D · f ticks low against the server's own step-start
timeline, the truth the G-DIL-0 sensor check compares it with. D grows with the server's load: on the brawler's smokes it is
typically about 0.075 ms (0.0045 tick at 60 Hz) and at most 0.24 ms (0.014 tick; rationale §6), and a heavier step moves every
client's estimate by its whole duration. Under `ClockDilation=On` the controller would hold each client's true lead D · f below
its target.

**What breaks if the tag moves.** Nothing at compile time: both are `double` seconds on the same clock. `SessionClockTest`'s
transcription of og-time-dilation task 2b's case reads 2821 µs instead of 4321 (seen failing, og-session-runtime task 24); a
Shadow smoke shows it as a negative `bias sHat` in the lead-split tool's G3 sensor check.

*R0, og-time-dilation task 2b:* written with the code. Verified: on Jolt the authority steps one tick per
`SimulationStepDriver::runAuthorityTick`, whose `beforeTick` reaches `releaseInputsForAuthorityTick` (the stamp) and whose
`afterTick` reaches `onStepEnded` → the publish; `getServerClock().getTick()` there is the tick that step's
`[AuthoritySimulation]` line printed. D's bound is the server's `[SimHost.Window]` `preUs` + `stepUs` on the `T8_shadow`,
`T8_on` and `T2b_shadow` smokes: median p50 sum 70–75 µs, largest max sum 237 µs. ⚠ Task 8's measured sensor bias (−0.04 to
−0.11 tick) is **not** D · f (rationale §6).

*R0, og-session-runtime task 24:* the call path on the tree: the brawler's `releaseInputsForAuthorityTick` calls the Unreal
session's `noteAuthorityStepTime_Step`, which calls `noteAuthorityStepStart`; its `onStepEnded` calls `afterStep_Step`, which
calls `publishServerTick` with the lanes' clock server (`BrawlerTickDriverGameUImpl.h`, `UESimulationSession.h`). The
measurement history above is carried from og-time-dilation task 2b, not re-measured.

*Was `UESimulationSession-guards.md` G-39 (moved by og-session-runtime task 24; og-time-dilation task 2b).*

---

## G-07 — the tier receipt pays the recompute's change, either sign, its first arrival included

**Tag site:** `SessionClock.h`, `onConnectionTier`, directly above
`payInputDelayTransition(sessionClockLines::kConnectionTierTag, deltaDelayTicks, ...)`.

**The prohibition.** ⛔ Do not gate this payment on the tier indices (`oldTier == newTier`, `tierDelayDeltaTicks(oldTier,
newTier)`), on whether a tier had arrived before (`hasReceivedTier()`, the retired `hadAnyTier`), or on the sign of the
change, and do not compute the change any other way than the recompute's return value.

**The consequence.** A first arrival reports `oldTier == newTier == 0` on a LAN wire, and its change is the fallback's
(`max(floor, 4)`) to the tier's delay: −1 at floor 3, −2 at floor 2. The server made that same change at its first sample of
the wire. Skip the payment and, if the server had parked any of the client's inputs at the fallback first, its drain stays
that many ticks late for the session (`[Release] late=1` on every release, the shape og-simulation-tests'
`Network/DelayTransitionTest.cpp` pins), and every relay stamp disagrees with the tick the authority applies. A gate on the
sign brings back the pre-P6-2 defect for every lowered delay; a gate on the indices or on a prior arrival drops the first
arrival, which at floors below 4 is the one transition every LAN join makes.

**What breaks if it moves.** It must follow `applyReplicatedConnectionTier`, whose return value it pays; above it there is no
change to pay. `SessionClockTest`'s tier-change order test fails if the payment moves above the recompute (seen failing,
og-session-runtime task 24); og-simulation-tests' `Network/DelayTransitionTest.cpp` tier cases model the arithmetic, and the
loopback smoke at floors 3 and 2 shows the `DOWNWARD … skip` line.

*R0, og-input-latency P6-2:* written with the code.
*R0, og-session-runtime task 24:* verified on the tree: `onConnectionTier` calls `applyReplicatedConnectionTier`, logs the
`[ConnectionTier] tier` line, then pays; `payInputDelayTransition` requests a transition only for a non-zero change.

*Was `UESimulationSession-guards.md` G-40 (moved by og-session-runtime task 24; og-input-latency P6-2).*

---

## §R — Retired ids

None.
