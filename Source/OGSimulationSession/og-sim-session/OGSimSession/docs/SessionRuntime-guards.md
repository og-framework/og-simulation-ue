<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: OnRep -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: knobLog -- moved by og-session-runtime task 31 (the session's composition, start and stop are og-sim-session's SessionRuntime; UESimulationSession is the thin Unreal adapter); kept in the history -->
<!-- lint-external-ref: m_tickDriver -- moved by og-session-runtime task 31 (the world build and the stepping to og-sim-session's SessionRuntime: buildWorld, startStepping, stopStepping; the static import to ueSessionEngine::importStatics; the present to UESessionPresentation; the world config to TickDriverBackendUImpl::worldConfig; slotKeyOf is slotKeyFor); kept in the history -->
# `SessionRuntime.h` — guards

Prohibitions with a tagged site in og-sim-session's `OGSimSession/SessionRuntime.h`. Ids are opaque, stable, and retired rather
than reused. The reasoning lives in `SessionRuntime-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

G-01 to G-05, G-07 and G-08 moved here with the code they mark when og-session-runtime task 31 moved the session's composition,
start and stop out of Unreal's `UESimulationSession` (G-05: out of `TickDriverBackendUImpl`); each entry names its old id, which
is retired there with this file's id as its successor. G-06 is new. What the compiler or a check holds is not here: the member
order (`compositionContractsHold`, the moved order `static_assert`s), the ports (`SessionRuntimeHost`, `SessionEngine`,
`SessionBodies`), each door's thread (the components' `SessionThreadChecks`), and the attach-before-steps order (`OG_CHECK`, G-06's
site).

---

## G-01 — the systems executor is emplaced beside the adapters, and hands a body-reading system the same body adapter the integration layer uses

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-01, with the code it marks. **Tag site:** `SessionRuntime.h`, directly above `std::optional<SystemsExec> m_systemsExec;`. Its emplace site is `start`'s configuration, `Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter)`, once for both roles, between the integration layer's `emplace` and the manager's. **R0 (task 31):** the two role branches became one configuration with role tests; the order integration, systems, manager holds in it. The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above `std::optional<SystemsExec> m_systemsExec;`. The two emplace sites
it governs are the game's hook `Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter)`, called once per role
branch of `beginPlay` (since og-ue-simulationcomponentandmanagersplit task 27b; the owner's `BeginPlay` before), each between
the integration layer's `emplace` and the manager's. The session hands the hook the two adapters; the hook constructs the
systems from what it is handed.

**The prohibition.** A system of the executor that reads body state (the brawler's hit detector,
`TraitsT::Composition`'s `HitDetectionSystem`) is constructed from `*m_physAdapter` and `*m_queryAdapter`, the SAME two
objects `m_integrationLayer` integrates with. ⛔ Never from `m_physReaderAdapter` (an `EngineTraits::ReaderAdapter`), and
never before the adapters are emplaced or after `m_manager` is.

**Rewritten for the traits types (og-simulationscheduler-withjolt task 51).** The text named the Chaos types until the
backend switch. The prohibition is the same in every configuration: a body-reading system reads the body state the
step itself reads, never the reader. *R0, og-ue-simulationcomponentandmanagersplit task 24a:* the moved text said
"only the Chaos configuration builds today"; both configurations build, and the Jolt one is the default since
og-simulationscheduler-withjolt task 18. The verification of the wrong adapter's behaviour is the Chaos configuration's,
and it names the brawler's detector, so it stayed with the game (`OGBrawlerSimulationManager-rationale.md` §2, in
OGBrawlerUnreal's docs). *og-session-runtime tasks 3–6* removed the former Chaos configuration, so Jolt is the only one;
the prohibition is unchanged.

**What breaks if the edit is made.**
* Handed the reader adapter, a body-reading system classifies against the game-thread copy of a body transform, an
  interpolated value from an earlier push, not this tick's, with a staleness that depends on game-thread timing.
  Both adapters satisfy the reader concept, so nothing fails to compile, and no LLT can see it: the LLT targets do not
  compile the Unreal layer.
* The executor cannot be a plain member: a body-reading system stores pointers to the adapters, which are
  `std::optional`s emplaced in the role branches, so a default-constructed executor would hold pointers into empty
  optionals. Emplaced after `m_manager`, the manager would bind a reference to an empty `std::optional`'s storage.

*Moved from `OGBrawlerSimulationManager-guards.md` G-75 (og-netcode-v2-field-defects task 9, 2026-09-23) by
og-ue-simulationcomponentandmanagersplit task 24a, with the declaration it marks; the text is that entry's, with the
brawler's type names replaced by the session's and the R0 line above.*

---

## G-02 — the authority branch sets the neutral input too, before any registration

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-23, with the code it marks. **Tag site:** `SessionRuntime.h`, directly above `m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());` in `start`'s configuration, which both roles run before the link is attached. **R0 (task 31):** one statement for both roles now (it was one per branch); both still set it before any registration, which the frame thread can make only after `start` returned. The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above `m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());`
on the authority branch of `beginPlay`.

**The prohibition, as it stood in the source:**

> Inject the game's zero input for the client input delay line. §5
>
> ⛔ SET ON THE AUTHORITY BRANCH TOO, and not as a precaution: a DEDICATED server reads this
> on every tick it substitutes an input for a remote character, and seeds each character's
> replicated applied-input with it. Deleting it makes the authority simulate - and publish
> to every peer - a zero forward vector for the whole of every join window.
>
> ⛔ ORDER IS LOAD-BEARING: this must precede every registerAuthorityOwner call.

**The consequence.** The authority simulates, and publishes to every peer, the input type's value-initialized default
instead of the game's neutral input through every join window (for the brawler, a zero aim vector where its neutral input
aims at `(0,0,1)`).

**What breaks if the tag moves.** The client branch's call has no tag of its own: the wrong edit it was guarded against, a
value other than the game's neutral input, is typed in the game's hook `neutralInput()`, which carries the game's guard (the
brawler's: `BrawlerSessionGameUImpl-guards.md` G-01, in OGBrawlerUnreal's docs; ruling Q-J).

*R0, task 27b:* verified in og-simulation: `SimulationInputResolution::setNeutralInput` writes the per-type neutral once;
the authority's input collection substitutes it for a character with no input (the provider-miss and underrun arms); and
`SimulationNetSync::registerAuthorityOwner` logs `[Warning][NeutralInput] registerAuthorityOwner id=%u ran BEFORE
setNeutralInput` when it has not run, so a late call is reported, not silent. Registration runs per character, from the
owner's registration path, after `beginPlay` (since task 28 the session's own `tryRegister`, called since task 29 by the
owner's registration skeleton, `UESimulatableOwnerBinding`).

*Moved from `OGBrawlerSimulationManager-guards.md` G-59 (the brawler's task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 27b, with the call it
marks: the authority branch's neutral input is the session's own statement in `beginPlay`, its value the game's hook.*

---

## G-03 — the tier cache is emplaced on the authority world too

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-06, with the code it marks. **Tag site:** `SessionRuntime.h`, directly above `m_clock->beginTierConsumer();` in `start`'s configuration (both roles; on an authority after the session knobs, on a client before the link is attached, as the two branches had it). **R0 (task 31):** the authority still emplaces the tier cache; the statement is shared by both roles. The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above the authority branch's `m_clock->beginTierConsumer();` in `beginPlay`
(since task 27b; the clock's member since og-session-runtime task 24). `beginPlay` calls `beginTierConsumer` once in each role branch, at the position of the two statements it
replaced; the wrong edit, dropping the authority's call or making the call client-only, is typed at that call. Until task
27b the start-up was the owner's `BeginPlay`, so the tag sat in `beginTierConsumer` above its emplace (task 25's A5); task
27b re-sited it at the caller.

**The prohibition.** ⛔ The tier consumer is emplaced, and the baseline published, on the authority world as well as on a
client: do not make `beginTierConsumer` client-only, and do not drop its authority call.

**What breaks if the edit is made.** A listen-server host's local player reads its input delay from this cache. No tier
ever arrives on an authority world (the server sends tiers to its og-net connections only, og-sim-net's `publishTier`; the
connection relay's property, an `OnRep` on a non-authority world, until og-input-latency task P5-7), so the authority cache
stays at the no-tier fallback for the whole session. Without the cache, the recompute returns early and
never publishes, and the effective-delay atomic keeps its initial `0`, where the floored worst-tier fallback is correct.

*R0, task 25:* verified: the atomic is initialized to `0` (`SimulationInputResolution.h`), the recompute returns `0` without
publishing when the consumer is empty, and the local input path reads the atomic once per tick on every role.

*R0, task 27b:* re-checked at the new site: `beginPlay` calls `beginTierConsumer()` on both branches, the authority's after
`applySessionKnobs` and before the connection and input listener registrations, the client's after the neutral input.

*R0, og-input-latency task P5-7:* re-checked: the authority's call follows `applySessionKnobs` (the listener registrations
are gone), and the client's precedes `beginClientSimNet`, whose replayed tier applies only to an emplaced consumer.

*R0, og-session-runtime task 24:* re-checked: `beginTierConsumer` is og-sim-session's `SessionClock`'s since task 24 (the
tier consumer, the recompute and the start-up publish moved with it), and `beginPlay` calls `m_clock->beginTierConsumer()` on
both branches at the same positions (the clock is emplaced right after the manager on each). The recompute still returns
early without a consumer, and the atomic still starts at `0`.

*Moved from `OGBrawlerSimulationManager-guards.md` G-61 (og-netcode-v2-field-defects task 12) by
og-ue-simulationcomponentandmanagersplit task 25. Its site was the authority branch's emplace in the owner's `BeginPlay`;
the emplace and the baseline publish moved into `beginTierConsumer`, and the tag moved with the emplace.*

---

## G-04 — the resim-trigger policy is applied once, after both role branches

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-24, with the code it marks. **Tag site:** `SessionRuntime.h`, directly above `applyResimTriggerPolicy(m_clockEngine, *m_manager, sessionKnobs);`, after both roles' configuration. **R0 (task 31):** unchanged: one application, after both roles' statements, with the knob sink now the runtime's clock engine (the same route, RouteOGLevelLine, on Unreal). The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above `ogsession::applyResimTriggerPolicy(knobLog, *m_manager, sessionKnobs);`
in `beginPlay`, the first statement after the role branches (the session's own member until og-session-runtime task 23).

**The prohibition, as it stood in the source** (the first of the four quotes G-17 carried; G-17 is og-sim-session's
`SessionKnobs-guards.md` G-05 since og-session-runtime task 23):

> ⛔ AFTER BOTH ROLE BRANCHES: duplicating apply-plus-proof is how the roles drift. §3

**The consequence.** If the call is duplicated into the two branches, or moved into one, the roles drift: one role applies
or proves a policy the other does not, and the `[ResimGate] session` line no longer comes from one place.

*R0, task 27b:* verified: `beginPlay` calls `applyResimTriggerPolicy` exactly once, after the `if (worldIsAuthority)` /
`else` block and before `m_game.afterSessionConfigured`; the `[ResimGate] session` line is printed by it on both roles.

*Split from G-17 by og-ue-simulationcomponentandmanagersplit task 27b (manager design §4.3; task 26b's A5): G-17's "after both branches" half is typed at the
call, which moved here with the start-up; G-17 keeps the body's three prohibitions.*

*R0, og-session-runtime task 23:* verified: `beginPlay` calls `ogsession::applyResimTriggerPolicy` exactly once, after the role
branches and before `ogsession::applyLatencyKnobs` and `m_game.afterSessionConfigured`. The call stays here; the body went to
og-sim-session with its guard (`SessionKnobs-guards.md` G-05).

---

## G-05 — `EndPlay` waits for every outstanding step task before any teardown

**Moved by og-session-runtime task 31 (2026-11-26)** from `TickDriverBackendUImpl-guards.md` G-01, with the code it marks. **Tag site:** `SessionRuntime.h`, directly above `m_tickBackend.stop();` in `stopStepping`, after the frame host stops (the engine adapter's `endFrames`) and the runner is joined. **R0 (task 31):** the tick backend is the runtime's member since task 31 (the Unreal wrapper was reduced to the body facts); `SessionRuntimeStartStopTest.cpp`'s prediction case dispatches a step, leaves it outstanding and checks that `stop` ran it before any teardown. The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

*Was `OGBrawlerSimulationManager-guards.md` G-80; moved here by og-simulationscheduler-withjolt task 83, when the Jolt host left the UImpl. Only its site and its cross-references changed.*

**Site:** `m_tickBackend.stop();` in `onEndPlayBegin`, after the frame host's tick functions are unregistered. *Moved
by og-tickdriver task 15d (2026-10-18): the site was `m_tickDriver->stop();` inside `if (m_tickDriver.has_value())`;
the tick driver is og-simulation's `SimulationTickBackend`'s member since then, and its `stop()` makes the same call
under the same condition. The prohibition and its id are unchanged.* The session's `endPlay` calls `onEndPlayBegin` as its first
statement (`UESimulationSession::endPlay`, which the brawler's actor calls from its `EndPlay` since
og-ue-simulationcomponentandmanagersplit task 27a; the actor's `EndPlay` made the call before). *Reworded by og-tickdriver task 8 (2026-10-16): the wait was the frame host's `waitForOutstandingSteps`
and is now the tick driver's `stop()`, which clears the driver's running flag and then waits for the pending and
the blocking step task. The prohibition is unchanged.*

**The prohibition** (og-simulationscheduler-withjolt task 54, design §9.1). ⛔ Do not move any
`EndPlay` statement above this wait, and do not drop it. The order is: unregister the two tick
functions (so no new step is dispatched), wait for the step task dispatched in this and earlier frames,
then everything else (`UESimulationSession-guards.md` G-21's resets, the listener unregistration, the post-tick-flush unbind).
The "do not move any statement above" half is typed in `UESimulationSession::endPlay`, above its
`m_physicsBackend.onEndPlayBegin();`, and is guarded there: `UESimulationSession-guards.md` G-20 (the brawler's
`OGBrawlerSimulationManager-guards.md` G-85 and G-64 until og-ue-simulationcomponentandmanagersplit task 27a). This entry's tag
guards the wait itself (dropping it, or moving a statement above it inside `onEndPlayBegin`).

**The consequence.** On a worker client the frame's step task runs on another thread through the rest
of the frame. A teardown above the wait resets the coordinator or the tier cache, or unbinds what the
step reads, while the step runs: a use-after-reset on a worker thread. The wait itself never deadlocks:
it checks that this thread does not hold the world mutex (`SimulationTickDriver-guards.md` G-05, the successor of the
retired frame-host G-05).

---

## G-06 — a client attaches the link before its steps start

**Tag site:** `SessionRuntime.h`, directly above the `OG_CHECK(!m_stepping, …)` in `attachClientLink`, before
`lanes->bindClientSession(*this)`.

**The prohibition.** ⛔ Never start a client's steps (`startStepping`, the tick backend's `start`) before `attachClientLink` has
run. The `OG_CHECK` stops it: `m_stepping` is set by `startStepping` (and on an authority by the runner's start), and the
attach refuses once it is.

**Why.** The link's roster seed and its bound flag are published to the step thread by nothing but that order: the attach
stores them with relaxed atomics, and the step task that reads them is dispatched only after `start` returns, so the
dispatch is the release (og-sim-net's `SimNetLink-rationale.md` §5). Attached after the steps started, a step could read a
half-seeded roster or a bound flag without its seed.

**Seen failing (task 31).** Poison P5 moved `startStepping()` above `configureSession()` in `start`: the prediction start/stop
test stopped on `Assertion failed: (!m_stepping)` at the attach.

*Added by og-session-runtime task 31 (lead note under Backlog task 31, review_SR5_28 (b) item 5).*

---

## G-07 — `m_physicsBackend.onEndPlayBegin()` stays the first statement of `endPlay`: nothing moves above it

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-20, with the code it marks. **Tag site:** `SessionRuntime.h`, directly above `stopStepping();`, the first statement of `stop` (which the Unreal session's `endPlay` calls first). **R0 (task 31):** `m_physicsBackend.onEndPlayBegin()` is `stopStepping()` now: the frame host's tick functions, the runner's join, the tick backend's stop, in that order; nothing moved above it. The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above `m_physicsBackend.onEndPlayBegin();`, the first statement of `endPlay`.

**The prohibition.** ⛔ Do not move any `endPlay` statement above `m_physicsBackend.onEndPlayBegin()`, do not add one above
it, and do not drop the call. Everything else in `endPlay` (the og-net unbind, G-21's resets, the static instance table,
the latency member's `end` with its post-tick-flush unbind; the backend's world-teardown entry point until og-session-runtime
task 5) runs after it. The rule is the same for every backend:
- on a driver-scheduled backend (the tick-driver backend, `TickDriverBackendUImpl`), `onEndPlayBegin` unregisters the
  frame host's two tick functions and waits for every outstanding step task (`TickDriverBackendUImpl-guards.md` G-01);
- a backend whose `onEndPlayBegin` has nothing to wait for may leave it empty (the former self-scheduled Chaos host's was,
  until og-session-runtime task 3 removed it); a teardown of such a backend that must run first belongs inside it, not
  above it.

**The consequence of getting it wrong.** On a worker-stepping peer the frame's step task runs on another thread through
the rest of the frame. A statement above the call tears its state down while the step still runs: a use-after-reset on
a worker thread, which nothing reports.

**What breaks if the tag moves.** The tag marks the call. A statement typed above it is typed directly under the tag.
Nothing mechanical checks the order: the race needs a step in flight at the owner's `EndPlay`. Deleting the call takes
the tag with it and orphans this entry.

*R0, task 27a:* verified: the Jolt host's `onEndPlayBegin` is `unregisterTickFunctions()` then `stop()`; the brawler's
Chaos host's is `{}`; every other `endPlay` statement follows the call, and the owner calls `endPlay` first in its
`EndPlay`, before `Super`. **Corrected:** the moved text said the step task "reads the coordinator, the tier cache and the
manager's state". Only a client steps on a worker (an authority steps inline, on the game thread), and a client has no
reception coordinator (it is emplaced on the authority only, §9.7); the tier consumer is read on the game thread only
(its relay listeners and the effective-delay recompute, §9.4). Of today's statements below the call, none tears down
what a worker step reads (the manager, the storage and the three peers, which `endPlay` does not reset); the rule keeps
every teardown, present or added later, after the wait.

*R0, og-input-latency task P5-7:* re-checked: the og-net unbind and og-sim-net's server's reset (an authority object; a client
has none) follow the call, and the tier consumer is fed on the game thread by the og-net client host (the relay listeners
until P5-7).

*R0, og-session-runtime task 11:* the no-session return (`UESimulationSession-rationale.md` §23) follows the call, which is
safe with nothing started: the frame host's `unregisterTickFunctions` returns on a null owner, no runner exists to join,
and the tick backend's `stop` has no driver.

*R0, og-session-runtime task 16:* the task-27a correction above ("an authority steps inline, on the game thread") and the
P5-7 one (the tier consumer fed on the game thread) describe retired modes. Since og-session-runtime task 14 an authority
steps on its runner, which `onEndPlayBegin` joins (`stopAuthorityLoop_GameThread`); since task 12 a client's tier consumer is
fed by its drain on the step's thread (the rationale's §24). The rule is unchanged: both threads have stopped when the call
returns.

*Moved from `OGBrawlerSimulationManager-guards.md` G-85 (og-simulationscheduler-withjolt task 83, rework 1; the half of the
old G-80 that is typed in the owner's teardown) by og-ue-simulationcomponentandmanagersplit task 27a, with the call it
marks: the owner's `EndPlay` statements before `Super` became `endPlay`.*

---

## G-08 — `endPlay` resets og-sim-net's server and the tier cache while `m_manager` still exists

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-21, with the code it marks. **Tag site:** `SessionRuntime.h`, directly above `m_simNetIdle.reset();` in `stop`, which resets the idle, the clock server, the server and its host while `m_manager` still exists (the runtime's member, destroyed after `stop` returns). **R0 (task 31):** unchanged; the authority start/stop test's server sentinel checks the runner was joined before the server went. The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above `m_simNetServer.reset();` in `endPlay`, after
`detachFromSimNetLanes();`, which follows `m_physicsBackend.onEndPlayBegin();` (`⛔G-20`); the server host's reset, the clears
of the two registered maps and the slot map, the unregistered-id set's clear (since og-input-latency task P5-9) and the tier
cache's reset (`m_clock->endTierConsumer()` since og-session-runtime task 24) follow it. (Since og-input-latency task P5-7; until then the tag stood above
the delayed-input map's clear, with the session's own coordinator's reset after it.)

**The prohibition, as it stood in the source:**

> ⛔ The coordinator borrows m_manager's TimeConfig, so it is reset BEFORE the manager. §2

> ⛔ Same borrow rule as the coordinator: the tier cache holds m_manager's TimeConfig. §2

and, at the coordinator's emplace:

> ⛔ AUTHORITY BRANCH ONLY, and it borrows m_manager's TimeConfig, so it must not outlive it. §7

**The consequence.** If either reset is moved below a reset of `m_manager` (none in `endPlay` today; the manager lives
until the session is destroyed), a `const TimeConfig&` dangles in the borrower between the two statements.

*R0, task 27a:* verified: `endPlay` never resets `m_manager`; the coordinator (`ServerReceptionCoordinator`) and the tier
consumer (`ReplicatedTierConsumer`) each keep a reference to the manager's `TimeConfig`. **Corrected:** the moved text said
these two resets are "the only reason neither borrower outlives the TimeConfig" and that deleting either makes a
reference dangle. That was true in the owner, where both were declared above the manager's owner. In the session both
are declared after `m_manager`, and `compositionContractsHold()` asserts it (§2: the tier consumer's row since task 25,
the coordinator's since task 26a), so member destruction destroys both borrowers first: the destruction half of this
rule is held by the compiler. What the tag still guards is the order inside `endPlay`: the resets stay above any
teardown of the manager.

*R0, og-input-latency task P5-7:* og-sim-net's server holds the reception coordinator and a `const TimeConfig&` of its own, and
`compositionContractsHold()` asserts it below the manager; so the rule is the same for the server as it was for the
coordinator, and the reset moved to the server.

*R0, og-session-runtime task 24:* the tier consumer is og-sim-session's `SessionClock`'s member since task 24; `endPlay`
resets it through `m_clock->endTierConsumer()` at the same position, and the clock is declared after `m_manager`
(`compositionContractsHold()`'s `m_manager < m_clock` row), so member destruction still takes the borrower first.

*Moved from `OGBrawlerSimulationManager-guards.md` G-64 (the brawler's task 12 conversion of its `EndPlay` and header
fences) by og-ue-simulationcomponentandmanagersplit task 27a, with the statements it marks: the two resets, the map's
clear and the tier consumer's reset are the session's own statements in `endPlay`.*
