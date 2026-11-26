<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: getTimeConfigPtr -- deleted by og-session-runtime task 31 (no caller); kept in the history -->
<!-- lint-external-ref: m_onTimingInfoReceivedCallback -- deleted by og-session-runtime task 31 (no caller); kept in the history -->
<!-- lint-external-ref: m_registeredWorld -- moved by og-session-runtime task 31 to UESessionRegistry and UESessionNoSession (UESessionRefusal.h); kept in the history -->
<!-- lint-external-ref: m_registeredAsAuthority -- moved by og-session-runtime task 31 to UESessionRegistry and UESessionNoSession (UESessionRefusal.h); kept in the history -->
<!-- lint-external-ref: setOnTimingInfoReceivedCallback -- deleted by og-session-runtime task 31 (no caller); kept in the history -->
<!-- lint-external-ref: getReceptionCoordinator -- deleted by og-session-runtime task 31 (no caller); kept in the history -->
<!-- lint-external-ref: onSimNetLanesStopping -- moved by og-session-runtime task 31 (the session's composition, start and stop are og-sim-session's SessionRuntime; UESimulationSession is the thin Unreal adapter); kept in the history -->
<!-- lint-external-ref: forgetSimNetCharacter -- moved by og-session-runtime task 31 (the session's composition, start and stop are og-sim-session's SessionRuntime; UESimulationSession is the thin Unreal adapter); kept in the history -->
<!-- lint-external-ref: knobLog -- moved by og-session-runtime task 31 (the session's composition, start and stop are og-sim-session's SessionRuntime; UESimulationSession is the thin Unreal adapter); kept in the history -->
<!-- lint-external-ref: applyTierTransitionStall -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
<!-- lint-external-ref: hadAnyTier -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
<!-- lint-external-ref: shouldStallForTierTransition -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root (retired G-19) -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root (retired G-19) -->
# `UESimulationSession.h` — guards

