<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: writesThisFrame -- a term in the stated ceiling formula (G-12), not a declared identifier -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root (G-19) -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root (G-19) -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (G-16): the ini key that was built and removed on a ruling. It must NOT resolve -->
# `UESimulationSession.h` — guards

Prohibitions with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulationSession.h`. Ids are opaque,
stable, and retired rather than reused. The reasoning lives in `UESimulationSession-rationale.md`. The class's one
`.cpp`, `UESimulationSession.cpp` (since task 27a: the test hitch cvar, its accessor and its sleep, rationale §10), shares this pair
and carries no tag.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The member order is checked by the compiler, not here: `compositionContractsHold()` asserts the construction chain and
the destruction-order rows (since task 25 also the tier consumer's, and since task 26a the reception coordinator's,
below the manager), and the constructor asserts `compositionContractsHold()` (rationale §2). So is the shape of
`getTimeConfigPtr` (a pointer, rationale §8), so is the session's being both reception sinks (rationale §9.7), and so is
PROBE A's window length against the resim-gate probe's (a `static_assert` in `onFrameStepsDue_GameThread`, rationale §9.9).

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
and it names the brawler's detector, so it stayed with the game (`SimulationManagerUImpl-rationale.md` §2, in
OGBrawlerUnreal's docs).

**What breaks if the edit is made.**
* Handed the reader adapter, a body-reading system classifies against the game-thread copy of a body transform, an
  interpolated value from an earlier push, not this tick's, with a staleness that depends on game-thread timing.
  Both adapters satisfy the reader concept, so nothing fails to compile, and no LLT can see it: the LLT targets do not
  compile the Unreal layer.
* The executor cannot be a plain member: a body-reading system stores pointers to the adapters, which are
  `std::optional`s emplaced in the role branches, so a default-constructed executor would hold pointers into empty
  optionals. Emplaced after `m_manager`, the manager would bind a reference to an empty `std::optional`'s storage.

*Moved from `SimulationManagerUImpl-guards.md` G-75 (og-netcode-v2-field-defects task 9, 2026-09-23) by
og-ue-simulationcomponentandmanagersplit task 24a, with the declaration it marks; the text is that entry's, with the
brawler's type names replaced by the session's and the R0 line above.*

---

## G-02 — `getVizSlice` reads the VIZ snapshot, never the live state

**Tag site:** `UESimulationSession.h`, directly above the `return m_storage.template get<Simulatable>(id).getVizState()...`
statement in `getVizSlice`.

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

*Moved from `SimulationManagerUImpl-guards.md` G-04 (og-netcode-v2-field-defects task 11, on `getRingoutVizState`;
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

*Moved from `SimulationManagerUImpl-guards.md` G-03 (og-netcode-v2-field-defects task 11) by
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

*Moved from `SimulationManagerUImpl-guards.md` G-15 (og-netcode-v2-field-defects task 11) by
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

**What breaks if the edit is made.** The floor rides the session relay (the timing relay) and the tier rides the
per-connection relay; the two `OnRep`s are independent and land in either order. Two writers each holding half the formula
answer with a stale other half whenever they interleave. Every publish (the tier change and its replay, the floor change
and its replay, and the start-up baseline in `beginTierConsumer`) goes through this one function, which reads both cached
inputs through `ReplicatedTierConsumer::effectiveInputDelayTicks`, the same lookups (`tierInputDelayTicks`, which floors,
and the floored worst-tier fallback) that the server's `ServerInputDelayQueue::effectiveDelay` uses for the same
connection. Its return value is the change in published delay, which is how a caller that must pay for an increase can.

*R0, task 25:* verified against `ReplicatedTierConsumer.h` and `ServerInputDelayQueue.h`; "the same derivation the
server's queue mirrors" is exact in the sense above (both ends call the same shared lookups), not a shared function.

*Moved from `SimulationManagerUImpl-guards.md` G-16 (og-netcode-v2-field-defects task 11) by
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
ever arrives on an authority world (the server writes the relay property; `OnRep` is a non-authority callback), so the
authority cache stays at the no-tier fallback for the whole session. Without the cache, the recompute returns early and
never publishes, and the effective-delay atomic keeps its initial `0`, where the floored worst-tier fallback is correct.

*R0, task 25:* verified: the atomic is initialized to `0` (`SimulationInputResolution.h`), the recompute returns `0` without
publishing when the consumer is empty, and the local input path reads the atomic once per tick on every role.

*R0, task 27b:* re-checked at the new site: `beginPlay` calls `beginTierConsumer()` on both branches, the authority's after
`applySessionKnobs` and before the connection and input listener registrations, the client's after the neutral input.

*Moved from `SimulationManagerUImpl-guards.md` G-61 (og-netcode-v2-field-defects task 12) by
og-ue-simulationcomponentandmanagersplit task 25. Its site was the authority branch's emplace in the owner's `BeginPlay`;
the emplace and the baseline publish moved into `beginTierConsumer`, and the tag moved with the emplace.*

---

## G-07 — the client binds its relay listeners, then PULLS the latched tier and floor

**Tag site:** `UESimulationSession.h`, directly above the client branch's `replayLatchedClientValues(uWorld);` in
`beginPlay` (since task 27b), directly after the session registers as the connection and input relay listeners. Until task
27b the start-up was the owner's `BeginPlay`, so the tag sat in `replayLatchedClientValues` above the connection relay's
pull (task 25's A5); task 27b re-sited it at the caller, where the order it guards is typed.

**The prohibition.** ⛔ Bind, then pull: neither pull may be deleted, and `replayLatchedClientValues` must not be called
before the session is registered as the connection-relay listener.

**What breaks if the edit is made.** The relay properties dirty only on change, so an `OnRep` that fired before the bind
is never re-notified; the relay latches it and hands it over at the pull (`replayLatchedTier` calls the client slot's
`onConnectionTierReplayed`, `replayLatchedRelayDelayFloor` its `onRelayDelayFloorReplayed`). Delete a pull, or pull before
the bind, and the client runs on the pre-arrival fallback for the whole session. A missing timing relay means no floor
yet; the relay's own client `BeginPlay` replays it if it turns up later.

*R0, task 25:* verified against `SimulationConnectionRelay.cpp` and `SimulationTimingRelay.cpp` (both replays look the
listener up in the client slot of their registries). The order of the bind before the call is the owner's statement
order until task 27b moves the start-up here.

*R0, task 27b:* re-checked: in `beginPlay`'s client branch the two listener registrations precede the call, and the call
precedes `simlog::setGlobal`; both pulls are still `replayLatchedClientValues`'s two statements. Deleting a pull is typed in
that body, which no longer has a tag; the call site's tag is the one that names both halves.

*Moved from `SimulationManagerUImpl-guards.md` G-62 (og-netcode-v2-field-defects task 12) by
og-ue-simulationcomponentandmanagersplit task 25. Its site was the owner's client-branch registration; the two pulls moved
into `replayLatchedClientValues`, and the tag moved with them.*

---

## G-08 — `hadAnyTier` is read before `applyReplicatedConnectionTier`

**Tag site:** `UESimulationSession.h`, the `const bool hadAnyTier =` statement in `onConnectionTierReceived`.

**The prohibition.** ⛔ Read `hadAnyTier` BEFORE `applyReplicatedConnectionTier`, which makes the consumer's
`hasReceivedTier()` true unconditionally.

**What breaks if the edit is made.** If the read comes after the apply, every call reports "had a tier", including the
first. The first real tier resolution then requests a spurious stall.

*R0, task 25:* verified: `ReplicatedTierConsumer::onReplicatedTierReceived` sets its received flag unconditionally.

*Moved from `SimulationManagerUImpl-guards.md` G-65 (og-netcode-v2-field-defects task 12) by
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

*Moved from `SimulationManagerUImpl-guards.md` G-66 (og-netcode-v2-field-defects task 12) by
og-ue-simulationcomponentandmanagersplit task 25, with the statement it marks.*

---

## G-10 — the destructor touches no `UObject`: it clears the static instance table and nothing else

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

*Moved from `SimulationManagerUImpl-guards.md` G-79 (og-brawler-uploadtosteam task 12) by
og-ue-simulationcomponentandmanagersplit task 25: the owner registry moved here, and the owner's destructor, whose whole
body this was, became the session's.*

---

## G-11 — the transport seam: a transport function reaches only its allow-list (deny by default)

**Tag site:** `UESimulationSession.h`, directly above `instanceFor`, the first declaration of the transport section.

**The prohibition.** ⛔ Until the type-free transport (M4, deferred) lands, a transport function reaches typed state only
through what that transport's binding would take (manager design §9): the manager's session controls (`runsPrediction`,
`getTimeConfig`, the two clamped setters and the six clock accessors), the input-delay atomic (through the delay publish
and its read), the resim-policy sink and the coordinator's tier tracking. Every other member, accessor or `TraitsT` reach
in a transport function is forbidden unless the table below names it, and a transport function calls into the session only
through another transport function (or the one accessor the table names).

**The allow-list, built from the moved code (og-ue-simulationcomponentandmanagersplit tasks 25 and 26b).** "Transport
state" is the transport's own members (`s_instances`, `m_replicatedTierConsumer`, `m_lastPublishedEffectiveInputDelayTicks`,
`m_onTimingInfoReceivedCallback`, `m_timingRelay`, and since task 26b the two frame probes `m_frameHealthProbe` and
`m_connectionBudgetProbe`), which M4 moves with the functions.

| function | may reach |
|---|---|
| `instanceFor`, `~UESimulationSession` | transport state `s_instances` |
| `setOnTimingInfoReceivedCallback` | transport state `m_onTimingInfoReceivedCallback` |
| `onTimingInfoReceived` | transport state `m_onTimingInfoReceivedCallback`; `m_manager`: `has_value`, `editNetworkEstimator`; **exempt (Q-I):** `m_latencyBudgetDiagnostics.noteWireSample` |
| `getSyncedTimingBuffer`, `replayLatchedClientValues` | transport calls only (`findTimingRelay`) |
| `findTimingRelay` | transport state `m_timingRelay`; the owner as an `AActor`: `m_owner.GetWorld` |
| `publishServerClock_GameThread` | `m_manager`: `has_value`, `runsPrediction`; the accessor `getServerClock` (itself checked: `m_manager`: `getServerClock`) |
| `publishClientEffectiveInputDelayTicks` / `getClientEffectiveInputDelayTicks` | `m_inputResolution`: `setClientEffectiveInputDelayTicks` / `getClientEffectiveInputDelayTicks` (the atomic) |
| `getReplicatedTierConsumer`, `onConnectionTierReceived`, `applyReplicatedConnectionTier` | transport state `m_replicatedTierConsumer` |
| `beginTierConsumer` | transport state `m_replicatedTierConsumer`; `m_manager`: `getTimeConfig` |
| `recomputeAndPublishEffectiveInputDelay` | transport state `m_replicatedTierConsumer`, `m_lastPublishedEffectiveInputDelayTicks`; `m_manager`: `has_value`, `getTimeConfig` |
| `applyTierTransitionStall`, `logRelayDelayFloorAdvisory` | `m_manager`: `has_value`, `getTimeConfig` |
| `applyReplicatedRelayDelayFloor` | `m_manager`: `has_value`, `getTimeConfig`, `setRelayDelayFloorTicks` |
| `requestInputDelayIncreaseStall` | `m_manager`: `has_value`, `runsPrediction`, `editClientClock` |
| `onConnectionTierReplayed`, `onRelayDelayFloorReceived`, `onRelayDelayFloorReplayed` | transport calls only |
| `onInputRelayHostReady` | **exempt (Q-I):** `TraitsT::Character`, the cast of the relay host's owner actor to the game's character |
| `readSessionKnobs` (task 26b) | nothing of the session: it reads the ini and returns the requests (a static member) |
| `applySessionKnobs` (task 26b) | `m_manager`: `getTimeConfig`, `setRelayDelayFloorTicks`, `setCorrectionRotationK` (the two clamped setters); transport calls `logRelayDelayFloorAdvisory`, `findTimingRelay` |
| `applyResimTriggerPolicy` (task 26b) | `m_manager`: `getTimeConfig`, and the resim-policy sink `setResimTriggerPolicy` |
| `onFrameStepsDue_GameThread` (task 26b) | transport state `m_frameHealthProbe`, `m_connectionBudgetProbe`, and its own one-shot latch for the `[PacketBudget]` line (a function-local static); `m_manager`: `has_value`, `runsPrediction`; the coordinator's presence test, `m_receptionCoordinator.has_value` (`⛔G-18`); the owner as an `AActor`: `m_owner.GetWorld` |

**The two exemptions (ruled Q-I, 2026-10-20).** `onInputRelayHostReady` casts the host's owner to `TraitsT::Character` and
attaches the host to that character's `USimulatableNetSyncComponent`; the wire sample in `onTimingInfoReceived` is the
session's own call to its typed latency member. Both stay typed under M4 (its binding would take them, or the game's
hook would), so the ruling exempts them instead of widening §9's list.

**One reach §9 does not name.** `findTimingRelay` iterates the owner's world for the timing relay (`m_owner.GetWorld()`),
and since task 26b PROBE 6 reads the owner's world for its net driver the same way. That is the owner as an `AActor`, a
`UObject` reach, not a game type; M4's binding would need the world or the actor.

**The knobs and the frame probes (task 26b)** are on the seam: the three knob functions reach the manager only through its
session controls (`getTimeConfig` and the two clamped setters) and the resim-policy sink, and the frame probes reach the
manager's role (`has_value`, `runsPrediction`) and the coordinator's presence, nothing else of the coordinator. That
presence test is the one reach into the input-reception state a seam function has, and §9 names it (PROBE 6's
coordinator-presence test).

**Exempt by design: the input reception (task 26a).** `hasServerTierWiring`, `getReceptionCoordinator`,
`getServerReceptionTick`, `noteDelayedInputComponent`, `deliverRemoteInput`, `relayRemoteInput`,
`beginReceptionCoordinator`, `releaseDelayedInputsForStep` and `forgetReceptionOwner`, with their state
(`m_receptionCoordinator`, `m_relayWriteProbe`, `m_delayedInputComponentsById`), are typed on the per-step release: erasing
the game's input type there would put an indirect call on the step path (manager design §9, step 5). They stay in this
class under M4, so they are not on the list above and the script does not check them. None of the functions above
reaches the map or PROBE 5, and only `onFrameStepsDue_GameThread` reaches the coordinator, for its presence.

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

## G-12 — the relay write probe's capacity is the CODEC CONSTANT `relayedInputRing::kMaxDepth`

**Tag site:** `UESimulationSession.h`, directly above `RelayWriteProbe m_relayWriteProbe{ RelayStageCapacity{ ...kMaxDepth } }`
(PROBE 5, part of the input reception's state, rationale §9.7).

**The prohibition.** ⛔ The probe's stage capacity is `relayedInputRing::kMaxDepth`, og-simulation's codec constant
(`RelayedInputRingCodec.h`). Do not feed the probe any other number: a `TimeConfig` field, a literal or a console variable.

**What breaks if the edit is made.** Under flush-on-poll the observable ceiling per frame is
`min(writesThisFrame, kMaxDepth)`, and the stage the probe models is sized by that compile-time constant, inside the codec.
The session-configurable depth that once sized the ring was retired with the replace-latest write path
(`UESimulationSession-rationale.md` §9.8, *The retired fourth knob*; the brawler's §3 until task 26b). Fed another number,
`observableX1000` reports a ceiling the running write path does not have.

*R0, task 26a:* verified. The probe takes the capacity as a `RelayStageCapacity` and treats 0 as 1; its observable count
is each frame's write run capped at that capacity, summed (og-simulation's `RelayWritePathProbe.h`); `kMaxDepth` is 8.

*Moved from `SimulationManagerUImpl-guards.md` G-17 (task 11's conversion of the owner's header fence) by
og-ue-simulationcomponentandmanagersplit task 26a, with the member it marks.*

---

## G-13 — the delayed-input map's erase stays in the unregistration, ahead of the component's unbind

**Tag site:** `UESimulationSession.h`, directly above `m_delayedInputComponentsById.erase(id);` in `forgetReceptionOwner`.

**The prohibition.** ⛔ `forgetReceptionOwner` erases the character's map entry, and the owner calls it from its
unregistration (the brawler's `unregisterFromNewFramework`, at the position of the statements it holds). Do not drop the
erase, move it out of this function, or move the call out of the unregistration: a game component unregisters first and
only then unbinds from its sibling, so the erase must have run by the time the unbind does.

**What breaks if the edit is made.** The map is walked or resolved by three readers, and an entry that outlives the
unregistration hands each of them a component that is unbound, or a character the simulation has already forgotten:
* the relay tap (`relayRemoteInput` → the binding's `stageRelayedInput`) dereferences the sibling pointer, which the unbind
  has cleared, with no check;
* the brawler's ring-out score push (`BrawlerRosterUImpl::pushRingoutScores`, from the game's post-visualization hook in
  `onPostPhysicsStep` since task 27a, the owner's post-physics step before) fails its
  fatal check that every routed id still has a score entry: the unregistration's first statement removed it (the systems
  executor's unregister notify, which reaches the score system synchronously on the game thread);
* the latency member's server loop reads each component's correction buffer; it skips an unbound component
  (`UELatencyBudgetDiagnostics-guards.md` G-03), the only one of the three with a backstop.

**What breaks if the tag moves.** Nothing mechanical checks the order: no test runs a component's teardown against a live
session, and each reader fails only on the frame after an unregistration that left its entry behind.

*New in og-ue-simulationcomponentandmanagersplit task 26a: the order was a review finding of tasks 21 and 23, held until
now by the statement order of the owner's unregistration alone.*

---

## G-14 — the relay-delay floor is read from the ini on the authority only

**Tag site:** `UESimulationSession.h`, directly above `if (worldIsAuthority && GConfig != nullptr)`, the floor intake in
`readSessionKnobs`. `beginPlay` calls `readSessionKnobs` (since task 27b; the owner's `BeginPlay` before), at the
position of the three intakes it holds.

**The prohibition, as it stood in the source:**

> ⛔ AUTHORITY ONLY, and that is CORRECTNESS: the floor is REPLICATED, so a client
> reading its own ini could disagree with the server it is meant to match.

**The consequence.** A client that takes its own ini floor predicts against a delay the server does not use.

**What breaks if the tag moves.** Nothing else distinguishes this gate from the rotation-width gate below it. That gate is
authority-only for a different and weaker reason (a client read has no reader).

*R0, task 26b:* verified. A client's floor arrives only through the timing relay: `onRelayDelayFloorReceived` and
`onRelayDelayFloorReplayed` clamp it and hand it to the manager (`applyReplicatedRelayDelayFloor`); `applySessionKnobs`
runs on the authority only (the owner calls it in its authority branch).

*Moved from `SimulationManagerUImpl-guards.md` G-57 (the brawler's, its task 12 conversion) by
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
read inside the codec and fed to PROBE 5 from the same constant (`⛔G-12`); the manager has no depth setter
(`SimulationManager.h` carries its own absence fence for one).

*Moved from `SimulationManagerUImpl-guards.md` G-56 (the brawler's, its task 12 conversion) by
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

*Moved from `SimulationManagerUImpl-guards.md` G-58 (the brawler's, its task 12 conversion) by
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

*Moved from `SimulationManagerUImpl-guards.md` G-63 (the brawler's, its task 12 conversion) by
og-ue-simulationcomponentandmanagersplit task 26b. Its site was the bare scope block after the role branches in the
owner's `BeginPlay`; the block's statements are `applyResimTriggerPolicy`'s body, and the tag moved to its first statement.*

---

## G-18 — the frame function's coordinator test guards PROBE 6 only, never PROBE A

**Tag site:** `UESimulationSession.h`, directly above `if (!m_receptionCoordinator.has_value()) return;` in
`onFrameStepsDue_GameThread`, between PROBE A and PROBE 6.

**The prohibition, as it stood in the source** (in the owner's `releaseDelayedInputsForStep`, before
og-simulationscheduler-withjolt task 51):

> ⛔ The coordinator early-return guards ONLY the drain, never PROBE A, which runs on both. §8

**Rewritten and re-sited, id kept (task 51), in the brawler's file.** Task 51 split the owner's
`releaseDelayedInputsForStep`: PROBE A, PROBE 6 and `[PacketBudget]` moved, in that order, into
`onFrameStepsDue_GameThread`, and the drain kept the old name with a coordinator test of its own. The return this tag
marks stands between PROBE A and PROBE 6. ⛔ Do not hoist it above PROBE A. PROBE 6 and `[PacketBudget]` below it are
server-only and correctly behind it.

**The consequence.** Hoisted to the top of the function, the return stops the frame-health probe on every client, because a
pure client never has a coordinator. The drain and the reap do not depend on it: `releaseDelayedInputsForStep` tests the
coordinator itself, the tick-driver binding's `afterInlineBatch` reaps only through a non-null `getReceptionCoordinator()`,
and a self-scheduled host tests it before its drain and reap (the brawler's Chaos host, `InjectInputs_External`).

*R0, task 26b:* verified against the session (`releaseDelayedInputsForStep`'s first statement is its own test) and the
brawler's two hosts (`BrawlerTickDriverGameUImpl.h`, `ChaosTickBackendUImpl.cpp`).

*Moved from `SimulationManagerUImpl-guards.md` G-70 (the brawler's; re-sited on the session's coordinator by task 26a) by
og-ue-simulationcomponentandmanagersplit task 26b, with the test it marks.*

---

## G-19 — the send-budget probe feeds the engine's cumulative counters, not `OutBytes` / `OutPackets`

**Tag site:** `UESimulationSession.h`, directly above the `m_connectionBudgetProbe.noteSample(` call in
`onFrameStepsDue_GameThread`.

**The prohibition, as it stood in the source:**

> ⛔ CUMULATIVE COUNTERS, NOT `OutBytes`/`OutPackets`: the engine zeroes those mid-window.

**The consequence.** The per-period accumulators reset on the engine's own schedule, so differencing them across a probe
window silently loses whatever was reset in the middle.

*R0, task 26b:* verified: the call passes `OutTotalBytes`, `OutTotalPackets` and `OutTotalPacketsLost`, and the probe's
own declaration states the same rule (og-simulation's `RelayWritePathProbe.h`, `ConnectionBudgetProbe::noteSample`).

*Moved from `SimulationManagerUImpl-guards.md` G-72 (the brawler's, its task 12 conversion; the call moved into
`onFrameStepsDue_GameThread` by og-simulationscheduler-withjolt task 51) by og-ue-simulationcomponentandmanagersplit task
26b, with the call it marks.*

---

## G-20 — `m_backend.onEndPlayBegin()` stays the first statement of `endPlay`: nothing moves above it

**Tag site:** `UESimulationSession.h`, directly above `m_backend.onEndPlayBegin();`, the first statement of `endPlay`.

**The prohibition.** ⛔ Do not move any `endPlay` statement above `m_backend.onEndPlayBegin()`, do not add one above
it, and do not drop the call. Everything else in `endPlay` (G-21's resets, the static instance table and the listener
unregistration, the latency member's `end` with its post-tick-flush unbind, the backend's `onEndPlayWorld`) runs after
it. The rule is the same for every backend:
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

*Moved from `SimulationManagerUImpl-guards.md` G-85 (og-simulationscheduler-withjolt task 83, rework 1; the half of the
old G-80 that is typed in the owner's teardown) by og-ue-simulationcomponentandmanagersplit task 27a, with the call it
marks: the owner's `EndPlay` statements before `Super` became `endPlay`.*

---

## G-21 — `endPlay` resets the coordinator and the tier cache while `m_manager` still exists

**Tag site:** `UESimulationSession.h`, directly above `m_delayedInputComponentsById.clear();` in `endPlay`, the first
statement after `m_backend.onEndPlayBegin();` (`⛔G-20`); the coordinator's and the tier cache's resets follow it.

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

*Moved from `SimulationManagerUImpl-guards.md` G-64 (the brawler's task 12 conversion of its `EndPlay` and header
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

*Moved from `SimulationManagerUImpl-guards.md` G-50 (the brawler's task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 27b, with the function
it marks: the brawler's file-local `bindCorrectionFieldDiffGate` became the session's private static member, unchanged,
and the start-up that calls it became `beginPlay`.*

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
owner's registration path, after `beginPlay`.

*Moved from `SimulationManagerUImpl-guards.md` G-59 (the brawler's task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 27b, with the call it
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

## §R — Retired ids

None.
