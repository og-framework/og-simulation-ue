<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: OnRep -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: onRelayDelayFloorReplayed -- deleted by og-input-latency task P5-9 (the relay actors, their listeners and the session's two replay members); kept in the history -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root (G-19) -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root (G-19) -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (G-16): the ini key that was built and removed on a ruling. It must NOT resolve -->
# `UESimulationSession.h` — guards

Prohibitions with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulationSession.h`. Ids are opaque,
stable, and retired rather than reused. The reasoning lives in `UESimulationSession-rationale.md`. The class's one
`.cpp`, `UESimulationSession.cpp` (since task 27a: the test hitch cvar, its accessor and its sleep; since og-input-latency
task P5-32 also the hitch's delay cvar and its marker line; rationale §10), shares this pair and carries no tag.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The member order is checked by the compiler, not here: `compositionContractsHold()` asserts the construction chain and
the destruction-order rows (since task 25 also the tier consumer's, and since og-input-latency task P5-7 og-sim-net's
server's, below the manager, its host and the detached lanes), and the constructor asserts `compositionContractsHold()`
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
OGBrawlerUnreal's docs).

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

## G-03 — `requestInputDelayIncreaseStall` keeps its `runsPrediction()` guard

**Tag site:** `UESimulationSession.h`, directly above the `if (!m_manager.has_value() || !m_manager->runsPrediction())`
early return in `requestInputDelayIncreaseStall`.

**The prohibition.** ⛔ Do not remove or weaken the early return: the stall request reaches the manager's client clock only
on a manager that predicts.

**What breaks if the edit is made.** `SimulationManager::editClientClock()` calls `std::terminate()` when the manager holds
no client clock, which is every authority and standalone manager. Remove the guard and the first call that reaches a
manager that does not predict ends the process: not a wrong number, a dead server.

*R0, og-ue-simulationcomponentandmanagersplit task 25:* verified against `SimulationManager.h` (`editClientClock` and
`getClientClock` both terminate on an empty client clock). The fence the entry was converted from named `getClientClock()`;
the call this guard protects is `editClientClock()`, as the brawler's task 11 already noted.

*Moved from `OGBrawlerSimulationManager-guards.md` G-03 (og-netcode-v2-field-defects task 11) by
og-ue-simulationcomponentandmanagersplit task 25, with the member it marks; the text is that entry's without the
header-fence quotation, which stays in the retired entry.*

---

## G-04 — the relay-delay-floor advisory is ADVISORY: never an assert

**Tag site:** `UESimulationSession.h`, directly above the declaration `void logRelayDelayFloorAdvisory(int32 floorTicks);`.

**The prohibition.** ⛔ The advisory reports; it never asserts, `ensure`s or `check`s. Its contract is a log line.

**What breaks if the edit is made.** Floor `0` is the documented "scheduled regime OFF" mode, and the function is called
from every intake point of the floor: the owner's ini override at start-up (`applySessionKnobs` in `beginPlay` since
og-ue-simulationcomponentandmanagersplit task 27b; the owner's `BeginPlay` before) and the floor listener, on a live change and on the latched replay. An
assert here would fire on the shipped default configuration. `classifyRelayDelayFloor` (og-simulation's
`Network/ConnectionTierTable.h`) never flags `0`; this function only reports what that classification says. The same
prohibition is guarded inside the body, at the classification `switch`, by **G-09**: the two tags sit where the two
different wrong edits would be typed (one would change the function's contract, the other would assert in its body).

*R0, task 25:* the moved text said "both intake points — the ini override and the `OnRep`". The floor listener has two
entry points, the change (`onRelayDelayFloorReceived`) and the replay (`onRelayDelayFloorReplayed`), and both reach this
function through `applyReplicatedRelayDelayFloor`, so there are three call paths over two intake sources.

*R0, og-input-latency task P5-7:* the client's floor intake is og-net's since P5-7: the client host applies every floor it is
handed, live or latched, through `onRelayDelayFloorReceived` (`UESimNetHost-guards.md` G-05), so the replay entry point has no
caller; the prohibition is unchanged.

*R0, og-input-latency task P5-9:* the replay entry point is deleted. The function is called from the ini override's intake
(`applySessionKnobs`) and the floor's receipt (`onRelayDelayFloorReceived`, through `applyReplicatedRelayDelayFloor`); the
prohibition is unchanged.

*Moved from `OGBrawlerSimulationManager-guards.md` G-15 (og-netcode-v2-field-defects task 11) by
og-ue-simulationcomponentandmanagersplit task 25, with the declaration it marks.*

---

## G-05 — ONE recompute publishes the effective input delay, over BOTH inputs

**Tag site:** `UESimulationSession.h`, directly above the declaration `int32 recomputeAndPublishEffectiveInputDelay();`.

**The prohibition.** ⛔ One site, both channels: no other code writes the effective-input-delay atomic
(`publishClientEffectiveInputDelayTicks`), and no caller derives half of the formula itself.

```
effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
                                 : rttTierInputDelays[kMaxConnectionTierIndex])