Prohibitions with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulationSession.h`. Ids are opaque,
stable, and retired rather than reused. The reasoning lives in `UESimulationSession-rationale.md`. The class's one
`.cpp`, `UESimulationSession.cpp` (since task 27a: the test hitch cvar, its accessor and its sleep; since og-input-latency
task P5-32 also the hitch's delay cvar and its marker line; rationale §10), shares this pair and carries no tag.

**If this file and the source disagree, the source is authoritative and this file is stale.**

**What the compiler checks instead (og-session-runtime task 33, the adapter task 31 left).** The constructor asserts
`adapterContractsHold()`: the member order (the latency diagnostics, the frame host and the presentation before the runtime,
the runtime before the bodies), the runtime's channel type, the game's six instance and static hooks
(`ueSimulationSessionHookNames::GameInstanceHooks`) and that no member shares a hook's name
(`ueSimulationSessionHookNames::SharesNoHookName`); `beginPlay` `checkf`s the step length against the tick rate, and
`findDriverSimNetLanes` the lanes' role (rationale §0.1, §0.2). The composition's order, the executors, the owner
traits and the client's bind order are the runtime's (`SessionRuntime.h`'s `compositionContractsHold()`,
`SessionRuntime-guards.md`). Until task 31 this paragraph listed them as this class's (`compositionContractsHold()` here).

Three entries are live (G-02, G-10, G-30); §R lists every retired id, and `UESimulationSession-rationale.md` §R
lists the successors of those tasks 23 to 31 retired.

---


## G-02 — `getVizSlice` reads the VIZ snapshot, never the live state

**Tag site:** `UESimulationSession.h`, directly above the `return vizState->getState().template get<SliceT>();` statement in
`getVizSlice` (until og-input-latency task P5-16, the `return m_storage.template get<Simulatable>(id).getVizState()...`
statement).

**Today (og-session-runtime task 33).** The tagged statement reads the runtime's held display:
`m_runtime.findDisplayVizState(id)` (og-sim-session's `SessionDisplay`, since task 25; `SessionDisplay-rationale.md` §4),
and the prohibition below holds unchanged: never the storage.

**Since og-input-latency task P5-16.** The prohibition now reads: `getVizSlice` answers from the game-thread display copy
(`findDisplayVizState`, the held display snapshot's state with the rendered poses overlaid, `UESimulationSession-rationale.md`
§17), and ⛔ never from the storage: neither a simulatable's `getAllState()` nor its `getVizState()`. What breaks: the storage's
state is what the step writes, so a read from the game thread is the crossing the snapshot removed; and nothing refreshes the
storage's visualization copy any more (the backend's refresh was replaced by the overlay on the session's copy), so
`getVizState()` would answer with the copy's value-initialised state (the brawler's `m_vizState()`). The paragraphs below are the entry as it stood
before the task: the "visualization copy" they name was the storage's.

**The prohibition.** `getVizSlice<SliceT>` answers from the simulatable's visualization copy, `getVizState()`. ⛔ Never
from `getAllState()`, the live simulation state, and the accessor offers no route to it.

**What breaks if the edit is made.** The visualization copy is taken on the game thread once per pass of
`onPostPhysicsStep` (the owner's post-physics step until task 27a), by the backend's `updateVisualization_GameThread`, on
the statement directly above the game's post-visualization hook (the brawler's score push). That copy is the sanctioned physics-to-game handoff. The live state
is what the physics thread writes while it steps, so a game-thread display reading it through this door would be a
cross-thread read that no crossing argument covers: the game's crossing table would gain an unlisted entry. The
accessor's other two properties, `const` and returning **by value**, are not prose: the game asserts them on the
instantiation it uses, beside its caller (the brawler's `ScoreboardVisualizationUImpl.cpp`, for `brawlerRingout::State`).

*R0, og-ue-simulationcomponentandmanagersplit task 24b:* the moved text said the copy is
`updateVisualizationAll(m_storage)`, which "takes `m_vizState = m_allState`". Both hosts do call og-simulation's
`updateVisualizationAll` on the session's storage, which calls each simulatable's `updateVizState()`, but the copy itself
is the game simulatable's (the brawler's `SimulatableBrawler::updateVizState`), and on the driver-scheduled (Jolt) host
the presenter then writes the rendered body poses into the copy's body states (`TickDriverPresenter`'s
`updateVisualization_GameThread`). A slice that is not a body state, such as the brawler's ring-out state, is the plain
copy in both configurations.

*Moved from `OGBrawlerSimulationManager-guards.md` G-04 (og-netcode-v2-field-defects task 11, on `getRingoutVizState`;
`getVizSlice` since og-ue-simulationcomponentandmanagersplit task 22) by og-ue-simulationcomponentandmanagersplit task
24b, with the accessor it marks; the text is that entry's, made game-neutral, with the R0 line above.*

---






## G-10 — the destructor touches no `UObject`: it clears the static instance table and unbinds from the og-net lanes, nothing else

**Tag site:** `UESimulationSession.h`, the first statement of `~UESimulationSession` (the `unregisterInstance()` call; the
`s_instances[0]` test until og-session-runtime task 16b).

**Today (og-session-runtime task 33).** The destructor is two statements: `unregisterInstance()`, which clears this
session's slot in `UESessionRegistry`'s static tables (`s_instances`, `s_started`; `UESessionRefusal.h`), and
`m_runtime.detachLink()`, which unbinds the runtime from the og-net lanes, a plain C++ object the driver owns. Neither
reaches a `UObject`. The prohibition is unchanged; where the text below says `detachFromSimNetLanes()`, read
`m_runtime.detachLink()` (task 31), and where it says `endPlay` unbinds, read the runtime's `stop` (`SessionRuntime-rationale.md`
§3), which `endPlay` calls.

**The prohibition.** ⛔ Do not call `GetWorld()` on the owner, or any other method that reaches the owner actor's outer,
its world or another `UObject`, from the destructor. Unbinding from engine objects belongs in `endPlay`, which the owner's
`EndPlay` calls for every world teardown while the world still exists (until og-session-runtime tasks 3 and 5 the former
self-scheduled backend removed its post-tick handle and its two solver callbacks there, in its world-teardown entry point).

**What breaks if the edit is made.** The session is a member of its owner actor, so its destructor runs inside the owner's,
inside the garbage collector's purge, after the owner's outer level may already have been destroyed. The brawler's owner
destructor once called `GetWorld()` to unbind a physics-scene delegate, and that read freed memory: a packaged Development
client crashed with an access violation in the engine's outer lookup under the actor's world getter under that destructor, called from the
engine's map load → memory trim → garbage collection (measured 2026-09-29, og-brawler-uploadtosteam task 12, the first
travel from the standalone join-screen world to a server). An editor `-game` run of the same travel did not crash, so
only a packaged client shows it.

**What breaks if the tag moves.** The tag marks the destructor's body. A `GetWorld()` typed above the tagged statement is
typed directly under the tag. Nothing mechanical checks the destructor: the crash depends on the collector's purge order.

*R0, task 25:* the self-scheduled host's teardown was the brawler's Chaos host's world-teardown entry point, which
removed the inject-inputs and post-tick handles and unregistered the solver callback object; the owner called it from its
`EndPlay` (og-session-runtime task 3 deleted that host, and task 5 the entry point). The registry's unregistration in the owner's `EndPlay` is unchanged; the destructor is its backstop for an
owner destroyed without `EndPlay`. *Since task 27a* both are statements of `endPlay` (rationale §10), which the owner's
`EndPlay` calls; the destructor is still the backstop.

*R0, og-input-latency task P5-7:* the destructor no longer unregisters relay listeners (the session implements none); its
last statement is `detachFromSimNetLanes()`, which reaches the og-net lanes object the session is still bound to (a plain C++
object the driver owns, not a `UObject`) and nothing once `endPlay` or the lanes' stop has unbound it.

*Moved from `OGBrawlerSimulationManager-guards.md` G-79 (og-brawler-uploadtosteam task 12) by
og-ue-simulationcomponentandmanagersplit task 25: the owner registry moved here, and the owner's destructor, whose whole
body this was, became the session's.*

---

## G-30 — the pawn's unregistration returns at once for an id already unregistered by a roster Remove or the lanes' stop

**Tag site:** `UESimulationSession.h`, directly above `if (m_simUnregisteredIds.erase(id) != 0u) return;`, the first statement
of `unregisterFromNewFramework` after its no-session return (og-session-runtime task 11, `UESimulationSession-rationale.md`
§23; the hook check that preceded it is the compile-time `GameInstanceHooks` assertion since task 31).

**Today (og-session-runtime task 33).** The ids are recorded by `onPredictionOutcome_GameThread` (a client's roster Remove,
the outcome `RosterRemoved`), `teardownComponents` (a client's teardown) and `onLinkStopping` (the link's stop; it was
`onSimNetLanesStopping`); `forgetComponents`, which the runtime's `stop` calls through the port, clears the set with the
maps (it was `endPlay`'s clear under G-21, now `SessionRuntime-guards.md` G-08's list), and the same `endPlay` clears the
registry slot after `stop`, so no `instanceFor` lookup reaches this function for a cleared id. The prohibition is
unchanged.

**The prohibition.** ⛔ Keep this early return, and keep it first (after the no-session return, which runs only in a
world that registered nothing). A client's roster Remove of a registered id
(og-sim-session's `SessionLifecycle::onRegisteredRemoved` on the step, its host work finished and the id recorded by
`onPredictionOutcome_GameThread` since og-session-runtime task 29; `onSimNetRegisteredRemoved` and
`collectLifecycleOutcomes_GameThread` before, since task 12) and the og-net lanes' stop (`onSimNetLanesStopping`) unregister a character before its pawn
ends play, and each records the id; the pawn's `EndPlay` then calls this function for the same id, and must do nothing. Do not
replace the set by a test of the registered map (a pending, never-registered pawn is in neither map and must still run the
whole sequence: its bodies exist), and do not erase the id anywhere else. `endPlay`'s clear of the whole set (G-21's list, since
og-input-latency task P5-9) is not such an erase: the same `endPlay` clears this session's registry slot (`s_instances`), and a
pawn's `EndPlay` reaches this function only through that registry (`instanceFor`), so no call for a recorded id follows the
clear.

**What breaks if the edit is made.** The id is unregistered twice: the game's roster and input-history forgets run twice, the
backend's unregistration runs a second time for bodies it already removed, and `NewFramework: unregistered simulatable id=`
prints twice for one leave (Ph3's leave check reads one line per leave, P7's C-4).

**What breaks if the tag moves.** Nothing mechanical: the double call needs a leave on a client whose Remove arrives before
the pawn's destruction, or a driver that stops before its world ends.

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-07 — RETIRED by og-input-latency task P5-7 (2026-11-18): became a `checkf`

**Was:** *"the client binds its relay listeners, then PULLS the latched tier and floor"*. Its tag sat in `UESimulationSession.h`,
directly above the client branch's call that pulled the latched tier and floor from the connection and timing relays.

**Why it retired.** Its subject moved. Since P5-7 the client session in the og-net driver's lanes latches the floor and the
tier, and `bindClientSession` replays them in one call, which binds the host before it replays (`UESimNetLanes-rationale.md`
§5). What the session still has to order is that call after the manager and the tier consumer exist, since each replayed value
applies to one of them; that is a `checkf` in `beginClientSimNet`, whose message names this guard.

### G-12 — RETIRED by og-input-latency task P5-7 (2026-11-18): its member was deleted

**Was:** *"the relay write probe's capacity is the CODEC CONSTANT `relayedInputRing::kMaxDepth`"*. Its tag sat above PROBE 5's
member.

**Why it retired.** PROBE 5 counted the writes of the session's relay sink, and the session has no relay sink since P5-7:
og-sim-net's server stages each relayed input into its own stager, which reports its own window (`[RelayFlush]`). The probe and
its `[RelayProbe.Write]` lines are gone (rationale §16.3).

### G-29 — RETIRED by og-input-latency task P5-7 (2026-11-18): its subject was deleted

**Was:** *"the correction-tick loop skips a component whose binding is not bound"*. Its tag sat above the skip in the session's
correction-tick walk (forEachAuthorityCorrectionTick, which og-session-runtime task 14 deleted with the in-frame server).

**Why it retired.** The walk read og-sim-net channels from P5-7, which have no binding to lose: a channel existed exactly while
its character was registered (the session's channel map was written at `Ready` and erased by `forgetSimNetCharacter`, G-13,
retired by task 14 too). The walk's remaining skips (a buffer shorter than its header, tick 0) were plain bounds.

### G-08 — RETIRED by og-input-latency P6-2 (2026-11-22): its subject was deleted

**Was:** *"`hadAnyTier` is read before `applyReplicatedConnectionTier`"*. Its tag sat on the `const bool hadAnyTier =`
statement in `onConnectionTierReceived`. Its prohibition: read it before the apply, which makes the consumer's
`hasReceivedTier()` true unconditionally, or "the first real tier resolution then requests a spurious stall".

**Why it retired.** `hadAnyTier` fed og-simulation's `shouldStallForTierTransition`, which P6-2 retired: the tier receipt now
pays the recompute's change, either sign, the first arrival included (`⛔G-40`), and reads no prior-arrival flag. The fact it
guarded, that a first arrival's `oldTier` of 0 is the wire's default and not a tier the client ran, still holds; the recompute
answers it by reading the consumer's own state, the fallback before the arrival (rationale §9.4).

### G-13 — RETIRED by og-session-runtime task 14 (2026-11-24): its site was deleted

**Was:** *"`forgetSimNetCharacter` stays in the unregistration, after `unregisterSimulatable` and before the game's forget"*. Its
tag sat above the forget call in the session's in-frame unregistration (unregisterCharacter).

**Why it retired.** Task 14 deleted that unregistration with the in-frame server: every authority session runs the authority
loop, whose unregistration goes through og-sim-net's queue on the runner (`requestRunnerUnregister_GameThread`, `⛔G-37`; since
og-session-runtime task 30 og-sim-session's `SessionAuthorityLifecycle`, `SessionLifecycle-guards.md` G-07), and a
client's teardown unregisters through its own queue before the lanes' stop reaches the session (the client arm of the call was
already unreachable: a `checkf` in `onSimNetLanesStopping` states it). Neither path calls `unregisterSimulatable` on the game
thread, so the order it guarded has no site. `forgetSimNetCharacter` is still called, by `unregisterCharacterHostOnly`, and its
server arm (og-sim-net's remove) went too. **Successor:** none: removed by og-session-runtime task 14.

### G-18 — RETIRED by og-session-runtime task 14 (2026-11-24): its site was deleted

**Was:** *"the frame function's coordinator test guards PROBE 6 only, never PROBE A"*. Its tag sat above the og-sim-net server's
presence test in `onFrameStepsDue_Step`, between PROBE A and PROBE 6.

**Why it retired.** Task 14 deleted PROBE 6 and the `[PacketBudget]` line, which ran only on the in-frame server, and with them
the test and the loop-on return under it: nothing in the function runs after PROBE A. **Successor:** none: removed by
og-session-runtime task 14.

### G-19 — RETIRED by og-session-runtime task 14 (2026-11-24): its site was deleted

**Was:** *"the send-budget probe feeds the engine's cumulative counters, not `OutBytes` / `OutPackets`"*. Its tag sat above PROBE 6's
sample call in `onFrameStepsDue_Step`.

**Why it retired.** Task 14 deleted PROBE 6 (the session's call and og-simulation's probe, `RelayProbes-rationale.md` §12): it
was fed only by the in-frame server, on the game thread. **Successor:** none: removed by og-session-runtime task 14.

### G-25 — RETIRED by og-session-runtime task 14 (2026-11-24): its site was deleted

**Was:** *"the systems executor is notified only once the character is in storage, and before `Ready`"*. Its tag sat above the
notify in the authority's unqueued arm of `tryRegister`, the in-frame server's game-thread registration.

**Why it retired.** Task 14 deleted that arm: an authority registers on its runner, a client through its queue. The rule is
each remaining path's own guard. **Successors:** `⛔G-36` (the authority runner's applied hook; since og-session-runtime task 30
og-sim-session's `SessionLifecycle-guards.md` G-06) and `⛔G-32` (a client's applied
hook; since og-session-runtime task 29 og-sim-session's `SessionLifecycle-guards.md` G-02).

### G-26 — RETIRED by og-session-runtime task 14 (2026-11-24): its site was deleted

**Was:** *"unregistration notifies the executor before `unregisterSimulatable`, behind the `has<>` guard"*. Its tag sat above
the `has<>` test at the top of the in-frame unregistration.

**Why it retired.** Task 14 deleted that unregistration with the in-frame server. The same order and the same guard are each
remaining path's own. **Successors:** `⛔G-31` (a client's queued unregistration) and `⛔G-35` (og-sim-net's unregistering hook,
which the authority runner's queue reaches); since og-session-runtime task 29 og-sim-session's `SessionLifecycle-guards.md` G-01
and, for the hook, `SessionLifecycle-guards.md` G-05 (a client) and `⛔G-41` (the authority; since og-session-runtime task 30
`SessionLifecycle-guards.md` G-08).

### G-14 — RETIRED by og-session-runtime task 23 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the relay-delay floor is read from the ini on the authority only"*. Its tag sat in `UESimulationSession.h`, above its site, the floor intake in `readSessionKnobs`.

**Why it retired.** Task 23 moved the knob intake out of this class into og-sim-session's engine-free `SessionKnobs.h`
(`readSessionKnobs`, `applySessionKnobs`, `applyResimTriggerPolicy`, `applyLatencyKnobs` and their helpers), the site with it.
The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's `SessionKnobs-guards.md`
G-01.

### G-15 — RETIRED by og-session-runtime task 23 (2026-11-25): moved with its site to og-sim-session

**Was:** *"there is no relay-ring depth ini intake, deliberately"*. Its tag sat in `UESimulationSession.h`, above its site, the blank line between the floor's and the rotation width's intakes in `readSessionKnobs`.

**Why it retired.** Task 23 moved the knob intake out of this class into og-sim-session's engine-free `SessionKnobs.h`
(`readSessionKnobs`, `applySessionKnobs`, `applyResimTriggerPolicy`, `applyLatencyKnobs` and their helpers), the site with it.
The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's `SessionKnobs-guards.md`
G-02.

### G-16 — RETIRED by og-session-runtime task 23 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the resim-trigger-policy intake is not authority-gated, is read once, and has no cooldown key"*. Its tag sat in `UESimulationSession.h`, above its site, the resim-trigger-policy intake in `readSessionKnobs`.

**Why it retired.** Task 23 moved the knob intake out of this class into og-sim-session's engine-free `SessionKnobs.h`
(`readSessionKnobs`, `applySessionKnobs`, `applyResimTriggerPolicy`, `applyLatencyKnobs` and their helpers), the site with it.
The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's `SessionKnobs-guards.md`
G-03.

### G-17 — RETIRED by og-session-runtime task 23 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the resim-trigger policy is applied and proved once, after both role branches"*. Its tag sat in `UESimulationSession.h`, above its site, the first statement of `applyResimTriggerPolicy`.

**Why it retired.** Task 23 moved the knob intake out of this class into og-sim-session's engine-free `SessionKnobs.h`
(`readSessionKnobs`, `applySessionKnobs`, `applyResimTriggerPolicy`, `applyLatencyKnobs` and their helpers), the site with it.
The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's `SessionKnobs-guards.md`
G-05.

### G-27 — RETIRED by og-session-runtime task 23 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the latency-knob intake is not authority-gated"*. Its tag sat in `UESimulationSession.h`, above its site, the latency-knob intake in `readSessionKnobs`.

**Why it retired.** Task 23 moved the knob intake out of this class into og-sim-session's engine-free `SessionKnobs.h`
(`readSessionKnobs`, `applySessionKnobs`, `applyResimTriggerPolicy`, `applyLatencyKnobs` and their helpers), the site with it.
The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's `SessionKnobs-guards.md`
G-04.

### G-03 — RETIRED by og-session-runtime task 24 (2026-11-25): moved with its site to og-sim-session

**Was:** *"`requestInputDelayTransition` keeps its `runsPrediction()` guard"*. Its tag sat in `UESimulationSession.h`, above the `runsPrediction()` early return in `requestInputDelayTransition`.

**Why it retired.** Task 24 moved the clock routing and the schedule out of this class into og-sim-session's engine-free
`SessionClock.h` (`SessionClock`: the tier and floor receipts, the effective-delay recompute and its payment, the clock
feeds and the window line, the dilation stage, the authority's rate line, its step start and the server tick's publish),
the site with it. The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's
`SessionClock-guards.md` G-01.

### G-04 — RETIRED by og-session-runtime task 24 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the relay-delay-floor advisory is ADVISORY: never an assert"*. Its tag sat in `UESimulationSession.h`, above the declaration of `logRelayDelayFloorAdvisory`.

**Why it retired.** Task 24 moved the clock routing and the schedule out of this class into og-sim-session's engine-free
`SessionClock.h` (`SessionClock`: the tier and floor receipts, the effective-delay recompute and its payment, the clock
feeds and the window line, the dilation stage, the authority's rate line, its step start and the server tick's publish),
the site with it. The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's
`SessionClock-guards.md` G-02.

### G-05 — RETIRED by og-session-runtime task 24 (2026-11-25): moved with its site to og-sim-session

**Was:** *"ONE recompute publishes the effective input delay, over BOTH inputs"*. Its tag sat in `UESimulationSession.h`, above the declaration of `recomputeAndPublishEffectiveInputDelay`.

**Why it retired.** Task 24 moved the clock routing and the schedule out of this class into og-sim-session's engine-free
`SessionClock.h` (`SessionClock`: the tier and floor receipts, the effective-delay recompute and its payment, the clock
feeds and the window line, the dilation stage, the authority's rate line, its step start and the server tick's publish),
the site with it. The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's
`SessionClock-guards.md` G-03.

### G-09 — RETIRED by og-session-runtime task 24 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the relay-delay-floor advisory is a log line, never an assert"*. Its tag sat in `UESimulationSession.h`, above the classification `switch` in `logRelayDelayFloorAdvisory`.

**Why it retired.** Task 24 moved the clock routing and the schedule out of this class into og-sim-session's engine-free
`SessionClock.h` (`SessionClock`: the tier and floor receipts, the effective-delay recompute and its payment, the clock
feeds and the window line, the dilation stage, the authority's rate line, its step start and the server tick's publish),
the site with it. The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's
`SessionClock-guards.md` G-04.

### G-38 — RETIRED by og-session-runtime task 24 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the dilation stage's sensor clock is `FPlatformTime::Seconds()`, never the driver's `hostNowSeconds`"*. Its tag sat in `UESimulationSession.h`, above the sensor clock's read in `runClockDilation_GameThread` (`beforePredictionPump` since).

**Why it retired.** Task 24 moved the clock routing and the schedule out of this class into og-sim-session's engine-free
`SessionClock.h` (`SessionClock`: the tier and floor receipts, the effective-delay recompute and its payment, the clock
feeds and the window line, the dilation stage, the authority's rate line, its step start and the server tick's publish),
the site with it. The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's
`SessionClock-guards.md` G-05.

### G-39 — RETIRED by og-session-runtime task 24 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the SimClock server's tick is published with its step's start time, never a time read at the publish"*. Its tag sat in `UESimulationSession.h`, above the SimClock server's publish in `afterStep_Step` (`publishServerTick` since).

**Why it retired.** Task 24 moved the clock routing and the schedule out of this class into og-sim-session's engine-free
`SessionClock.h` (`SessionClock`: the tier and floor receipts, the effective-delay recompute and its payment, the clock
feeds and the window line, the dilation stage, the authority's rate line, its step start and the server tick's publish),
the site with it. The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's
`SessionClock-guards.md` G-06.

### G-40 — RETIRED by og-session-runtime task 24 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the tier receipt pays the recompute's change, either sign, its first arrival included"*. Its tag sat in `UESimulationSession.h`, above the tier receipt's pay call in `onConnectionTierReceived` (`onConnectionTier` since).

**Why it retired.** Task 24 moved the clock routing and the schedule out of this class into og-sim-session's engine-free
`SessionClock.h` (`SessionClock`: the tier and floor receipts, the effective-delay recompute and its payment, the clock
feeds and the window line, the dilation stage, the authority's rate line, its step start and the server tick's publish),
the site with it. The entry moved with its text and its R0 history, re-verified. **Successor:** og-sim-session's
`SessionClock-guards.md` G-07.

### G-31 — RETIRED by og-session-runtime task 29 (2026-11-25): moved with its site to og-sim-session

**Was:** *"an immediate client's queued unregistration notifies the executor before it enqueues, behind the `has<>` guard"*. Its
tag sat in `enqueueQueuedUnregister_Step`, above the notified-set and storage test before `client.unregisterCharacter`.

**Why it retired.** Task 29 moved the prediction role's registration and unregistration out of this class into og-sim-session's
engine-free `SessionLifecycle.h`, the site with it (`enqueueUnregister`). The entry moved with its text, re-verified.
**Successor:** og-sim-session's `SessionLifecycle-guards.md` G-01.

### G-32 — RETIRED by og-session-runtime task 29 (2026-11-25): moved with its site to og-sim-session

**Was:** *"an immediate client notifies the executor of a registration only in its `applied` hook, and not when an unregister is
pending"*. Its tag sat in `onQueuedRegisterApplied_Step`, above the pending-unregister test.

**Why it retired.** As G-31: the site moved to `SessionLifecycle::onRegisterApplied`. **Successor:** og-sim-session's
`SessionLifecycle-guards.md` G-02.

### G-33 — RETIRED by og-session-runtime task 29 (2026-11-25): moved with its decision to og-sim-session

**Was:** *"an immediate client's game-thread unregistration releases the bodies only after the step has applied it"*. Its tag sat
in `requestQueuedUnregister_GameThread`, above `unregisterCharacterHostOnly(id, &owner, /*releaseBodiesNow=*/false)`.

**Why it retired.** As G-31. The decision to keep the bodies is now `SessionLifecycle::requestUnregister`'s answer
`UnregisterRelease::AfterApplied`, where the tag sits; the adapter's one host-work call passes `release ==
UnregisterRelease::Now` (`unregisterFromNewFramework`). **Successor:** og-sim-session's `SessionLifecycle-guards.md` G-03.

### G-34 — RETIRED by og-session-runtime task 29 (2026-11-25): moved with its site to og-sim-session

**Was:** *"an immediate client's teardown stops the steps before it unregisters anything"*. Its tag sat in
`teardownImmediateClient_GameThread`, above `m_physicsBackend.onEndPlayBegin()`.

**Why it retired.** As G-31: the site moved to `SessionLifecycle::teardown`, above the port's `stopSteps()` (which the adapter's
port maps to `onEndPlayBegin`). The R0 of the move corrected one claim: the overlap check is og-sim's `SimInbox::drain`, not
og-sim-net's. **Successor:** og-sim-session's `SessionLifecycle-guards.md` G-04.

### G-35 — RETIRED by og-session-runtime task 29 (2026-11-25): split by role

**Was:** *"the og-sim-net hosts' unregistering hook notifies the executor only for a notified id still in storage"*. Its tag sat
in `onSimNetLifecycleUnregistering`, which both og-sim-net hosts reached (the client's link session and the server's host).

**Why it retired.** Task 29 moved the prediction role's half into og-sim-session (`SessionLifecycle::onUnregistering`, with its own
notified set); the authority's stays here until task 30, with its own id. **Successors:** og-sim-session's
`SessionLifecycle-guards.md` G-05 (a client) and `⛔G-41` (the authority; since og-session-runtime task 30 og-sim-session's
`SessionLifecycle-guards.md` G-08).

### G-36 — RETIRED by og-session-runtime task 30 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the authority loop notifies the executor of a registration in the `applied` hook on the runner, and records it"*. Its
tag sat in `onRunnerRegisterApplied_Runner`, above `m_manager->notifyCharacterRegistered(id);`.

**Why it retired.** Task 30 moved the authority's registration and unregistration out of this class into og-sim-session's
engine-free `SessionAuthorityLifecycle` (`SessionLifecycle.h`), the site with it (`onRegisterApplied`, above the port's
`notifyRegistered`). The entry moved with its text, re-verified. **Successor:** og-sim-session's `SessionLifecycle-guards.md`
G-06.

### G-37 — RETIRED by og-session-runtime task 30 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the authority loop releases a character's bodies through og-sim-net's queue once a registration was posted"*. Its tag
sat in `requestRunnerUnregister_GameThread`, inside the runner request (a closure), above `if (!registerPosted)`.

**Why it retired.** As G-36: the runner request is now data (`authorityRequest::Unregister`), and the test is in the lifecycle's
runner side (`run(Unregister&)`, above `if (!request.registerPosted)`). **Successor:** og-sim-session's
`SessionLifecycle-guards.md` G-07.

### G-41 — RETIRED by og-session-runtime task 30 (2026-11-25): moved with its site to og-sim-session

**Was:** *"the authority's unregistering hook notifies the executor only for a notified id still in storage"*. Its tag sat in
`onSimNetLifecycleUnregistering`, above the notified-set and storage test.

**Why it retired.** As G-36: the authority's notified set moved into `SessionAuthorityLifecycle` with the hook's body
(`onUnregistering`); `onSimNetLifecycleUnregistering` forwards to it (the og-sim-net server's host calls it: og-sim-net's
`SimNetServerHostCore` through Unreal's `UESimNetServerHostPort` since og-session-runtime task 31a; task 31 moves that host). **Successor:** og-sim-session's `SessionLifecycle-guards.md` G-08.

### G-01 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to og-sim-session

**Was:** *"the systems executor is emplaced beside the adapters, and hands a body-reading system the same body adapter the integration layer uses"*. Its tag sat at: `UESimulationSession.h`, directly above `std::optional<SystemsExec> m_systemsExec;`. The two emplace sites it governs are the game's hook `Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter)`, called once per role branch of `beginPlay` (since og-ue-simulationcomponentandmanagersplit task 27b; the owner's `BeginPlay` before), each between the integration layer's `emplace` and the manager's. The session hands the hook the two adapters; the hook constructs the systems from what it is handed.

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `SessionRuntime-guards.md` G-01.

### G-23 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to og-sim-session

**Was:** *"the authority branch sets the neutral input too, before any registration"*. Its tag sat at: `UESimulationSession.h`, directly above `m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());` on the authority branch of `beginPlay`.

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `SessionRuntime-guards.md` G-02.

### G-06 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to og-sim-session

**Was:** *"the tier cache is emplaced on the authority world too"*. Its tag sat at: `UESimulationSession.h`, directly above the authority branch's `m_clock->beginTierConsumer();` in `beginPlay` (since task 27b; the clock's member since og-session-runtime task 24). `beginPlay` calls `beginTierConsumer` once in each role branch, at the position of the two statements it replaced; the wrong edit, dropping the authority's call or making the call client-only, is typed at that call. Until task 27b the start-up was the owner's `BeginPlay`, so the tag sat in `beginTierConsumer` above its emplace (task 25's A5); task 27b re-sited it at the caller.

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `SessionRuntime-guards.md` G-03.

### G-24 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to og-sim-session

**Was:** *"the resim-trigger policy is applied once, after both role branches"*. Its tag sat at: `UESimulationSession.h`, directly above `ogsession::applyResimTriggerPolicy(knobLog, *m_manager, sessionKnobs);` in `beginPlay`, the first statement after the role branches (the session's own member until og-session-runtime task 23).

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `SessionRuntime-guards.md` G-04.

### G-20 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to og-sim-session

**Was:** *"`m_physicsBackend.onEndPlayBegin()` stays the first statement of `endPlay`: nothing moves above it"*. Its tag sat at: `UESimulationSession.h`, directly above `m_physicsBackend.onEndPlayBegin();`, the first statement of `endPlay`.

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `SessionRuntime-guards.md` G-07.

### G-21 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to og-sim-session

**Was:** *"`endPlay` resets og-sim-net's server and the tier cache while `m_manager` still exists"*. Its tag sat at: `UESimulationSession.h`, directly above `m_simNetServer.reset();` in `endPlay`, after `detachFromSimNetLanes();`, which follows `m_physicsBackend.onEndPlayBegin();` (`⛔G-20`); the server host's reset, the clears of the two registered maps and the slot map, the unregistered-id set's clear (since og-input-latency task P5-9) and the tier cache's reset (`m_clock->endTierConsumer()` since og-session-runtime task 24) follow it. (Since og-input-latency task P5-7; until then the tag stood above the delayed-input map's clear, with the session's own coordinator's reset after it.)

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `SessionRuntime-guards.md` G-08.

### G-22 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to UESessionEngine.h

**Was:** *"the field-diff predicate must ask the category and verbosity `RouteOGMessage` sends `[DivergenceProbe` to, live"*. Its tag sat at: `UESimulationSession.h`, directly above the `correctionFieldDiff::setEnabledPredicate(` call in the private static member `bindCorrectionFieldDiffGate`, which `beginPlay` calls at the end of each role branch.

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `UESessionEngine-guards.md` G-01.

### G-28 — RETIRED by og-session-runtime task 31 (2026-11-26): moved with its site to UESessionEngine.h

**Was:** *"the margin predicate must ask the category `RouteOGMessage` sends `[DivergenceProbe` to, at `Log`, live"*. Its tag sat at: `UESimulationSession.h`, directly above the `correctionVerdictMargin::setEnabledPredicate(` call in `bindCorrectionFieldDiffGate`, the second predicate install, after G-22's and a `static_assert` on the route.

**Why it retired.** Task 31 moved the session's composition, start and stop out of the Unreal adapter (and the tick backend out of its Unreal wrapper) into og-sim-session's engine-free `SessionRuntime.h`, or the engine's parts into `UESessionEngine.h`, the site with it. The entry moved with its text. **Successor:** `UESessionEngine-guards.md` G-02.

### G-11 — RETIRED by og-session-runtime task 33b (2026-11-26): its subject left the class

**Was:** *"the transport seam: a transport function reaches only its allow-list (deny by default)"*. Its tag sat at: `UESimulationSession.h`, directly above `instanceFor`, the first declaration of the transport section.

**Why it retired.** Tasks 23 to 31 moved every function of its allow-list into og-sim-session (the clock and the tier and floor receipts to `SessionClock`, the knobs to `SessionKnobs`, the frame probe and the og-net hookup to `SessionRuntime`, the reception to `SessionNetHooks`), and task 31 deleted `setOnTimingInfoReceivedCallback`, `getTimeConfigPtr` and `getReceptionCoordinator` (no caller). What stayed at the tag was `instanceFor`, which reaches only the registry (`UESessionRegistry::instanceFor`), and the destructor, which G-10 guards. **Successor:** none; no function of the seam is this class's.

**The entry as it stood**, under the names the code had, is the record below.

**The prohibition.** ⛔ Until the type-free transport (M4, deferred) lands, a transport function reaches typed state only
through what that transport's binding would take (manager design §9): the manager's session controls (`runsPrediction`,
`getTimeConfig`, the clamped setters and the six clock accessors), the input-delay atomic (through the delay publish
and its read), the resim-policy sink and the coordinator's tier tracking. Every other member, accessor or `TraitsT` reach
in a transport function is forbidden unless the table below names it, and a transport function calls into the session only
through another transport function (or the one accessor the table names).

**The allow-list, built from the moved code (og-ue-simulationcomponentandmanagersplit tasks 25 and 26b).** "Transport
state" is the transport's own members (`s_instances`, `m_replicatedTierConsumer`, `m_lastPublishedEffectiveInputDelayTicks`,
`m_onTimingInfoReceivedCallback`, and since task 26b the frame probe `m_frameHealthProbe`; PROBE 6's until og-session-runtime
task 14, the timing relay pointer until og-input-latency task P5-7), which M4 moves with the functions.

| function | may reach |
|---|---|
| `instanceFor`, `~UESimulationSession` | transport state `s_instances`; since og-session-runtime task 16b also each registered session's `m_registeredWorld` and `m_registeredAsAuthority` (`instanceFor`) and `unregisterInstance` (the destructor), which reaches `s_instances` and `s_startedSessions` |
| `setOnTimingInfoReceivedCallback` | transport state `m_onTimingInfoReceivedCallback` |
| `onTimingInfoReceived` | transport state `m_onTimingInfoReceivedCallback`; `m_manager`: `has_value`, `editNetworkEstimator`; **exempt (Q-I):** `m_latencyBudgetDiagnostics.noteWireSample` |
| `publishClientEffectiveInputDelayTicks` / `getClientEffectiveInputDelayTicks` | `m_inputResolution`: `setClientEffectiveInputDelayTicks` / `getClientEffectiveInputDelayTicks` (the atomic) |
| `getReplicatedTierConsumer`, `applyReplicatedConnectionTier` | transport state `m_replicatedTierConsumer` |
| `onConnectionTierReceived` | transport calls `applyReplicatedConnectionTier`, `payInputDelayTransition`; `m_manager`: `has_value`, `getTimeConfig` (the floor its line prints; og-input-latency P6-2) |
| `beginTierConsumer` | transport state `m_replicatedTierConsumer`; `m_manager`: `getTimeConfig` |
| `recomputeAndPublishEffectiveInputDelay` | transport state `m_replicatedTierConsumer`, `m_lastPublishedEffectiveInputDelayTicks`; `m_manager`: `has_value`, `getTimeConfig` |
| `logRelayDelayFloorAdvisory` | `m_manager`: `has_value`, `getTimeConfig` (`applyTierTransitionStall` was here too until og-input-latency P6-2 deleted it) |
| `applyReplicatedRelayDelayFloor` | `m_manager`: `has_value`, `getTimeConfig`, `setRelayDelayFloorTicks` |
| `requestInputDelayTransition` (`requestInputDelayIncreaseStall` until og-input-latency P6-2) | `m_manager`: `has_value`, `runsPrediction`, `editClientClock` |
| `payInputDelayTransition` (og-input-latency P6-2) | transport call `requestInputDelayTransition` only |
| `onRelayDelayFloorReceived` | transport calls only (its replay twin and the tier's were deleted by og-input-latency task P5-9) |
| `onFrameStepsDue_Step` (task 26b) | transport state `m_frameHealthProbe` and the runner's wake count; `m_manager`: `has_value`, `runsPrediction` (until og-session-runtime task 14 also PROBE 6's state and its one-shot `[PacketBudget]` latch, the server's presence test and the owner as an `AActor`, for the in-frame server) |

**The exemption (ruled Q-I, 2026-10-20).** The wire sample in `onTimingInfoReceived` is the session's own call to its typed
latency member; it stays typed under M4 (its binding would take it), so the ruling exempts it instead of widening §9's list.
The ruling's other exemption, the relay host's character cast in the input-relay listener member, went with that member
(og-input-latency task P5-7).

**One reach §9 did not name.** From task 26b until og-session-runtime task 14 PROBE 6 read the owner's world for its net
driver (the timing relay's lookup iterated it the same way until P5-7): the owner as an `AActor`, a `UObject` reach, not a game
type. It went with PROBE 6.

**The knobs and the frame probe (task 26b)** were on the seam: the three knob functions reached the manager only through its
session controls (`getTimeConfig` and the two clamped setters) and the resim-policy sink, and the frame probe reaches the
manager's role (`has_value`, `runsPrediction`) only. **The knob functions left this class with og-session-runtime task 23**: they
are og-sim-session's (`SessionKnobs.h`), free functions that take what they reach as arguments (the manager, og-sim-net's server
as a pointer, the advisory, the dilation controller and window, a log sink), so their four rows went from the table; what
`beginPlay` passes them is its own reach. Until og-session-runtime task 14 PROBE 6 also reached the coordinator's
presence, the one reach into the input-reception state a seam function had (§9 named it).

**A fourth knob function (og-input-latency task 4).** `applyLatencyKnobs` reached the manager through `getTimeConfig` and
two more clamped setters, `setRedundancyFrames` and `setRelayResendEntries`. They are the same kind of session control
as the first two (an integer clamped into one `TimeConfig` field), so "the clamped setters" above names four. (Its row went
with the move, og-session-runtime task 23.)

**Exempt by design: the input reception and the og-net hookup.** `getReceptionCoordinator`,
`getServerReceptionTick`, `releaseDelayedInputsForStep` and `afterAuthorityBatch` (the reap; og-sim-session's `SessionNetHooks`
since og-session-runtime task 26, over the port's `Server`) are typed on the per-step release
(task 26a; the in-frame server's correction-tick walk for the latency member too, until og-session-runtime task 14): erasing the game's input type there would put
an indirect call on the step path (manager design §9, step 5). Since og-input-latency task P5-7 they reach og-sim-net's server,
which is typed on the game's composition, and the og-net hookup joins them: `findDriverSimNetLanes`, `beginServerSimNet`,
`beginClientSimNet`, `logSimNetHostLine`, `onSimNetLanesStopping`, `detachFromSimNetLanes`, `clientBindStep`,
`forgetSimNetCharacter` and `onSimNetRegisteredRemoved`, with their state (`m_simNetLanes`, `m_simNetServerHost`,
`m_simNetServer`, `m_registeredComponentsById`, `m_simUnregisteredIds`; the authority's slot map is og-sim-session's
`SessionAuthorityLifecycle`'s since og-session-runtime task 30). og-sim-net is the
typed transport M4 was to make type-free, so they stay in this class, are not on the list above, and the script does not check
them. Of the functions above, `applySessionKnobs` reached the server (to publish) until og-session-runtime task 23 moved it to
og-sim-session, which `beginPlay` passes the server. Until og-session-runtime task 14 the in-frame
server's clock publish reached it too, `onFrameStepsDue_Step` tested its presence, and the hookup also held the in-frame
server's add and unregistration with their channel map and the session's detached lanes. Until P5-7 this list was the session's own coordinator, the delayed-input map and
its registration, the two sinks with PROBE 5, the coordinator's emplace and the forget.

**What breaks if the edit is made.** A typed reach in a transport function compiles and behaves; what breaks is M4. Each
reach outside the list is a dependency the type-free transport class cannot satisfy, found only when that refactor is
attempted.

**How it is checked.** The seam script of og-ue-simulationcomponentandmanagersplit (transport_seam.py, in that initiative's td25 evidence folder, outside this repository) extracts each listed
function's body from this header by brace matching and denies every data member, member call and `TraitsT`-derived name
the function's list does not name, every call into the session that is not another listed function, and every allowance
the body does not use, so the list in the script and the table here stay the moved code's exactly. Since task 26b it also
reads `(*this).X` and `(*this)->X` as `this->X`, and denies every other use of the session object inside a listed
function (`*this`, or a bare `this` that is not `this->`, a comparison or a lambda capture), so a local alias of the
session cannot carry a call past the check. Its seen-failing arms each fail it: task 25's three (a `TraitsT::PlayerInput`
use in a tier helper, an `m_storage` read, a call to `getReceptionCoordinator()`), and task 26b's (a knob function calling
a manager member outside its list, `(*this).getReceptionCoordinator()`, a reference and a pointer alias of the session, and
an allowance the body stopped using). The script is the reviewer's tool, not a build step: nothing in the build runs it.
