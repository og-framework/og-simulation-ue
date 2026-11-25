<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: applyTierTransitionStall -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
<!-- lint-external-ref: hadAnyTier -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
<!-- lint-external-ref: shouldStallForTierTransition -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
<!-- lint-external-ref: OnRep -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root (retired G-19) -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root (retired G-19) -->
# `UESimulationSession.h` — guards

Prohibitions with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulationSession.h`. Ids are opaque,
stable, and retired rather than reused. The reasoning lives in `UESimulationSession-rationale.md`. The class's one
`.cpp`, `UESimulationSession.cpp` (since task 27a: the test hitch cvar, its accessor and its sleep; since og-input-latency
task P5-32 also the hitch's delay cvar and its marker line; rationale §10), shares this pair and carries no tag.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The member order is checked by the compiler, not here: `compositionContractsHold()` asserts the construction chain and
the destruction-order rows (since task 25 also the tier consumer's, the clock's since og-session-runtime task 24, and since og-input-latency task P5-7 og-sim-net's
server's, below the manager and its host; the session's own detached lanes until og-session-runtime task 14), and the constructor asserts `compositionContractsHold()`
(rationale §2). So is the shape of `getTimeConfigPtr` (a pointer, rationale §8), so is the owner type, og-sim-net's one
channel for both roles (since P5-7; the session's being both reception sinks before, rationale §16.7), so is the order of the
client's og-net bind after the manager and the tier consumer (a `checkf` in `beginClientSimNet` since P5-7, which replaced
G-07), and so is PROBE A's window length against the resim-gate probe's (a `static_assert` in
`onFrameStepsDue_Step`, rationale §9.9).
So are the game's hooks: each sequence that calls them checks them first (`beginPlay`, rationale §11.1; `tryRegister` and
`unregisterFromNewFramework`, rationale §12.5), and since task 28 `compositionContractsHold()` asserts that no session member
shares a hook's name (rationale §12.5). Since task 34 it also asserts that each of the five core-system aliases names the
composition's own type (rationale §13). Since task 35 it also asserts that the two executors, `IntegrationExec` and
`SystemsExec`, name the composition's own types, and that `EngineTraits` models `PhysicsEngineTraits` (rationale §14).

---

## G-01 — the systems executor is emplaced beside the adapters, and hands a body-reading system the same body adapter the integration layer uses

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

## G-02 — `getVizSlice` reads the VIZ snapshot, never the live state

**Tag site:** `UESimulationSession.h`, directly above the `return vizState->getState().template get<SliceT>();` statement in
`getVizSlice` (until og-input-latency task P5-16, the `return m_storage.template get<Simulatable>(id).getVizState()...`
statement).

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




## G-06 — the tier cache is emplaced on the authority world too

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


## G-10 — the destructor touches no `UObject`: it clears the static instance table and unbinds from the og-net lanes, nothing else

**Tag site:** `UESimulationSession.h`, the first statement of `~UESimulationSession` (the `unregisterInstance()` call; the
`s_instances[0]` test until og-session-runtime task 16b).

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

## G-11 — the transport seam: a transport function reaches only its allow-list (deny by default)

**Tag site:** `UESimulationSession.h`, directly above `instanceFor`, the first declaration of the transport section.

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
`getServerReceptionTick`, `releaseDelayedInputsForStep` and `afterAuthorityBatch` (the reap) are typed on the per-step release
(task 26a; the in-frame server's correction-tick walk for the latency member too, until og-session-runtime task 14): erasing the game's input type there would put
an indirect call on the step path (manager design §9, step 5). Since og-input-latency task P5-7 they reach og-sim-net's server,
which is typed on the game's composition, and the og-net hookup joins them: `findDriverSimNetLanes`, `beginServerSimNet`,
`beginClientSimNet`, `logSimNetHostLine`, `onSimNetLanesStopping`, `detachFromSimNetLanes`, `clientBindStep`,
`forgetSimNetCharacter` and `onSimNetRegisteredRemoved`, with their state (`m_simNetLanes`, `m_simNetServerHost`,
`m_simNetServer`, `m_registeredComponentsById`, `m_authoritativeSlotsByActor`, `m_simUnregisteredIds`). og-sim-net is the
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

---

## G-20 — `m_physicsBackend.onEndPlayBegin()` stays the first statement of `endPlay`: nothing moves above it

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

## G-21 — `endPlay` resets og-sim-net's server and the tier cache while `m_manager` still exists

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

---

## G-22 — the field-diff predicate must ask the category and verbosity `RouteOGMessage` sends `[DivergenceProbe` to, live

**Tag site:** `UESimulationSession.h`, directly above the `correctionFieldDiff::setEnabledPredicate(` call in the private
static member `bindCorrectionFieldDiffGate`, which `beginPlay` calls at the end of each role branch.

**The prohibition, as it stood in the source:**

> ⛔ A PREDICATE, NOT A LATCHED BOOL, AND THAT IS WHY IT IS A LAMBDA. `UE_LOG_ACTIVE`
> re-reads the category's CURRENT verbosity, so `LogOGDivergenceProbe Verbose` typed into
> the console mid-session starts naming fields on the next correction, and Warning stops
> it again. Reading it once at BeginPlay would pin the session to whatever the ini said.
>
> ⛔ IT MUST NAME THE SAME CATEGORY `RouteOGMessage` SENDS `[DivergenceProbe` TO, at the
> SAME verbosity the line carries (`[Verbose]`). Re-route that prefix and this gate starts
> answering about a channel the line no longer uses — it would then pay for a walk whose
> result is dropped, or drop a walk whose line is printed. This predicate and
> `RouteOGMessage`'s `[DivergenceProbe` arm are the ONLY TWO SITES IN CODE that USE this
> category — one reads its verbosity, the other logs to it; the DECLARE, the DEFINE and the
> ini value are the only other mentions. Change one and you must change the other.

**The consequence.** A predicate asking a different category than the router uses either pays for a per-field walk whose
line is then dropped, or skips a walk whose line is printed without its `field=` tail. A latched bool loses the console
switch mid-session.

**What breaks if the tag moves.** The other half of this pair is the route: og-simulation's `kSimulationLogRoutes` row
for `SimulationLogRoute::DivergenceProbe`, which `ogLogRouteOf` maps to `OGLogRoute::DivergenceProbe`, the arm of
`RouteOGMessage` that logs to `LogOGDivergenceProbe` (`UESimulationLogRoutes.h`). The route table's `static_assert`s check
its rows, not that the route and this predicate name the same category; this tag is the only thing that says so at the
predicate. A re-route is typed at either of the two route sites.

*R0, task 27b:* verified: in code, `LogOGDivergenceProbe` is used at exactly two sites, this predicate's `UE_LOG_ACTIVE`
and `RouteOGMessage`'s `DivergenceProbe` arm; its other code mentions are the `DECLARE` (`UESimulationLogRoutes.h`), the
`DEFINE` (`UESimulationLogRoutes.cpp`) and a game's ini value (the brawler's `Config/DefaultEngine.ini` sets `Verbose`);
every other mention is a comment. og-simulation's predicate is empty until a host sets one, and
`correctionFieldDiff::enabled()` is false while it is empty.

*Moved from `OGBrawlerSimulationManager-guards.md` G-50 (the brawler's task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 27b, with the function
it marks: the brawler's file-local `bindCorrectionFieldDiffGate` became the session's private static member, unchanged,
and the start-up that calls it became `beginPlay`.*

*R0, og-input-latency task 2h:* two statements above are no longer true, and one never was. Since task 2h the category
has a THIRD use in code, the margin predicate below this one (`UE_LOG_ACTIVE(LogOGDivergenceProbe, Log)`, `⛔G-28`), so
the quoted "ONLY TWO SITES" and 27b's "exactly two sites" now read "three". And the brawler's `Config/DefaultEngine.ini`
sets the category to `Warning`, not `Verbose`, at `HEAD` and in today's tree (`LogOGDivergenceProbe=Warning`). The
prohibition itself is unchanged.

---

## G-23 — the authority branch sets the neutral input too, before any registration

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

## G-24 — the resim-trigger policy is applied once, after both role branches

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

## G-28 — the margin predicate must ask the category `RouteOGMessage` sends `[DivergenceProbe` to, at `Log`, live

**Tag site:** `UESimulationSession.h`, directly above the `correctionVerdictMargin::setEnabledPredicate(` call in
`bindCorrectionFieldDiffGate`, the second predicate install, after G-22's and a `static_assert` on the route.

**The prohibition.** ⛔ The margin predicate is G-22's twin at another level: a lambda that re-reads
`UE_LOG_ACTIVE(LogOGDivergenceProbe, Log)` on every call, never a bool latched at `beginPlay`, and never a level other
than the one the `[DivergenceProbe.Margin]` line is printed at. That line carries no severity token, so it goes out at
the `[DivergenceProbe` route's plain level, `Log`. Do not change the predicate's category or level without the route,
and do not raise it to `Verbose` to share G-22's answer.

**The consequence.** A predicate asking another category, or a higher level, pays for a walk of every landed verdict
whose line is dropped, or drops the walk while the line would print: `LogOGDivergenceProbe=Log` then shows no Margin
lines at all. A latched bool loses the console switch mid-session. At `Verbose` the margin would run only when the
per-correction Verbose lines also run, which is the cost the second predicate exists to avoid.

**What breaks if the tag moves.** Half of the pair is checked by the compiler: the `static_assert` above the call fails
if `[DivergenceProbe.Margin]` stops routing to `OGLogRoute::DivergenceProbe` at plain level `ELogVerbosity::Log`. The
other half is not: that `OGLogRoute::DivergenceProbe` logs to `LogOGDivergenceProbe` is a `switch` arm in
`RouteOGMessage` (`UESimulationLogRoutes.h`), which is not `constexpr`. This tag is what says so at the predicate.

*R0, og-input-latency task 2h:* written with the code. `ogLogRouteFor(TEXT("[DivergenceProbe.Margin]"))` finds the row
whose prefix is `[DivergenceProbe` (`kSimulationLogRoutes`, plain level `HostLogLevel::Log`, mapped to
`ELogVerbosity::Log`). `RouteOGMessage` strips only `[Verbose]` and `[Warning]`, then matches the prefix and sends a
`DivergenceProbe` line to `LogOGDivergenceProbe`. The brawler's `Config/DefaultEngine.ini` ships the category at
`Warning`, so the predicate answers false there. og-simulation's `correctionVerdictMargin::enabled()` is false while
no predicate is installed.

---

## G-30 — the pawn's unregistration returns at once for an id already unregistered by a roster Remove or the lanes' stop

**Tag site:** `UESimulationSession.h`, directly above `if (m_simUnregisteredIds.erase(id) != 0u) return;`, the first statement
of `unregisterFromNewFramework` after its hook check (og-input-latency task P5-7) and its no-session return
(og-session-runtime task 11, `UESimulationSession-rationale.md` §23).

**The prohibition.** ⛔ Keep this early return, and keep it first (after the no-session return, which runs only in a
world that registered nothing). A client's roster Remove of a registered id
(`onSimNetRegisteredRemoved` on the step, its host work finished and the id recorded by `collectLifecycleOutcomes_GameThread`
since og-session-runtime task 12) and the og-net lanes' stop (`onSimNetLanesStopping`) unregister a character before its pawn
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

## G-31 — an immediate client's queued unregistration notifies the executor before it enqueues, behind the `has<>` guard

**Tag site:** `UESimulationSession.h`, `enqueueQueuedUnregister_Step`, directly above
`if (m_systemsNotified_Step.erase(id) != 0u && m_storage.template has<Simulatable>(id))`, before `client.unregisterCharacter`
(og-input-latency task P5-17).

**The prohibition.** ⛔ Keep the systems executor's notify here, before the enqueue, and keep both conditions. Do not move the
notify into the unregister's `applied` hook, and do not drop the notified-set test or the storage test. The enqueued job runs at
the top of the drain that follows, and og-sim-net's `unregisterSimulatable` takes the character out of storage first; the
`applied` hook runs after it.

**What breaks if the edit is made.**
* Moved into `applied`, or after the drain: a system's `onCharacterUnregistered` reaches a character storage has already freed.
  The brawler's hit routing takes `&view.get<SimulatableBrawler>(id)` there (the R0 of G-26, retired by og-session-runtime task 14), so it reads a destroyed object.
* The notified-set test dropped: the executor is told of the removal of a character it was never told of (one whose register
  applied while an unregister was pending, `⛔G-32`). The brawler's systems erase by id or by pointer and tolerate it; the pair of
  notifies no longer matches.
* The storage test dropped: the notify runs for an id og-sim-net already removed (a roster Remove in an earlier drain), and the
  hook's `view.get<>(id)` has nothing to return.

**What breaks if the tag moves.** Nothing mechanical: no test unregisters a character from a live immediate session.

---

## G-32 — an immediate client notifies the executor of a registration only in its `applied` hook, and not when an unregister is pending

**Tag site:** `UESimulationSession.h`, `onQueuedRegisterApplied_Step`, directly above
`if (m_unregisterRequested_Step.find(id) == m_unregisterRequested_Step.end())`, which guards `notifyCharacterRegistered`
(og-input-latency task P5-17).

**The prohibition.** ⛔ The queued registration's executor notify stays here: after og-sim-net's `registerSimulatable` put the
character in storage, on the drain's thread, and only when no unregister request for the id is pending. Do not move it to the game
thread's finish (`finishQueuedRegistration`), and do not drop the pending-unregister test.

**What breaks if the edit is made.**
* Moved to the game thread: the notify writes the systems' maps (the hit routing's root-body map, the score map) while a step may
  be reading them on a worker, and every step that ran between the apply and the finish integrated the character with no
  hit-routing or hit-detection entry, so hits on it went unrouted.
* The pending-unregister test dropped: a character unregistered before its registration applied (its pawn ended play in
  between) is registered with the executor by its register job and removed from storage by the unregister job that follows it in
  the same drain, with no notify between (`⛔G-31` found it not notified). The executor keeps a pointer to a destroyed character.

**What breaks if the tag moves.** Nothing mechanical.

---

## G-33 — an immediate client's game-thread unregistration releases the bodies only after the step has applied it

**Tag site:** `UESimulationSession.h`, `requestQueuedUnregister_GameThread`, directly above
`unregisterCharacterHostOnly(id, &owner, /*releaseBodiesNow=*/false);` (og-input-latency task P5-17).

**The prohibition.** ⛔ When the id is queued or registered, the game thread posts the request, holds the component and runs the
host work with `releaseBodiesNow` false. Do not pass true here, and do not call the backend's `onUnregistered_GameThread` before
the `UnregisterApplied` outcome (`collectLifecycleOutcomes_GameThread`) or the teardown.

**What breaks if the edit is made.** The backend's unregistration releases the character's physics slot, unbinding its bodies from
the bind table on the game thread (`JoltTickWorldHost::release`). The character is still in storage until a step's drain
applies the queued unregister, and that drain runs at the start of the first step that begins after the request is posted
(`receiveForClientStep_Step`, §18.2 of the rationale). The game thread does not wait for the steps: a frame-bounded step runs on
an executor task while the game thread runs, so a step that began before the request was posted, past its drain, can still be running, and it
integrates a character whose bodies are no longer bound. (Before og-input-latency task P5-30 the drain ran after the integrate,
so the first step after the post integrated the character too.)

**What breaks if the tag moves.** Nothing mechanical; a leave in an immediate smoke may survive it when no step falls in the window.

---

## G-34 — an immediate client's teardown stops the steps before it unregisters anything

**Tag site:** `UESimulationSession.h`, `teardownImmediateClient_GameThread`, directly above `m_physicsBackend.onEndPlayBegin();`
(og-input-latency task P5-17).

**The prohibition.** ⛔ The teardown's first statement after its once-only test stops the backend's steps and waits for them. Do
not move anything above it, and do not drop it on the grounds that `endPlay` already called it: the lanes' stop handler calls the
teardown with the steps still running.

**What breaks if the edit is made.** The teardown's game-thread drain and its unregistrations then run while a step drains the
same og-sim-net session and runs the same simulation on a worker: two drains (og-sim-net's in-drain flag `OG_CHECK`s one that
overlaps; one that does not overlap is a data race), and storage changed under a running step.

**What breaks if the tag moves.** Nothing mechanical; a client that leaves while its steps run hits it.

---

## G-35 — the og-sim-net hosts' unregistering hook notifies the executor only for a notified id still in storage

**Tag site:** `UESimulationSession.h`, `onSimNetLifecycleUnregistering`, directly above
`if (m_systemsNotified_Step.erase(id) != 0u && m_storage.template has<Simulatable>(id))` (og-input-latency task P5-19).

**The prohibition.** ⛔ Keep both tests, and keep the notify in this hook. Do not drop the `erase` test, do not drop the `has<>`
test, and do not move the notify after the hook returns.

**The consequence.** og-sim-net calls the hook before the facade of every queue-registered character's unregister (a roster
Remove, a queued unregister, `removeCharacter`; `SimNetHost-rationale.md` §4a), on the drain's thread. Without the `erase` test
the executor hears of one unregistration twice (an immediate client's queued unregister already notified at its enqueue, G-31).
Without the `has<>` test the brawler's `brawlerHitRouting::System::onCharacterUnregistered` takes `view.get<SimulatableBrawler>(id)`
of a character that is not there (the reason of G-26, retired by og-session-runtime task 14). Moved out of the hook, the notify runs after the facade destroyed the storage
entry, or not at all for a roster Remove: the hit-routing system then keeps a pointer to the destroyed brawler (og-input-latency
task P5-17's F1).

**What breaks if the tag moves.** Nothing mechanical. The og-sim-net cases `SimNetLifecycleUnregisteringHook.*` pin that the hook
is called before the facade; nothing reaches this answer in a test (it needs the Unreal session).

---

## G-36 — the authority loop notifies the executor of a registration in the `applied` hook on the runner, and records it

**Tag site:** `UESimulationSession.h`, `onRunnerRegisterApplied_Runner`, directly above `m_manager->notifyCharacterRegistered(id);`
(og-input-latency task P5-19).

**The prohibition.** ⛔ On the authority loop the executor's registration notify runs here, in og-sim-net's `applied` callback, on
the runner, right after the queued registration's facade; and the next statement records the id in `m_systemsNotified_Step`. Do
not move the notify to the game thread's finish of the registration (`tryRegisterOnRunner`'s `applied` branch), and do not drop
the record.

**The consequence.** The executor's systems are stepped by the runner: a game-thread notify mutates them while a step reads them.
Without the record, `onSimNetLifecycleUnregistering` (G-35) finds no notified id at the character's unregistration and never tells
the executor, so the hit-routing system keeps a pointer into freed storage.

**What breaks if the tag moves.** Nothing at compile time; on the authority (every one runs the loop since og-session-runtime
task 14) a leave or a hit after a leave reaches freed storage, or a join races a step.

---

## G-37 — the authority loop releases a character's bodies through og-sim-net's queue once a registration was posted

**Tag site:** `UESimulationSession.h`, `requestRunnerUnregister_GameThread`, inside the runner request, directly above
`if (!registerPosted)` (og-input-latency task P5-19).

**The prohibition.** ⛔ When a registration was posted to the runner for this id (registered, or still in flight), release the
bodies only in the `applied` callback of `SimNetServer::unregisterCharacter`. Do not release them directly, and do not drop
`registerPosted` from the test.

**The consequence.** The registration reaches og-sim-net's lifecycle queue in a runner request and is applied by the server's
drain after every request of that wake. A direct release then runs before a registration still in the queue: the character is
registered afterwards with released bodies, its slot free for the next join, and no owner left on the game thread to unregister
it. Through the queue the unregister is applied after the registration (the queue is FIFO), the systems are notified first (G-35),
and the bodies go after the facade, in that order.

**What breaks if the tag moves.** Nothing mechanical. A pawn that leaves within a frame or two of joining keeps a registered
character in the simulation.

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
loop, whose unregistration goes through og-sim-net's queue on the runner (`requestRunnerUnregister_GameThread`, `⛔G-37`), and a
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
each remaining path's own guard. **Successors:** `⛔G-36` (the authority runner's applied hook) and `⛔G-32` (a client's applied
hook).

### G-26 — RETIRED by og-session-runtime task 14 (2026-11-24): its site was deleted

**Was:** *"unregistration notifies the executor before `unregisterSimulatable`, behind the `has<>` guard"*. Its tag sat above
the `has<>` test at the top of the in-frame unregistration.

**Why it retired.** Task 14 deleted that unregistration with the in-frame server. The same order and the same guard are each
remaining path's own. **Successors:** `⛔G-31` (a client's queued unregistration) and `⛔G-35` (og-sim-net's unregistering hook,
which the authority runner's queue reaches).

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