```

**What breaks if the edit is made.** The floor and the tier are two independent og-net Control messages since
og-input-latency task P5-7 (the session relay's and the per-connection relay's properties before), and they land in either
order. Two writers each holding half the formula answer with a stale other half whenever they interleave. Every publish
(the tier change, the floor change and the start-up baseline in `beginTierConsumer`; until og-input-latency task P5-9 also
their replay entry points) goes
through this one function, which reads both cached
inputs through `ReplicatedTierConsumer::effectiveInputDelayTicks`, the same lookups (`tierInputDelayTicks`, which floors,
and the floored worst-tier fallback) that the server's `ServerInputDelayQueue::effectiveDelay` uses for the same
connection. Its return value is the change in published delay, which is how a caller that must pay for an increase can.

*R0, task 25:* verified against `ReplicatedTierConsumer.h` and `ServerInputDelayQueue.h`; "the same derivation the
server's queue mirrors" is exact in the sense above (both ends call the same shared lookups), not a shared function.

*Moved from `OGBrawlerSimulationManager-guards.md` G-16 (og-netcode-v2-field-defects task 11) by
og-ue-simulationcomponentandmanagersplit task 25, with the declaration it marks; the two header-fence quotations stay in
the retired entry.*

---

## G-06 — the tier cache is emplaced on the authority world too

**Tag site:** `UESimulationSession.h`, directly above the authority branch's `beginTierConsumer();` in `beginPlay` (since
task 27b). `beginPlay` calls `beginTierConsumer` once in each role branch, at the position of the two statements it
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

*Moved from `OGBrawlerSimulationManager-guards.md` G-61 (og-netcode-v2-field-defects task 12) by
og-ue-simulationcomponentandmanagersplit task 25. Its site was the authority branch's emplace in the owner's `BeginPlay`;
the emplace and the baseline publish moved into `beginTierConsumer`, and the tag moved with the emplace.*

---

## G-08 — `hadAnyTier` is read before `applyReplicatedConnectionTier`

**Tag site:** `UESimulationSession.h`, the `const bool hadAnyTier =` statement in `onConnectionTierReceived`.

**The prohibition.** ⛔ Read `hadAnyTier` BEFORE `applyReplicatedConnectionTier`, which makes the consumer's
`hasReceivedTier()` true unconditionally.

**What breaks if the edit is made.** If the read comes after the apply, every call reports "had a tier", including the
first. The first real tier resolution then requests a spurious stall.

*R0, task 25:* verified: `ReplicatedTierConsumer::onReplicatedTierReceived` sets its received flag unconditionally.

*Moved from `OGBrawlerSimulationManager-guards.md` G-65 (og-netcode-v2-field-defects task 12) by
og-ue-simulationcomponentandmanagersplit task 25, with the statement it marks.*

---

## G-09 — the relay-delay-floor advisory is a log line, never an assert

**Tag site:** `UESimulationSession.h`, the `switch (classifyRelayDelayFloor(...))` in `logRelayDelayFloorAdvisory`.

**The prohibition.** ⛔ ADVISORY ONLY, never an assert: floor `0` is scheduled-regime-OFF, and `classifyRelayDelayFloor`
never flags it.

**What breaks if the edit is made.** An assert on `BelowHiccupBaseline` takes a server down on a legal configured floor of
`1` (the classification's one value below the hiccup baseline that is not off, unless the worst tier's delay is itself
`1` or less, when `1` is uniform-D).

**The same prohibition, at the declaration:** G-04 carries it at the declaration of `logRelayDelayFloorAdvisory`.

*R0, task 25:* verified against `classifyRelayDelayFloor` (`<= 0` is `None`, uniform-D is tested first, then `== 1`).

*Moved from `OGBrawlerSimulationManager-guards.md` G-66 (og-netcode-v2-field-defects task 12) by
og-ue-simulationcomponentandmanagersplit task 25, with the statement it marks.*

---

## G-10 — the destructor touches no `UObject`: it clears the static instance table and unbinds from the og-net lanes, nothing else

**Tag site:** `UESimulationSession.h`, the first statement of `~UESimulationSession` (the `s_instances[0]` test).

**The prohibition.** ⛔ Do not call `GetWorld()` on the owner, or any other method that reaches the owner actor's outer,
its world or another `UObject`, from the destructor. Unbinding from engine objects belongs in `endPlay`, which the owner's
`EndPlay` calls for every world teardown while the world still exists (the self-scheduled backend removes its post-tick handle and
its two solver callbacks there, in its `onEndPlayWorld`).

**What breaks if the edit is made.** The session is a member of its owner actor, so its destructor runs inside the owner's,
inside the garbage collector's purge, after the owner's outer level may already have been destroyed. The brawler's owner
destructor once called `GetWorld()` to unbind a physics-scene delegate, and that read freed memory: a packaged Development
client crashed with an access violation in the engine's outer lookup under the actor's world getter under that destructor, called from the
engine's map load → memory trim → garbage collection (measured 2026-09-29, og-brawler-uploadtosteam task 12, the first
travel from the standalone join-screen world to a server). An editor `-game` run of the same travel did not crash, so
only a packaged client shows it.

**What breaks if the tag moves.** The tag marks the destructor's body. A `GetWorld()` typed above the tagged statement is
typed directly under the tag. Nothing mechanical checks the destructor: the crash depends on the collector's purge order.

*R0, task 25:* the self-scheduled host's teardown is `ChaosTickBackendUImpl::onEndPlayWorld` (the brawler's), which
removes the inject-inputs and post-tick handles and unregisters the solver callback object; the owner calls it from its
`EndPlay`. The registry's unregistration in the owner's `EndPlay` is unchanged; the destructor is its backstop for an
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
`m_onTimingInfoReceivedCallback`, and since task 26b the two frame probes `m_frameHealthProbe` and `m_connectionBudgetProbe`;
the timing relay pointer until og-input-latency task P5-7), which M4 moves with the functions.

| function | may reach |
|---|---|
| `instanceFor`, `~UESimulationSession` | transport state `s_instances` |
| `setOnTimingInfoReceivedCallback` | transport state `m_onTimingInfoReceivedCallback` |
| `onTimingInfoReceived` | transport state `m_onTimingInfoReceivedCallback`; `m_manager`: `has_value`, `editNetworkEstimator`; **exempt (Q-I):** `m_latencyBudgetDiagnostics.noteWireSample` |
| `publishServerClock_GameThread` | `m_manager`: `has_value`, `runsPrediction`; the accessor `getServerClock` (itself checked: `m_manager`: `getServerClock`); og-sim-net's server (`m_simNetServer`: `has_value`, `publishClock`) since og-input-latency task P5-7 |
| `publishClientEffectiveInputDelayTicks` / `getClientEffectiveInputDelayTicks` | `m_inputResolution`: `setClientEffectiveInputDelayTicks` / `getClientEffectiveInputDelayTicks` (the atomic) |
| `getReplicatedTierConsumer`, `onConnectionTierReceived`, `applyReplicatedConnectionTier` | transport state `m_replicatedTierConsumer` |
| `beginTierConsumer` | transport state `m_replicatedTierConsumer`; `m_manager`: `getTimeConfig` |
| `recomputeAndPublishEffectiveInputDelay` | transport state `m_replicatedTierConsumer`, `m_lastPublishedEffectiveInputDelayTicks`; `m_manager`: `has_value`, `getTimeConfig` |
| `applyTierTransitionStall`, `logRelayDelayFloorAdvisory` | `m_manager`: `has_value`, `getTimeConfig` |
| `applyReplicatedRelayDelayFloor` | `m_manager`: `has_value`, `getTimeConfig`, `setRelayDelayFloorTicks` |
| `requestInputDelayIncreaseStall` | `m_manager`: `has_value`, `runsPrediction`, `editClientClock` |
| `onRelayDelayFloorReceived` | transport calls only (its replay twin and the tier's were deleted by og-input-latency task P5-9) |
| `readSessionKnobs` (task 26b) | nothing of the session: it reads the ini and returns the requests (a static member) |
| `applySessionKnobs` (task 26b) | `m_manager`: `getTimeConfig`, `setRelayDelayFloorTicks`, `setCorrectionRotationK` (the two clamped setters); transport call `logRelayDelayFloorAdvisory`; og-sim-net's server (`m_simNetServer`: `has_value`, `publishFloor`) since og-input-latency task P5-7, where the timing relay's lookup stood |
| `applyResimTriggerPolicy` (task 26b) | `m_manager`: `getTimeConfig`, and the resim-policy sink `setResimTriggerPolicy` |
| `applyLatencyKnobs` (og-input-latency task 4) | `m_manager`: `getTimeConfig`, `setRedundancyFrames`, `setRelayResendEntries` (two more clamped setters) |
| `onFrameStepsDue_Step` (task 26b) | transport state `m_frameHealthProbe`, `m_connectionBudgetProbe`, and its own one-shot latch for the `[PacketBudget]` line (a function-local static); `m_manager`: `has_value`, `runsPrediction`; the server's presence test, `m_simNetServer.has_value` (`⛔G-18`; the coordinator's presence until og-input-latency task P5-7); the owner as an `AActor`: `m_owner.GetWorld` |

**The exemption (ruled Q-I, 2026-10-20).** The wire sample in `onTimingInfoReceived` is the session's own call to its typed
latency member; it stays typed under M4 (its binding would take it), so the ruling exempts it instead of widening §9's list.
The ruling's other exemption, the relay host's character cast in the input-relay listener member, went with that member
(og-input-latency task P5-7).

**One reach §9 does not name.** Since task 26b PROBE 6 reads the owner's world for its net driver (`m_owner.GetWorld()`; the
timing relay's lookup iterated it the same way until P5-7). That is the owner as an `AActor`, a `UObject` reach, not a game
type; M4's binding would need the world or the actor.

**The knobs and the frame probes (task 26b)** are on the seam: the three knob functions reach the manager only through its
session controls (`getTimeConfig` and the two clamped setters) and the resim-policy sink, and the frame probes reach the
manager's role (`has_value`, `runsPrediction`) and the coordinator's presence, nothing else of the coordinator. That
presence test is the one reach into the input-reception state a seam function has, and §9 names it (PROBE 6's
coordinator-presence test).

**A fourth knob function (og-input-latency task 4).** `applyLatencyKnobs` reaches the manager through `getTimeConfig` and
two more clamped setters, `setRedundancyFrames` and `setRelayResendEntries`. They are the same kind of session control
as the first two (an integer clamped into one `TimeConfig` field), so "the clamped setters" above now names four. The
seam script's list predates them: run it with the row above added, or it does not check this function.

**Exempt by design: the input reception and the og-net hookup.** `hasServerTierWiring`, `getReceptionCoordinator`,
`getServerReceptionTick`, `releaseDelayedInputsForStep`, `afterAuthorityBatch` (the reap) and `forEachAuthorityCorrectionTick`
(the walk for the latency member) are typed on the per-step release (task 26a): erasing the game's input type there would put
an indirect call on the step path (manager design §9, step 5). Since og-input-latency task P5-7 they reach og-sim-net's server,
which is typed on the game's composition, and the og-net hookup joins them: `findDriverSimNetLanes`, `beginServerSimNet`,
`beginClientSimNet`, `logSimNetHostLine`, `onSimNetLanesStopping`, `detachFromSimNetLanes`, `clientBindStep`,
`addSimNetCharacter`, `registeredChannelFor`, `forgetSimNetCharacter`, `onSimNetRegisteredRemoved` and `unregisterCharacter`,
with their state (`m_detachedSimNetLanes`, `m_simNetLanes`, `m_simNetServerHost`, `m_simNetServer`,
`m_registeredComponentsById`, `m_simChannelsById`, `m_authoritativeSlotsByActor`, `m_simUnregisteredIds`). og-sim-net is the
typed transport M4 was to make type-free, so they stay in this class, are not on the list above, and the script does not check
them. Of the functions above, `publishServerClock_GameThread` and `applySessionKnobs` reach the server (to publish), and
`onFrameStepsDue_Step` its presence. Until P5-7 this list was the session's own coordinator, the delayed-input map and
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

## G-13 — `forgetSimNetCharacter` stays in the unregistration, after `unregisterSimulatable` and before the game's forget

**Tag site:** `UESimulationSession.h`, directly above `forgetSimNetCharacter(id, isAuthority);` in `unregisterCharacter`
(since og-input-latency task P5-7; until then above the delayed-input map's erase in the session's reception forget, which the
unregistration called at this position).

**The prohibition.** ⛔ The unregistration calls `forgetSimNetCharacter` here, once per id, after `unregisterSimulatable` and
before `m_game.onUnregistered`. Do not drop the call, do not move it above `unregisterSimulatable`, and do not move it out of
the unregistration: it erases the registered-component map, the channel map and the slot, then removes the server's
character (og-sim-net's `removeCharacter`, which destroys the channel) or marks a client's binding unregistered.

**What breaks if the edit is made.**
* Moved above `unregisterSimulatable`: on the server `removeCharacter` destroys the channel the net-sync peer still holds as
  both owners, and `unregisterSimulatable` then clears callbacks on a destroyed object, on the game thread.
* Dropped, or moved out: the maps keep the id after the character left storage. The brawler's ring-out score push
  (`BrawlerRosterUImpl::pushRingoutScores`, from the game's post-visualization hook) fails its fatal check that every
  registered id still has a score entry (the unregistration's first statement, the executor's notify, removed it); the
  latency member's correction-tick walk (`forEachAuthorityCorrectionTick`) reads a channel `removeCharacter` never destroyed,
  so the server keeps relaying and correcting a character that left; and the client's roster ledger stays `registered`, so its
  next Remove unregisters a second time.

**What breaks if the tag moves.** Nothing mechanical checks the order: no test runs a component's teardown against a live
session.

*New in og-ue-simulationcomponentandmanagersplit task 26a (the map's erase, held until then by the statement order of the
owner's unregistration alone). Re-sited and re-worded by og-input-latency task P5-7: the map's erase is part of
`forgetSimNetCharacter`, and the reader that dereferenced the sibling (the session's relay sink) is gone with the sinks.*

---

## G-14 — the relay-delay floor is read from the ini on the authority only

**Tag site:** `UESimulationSession.h`, directly above `if (worldIsAuthority && GConfig != nullptr)`, the floor intake in
`readSessionKnobs`. `beginPlay` calls `readSessionKnobs` (since task 27b; the owner's `BeginPlay` before), at the
position of the intakes it holds (three until og-input-latency task 4 added the latency knobs' two).

**The prohibition, as it stood in the source:**

> ⛔ AUTHORITY ONLY, and that is CORRECTNESS: the floor is REPLICATED, so a client
> reading its own ini could disagree with the server it is meant to match.

**The consequence.** A client that takes its own ini floor predicts against a delay the server does not use.

**What breaks if the tag moves.** Nothing else distinguishes this gate from the rotation-width gate below it. That gate is
authority-only for a different and weaker reason (a client read has no reader).

*R0, task 26b:* verified. A client's floor arrives only through the timing relay: `onRelayDelayFloorReceived` and
`onRelayDelayFloorReplayed` clamp it and hand it to the manager (`applyReplicatedRelayDelayFloor`); `applySessionKnobs`
runs on the authority only (the owner calls it in its authority branch).

*R0, og-input-latency task P5-7:* the floor is og-sim-net's `RelayFloor` Control message since P5-7, published by
`applySessionKnobs` through the server's `publishFloor`; a client still learns it only from the server (the client host hands it
to `onRelayDelayFloorReceived`). Unchanged: the gate.

*Moved from `OGBrawlerSimulationManager-guards.md` G-57 (the brawler's, its task 12 conversion) by
og-ue-simulationcomponentandmanagersplit task 26b, with the intake it marks.*

---

## G-15 — there is no relay-ring depth ini intake, deliberately

**Tag site:** `UESimulationSession.h`, the blank line between the relay-delay-floor intake and the rotation-width intake
in `readSessionKnobs`, where the retired intake used to be. An absence tag: the line under it is blank.

**The prohibition, as it stood in the source** (two fences, one at the intake and one at the apply block. They are one
tag, because the apply block has nothing left to re-add without the intake first):

> ⛔ RETIRED: there is deliberately no relay ring depth ini intake here any more. It read
> a session-configurable retention depth for the outbound ring's replace-latest write
> path; the flush-on-poll replacement takes its capacity from `relayedInputRing::kMaxDepth`,
> a compile-time constant with no ini key, so the intake, its clamp, its setter and its
> startup proof line were all removed together. §6

> ⛔ RETIRED: there is deliberately no relay ring depth clamp, setter or proof-line block
> here any more. It published a session-configurable retention depth that flush-on-poll
> had already made inert - the stage capacity is `relayedInputRing::kMaxDepth`, a
> compile-time constant - so the whole inert path went rather than keep publishing a
> number nothing on the live relay path reads. §6

(The `§6` the quotes end with was the brawler's rationale section on the relay ring; the depth's argument is
`UESimulationSession-rationale.md` §9.8, *The retired fourth knob*, and §9.7, *The relay tap*.)

**The consequence.** Restoring the key restores a knob that nothing on the live path reads. That is the "silently inert
setting" class the brawler's session file shipped three times.

**What breaks if the tag moves.** This is an absence fence: no identifier exists to grep for, and the tag is the only
record.

*R0, task 26b:* verified: the stage capacity is `relayedInputRing::kMaxDepth` (og-simulation's `RelayedInputRingCodec.h`),
read inside the codec and fed to PROBE 5 from the same constant (G-12, retired by og-input-latency task P5-7 with PROBE 5); the
manager has no depth setter (`SimulationManager.h` carries its own absence fence for one).

*Moved from `OGBrawlerSimulationManager-guards.md` G-56 (the brawler's, its task 12 conversion) by
og-ue-simulationcomponentandmanagersplit task 26b, with the intakes it sits between.*

---

## G-16 — the resim-trigger-policy intake is not authority-gated, is read once, and has no cooldown key

**Tag site:** `UESimulationSession.h`, directly above `if (GConfig != nullptr)`, the resim-trigger-policy intake in
`readSessionKnobs`.

**The prohibition, as it stood in the source:**

> ⛔ THERE IS DELIBERATELY NO `ResimCooldownTicks` KEY. A trigger-rate ceiling was built
> here and REMOVED on a user ruling: it defers acting on a correction already known to
> disagree, which is the defect this mechanism repairs. If a design document names that
> key, the document predates the ruling. The throttle is structural instead.
>
> ⚠ NOT AUTHORITY-GATED, unlike the two above: the gate exists ONLY on a predicting client.
>
> ⛔ ONE-SHOT, and here that is THREAD SAFETY: the policy is read unsynchronized at every
> correction landing, which is sound ONLY because it is written once before any land.

and, from the brawler's file banner:

> ⛔ NO KNOB HERE MAY BECOME A CVAR. Each is read ONCE at composition: the
> rotation width because a cadence that moves mid-run makes a probe window
> unattributable, and the resim policy because it is pushed into every
> correction cache and read unsynchronized on the landing path - which is sound
> only because it is written before any correction can land. §3

**The consequence.** Copying the neighbours' `worldIsAuthority &&` guard onto this intake reads the ini on the one role that
cannot use it and skips it on the only role that can. Turning the policy into a cvar makes it a value written mid-session
that `policyEnforcesDepthCeiling` reads on the physics thread with no synchronization.

**What breaks if the tag moves.** The rotation-width intake above has the same no-cvar rule for the weaker reason
(attribution). This tag sits on the intake where the rule is a correctness matter.

*R0, task 26b:* verified: the key is no identifier anywhere in the tree and no ini sets it; the name survives only inside
the removed setter's name, in two comments (og-simulation's own absence fence on the manager, `SimulationManager.h`, and
og-simulation-tests' `ResimGatePolicyTest.cpp`). The manager's `onCheckIsSimilar`
reads the policy through `resimGate::policyEnforcesDepthCeiling` with no synchronization, and `setResimTriggerPolicy` also
pushes it into the reconciliation peer's correction caches (og-simulation's `SimulationManager.h`).

*Moved from `OGBrawlerSimulationManager-guards.md` G-58 (the brawler's, its task 12 conversion) by
og-ue-simulationcomponentandmanagersplit task 26b, with the intake it marks.*

---

## G-17 — the resim-trigger policy is applied and proved once, after both role branches

**Tag site:** `UESimulationSession.h`, directly above `if (knobs.hasIniResimTriggerPolicy)`, the first statement of
`applyResimTriggerPolicy`. `beginPlay` calls `applyResimTriggerPolicy` once, after both role branches (since task 27b;
the owner's `BeginPlay` before), at the position of the scope block it replaced. The first quoted prohibition below, "after
both role branches", is typed at that call, so since task 27b it has its own tag there: **G-24**. This entry keeps the
body's three.

**The prohibition, as it stood in the source:**

> ⛔ AFTER BOTH ROLE BRANCHES: duplicating apply-plus-proof is how the roles drift. §3

> STEP 2 - PARSE + VALIDATE. ⛔ An unrecognised string is REPORTED and the default kept:
> a typo silently selecting the other value changes the gate on a build nobody touched. §3

> STEP 4 - THE PROOF LINE. Unconditional, at Warning; see the banner. §3
>
> ⛔ THIS LINE IS THE BEHAVIOUR-NEUTRALITY RECEIPT: a later claim names WHICH POLICY WAS LIVE.
>
> ⛔ Values are read back from TimeConfig, so it cannot claim a setting nothing stored.
>
> ⛔ `depthPolicy` and `rateLimit` state inertness rather than falling silent. §3

**The consequence.** If apply-plus-proof is duplicated into the two branches, the roles drift. If an unrecognised string
falls through to a value, the gate changes on a build nobody touched. If the proof line becomes conditional, a run's log
can no longer say which policy was live.

*R0, task 26b:* the brawler's entry carried a correction (its rationale §13.10, C13-1): the banner's reason for
`Warning`, that the project ini sets `LogOGNet=Warning`, was false at `b9f6d81`, where that ini set `Verbose`. Re-checked
on today's tree: the brawler project's `Config/DefaultEngine.ini` sets `LogOGNet=Warning` again, so the banner's reason
holds today. The line's other reason, that it is unconditional, holds: the `[ResimGate] session` line is printed on every
path through the function, with the values read back from the manager's `TimeConfig`.

*Moved from `OGBrawlerSimulationManager-guards.md` G-63 (the brawler's, its task 12 conversion) by
og-ue-simulationcomponentandmanagersplit task 26b. Its site was the bare scope block after the role branches in the
owner's `BeginPlay`; the block's statements are `applyResimTriggerPolicy`'s body, and the tag moved to its first statement.*

---

## G-18 — the frame function's coordinator test guards PROBE 6 only, never PROBE A

**Tag site:** `UESimulationSession.h`, directly above `if (!m_simNetServer.has_value()) return;` in
`onFrameStepsDue_Step`, between PROBE A and PROBE 6 (the test was the session's own coordinator's presence until
og-input-latency task P5-7; og-sim-net's server holds the coordinator since).

**The prohibition, as it stood in the source** (in the owner's `releaseDelayedInputsForStep`, before
og-simulationscheduler-withjolt task 51):

> ⛔ The coordinator early-return guards ONLY the drain, never PROBE A, which runs on both. §8

**Rewritten and re-sited, id kept (task 51), in the brawler's file.** Task 51 split the owner's
`releaseDelayedInputsForStep`: PROBE A, PROBE 6 and `[PacketBudget]` moved, in that order, into
`onFrameStepsDue_GameThread`, and the drain kept the old name with a coordinator test of its own. The return this tag
marks stands between PROBE A and PROBE 6. ⛔ Do not hoist it above PROBE A. PROBE 6 and `[PacketBudget]` below it are
server-only and correctly behind it.

**The consequence.** Hoisted to the top of the function, the return stops the frame-health probe on every client, because a
pure client never has an og-sim-net server. The drain and the reap do not depend on it: `releaseDelayedInputsForStep` and the reap,
`afterAuthorityBatch`, each test the coordinator themselves (the reap since og-input-latency task P5-5; until then the
tick-driver binding's `afterInlineBatch` reaped only through a non-null `getReceptionCoordinator()`), and a self-scheduled
host tests it before its drain and reap (the brawler's Chaos host, `InjectInputs_External`, through `hasServerTierWiring()`
since P5-5).

*R0, task 26b:* verified against the session (`releaseDelayedInputsForStep`'s first statement is its own test) and the
brawler's two hosts (`BrawlerTickDriverGameUImpl.h`, `ChaosTickBackendUImpl.cpp`).

*R0, og-input-latency task P5-5:* re-verified: `afterAuthorityBatch`'s one statement is the coordinator test with the reap
under it; `afterInlineBatch` calls it, and `InjectInputs_External` calls the drain and it behind `hasServerTierWiring()`.

*R0, og-input-latency task P5-7:* re-verified on og-sim-net's server: `releaseDelayedInputsForStep` and `afterAuthorityBatch`
each test `m_simNetServer` first, and `hasServerTierWiring()` is its presence.

*Moved from `OGBrawlerSimulationManager-guards.md` G-70 (the brawler's; re-sited on the session's coordinator by task 26a) by
og-ue-simulationcomponentandmanagersplit task 26b, with the test it marks.*

---

## G-19 — the send-budget probe feeds the engine's cumulative counters, not `OutBytes` / `OutPackets`

**Tag site:** `UESimulationSession.h`, directly above the `m_connectionBudgetProbe.noteSample(` call in
`onFrameStepsDue_Step`.

**The prohibition, as it stood in the source:**

> ⛔ CUMULATIVE COUNTERS, NOT `OutBytes`/`OutPackets`: the engine zeroes those mid-window.

**The consequence.** The per-period accumulators reset on the engine's own schedule, so differencing them across a probe
window silently loses whatever was reset in the middle.

*R0, task 26b:* verified: the call passes `OutTotalBytes`, `OutTotalPackets` and `OutTotalPacketsLost`, and the probe's
own declaration states the same rule (og-simulation's `RelayWritePathProbe.h`, `ConnectionBudgetProbe::noteSample`).

*Moved from `OGBrawlerSimulationManager-guards.md` G-72 (the brawler's, its task 12 conversion; the call moved into
`onFrameStepsDue_GameThread` by og-simulationscheduler-withjolt task 51) by og-ue-simulationcomponentandmanagersplit task
26b, with the call it marks.*

---

## G-20 — `m_physicsBackend.onEndPlayBegin()` stays the first statement of `endPlay`: nothing moves above it

**Tag site:** `UESimulationSession.h`, directly above `m_physicsBackend.onEndPlayBegin();`, the first statement of `endPlay`.

**The prohibition.** ⛔ Do not move any `endPlay` statement above `m_physicsBackend.onEndPlayBegin()`, do not add one above
it, and do not drop the call. Everything else in `endPlay` (the og-net unbind, G-21's resets, the static instance table,
the latency member's `end` with its post-tick-flush unbind, the backend's `onEndPlayWorld`) runs after it. The rule is the same for every backend:
- on a driver-scheduled backend (the tick-driver backend, `TickDriverBackendUImpl`), `onEndPlayBegin` unregisters the
  frame host's two tick functions and waits for every outstanding step task (`TickDriverBackendUImpl-guards.md` G-01);
- on a self-scheduled backend it may be empty (the brawler's Chaos host's is an empty inline body today); a teardown of
  such a backend that must run first belongs inside it, not above it.

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

*Moved from `OGBrawlerSimulationManager-guards.md` G-85 (og-simulationscheduler-withjolt task 83, rework 1; the half of the
old G-80 that is typed in the owner's teardown) by og-ue-simulationcomponentandmanagersplit task 27a, with the call it
marks: the owner's `EndPlay` statements before `Super` became `endPlay`.*

---

## G-21 — `endPlay` resets og-sim-net's server and the tier cache while `m_manager` still exists

**Tag site:** `UESimulationSession.h`, directly above `m_simNetServer.reset();` in `endPlay`, after
`detachFromSimNetLanes();`, which follows `m_physicsBackend.onEndPlayBegin();` (`⛔G-20`); the server host's reset, the clears
of the two registered maps and the slot map, the unregistered-id set's clear (since og-input-latency task P5-9) and the tier
cache's reset follow it. (Since og-input-latency task P5-7; until then the tag stood above
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

**Tag site:** `UESimulationSession.h`, directly above `applyResimTriggerPolicy(sessionKnobs);` in `beginPlay`, the first
statement after the role branches.

**The prohibition, as it stood in the source** (the first of the four quotes G-17 carries):

> ⛔ AFTER BOTH ROLE BRANCHES: duplicating apply-plus-proof is how the roles drift. §3

**The consequence.** If the call is duplicated into the two branches, or moved into one, the roles drift: one role applies
or proves a policy the other does not, and the `[ResimGate] session` line no longer comes from one place.

*R0, task 27b:* verified: `beginPlay` calls `applyResimTriggerPolicy` exactly once, after the `if (worldIsAuthority)` /
`else` block and before `m_game.afterSessionConfigured`; the `[ResimGate] session` line is printed by it on both roles.

*Split from G-17 by og-ue-simulationcomponentandmanagersplit task 27b (manager design §4.3; task 26b's A5): G-17's "after both branches" half is typed at the
call, which moved here with the start-up; G-17 keeps the body's three prohibitions.*

---

## G-25 — the systems executor is notified only once the character is in storage, and before `Ready`

**Tag site:** `UESimulationSession.h`, directly above `m_manager->notifyCharacterRegistered(id);` in `tryRegister`, after
`registerSimulatable` and the game's `afterRegistered` hook, before the backend's `onRegistered_GameThread` and the
`return TryRegisterStatus::Ready;`.

**The prohibition, as it stood in the source:**

> Notify the executor: the character is IN STORAGE, so
> brawlerHitRouting::System::onCharacterRegistered can index it and read capsuleBodyId. §10
>
> ⛔ The notify and the drop of the adapter's m_byRootBodyId insert land TOGETHER.

**Why, in the session's terms.** The notify reaches every system of the systems executor whose role affinity fires on this
peer, synchronously on the game thread, and hands each a storage view: a system's `onCharacterRegistered` may read the
just-registered character out of storage. So the notify must follow `registerSimulatable`, which puts the character there,
and must run before `tryRegister` returns `Ready`, after which the owner's component starts using the registration (since
og-input-latency task P5-7 `tryRegister` writes the registered-component map itself, before the notify, and the ring-out push
reads it from the next post-physics pass on; until P5-7 the authority's route entry was written by the binding's registration
skeleton after `Ready`, its guard G-11 then).

**The consequence.** A notify moved above `registerSimulatable` hands a system a character that is not in storage. A notify
moved after the `return` (into the owner's loop) lets a reader of the route table reach a character whose systems have not
indexed it. The brawler has both kinds of dependant: its hit-routing and hit-detection systems read the character's
capsule body id out of storage in `onCharacterRegistered`, and its ring-out score system seeds the roster entry the score
push's fatal check requires (`BrawlerSessionGameUImpl-rationale.md` §7, in OGBrawlerUnreal's docs).

*R0, task 28:* verified: `SimulationManager::notifyCharacterRegistered` forwards to `SimulationSystemsExecutor::notifyCharacterRegistered`
with the storage and the role, which calls each system whose `firesOnRole` holds with a `projectTo` view. The quoted second
line is history: the adapter-owned root-body map is gone; the root-body map is the brawler's hit-routing system's own
(`m_byRootBodyId`), written only by its `onCharacterRegistered`. The source's §10 said the notify ran on the authority path; it
runs on both roles, unconditionally, and the role filter is per system (rationale §12.4).

*Moved from `OGBrawlerSimulationManager-guards.md` G-69 (the brawler's task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 28, with the call it
marks: `tryRegister` became the session's own sequence.*

---

## G-26 — unregistration notifies the executor before `unregisterSimulatable`, behind the `has<>` guard

**Tag site:** `UESimulationSession.h`, directly above `if (m_storage.template has<Simulatable>(id))` at the top of
`unregisterFromNewFramework` (after its hook check).

**The prohibition, as it stood in the source:**

> ⛔ Drop the routing entry BEFORE unregisterSimulatable destroys it, while still in storage. §10
>
> ⛔ The has<> guard is preserved: an unregistered character's view.get<>(id) is unsafe.

**The consequence.** A notify after the destroy lets a system's unregister hook reach a character that storage has already
freed. Dropping the guard notifies for a character that never finished registering (its `tryRegister` returned `Pending`, or
never ran), and a hook's `view.get<>(id)` then has nothing to return.

*R0, task 28:* verified: the brawler's hit-routing system's `onCharacterUnregistered` takes `&view.get<SimulatableBrawler>(id)`
and erases its root-body entries by that pointer, so it needs the character still in storage; the hit-detection and score
systems erase by id. `unregisterSimulatable` is the call that removes the character from storage. The unregistration is
called for any component ending play, including one whose registration never reached `Ready`.

*Moved from `OGBrawlerSimulationManager-guards.md` G-73 (the brawler's task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 28, with the test it
marks: `unregisterFromNewFramework` became the session's own sequence.*

---

## G-27 — the latency-knob intake is not authority-gated

**Tag site:** `UESimulationSession.h`, directly above `if (GConfig != nullptr)`, the latency-knob intake in
`readSessionKnobs` (the `RedundancyFrames` and `RelayResendEntries` keys), after the resim-trigger-policy intake and
before the `return`.

**The prohibition.** ⛔ Do not give this intake its neighbours' `worldIsAuthority &&` guard. `RedundancyFrames` is read
only by a predicting client's send, so an authority-gated read skips it on the one role that uses it.
`RelayResendEntries` is read only by the authority's flush, and reading it on both roles keeps one `TimeConfig` shape, as
the resim-trigger-policy intake does (`⛔G-16`).

**The consequence.** With the guard, a client never reads `RedundancyFrames`. The client's `[LatencyKnobs]` proof line
still prints, reporting `compiled`, so the knob looks applied in the server's log and is silently inert where it acts.

**What breaks if the tag moves.** The floor and rotation-width intakes above are authority-gated for their own reasons
(`⛔G-14`), so copying their shape is the natural edit; this tag is what marks the difference at the line where it would
be typed.

*R0, og-input-latency task 4:* written with the code. `applyLatencyKnobs` runs after both role branches in `beginPlay`,
and on every peer it prints one `[LatencyKnobs]` line with the values read back from the manager's `TimeConfig`. The
fields have no reader yet (og-input-latency tasks 7 and 8 add them); the prohibition is about the intake's shape, which
those tasks rely on.

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
of `unregisterFromNewFramework` after its hook check (og-input-latency task P5-7).

**The prohibition.** ⛔ Keep this early return, and keep it first. A client's roster Remove of a registered id
(`onSimNetRegisteredRemoved`) and the og-net lanes' stop (`onSimNetLanesStopping`) unregister a character before its pawn ends
play, and each records the id; the pawn's `EndPlay` then calls this function for the same id, and must do nothing. Do not
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
  The brawler's hit routing takes `&view.get<SimulatableBrawler>(id)` there (`⛔G-26`'s R0), so it reads a destroyed object.
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
of a character that is not there (G-26's reason). Moved out of the hook, the notify runs after the facade destroyed the storage
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

**What breaks if the tag moves.** Nothing at compile time; with `og.Sim.AuthorityLoop=1` a leave or a hit after a leave reaches
freed storage, or a join races a step.

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

**Was:** *"the correction-tick loop skips a component whose binding is not bound"*. Its tag sat above the skip in
`forEachAuthorityCorrectionTick`.

**Why it retired.** The walk reads og-sim-net channels since P5-7, which have no binding to lose: a channel exists exactly while
its character is registered (`m_simChannelsById` is written at `Ready` and erased by `forgetSimNetCharacter`, `⛔G-13`). The
walk's remaining skips (a buffer shorter than its header, tick 0) are plain bounds.
