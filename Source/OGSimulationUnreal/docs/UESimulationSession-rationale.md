<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: getSyncedTimingBuffer -- deleted by og-input-latency task P5-26 (og-simulation's SimulationManagerConcept.h lost the timing-buffer owner concepts, which nothing modelled since P5-7); kept in the history -->
<!-- lint-external-ref: SimulationManagerOwnerConcept -- deleted by og-input-latency task P5-26 (og-simulation's SimulationManagerConcept.h lost the timing-buffer owner concepts, which nothing modelled since P5-7); kept in the history -->
<!-- lint-external-ref: getLocalInputCache -- deleted by og-input-latency task P5-16 (the game-thread readers read the session's display snapshot); kept in the history -->
<!-- lint-external-ref: getRelayedReadObservations -- deleted by og-input-latency task P5-16 (the game-thread readers read the session's display snapshot); kept in the history -->
<!-- lint-external-ref: getRelayedInputArrivals -- deleted by og-input-latency task P5-16 (the game-thread readers read the session's display snapshot); kept in the history -->
<!-- lint-external-ref: getStorage -- deleted by og-input-latency task P5-16 (the game-thread readers read the session's display snapshot); kept in the history -->
<!-- lint-external-ref: onInputRelayHostReady -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulationTimingRelayListener -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulationConnectionRelayListener -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulationInputRelayListener -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ASimulationInputRelay -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: OnRep -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: PreReplication -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: onConnectionTierReplayed -- deleted by og-input-latency task P5-9 (the relay actors, their listeners and the session's two replay members); kept in the history -->
<!-- lint-external-ref: onRelayDelayFloorReplayed -- deleted by og-input-latency task P5-9 (the relay actors, their listeners and the session's two replay members); kept in the history -->
<!-- lint-external-ref: SimulatableNetSyncComponent.cpp -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: UESimulatableOwnerBinding::receiveRemoteMoveBundle -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: USimulatableNetSyncComponent -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: attachInputRelayHost -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: noteDelayedInputComponent -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: replayLatchedClientValues -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: payForIncrease -- removed by og-input-latency task P5-10 (the always-true parameter of applyReplicatedRelayDelayFloor); kept in the history -->
<!-- lint-external-ref: findTimingRelay -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: beginReceptionCoordinator -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: forgetReceptionOwner -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: getDelayedInputComponentsById -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: m_delayedInputComponentsById -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: m_backendWorlds -- RENAMED by og-ue-simulationcomponentandmanagersplit task 35 to m_physicsBackendWorlds; kept in the task-24a seen-failing record of §2 and in §14's record of the rename -->
<!-- lint-external-ref: UNetConnection::Tick -- Unreal Engine method, outside every scan root (§9.9) -->
<!-- lint-external-ref: UNetConnection::IsNetReady -- Unreal Engine method, outside every scan root (§9.9) -->
<!-- lint-external-ref: UDataStreamChannel::Tick -- Unreal Engine method, outside every scan root (§9.9) -->
<!-- lint-external-ref: UDataStreamChannel::WriteData -- Unreal Engine method, outside every scan root (§9.9) -->
<!-- lint-external-ref: GameEngine::GetMaxTickRate -- Unreal Engine method, outside every scan root (§9.9) -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root (§9.9) -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root (§9.9) -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root (§9.9) -->
<!-- lint-external-ref: StatPeriod -- Unreal Engine stat window, outside every scan root (§9.9) -->
<!-- lint-external-ref: DesiredTickRate -- Unreal Engine net-driver field, outside every scan root (§9.9) -->
<!-- lint-external-ref: MaxNetTickRate -- Unreal Engine net-driver setting, outside every scan root (§9.9) -->
<!-- lint-external-ref: BaseEngine.ini -- Unreal Engine's own config file, outside every scan root (§9.9) -->
<!-- lint-external-ref: notReady -- a field NAME inside one probe's log format string, not a declared identifier (§9.9) -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (§9.8, G-16): the ini key that was built and removed on a ruling. It must NOT resolve -->
# `UESimulationSession` — rationale

Companion to `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulationSession.h` and, since task 27a, its one `.cpp`,
`UESimulationSession.cpp`. The source carries no prose: a two-line docs pointer, code, assertion messages, and one-line
tags `⛔G-nn` → an entry in `UESimulationSession-guards.md` (the `.cpp` has none).

> **Licence.** This tier is **MPL-2.0**, matching the code it describes. The class and the text of §2 and §3 moved here
> from the brawler's session actor and its rationale (BUSL-1.1); the user ruled the re-licensing on 2026-10-20.

* **§1** says what the class is, as of og-ue-simulationcomponentandmanagersplit task 24a, and what it does not hold yet.
* **§2** is the member order: construction, destruction, and the assertions that keep it.
* **§3** is the backend's two members.
* **§4** is who reaches the private members, and the interim path to the owner.
* **§5** is the `HasAuthority` poison.
* **§6** is how it is built, and what that changed (lead ruling F1).
* **§7** is the provenance.
* **§8** is the composition reads, the visualization doors and the post-simulation pass (task 24b), and how a game's
  parts and UI reach them.
* **§9** is the transport (task 25 onward): the owner registry, the relay listeners, the effective input delay, the stall
  and the timing relay, and the seam that keeps it ready for the type-free transport; since task 26a the input
  reception (§9.7), which the seam exempts; and since task 26b the session knobs (§9.8) and the frame probes, PROBE A and
  PROBE 6 (§9.9). One contiguous section.
* **§10** is the teardown and the post-physics pass (task 27a): `endPlay`, `onPostPhysicsStep`, the game's
  post-visualization hook, and the test hitch cvars in the `.cpp` (since og-input-latency task P5-32 with its delay and
  its marker).
* **§11** is the start-up (task 27b): `beginPlay`, the game's six start-up hooks and their inline check, the neutral input
  on both roles, the field-diff gate, the step-length check and the backend line.
* **§12** is the registration sequences (task 28): `tryRegister` and `unregisterFromNewFramework`, the pending record, the
  game's four registration hooks and their inline checks, and the name-clash row over all ten hooks.
* **§13** is the core systems' spellings (task 34): the five aliases spelled with og-simulation's own templates, the
  user's ruling behind them, and the rows that check them against the game's composition.
* **§14** is the executors and the engine contract (task 35): `IntegrationExec` and `SystemsExec` spelled with
  og-simulation's executors, the composition's systems list, the `PhysicsEngineTraits` assertion, and the rename of the
  backend's type and members to `PhysicsBackend`, `m_physicsBackend` and `m_physicsBackendWorlds`.
* **§15** is og-input-latency task P5-5: the seams the switch to og-net re-points, the reap `afterAuthorityBatch`, the slot
  doors `physicsSlotFor` and `recordAuthoritativeSlot`, and the correction-tick walk `forEachAuthorityCorrectionTick`.
* **§16** is og-input-latency task P5-7, the switch: the sim traffic on og-net's sim lanes, frame-locked; og-sim-net's server
  in this session and the lanes' client session; registration through the roster; and the two early unregistration paths.
  Where an earlier section describes the relays, the listeners, the session's own coordinator or its component map, §16 is
  what replaced it.
* **§17** is og-input-latency task P5-16: the display snapshot the game thread draws from, where it is published (after
  the step batch on the step's thread for a Jolt client since og-input-latency task P5-36), the game-thread view of it, and
  the doors that read it.
* **§18** is og-input-latency task P5-17: immediate mode on a client (`SimTrafficImmediate=1`), its lines, the step hooks,
  registration through og-sim-net's lifecycle queue, unregistration, teardown, and what frame-locked keeps.
* **§19** is og-input-latency task P5-19: the authority loop (`og.Sim.AuthorityLoop=1`), the dedicated server stepping on its
  runner: the decision, start, og-sim-net and registration on the runner, the game thread on the loop, and stop; and
  task P5-37's step time on the runner's `[AuthoritySimulation]` and `[Release]` lines (§19.7).
* **§20** is og-input-latency task P3-10: the clock's RTT source, its two feeds, the `ClockRttSource` knob and the shadow line;
  and task P3-13's `JitterMultiplier` knob, on the same proof line.
* **§21** is og-input-latency task P5-20: the game-thread paths off the authority on the loop: PROBE A per stepping wake,
  PROBE 6 retired, the authority presentation snapshot, the visualization gate, the probe stamps, and what the game thread
  still does.

---

## §1 What the class is

`UESimulationSession<TraitsT>` is a game's simulation session, the game-agnostic half of what was one session actor. A
game's session owner (`TraitsT::SessionOwner`, an `AActor`) holds it as a member and names its game's types through the
traits (`TraitsT`) and its game's statements through the static hooks of `TraitsT::Game`. It names no game type.

**As of task 24a it is the session core:** the latency member, the backend's worlds, the three adapter slots, the
storage, the static data, the three peers (reconciliation, input resolution, net sync), the systems executor, the
integration layer, the manager and the backend, in that order (§2). **Since task 24b** it also holds the composition
reads, the three visualization doors, `getVizSlice` and the post-simulation pass (§8). **Since task 25** it holds the
first part of the transport (§9): the owner registry, the three relay listener interfaces it implements, the tier
consumer and the effective input delay, the stall request and the timing relay. **Since task 26a** it holds the input
reception (§9.7): the reception coordinator, the delayed-input map, the two reception sinks with PROBE 5, the per-step
release, the reap and the forget. **Since task 26b** it holds the rest of the transport: the three session knobs (§9.8)
and the per-frame work before the steps with its two probes, PROBE A and PROBE 6 (§9.9). **Since task 27a** it holds
the teardown, `endPlay`, and the post-physics pass, `onPostPhysicsStep`, with the test hitch cvar's accessor in its
`.cpp` (§10). **Since task 27b** it holds the start-up, `beginPlay`, with the game's six start-up hooks (§11). **Since
task 28** it holds the registration sequences, `tryRegister` and `unregisterFromNewFramework`, with the pending record and
`TryRegisterStatus`, and calls the game's four registration hooks (§12). **Since og-input-latency task P5-7** its traffic is
og-net's (§16): it holds og-sim-net's server on the authority and binds the og-net driver's client session on a client; the
relay listeners, the timing relay, its own reception coordinator, the delayed-input map, the two sinks and PROBE 5 are gone.
The owner keeps the wrapper that turns its game's
character id into the storage key for `unregisterFromNewFramework` (since task 36 its only one: the `tryRegister` and
`noteDelayedInputComponent` wrappers had no caller since task 29 and are removed), and reaches no private member any
more (§4).

**The brawler** is the one user: `AOGBrawlerSimulationManager` holds `UESimulationSession<BrawlerSessionTraitsUImpl>` as its
member `m_session` (`OGBrawlerSimulationManager-rationale.md` §2, in OGBrawlerUnreal's docs).

**Not copyable.** The copy constructor and assignment are deleted: the members bind each other by reference (§2), and
the backend holds the session by reference.

**Not polymorphic.** From task 25 until og-input-latency task P5-7 it derived from the three relay listener interfaces
(§9), with a virtual destructor and a vtable. Since P5-7 it has no base: the og-net hosts call its public members directly
(§16).

---

## §2 Member order

C++ constructs members in declaration order and destroys them in reverse, whatever the initializer lists say. The order
is today's relative order from the owner, where these members lived until task 24a, with the two references first:

```
m_owner&, m_game& → m_latencyBudgetDiagnostics → m_physicsBackendWorlds → m_physAdapter, m_physReaderAdapter, m_queryAdapter
    → m_storage → m_staticData → m_reconciliation → m_inputResolution → m_netSync
    → m_systemsExec → m_integrationLayer → m_manager → m_physicsBackend
    → m_replicatedTierConsumer, m_lastPublishedEffectiveInputDelayTicks
    → m_detachedSimNetLanes, m_simNetLanes, m_simNetServerHost, m_simNetServer
    → m_frameHealthProbe, m_connectionBudgetProbe
    → m_registeredComponentsById, m_simChannelsById, m_authoritativeSlotsByKey, m_simUnregisteredIds
    → m_onTimingInfoReceivedCallback
    → m_pendingRegistrations
```

* **`m_owner` and `m_game` come first.** They are references, which have no destruction order, and the backend is
  constructed with the session, so both must be bound before it. No backend reads them during its construction today
  (the tick-driver host stores the session in its game binding, the self-scheduled host stores it), but a reference
  declared after `m_physicsBackend` would still be unbound while the backend is built, and one that read it would read an
  unbound reference.
* **The latency member comes next, so it is destroyed last.** It was already declared before the worlds, the manager
  and the backend in the owner. Its initializer is `{ std::as_const(*this) }`: it holds a const reference to the session
  (`UELatencyBudgetDiagnostics-rationale.md` §4 says why the `std::as_const`).
* **`m_staticData{ Game::makeStaticData() }`** is the game's static data, built by its one hook that reads the game's
  static-data console variables once. The hook returns a prvalue, so the member is initialized in place (guaranteed copy
  elision): the static-data type need not be copyable or movable, and the brawler's is neither. The call runs after
  `m_storage` is built and before the peers, where the owner's cvar member and its static data were constructed before
  task 24a.

**The transport state comes last** (task 25), after `m_physicsBackend`, as manager design §4.5 placed it. It is emplaced in
the owner's start-up and reset in `endPlay` (the owner's `EndPlay` until task 27a, §10), so where it sits in construction is unobservable; in destruction
it now goes before the backend and the manager. The registry is a static member and has no place in the order. The
input reception's state (task 26a) sits inside it in the owner's relative order: the coordinator, PROBE 5 and the map,
after the delay publish mark. The map holds weak handles and the probe holds plain counters, so neither one's destruction
is observable; the coordinator borrows the manager's `TimeConfig` and is asserted below the manager. The two frame probes
(task 26b) sit where the owner declared them, PROBE A after the coordinator and PROBE 6 after PROBE 5; both hold plain
counters, so their destruction is not observable either. **Since og-input-latency task P5-7** the og-net members stand where
the coordinator stood (§16): the session's own detached lanes, the lanes pointer, the server's host and the server, which
holds the reception coordinator; the server is destroyed first among them (it references the other three and the manager's
`TimeConfig`), and one row asserts it. The component map is the registered-component map, beside the channel map, the slot map
and the early-unregistration set; PROBE 5 and the timing relay pointer are gone.

**The pending registrations come last** (task 28), as manager design §4.5 placed them. In the owner the map was declared
after the session member, so it was destroyed before the whole session; here it is destroyed first among the session's
members, but after the destructor's body, which clears the registry slots (`⛔G-10`). A record holds a simulatable value
and the game's input provider (the brawler's: a lambda that captures its input component's pointer and the id), and
neither one's destruction touches the registry, so the change is not observable. Its declaration is not asserted: nothing
borrows from it.

**What is asserted.** `compositionContractsHold()` holds six `static_assert`s over `__builtin_offsetof` (four before
task 25, five before task 26a), the `ManagerType` and `getTimeConfigPtr` rows, since task 26a the two reception sink
concepts (§9.7; replaced by og-input-latency task P5-7 with the owner-type row and the re-send bound, §16), since task 34 the
five core-system rows (§13), and since task 35 the two executor rows and the engine contract (§14), and the
constructor asserts it, so every row is checked wherever the constructor is instantiated (a member function of a class
template is compiled only when it is used, and the constructor is the one member every owner uses):

| row | why |
|---|---|
| `m_storage` < `m_staticData` < `m_reconciliation` < `m_inputResolution` < `m_netSync` | construction: each peer binds the members above it in its initializer, and the static data's hook runs between the storage and the peers |
| `Storage`, `Reconciliation`, `InputResolution`, `NetSync` and `ManagerType` are the composition's own `Storage`, `Reconciliation`, `InputResolution`, `NetSync` and `Manager` | the session requires og-simulation's own core systems and spells them itself (user ruling 2026-10-24, §13); a composition that names another type fails here |
| `IntegrationExec` and `SystemsExec` are the composition's own `IntegrationExec` and `SystemsExec`; `EngineTraits` models og-simulation's `PhysicsEngineTraits` | the session requires og-simulation's own executors and spells them itself, and the engine is the game's choice through its traits (user ruling 2026-10-24, §14); a composition whose executors differ, or a traits engine that is no engine, fails here |
| `ManagerType` is the tick-driver game binding's `Manager` | `ManagerType` is private; the tick-driver host names the manager through the game binding (`TraitsT::TickDriverGame<UESimulationSession>::Manager`), so the two must be one `SimulationManager` instantiation |
| `m_physicsBackendWorlds` < `m_physAdapter` < {`m_physReaderAdapter`, `m_queryAdapter`} < `m_integrationLayer` | destruction: the backend's worlds outlive every adapter bound to them, and the integration layer goes before the adapters it holds |
| `m_manager` < `m_physicsBackend` | destruction: the backend holds the step driver, which unregisters its resync callback from the manager's client clock when it goes, so the backend goes first |
| `m_manager` < `m_replicatedTierConsumer` | destruction: the tier consumer borrows the manager's `TimeConfig`, so it goes first (task 25; in the owner it was declared above the manager's owner, and only the owner's `EndPlay` reset protected it; that reset is `endPlay`'s since task 27a, `⛔G-21`) |
| `m_manager` < `m_simNetServer`, and `m_detachedSimNetLanes` < `m_simNetServerHost` < `m_simNetServer` | destruction: og-sim-net's server (with the reception coordinator inside it) references the manager's `TimeConfig`, its host and the lanes' transport, so it goes first (og-input-latency task P5-7; the row was `m_manager` < the session's own coordinator from task 26a) |

Every message names the row it replaced, except the five core-system rows (task 34) and task 35's three rows, which
replaced no check: their messages state the ruling instead (§13, §14). The rows inside the backend and its worlds are their own assertions
(`TickDriverBackendUImpl-rationale.md` §1), and the tick-driver host asserts `m_physicsBackendWorlds` < `m_physicsBackend` itself.

**The reorder hazard, stated exactly.** There is no `-Wreorder`-as-error anywhere in this tree, so a member outside the
asserted chain is unprotected, and a reordered member inside it would still compile were it not for the assertions. It
has not bitten because every peer constructor is a trivial reference store: none calls into a sibling peer during
construction, so a reorder today would only bind a reference to a not-yet-constructed object that is not yet accessed.
It becomes load-bearing the day a peer constructor's body calls a method on one of those sibling references. *Moved from
`OGBrawlerSimulationManager-rationale.md` §2 ("The reorder hazard, stated exactly"), where task 11 of the brawler's comment
extraction first asserted it; that task poisoned three reorders and each fired its assertion.*

**Seen failing (task 24a).** Swapping the declarations of `m_manager` and `m_backendWorlds` (`m_physicsBackendWorlds`
since task 35) fails the build with the
adapter row's message (the task's notes quote it). **Task 25:** declaring `m_replicatedTierConsumer` above `m_manager`
fails the build with the tier consumer's row. **Task 26a:** declaring the reception coordinator above `m_manager` failed
it with the reception coordinator's row (that row is the og-sim-net server's since og-input-latency task P5-7).

**The systems executor** (`⛔G-01`) is emplaced, not constructed: its body-reading systems hold the adapters, which are
emplaced in the role branches.

---

## §3 The backend's two members

`typename TraitsT::PhysicsBackendWorlds m_physicsBackendWorlds` and `typename TraitsT::template PhysicsBackend<UESimulationSession> m_physicsBackend{
*this, m_physicsBackendWorlds }`. The traits pick the configuration's host:

* **A driver-scheduled engine (Jolt)**: the worlds are `TickDriverBackendWorldsUImpl` over the engine's world host, and
  the backend is `TickDriverBackendUImpl` over the game's tick-driver binding, `TraitsT::TickDriverGame<UESimulationSession>`
  (`TickDriverBackendUImpl-rationale.md` §1, §10, §12).
* **A self-scheduled engine (Chaos)**: the game's own host, with an empty worlds struct (the brawler's
  `ChaosTickBackendUImpl-rationale.md`, in OGBrawlerUnreal's docs).

**Destruction order.** The backend is declared last, directly after `m_manager`, and the worlds above the adapters, so
`m_physicsBackend` is destroyed first (on Jolt the tick backend with its driver and presenter, then the render-target table,
the game binding and the frame host, while the manager still lives), then the manager, the integration layer and the
adapters, and last the worlds (on Jolt the world host and its engine). The two rows of §2 assert it, the same in both
configurations. *Moved from `OGBrawlerSimulationManager-rationale.md` §2 ("The backend's two members and the destruction
order", og-simulationscheduler-withjolt task 83).*

**The seam.** The owner calls the backend through one fixed set of entry points, each from exactly one site. The set is
a game concept (the brawler's `PhysicsBackendHost`, `PhysicsBackendHostUImpl.h`), so this class cannot assert it; the
game asserts it beside its traits, on the type this class holds (`BrawlerSessionTraitsUImpl.h` for the brawler).

---

## §4 Access: the friends, the accessors and the interim path

**The friends (ruled, Q-G).** The class befriends `typename TraitsT::template PhysicsBackend<UESimulationSession>`,
`typename TraitsT::template TickDriverGame<UESimulationSession>` and its latency member, `UELatencyBudgetDiagnostics<TraitsT>`.
These were the owner's three friend lines (the configuration's backend, the tick-driver binding template and the latency
template); the owner kept its own three for the interim path below until task 27b deleted them (by then they
covered no call). The hosts need mutable production state on the physics thread (the manager's simulation calls, the adapter
emplacement, the storage), and og-simulation's tick-driver binding hands out `Manager&` and `Storage&`, so they are
friends, not accessor users, and their spellings did not change: `m_owner.m_manager`, `m_owner.m_physAdapter`,
`*m_session.m_manager` read the session's members exactly as they read the owner's.

**The owner was a friend too, until task 28.** The owner's start-up, teardown, registration and the rest of the
transport lived in the owner and read and write the session's members (`m_session.m_manager`, `m_session.m_storage`,
…), and since task 25 its private transport members too: the owner writes the registry slots, calls `beginTierConsumer`,
`replayLatchedClientValues`, `logRelayDelayFloorAdvisory`, `findTimingRelay` and `publishServerClock_GameThread` at the
positions of the statements they hold, and reset the tier consumer in its `EndPlay` until task 27a. Since task 26a it
also calls `beginReceptionCoordinator` and `forgetReceptionOwner` at the positions of the statements they hold (and,
until task 27a, read the coordinator and the delayed-input map in its post-physics step and `EndPlay`). Since task 26b
it calls the three knob functions at the positions of the statements they hold (`readSessionKnobs`, `applySessionKnobs`
in its authority branch, `applyResimTriggerPolicy` after both branches); its frame work is the session's own (§9.9).
Since task 27a its teardown and its post-physics pass are the session's own too (§10): its `EndPlay` calls the public
`endPlay`, then its `Super`, and reads no member. Since task 27b its start-up is the session's own too (§11): its
`BeginPlay` calls `Super`, then the public `beginPlay`, and reads no member; the registry writes, the listener
registrations and every helper call of the start-up are the session's own statements.

**The line went with task 28 (ruling F27b-1).** The design dropped the owner's friendship with the start-up, but task 27b
measured that the owner's two registration functions still reached private members: `tryRegister` called the backend's
two registration entry points, the body adapter's resolvability test, the four-peer registration and the manager's
registration notice, and `unregisterFromNewFramework` the manager's notice, the four-peer unregistration,
`forgetReceptionOwner` and the backend's unregistration (with the line deleted, `C2248` on `m_physicsBackend`, `m_physAdapter`,
`m_storage`, the three peers, `m_manager` and `forgetReceptionOwner`, and nowhere else). The lead ruled the line kept until
those two moved. Task 28 moved them (§12) and deleted `friend SessionOwner;`; every configuration builds without it, so the
owner reaches only the public surface. Since then the class befriends exactly the three Q-G friends above.

**The public surface is narrow.** This paragraph lists it as it grew, task by task; what og-input-latency tasks P5-7, P5-9
and P5-26 removed is named at its end. `owner()` (const and mutable), the latency member's `get`/`edit` pair, since task
24b the composition reads and doors of §8, and since task 25 the transport's public members (§9): `instanceFor`, the five
listener overrides, `getSyncedTimingBuffer` and `setOnTimingInfoReceivedCallback` (og-simulation's owner concept),
`publishClientEffectiveInputDelayTicks`, and two const reads for a game's parts, `getClientEffectiveInputDelayTicks` and
`getReplicatedTierConsumer`; and since task 26a the reception's public members (§9.7): the coordinator's type and its
three accessors, which the component binding's RPC tail called, the route's registration, and the two sinks, which the
coordinator called; and since task 27a `endPlay`, which the owner calls (public, so it still can once task 27b drops the
owner's friendship), and two const reads for the game's post-visualization hook, `getSystemsExec` and
`getDelayedInputComponentsById` (§10); and since task 27b `beginPlay`, which the owner calls after its `Super` (§11); and
since task 28 `tryRegister` and `unregisterFromNewFramework`, which the owner's wrappers call (§12; since task 29 the
component's registration skeleton, `UESimulatableOwnerBinding`, calls `tryRegister` and the route's registration
directly, and since task 36 the owner has no wrapper for either). **Since og-input-latency task P5-7** (§16) the listener
overrides are plain members the og-net hosts call, the timing buffer's accessor, the route's registration and the two sinks
are gone, the game's const read is `getRegisteredComponentsById`, the coordinator's accessors answer from og-sim-net's server,
and `clientBindStep` is the client's roster gate, which the game's key hook calls; the reception's members left are the
coordinator's type, `hasServerTierWiring` (the Chaos host's test), `getReceptionCoordinator` (no caller left in code) and
`getServerReceptionTick` (the server host's `receptionTick()`), §16.3. og-input-latency task P5-9 deleted the two `Replayed`
overrides, so three listener overrides remain (`onConnectionTierReceived`, `onRelayDelayFloorReceived`,
`onTimingInfoReceived`), and task P5-26 deleted `getSyncedTimingBuffer`. The og-sim-net hosts also call
`onSimClockSample` (og-input-latency task P3-10, §20) and `onSimNetLifecycleUnregistering` (`⛔G-35`). The owner keeps its own
`getLatencyBudgetDiagnostics()` / `editLatencyBudgetDiagnostics()`, which forward here, so its latency lookups (`instanceFor`)
are unchanged. *R0, task 28:* the mutable one keeps one caller, the latency member's own `instanceFor`
(`UELatencyBudgetDiagnostics.h`), which reaches it through `TraitsT::SessionOwner::instanceFor`; the const one has no caller
left in code (the self-scheduled host reads the session's own pair). No accessor
hands a game's UI or its parts a mutable production peer: the mutable members of §8 are the storage, the server clock,
the reconciliation and the visualization doors, each one the owner already exposed publicly before task 24b, kept with
its signature (§8). Their one game caller, the brawler's component visualization, only reads the simulatable's
visualization copy through the storage, queries the visualization world, and registers its own visualization volume.

**The interim path (task 24a until task 27a).** Some members the hosts called were still the owner's: the per-frame
work before the steps, the reception coordinator, the per-step release, the post-physics pass and the `UObject` calls
(`GetWorld`, `GetNetMode`). The hosts reached them as `session.owner().X()`, under the owner's existing friend lines;
tasks 26a to 27a re-pointed each to the session as it moved (task 25 moved nothing the hosts call). Task 26a re-pointed
the reception: the tick-driver binding's release and reap, and the self-scheduled host's coordinator test, release and
reap, call the session; task 26b re-pointed the per-frame work, so both hosts call the session's
`onFrameStepsDue_Step`; task 27a re-pointed the post-physics pass, so the tick-driver binding's `onPostPhysics` and
the self-scheduled host's post-tick lambda call the session's `onPostPhysicsStep` (§10). The path is closed: what still
goes through `owner()` is the `UObject` calls, which are the owner's for good (a host's `GetWorld` and `GetNetMode`, the
weak-lambda owners below, the tick-driver host's frame-host owner), and the owner's own `instanceFor`, which returns
`&session->owner()` from the session's registry. That last one, and the weak-lambda owners and the frame host's
`UObject&`, need the mutable `owner()`, so it stays public (task 27a checked; task 27b re-checked: the start-up reads
the owner's `GetWorld()` and `GetNetMode()` through the session's own `m_owner`, so it added no caller, and the callers
above are unchanged). Task 24b re-pointed the composition reads: the tick-driver
host's `runsPrediction()` and the self-scheduled host's four forwards (`getServerClock`, `getClientClock`,
`runsPrediction`, `editStorage`) now call the session directly. The calls are inline forwards, so a step costs the same:
task 24a read the disassembly of the per-step release and the step stamps.

**Weak-lambda owners stay the actor.** A delegate's weak object must be a `UObject`, and this class is not one. Every
`AddWeakLambda` that took the owner still takes it, as `&m_owner.owner()` (the latency member's post-tick-flush binding,
and the self-scheduled host's two physics-scene bindings).

---

## §5 The `HasAuthority` poison

The header defines `HasAuthority` to an undeclared name after its includes and undefines it at its end, as
`TickDriverBackendUImpl.h` does around its bodies. A session's role is its world's net mode (`worldIsAuthority`) or the
manager's `runsPrediction()`, never the owner actor's `HasAuthority()`, which the actor's own `.cpp` poisons the same
way. A call typed anywhere in the header is a compile error; task 24a saw one fail.

---

## §6 How it is built

* **Header-only.** The class and its constructor are in the header; the game instantiates it, as a member of its owner.
  The constructor is defined after the class inside one `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair, and since task
  24b so are the four members that were defined out of line in the owner's `.cpp`, inside its own pair: the
  post-simulation pass and the three visualization doors. The composition reads, `getVizSlice` and the parts' two const
  reads are in the class, as the owner's were in its header, which no pair covered.
* ⚠ **The pair does not decide how the constructor compiles** (lead ruling F1, 2026-10-21). MSVC compiles a template's
  instantiations with the `#pragma optimize` state at the END of the instantiating translation unit, and every file of
  the game ends with `OGSIM_OPTIMIZE_ON`, so in a Development build the constructor, and with it every member
  initializer that moved here, compiles optimized, where the owner's constructor compiled them unoptimized. The output
  is the same; the task's notes list what changed state and where floating point is involved.
* **The game's static data is built in the game's hook,** a non-template function the game compiles under its own pair,
  so the cvar read and the static data's construction keep their optimization state.
* **The transport's out-of-line members (task 25)** are defined after the class in a pair of their own, so the block moves
  as one. By ruling F1 they compile optimized wherever they are instantiated; in the owner, where they were non-template
  members of its `.cpp` under its pair, they compiled unoptimized. The task's notes list them and their floating point.
  The knob functions and `onFrameStepsDue_Step` (task 26b) are defined at the end of the same pair, and compile
  optimized for the same reason; the frame probes' two microsecond stamps are their only floating point, and task 26b's
  notes compare their disassembly before and after.
* **The input reception's out-of-line members (task 26a)** are defined after the transport's pair in a pair of their
  own, so M4 can leave them in place. By ruling F1 they too compile optimized; in the owner they compiled unoptimized.
  The coordinator's accessors and the sinks' declarations are in the class, as the owner's accessors were in its header.
* **The two sequences of task 27a,** `endPlay` and `onPostPhysicsStep`, are defined after the reception's pair in a pair
  of their own. By ruling F1 they compile optimized; in the owner, non-template members of its `.cpp` under its pair,
  they compiled unoptimized. They hold no floating-point arithmetic: the test hitch's `static_cast<float>(hitchMs) /
  1000.f` is the `.cpp`'s (below), because optimized under `/fp:fast` that division became a multiply by `0.001f` and
  changed the slept milliseconds for about half of all values (task 27a's finding F1-b; the lead ruled it fixed, not
  accepted). Their two new const reads are in the class.
* **The start-up of task 27b,** `beginPlay`, is defined in the same pair as the two sequences of task 27a, before them.
  By ruling F1 it compiles optimized; in the owner, a non-template member of its `.cpp` under its pair, it compiled
  unoptimized. Its one floating-point expression is the step-length check (§11.5); the task's notes compare its
  instructions before and after. The game's hooks it calls keep their state: they are non-template functions the game
  compiles under its own pair. `bindCorrectionFieldDiffGate` is defined in the class, outside every pair, as it was
  outside the owner's (a file-local function above the owner's `OGSIM_OPTIMIZE_OFF`); a template member compiles optimized
  anyway (ruling F1).
* **The registration sequences of task 28,** `tryRegister` and `unregisterFromNewFramework`, are defined after the
  sequences' pair in a pair of their own. By ruling F1 they compile optimized; in the owner, non-template members of its
  `.cpp` under its pair, they compiled unoptimized. They hold no floating-point arithmetic. The game's four registration
  hooks keep their state: they are non-template functions the game compiles under its own pair (the brawler's
  `BrawlerSessionGameUImpl.cpp` since its task 33, `OGBrawlerSimulationManager.cpp` before), and so do the game's parts
  they call.
* **The `.cpp` (task 27a)** holds what a header cannot: the test hitch console variable, a namespace-scope object that
  must be registered once, its exported accessor and the exported sleep (§10); since og-input-latency task P5-32 also the
  hitch's delay variable, which the same accessor reads. The plugin compiles it on its own (the
  module is not unity), and the two functions, non-templates, sit in a pair of their own, so their statements keep the
  unoptimized state they had in the owner's post-physics step: the sleep's division compiles to `divss` by `1000.0f`, as
  in the owner, not to a multiply by its reciprocal. The game's post-visualization hook is likewise a non-template function in
  the game's own `.cpp` (the brawler's `BrawlerSessionGameUImpl.cpp` since its task 33, `OGBrawlerSimulationManager.cpp`
  before, under its pair).
* **One member is explicitly instantiated** (task 25): `onInputRelayHostReady` casts to the game's character, so its body
  needs the character's complete type. A virtual member is instantiated wherever the class's vtable is emitted, which
  includes the owner's generated code (its vtable-helper constructor builds the owner's members, the session among them),
  which does not include the character's header. So the game declares that one member `extern template` beside the owner's class and instantiates it once
  in the owner's `.cpp` (the brawler's `OGBrawlerSimulationManager.h` and `.cpp`), the pattern the component binding uses for
  the whole class.

---

## §7 Provenance

Created by og-ue-simulationcomponentandmanagersplit task 24a (2026-10-21) from `AOGBrawlerSimulationManager`'s members and
its `compositionContractsHold()` rows (`OGBrawlerSimulationManager.h`, the brawler's session actor). The member declarations
and their initializers moved unchanged except: the owner's cvar member and its static-data initializer became the game
hook `makeStaticData()`; `*this` in the backend's and the latency member's initializers is the session; the types come
from the traits. The guard on the systems executor moved with it (`OGBrawlerSimulationManager-guards.md` G-75 → G-01 here).

Task 24b (2026-10-21) moved the owner's composition reads, its three visualization doors, `getVizSlice` and
`onPostSimulationGameThread` here, statement for statement (§8), with the guard on `getVizSlice`
(`OGBrawlerSimulationManager-guards.md` G-04 → G-02 here) and the `getTimeConfigPtr` assertion.

Task 25 (2026-10-21) moved the transport's registry, the three relay listener interfaces, the tier and floor helpers, the
effective input delay, the stall request and the timing relay here (§9), with eight guards
(`OGBrawlerSimulationManager-guards.md` G-03, G-15, G-16, G-61, G-62, G-65, G-66 and G-79 → G-03 to G-10 here), the owner's
destructor (now this class's), a new order row (the tier consumer) and a new guard, the transport seam (G-11). The
rationale came from the owner's §1 (its transport parts), §5 and §6 (the host boundary).

Task 26a (2026-10-21) moved the input reception here (§9.7): the coordinator and its emplace (now
`beginReceptionCoordinator`), its three accessors, the delayed-input map and its registration, the two sinks with PROBE 5
(its member too, with its guard: `OGBrawlerSimulationManager-guards.md` G-17 → G-12 here), the per-step release, and the reap
and the forget (now `forgetReceptionOwner`). It added a guard (G-13, the map's erase), an order row (the coordinator) and
the two sink concepts, asserted on the session where the owner asserted them on itself. The rationale came from the
owner's §1 (the wart), §6 (the relay tap), §7 and §8 (PROBE 5).

Task 26b (2026-10-21) moved the rest of the transport here: the three session knobs, as three functions the owner calls at
the positions of the statements they hold (§9.8), and the per-frame work before the steps, `onFrameStepsDue_GameThread`,
with PROBE A, the coordinator test, the `[PacketBudget]` line and PROBE 6, and the two probes' members (§9.9). Six guards
moved with them (`OGBrawlerSimulationManager-guards.md` G-57, G-56, G-58, G-63, G-70 and G-72 → G-14 to G-19 here, in that
order) and one derivation (its D-50 → D-01 here). The rationale came from the owner's §3, the PROBE A and PROBE 6 parts of
its §8, and its §13.8. The seam script gained the four functions (§9.1).

Task 27a (2026-10-21) moved the owner's teardown and its post-physics pass here, as `endPlay` and `onPostPhysicsStep`,
straight-line and statement for statement (§10): the latency member's `end` and `tick_GameThread` became the session's
own statements, and the brawler's ring-out score push became the game's hook `afterVisualization`, at its position. Two
guards moved with them (`OGBrawlerSimulationManager-guards.md` G-85 and G-64 → G-20 and G-21 here). The test hitch console
variable moved, unchanged, into the new `UESimulationSession.cpp`, read through the exported accessor
`consumeTestGameThreadHitchMs`, its sleep through `sleepTestGameThreadHitch` (the lead's F1-b ruling). The owner's `EndPlay` became `endPlay` then `Super`, and both hosts call
`onPostPhysicsStep`. The rationale came from the owner's §1 (its thread table's rows), §2 (the teardown), §16 (the latency
member's call sites) and §18 (the seam's call sites and the hitch variable).

Task 27b (2026-10-21) moved the owner's start-up here, as `beginPlay`, straight-line and statement for statement (§11),
with the owner's file-local `bindCorrectionFieldDiffGate` as a private static member. The latency member's `begin` became
the session's own statement, the step-length check reads `TraitsT::NetConfig` and its `checkf` texts are neutral, and the
brawler statements became six hooks of `TraitsT::Game`, each at its statements' position. Two guards moved with it
(`OGBrawlerSimulationManager-guards.md` G-50 and G-59 → G-22 and G-23 here); G-17's "after both branches" half became G-24 at
the call, and G-06 and G-07 were re-sited at their calls. The owner's third guard of the start-up, G-60 (the neutral input's
value), stayed with the game's hook (ruling Q-J). The rationale came from the owner's §2 (the start-up's order), §4 (the
field-diff gate), §5 (the neutral input on both roles) and §18 (the backend line and the step length).

Task 28 (2026-10-21) moved the owner's registration sequences here, `tryRegister` and `unregisterFromNewFramework`, with
the pending record and `TryRegisterStatus`, statement for statement (§12). The latency member's input-provider wrap and its
forget became the session's own statements, the character cast reads `TraitsT::Character` and its `checkf` text is neutral,
and the brawler statements became four hooks of `TraitsT::Game`, each at its statements' position. Two guards moved with
them (`OGBrawlerSimulationManager-guards.md` G-69 and G-73 → G-25 and G-26 here). The owner's interim friendship was deleted
(§4), and the sentence of the start-up's hook check that no session member shares a hook's name became a compile-time row
(§12.5). The rationale came from the owner's §10 and the generic part of its §13.4.

---

## §8 The composition reads, the visualization doors and the post-simulation pass (task 24b)

Task 24b moved these members from the owner, statement for statement; only the spellings changed (the owner's
m_session.m_X became the session's m_X, the game's simulatable type became `TraitsT::Simulatable`, and a member
template called on a dependent object gained its `template` keyword):

| member | what it forwards to |
|---|---|
| `getServerClock`, `editServerClock`, `getClientClock`, `runsPrediction` | the manager's clocks and role test |
| `editStorage`, `editReconciliation` (mutable and const) | the storage and the reconciliation peer themselves |
| `getLastRelayedInput`, `getLocalInputCache`, `isLocallyControlledOnThisPeer` | the input-resolution peer and its diagnostic view |
| `getRelayedReadObservations`, `getRelayedInputArrivals` | the input-resolution peer's diagnostic view |
| `getVizSlice<SliceT>` | one slice of a simulatable's visualization copy (`⛔G-02`) |

⚠ **Since og-input-latency task P5-16 the visualization rows of this table are history.** `getLastRelayedInput`,
`isLocallyControlledOnThisPeer` and `getVizSlice` answer from the display snapshot, and `editReconciliation`,
`getLocalInputCache` and the two relayed-ring doors are deleted (§17.4). The paragraphs below on `getVizSlice` and the
narrow read doors describe the doors as task 24b moved them.
| `getTimeConfigPtr` | the manager's `TimeConfig`, or a null pointer |
| `editVizQuery`, `getVizReader`, `registerVizVolume` | the backend's three visualization doors |
| `onPostSimulationGameThread` | the manager's post-simulation pass |

Two members are new, and read-only: `getManager()` (the manager's `std::optional`, by const reference) and
`getStorage()` (the storage, by const reference; deleted by og-input-latency task P5-16, §17.4). They exist for a game's parts, which read through a const reference
to the session and are not its friends (below); the owner and the hosts read the members directly.

**The clock reads dereference the manager unchecked,** as the owner's did. They are for callers that run only while
the manager exists: a game's per-character visualization, which runs once the character is in the storage, and the
hosts, once stepping has started. A caller that may run before `BeginPlay` builds the manager asks
`getTimeConfigPtr()` or `getManager()` first.

**`getTimeConfigPtr()` returns a pointer, not a reference,** so the state before the manager is built (no manager,
therefore no `TimeConfig`) is a representable null pointer rather than a reference into an empty optional. Callers bind
lazily and retry. The shape is asserted: a row of `compositionContractsHold()` compares the member's type with
`const TimeConfig* (UESimulationSession::*)() const`. (The owner asserted the same on its own member after its class;
the assertion moved with the member, re-expressed on the session's member pointer.) Nothing in the tree calls it today.

**`getVizSlice<SliceT>(id)`** answers from the simulatable's visualization copy (`⛔G-02`), or `std::nullopt` when the
storage holds no simulatable with that id. It is `const` and returns a copy, never a reference into the storage. A game
asserts both on the instantiation it uses, beside its caller, because a member pointer to the template must name a
concrete slice type, which the session cannot (the brawler: `ScoreboardVisualizationUImpl.cpp`, for
`brawlerRingout::State`). It names no game type: `SliceT` is the caller's.

**The narrow read doors.** `getLastRelayedInput` and `getLocalInputCache` are one-purpose entry points onto objects
driven otherwise only by the manager's tick loop. Handing game-thread code a general handle to the input-resolution
peer would invite exactly the cross-thread reach these doors bound, so do not widen one into an accessor of the peer.
They differ in their thread:
* `getLastRelayedInput` reads the relayed-input store on the game thread, the thread of that store's writer, and
  returns a copy, so it is no crossing (og-simulation's `SimulationInputResolution-rationale.md` §14).
* `getLocalInputCache` and the two relayed rings (`getRelayedReadObservations`, `getRelayedInputArrivals`) are
  game-thread doors onto data the physics thread writes. They return pointers to `const`, and the compiler holds that
  (a mutable pointer from the const diagnostic view does not convert). The tear is the game's to argue at its reader
  (the brawler's input-history display, `InputHistoryDisplay-rationale.md` §8.6, in OGBrawlerUnreal's docs).
* `isLocallyControlledOnThisPeer(id)` is `getLocalInputCache(id) != nullptr`: a capture line exists for exactly the
  characters this peer drives, which stays right under couch co-op and on a listen host, where a role test is wrong.

**The visualization doors are handles,** an exception to the narrow rule that they inherit from the owner's accessors
they replaced (og-simulationscheduler-withjolt task 51): a game's visualizers are templates over the query and reader
types and call them directly, on the game thread. `editVizQuery()` and `getVizReader()` return references to a world
every peer's game thread may query; `registerVizVolume(descriptor, owner)` registers a visualization volume and returns
its optional id, empty while a driver-scheduled host's slot acquire waits. What each host answers is that host's
(`TickDriverBackendUImpl-rationale.md` §3; the brawler's `ChaosTickBackendUImpl-rationale.md`).

**`onPostSimulationGameThread()`** is the manager's post-simulation pass, which `onPostPhysicsStep` calls (§10; the
owner's post-physics step until task 27a) between the backend's present and the timing write. It is defined out of line, after the class. In the owner it had to
be: its body instantiates the manager's net-sync code, which needs the game's complete component type, and an inline
body in the owner's header failed to compile (`C2027`, og-netcode-v2-field-defects task 11). In the session the
body is a template member, compiled only where it is used, in the owner's `.cpp`, which sees the component.

**The game's parts read through `const Session&`** (the initiative's review, finding m-13). A part (the brawler's
input-history feed) holds a const reference to the session, bound in the owner's member initializer to the owner's
`m_session` member before that member is constructed, and read only after it is: the parts are called from the
component's tick, the game's HUD and the unregistration (the game's `onUnregistered` hook, from the session's
`unregisterFromNewFramework` since task 28; the owner's unregistration before), never from a constructor. Binding a reference to the session object is
well-defined then; binding one to a member of the session before the session's construction began would not be
([class.cdtor]/1), which is why no part holds a reference into the session. A part reads only const members, so it
needs no friendship. What it reads of the transport (the tier consumer and the effective delay) are the session's own
const reads since task 25 (§9); before, it read them through the const `owner()`.

**Callers.** A game's UI reaches the session through its owner's accessor (the brawler's `session()`, const and
mutable, beside its `roster()` and `inputHistoryFeed()`); the hosts reach it as the session they hold.

---

## §9 Transport

The session's transport is its link to the replicated relays and the client's input-delay bookkeeping: the owner
registry, the three relay listeners, the tier consumer and the effective input delay, the stall request, and the timing
relay with its callback. Task 25 moved these from the owner, and task 26b the session knobs (§9.8) and the frame probes
(§9.9). Every transport paragraph of tasks 25, 26a and 26b goes in this section, so the type-free transport of manager
design §9 (M4, deferred) can take it as one block. The input-reception section (task 26a) sits here too, as §9.7, marked
as exempt from the seam; M4 leaves it in this class.

**Since og-input-latency task P5-7 the relays are not the transport** (§16). The tier and floor helpers, the effective input
delay, the stall request, the registry, the knobs and the frame probes of this section are unchanged; what they are fed by
changed. The listener interfaces, the timing relay, the session's coordinator, its component map, the two sinks and PROBE 5
are retired, and §9.2, §9.5, §9.6 and §9.7 say so where they described them.

### 9.1 What moved (task 25), and the seam

| member | what it is |
|---|---|
| `instanceFor`, the static registry `s_instances`, the destructor | the owner registry: one session per world role (slot 0 authority, slot 1 client) |
| `onTimingInfoReceived`, `onRelayDelayFloorReceived`, `onRelayDelayFloorReplayed` | `ISimulationTimingRelayListener` |
| `onConnectionTierReceived`, `onConnectionTierReplayed` | `ISimulationConnectionRelayListener` |
| `onInputRelayHostReady` | `ISimulationInputRelayListener` |
| `applyReplicatedConnectionTier`, `applyReplicatedRelayDelayFloor`, `applyTierTransitionStall`, `logRelayDelayFloorAdvisory` | the tier and floor helpers |
| `recomputeAndPublishEffectiveInputDelay`, `publishClientEffectiveInputDelayTicks`, `getClientEffectiveInputDelayTicks`, `getReplicatedTierConsumer` | the effective input delay and the tier consumer |
| `requestInputDelayIncreaseStall` | the stall request |
| `beginTierConsumer`, `replayLatchedClientValues` | extracted from the owner's start-up, called there at the position of the statements they hold |
| `getSyncedTimingBuffer`, `setOnTimingInfoReceivedCallback`, `findTimingRelay`, `publishServerClock_GameThread` | the timing relay, the callback and the server clock's publish |

Every body moved statement for statement; the task's notes hold the token comparison. The spellings changed where the
owner reached the session (`m_session.m_manager` is `m_manager`), where it reached itself as an actor (its world is
`m_owner.GetWorld()`), and in three places that are new or re-targeted:

* **The wire sample** in `onTimingInfoReceived` is the session's own call to its latency member
  (`m_latencyBudgetDiagnostics.noteWireSample`), where the owner called its forward to the same member.
* **`onInputRelayHostReady` attached the host to the character's `USimulatableNetSyncComponent`** (the member was removed
  by og-input-latency task P5-7, the component deleted by P5-8). The owner found the
  game's component and called its `attachInputRelayHost`, which the component's binding forwarded to that same sibling (the
  character bound the two in its `PostInitializeComponents`), so the call landed on the same object. The cast of the host's
  owner to `TraitsT::Character` is the one game type the transport names.
* **`publishServerClock_GameThread`** is new (the initiative's review, finding m-14): the authority's server-clock write into
  the timing relay's buffer, extracted from the owner's post-physics step and called at the position of the block it
  replaced (in `onPostPhysicsStep` since task 27a, §10).

The `getSyncedTimingBuffer` check text is game-neutral (it named the owner's class). No log text, field, level or category
changed.

**The seam (`⛔G-11`).** Until M4 lands, a transport function reaches typed state only through what M4's binding would take
(manager design §9): the manager's session controls, the input-delay atomic, the resim-policy sink and the coordinator's
tier tracking. The guard's table is the exact list, function by function, built from the moved code, with the two ruled
exemptions (Q-I: `onInputRelayHostReady`'s character cast, and the wire sample) and one reach §9 does not name: the owner's
world, which `findTimingRelay` iterates for the relay (and, since task 26b, PROBE 6 reads for its net driver). A script of
the initiative checks the list deny-by-default; the guard says how. Task 26b put the three knob functions and the frame
function on the list, closed a gap the script had (a call through `(*this).` or through a local alias of the session got
past it) and added the arms that show both fail it.

### 9.2 The owner registry and the listeners

**One session per world role.** `instanceFor(isAuthority)` returns the session registered for the authority world (slot 0)
or the client world (slot 1), or a null pointer. A listen server runs one owner and one session per world, so both slots
can be filled in one process. The owner writes its slot in its start-up (the owner's `BeginPlay` until task 27b), with
`checkf` on a slot already taken, and `endPlay` clears it (the owner's `EndPlay` until task 27a, §10). The owner's own `instanceFor` forwards here and returns
the registered session's owner, so its callers (the brawler's component, its HUD, the latency member, the component
binding's reception lookup) are unchanged.

**The registry is an inline static member of the class template.** One copy exists per instantiation and per module that
instantiates it: only the game module instantiates the session over its traits, so there is one table, as there was one
static array in the owner.

**The listeners are the og-net hosts since og-input-latency task P5-7.** From task 25 the session implemented the three
relay listener interfaces and registered itself in each one's registry; P5-7 removed the bases and the registrations. The
same members (`onTimingInfoReceived`, `onRelayDelayFloorReceived`, `onConnectionTierReceived`; their replay twins until
og-input-latency task P5-9 deleted them) are public, and the client host calls them while the session is bound (§16).

**The destructor** clears the registry slot if it still names this session, and unbinds from the og-net lanes it is still
bound to (`⛔G-10`). It is a backstop: `endPlay`, which the owner's `EndPlay` calls, does both first in every world teardown.
It runs inside the owner's destructor, during the garbage collector's purge, which is why it touches no `UObject` (the lanes
are not one). The owner's destructor, whose whole body this was, is gone.

**One listener per WORLD, not per CHARACTER.** A tier is a property of the wire, and every character on a machine shares
one wire. The retired per-character shape had N characters each driving the same effective-delay atomic and each
requesting its own tier-transition stall, so for a couch co-op client one wire transition produced two stall requests
against a clock that accumulates debt. One listener per world makes the request count match the transition count.

### 9.3 Threads: the transport's one crossing

**The write crossing carries one scalar.** `publishClientEffectiveInputDelayTicks` writes a lone `std::atomic<int32>`
inside the input-resolution peer (`SimulationInputResolution`'s effective-delay atomic), which the physics thread loads
once per tick in `collectInputAll` and once per resimulated tick in `collectResimInputAll`. The worst a race can do is
apply a new delay one tick late. Everything else in the transport is game-thread-only: the listener members are called by
the client host from the og-net driver's `PostTickDispatch` (the relays' replication callbacks until og-input-latency task
P5-7), and the registry and the tier consumer are read and written only there, in the start-up, and in the session's
teardown and post-physics pass (§10; the owner's until task 27a). *Moved from the brawler's
`OGBrawlerSimulationManager-rationale.md` §1 ("The write crossing carries one scalar"); R0, task 25: the resimulation load was
not listed.*

**Why the passthroughs are narrow.** `requestInputDelayIncreaseStall` and `publishClientEffectiveInputDelayTicks` are
one-purpose entry points onto objects otherwise driven only by the manager's tick loop (the client clock, the input
resolution's atomic). Handing game-thread code a general mutable handle to one of them would invite exactly the
cross-thread reach the entry point exists to bound, so do not widen one into an accessor. The stall request is private
to the session since task 25 (only the tier and floor helpers call it); the publish is public, and its one caller is the
recompute (`⛔G-05`). `requestInputDelayIncreaseStall` also guards on `runsPrediction()` (`⛔G-03`): a server or standalone
manager has no client clock, and `editClientClock()` would `std::terminate`. *Moved from §1 of the brawler's rationale
("Why the passthroughs are narrow rather than `edit*()` accessors", these two names and its last paragraph).*

### 9.4 The client's effective input delay

Two independent channels feed one number:

```
effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
                                 : rttTierInputDelays[kMaxConnectionTierIndex])
```

The pre-arrival case is therefore `max(floor, rttTierInputDelays[kMaxConnectionTierIndex])`. The published value lands in
the input-resolution peer's effective-delay atomic.

The **floor** and the **tier** are two og-net Control messages since og-input-latency task P5-7 (the session relay and the
per-connection relay carried them before). They are independent and can land in either order, which is exactly
why both call **one** recompute holding **both** cached inputs rather than each writing the atomic on its own (`⛔G-05`):
two writers each holding half the formula would answer with a stale other half whenever their arrivals interleaved. The
start-up baseline publish (`beginTierConsumer`) goes through the same site.

Both arms are evaluated inside `ReplicatedTierConsumer::effectiveInputDelayTicks`, which uses the same shared lookups
(`tierInputDelayTicks`, which floors, and the floored worst-tier fallback) as the server's
`ServerInputDelayQueue::effectiveDelay` for the same connection. That sharing is what makes the two ends agree by
construction rather than by coincidence.

`recomputeAndPublishEffectiveInputDelay` **returns the change** in published delay (new − previous) so a caller that must
pay for an increase can. A caller publishing a baseline rather than reacting to a transition ignores it.
`m_lastPublishedEffectiveInputDelayTicks` is kept on the session rather than read back from the atomic, because the delta
is a game-thread bookkeeping quantity: the atomic exists to hand one scalar to the physics thread, not to be used as shared
state. It is seeded to `0`, so the start-up publish reports its full value as the delta, which `beginTierConsumer`
deliberately ignores, since nothing has been predicted yet.

*R0, task 25:* the moved text said the bookkeeping value is kept "rather than read back out of the net sync"; the atomic
is the input-resolution peer's, not the net sync's.

**The tier is server-owned.** The server derives each connection's tier from its own per-connection ping reading
(og-simulation's `ServerReceptionCoordinator`) and sends the result to the owning client, as og-sim-net's
`SimNetServer::publishTier` since og-input-latency task P5-7 (the connection relay's tier property before). The client never
computes a tier and never samples the round trip for tier
purposes, which is what makes a client/server tier disagreement impossible by construction rather than merely unlikely.

**The tier consumer is emplaced on both roles** (`⛔G-06`). A dedicated server never reads the value it publishes there,
but a listen-server host does: its local player's input delay comes from that cache, and an empty cache would leave the
atomic at `0` where the no-tier fallback is correct. No tier ever reaches an authority world, because the server sends tiers
to its connections only, so the authority cache stays at that fallback for the whole session.

**The tier consumer is destroyed before the manager.** It borrows the manager's `TimeConfig`. It is declared after
`m_manager`, so member destruction takes it first, and a `compositionContractsHold()` row asserts that order (§2).
`endPlay` also resets it while the manager still exists, as the owner's `EndPlay` did before task 27a (`⛔G-21`).

#### The preserved tier-0 quirk, and its two consequences

**The server never publishes tier 0 as a FIRST value:** the last published tier for a connection is taken as 0 until the
first publish (og-sim-net's `publishTier` keeps that rule since og-input-latency task P5-7). A wire that never leaves tier 0
therefore produces no publish and no call into `onConnectionTierReceived` at all: the client sits on the pre-arrival no-tier
fallback while the server parks at tier-0 delay. That standing divergence is preserved deliberately; changing its cause
changes felt input lag and belongs with the floor work, not with a transport move.

**The second consequence** was a real defect. That fabricated `oldTier = 0` used to reach `applyTierTransitionStall` as if it
were the client's real prior tier, requesting a spurious multi-tick prediction stall on the connection's **first** real tier
resolution. It was fixed by giving the stall decision `hadAnyTier`, whether the client had already received an
authoritative tier, as an explicit input, so a first-ever resolution requests zero stall whichever tier arrives. The
standing divergence above is unchanged by that fix: `hadAnyTier` only ever suppresses a stall.

`hadAnyTier` **must be captured before `applyReplicatedConnectionTier`** (`⛔G-08`), because that call feeds the tier
consumer, whose `hasReceivedTier()` becomes true unconditionally as a side effect.

The decision itself is og-simulation's `shouldStallForTierTransition` (`Network/ConnectionTierTable.h`), a pure function
with its own low-level test coverage; `applyTierTransitionStall` is only the plumbing around it, and non-positive results
are dropped by the client clock too (`ClientPredictionClock::requestInputDelayIncreaseStall` returns on a delta of zero or
less).

*R0, task 25:* the moved text said "the core never publishes tier 0 … (`m_lastPublishedTier` is baselined at 0)"; that
member is the coordinator's map, read with 0 for an owner it has not published, which is what the sentence above says.

#### Replayed values never stall

Until og-input-latency task P5-9 deleted them, `onConnectionTierReplayed` and `onRelayDelayFloorReplayed` applied and
republished but **did not** stall. For the tier, it is the
first tier ever applied, so no tick was predicted against a previous tier's delay. For the floor, the pull happens inside
the owner's start-up, before the first prediction tick.

**Since og-input-latency task P5-7 nothing called the two replay members, and P5-9 deleted them.** og-net delivers the floor (and any early tier)
before the client's world exists, so every value is latched and handed over at the bind; the client host applies a handed-over
value as received (`UESimNetHost-guards.md` G-05), because today's relay delivered it after the bind, as received, in the common
case (§16.7).

Since og-input-latency task P5-10 `applyReplicatedRelayDelayFloor` has no `payForIncrease` parameter. Its one caller,
`onRelayDelayFloorReceived`, passed `true`; the `false` arm was the deleted floor replay's.

A floor **rise** on the live channel is indistinguishable from an upward tier transition to the client: the frontier must
fall back by the difference, which the clock pays down as stall ticks.

Both listeners **bound, then PULLED** (guard G-07, retired by og-input-latency task P5-7): each relay property dirtied only on
change, so an `OnRep` that fired before the bind was latched by the relay and handed over at the pull. Since P5-7 the client
session in the driver's lanes latches the floor and the tier, and `bindClientSession` replays them; the call's order after the
manager and the tier consumer is a `checkf` in `beginClientSimNet` (§16.2).

#### The floor advisory

`logRelayDelayFloorAdvisory` reports what og-simulation's `classifyRelayDelayFloor` says about the session floor, from every
intake: the owner's ini override at start-up and the floor listener's two entry points. It is advisory only (`⛔G-04`,
`⛔G-09`): floor `0` is the documented off mode and is never flagged, and floor `1` is a legal configuration.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §5 (all but "The zero input" and "Inbound-hit routing",
which are the brawler's and stayed), made game-neutral, with the R0 corrections above.*

### 9.5 The relay ring (retired host boundary)

From task 25 until og-input-latency task P5-7 the session bridged each input relay host (`ASimulationInputRelay`, which cannot
name a game's character) to its character's net-sync sibling, from the host's own calls in (`onInputRelayHostReady`, an
`ISimulationInputRelayListener` member). Since P5-7 no input relay is spawned or found: the server's stager
(`RelayRingStager`, one per character in og-sim-net's server) publishes each frame's ring on og-net's SimRelay lane, and the
client session stores it in that character's channel (`SimNetChannel::storeRing`), which fires the arrival callback og-sim's
`SimulationNetSync` bound on the channel at a remote character's registration (§16). A ring that arrives before its character's roster Add is held per sim id and stored at
the Add (`SimNetClient-rationale.md`).

### 9.6 The server clock and its callback

**`publishServerClock_GameThread`** publishes the authority's server-clock tick through og-sim-net's server
(`SimNetServer::publishClock`), from `onPostPhysicsStep` (§10), between the post-simulation pass and the backend's
visualization update, only on a manager that does not predict and only while the server exists. The server sends it at the
next `TickFlush`, as a `ClockPublish` on the SimClock lane, and only when the tick changed. Until og-input-latency task P5-7 it
was written into the timing relay's synced buffer, which a relay found once per world (and logged `SimulationManager: timing
relay found`).

**`onTimingInfoReceived`** first hands the round trip to the latency member's wire sample, then either to the callback set
by `setOnTimingInfoReceivedCallback`, if one is set, or to the manager's network estimator (round trip and authority tick;
since og-input-latency task P3-10 the round trip goes through `noteEngineRttSample`, which reaches the estimate only while the
engine is its active RTT source, §20).
The client host calls it for every newer clock the client session delivers, with the round trip it reads from the server
connection's FNetPing (`UESimNetHost-rationale.md`), and drops a clock while no session is bound. No caller in the tree sets the
callback. Until P5-7 the session modelled og-simulation's `SimulationManagerOwnerConcept` with it and the timing buffer's
accessor; it has no timing buffer since, so it no longer does, and the brawler's concept test no longer asserts it.

### 9.7 Input reception: og-sim-net's (retired in this class by og-input-latency task P5-7)

From task 26a until P5-7 this class held the server's input reception: its own `ServerReceptionCoordinator` over the engine's
connection handle, an id → component delivery map and its registration, the delivery and relay sinks the coordinator called,
PROBE 5 (relay writes per frame), the drain, the reap and the forget. The component binding's RPC tail
(`UESimulatableOwnerBinding::receiveRemoteMoveBundle`) fed the coordinator. Since P5-7 og-sim-net's `SimNetServer` holds the
coordinator (its `Coordinator`, over `SimNetConnectionHandle`), and is both sinks itself: the release delivers through the
character's channel, and the relay sink stages into the character's stager. The session keeps the members its callers name,
re-pointed (§16.3):

| member | since P5-7 |
|---|---|
| `ReceptionCoordinator`, `getReceptionCoordinator`, `hasServerTierWiring` | og-sim-net's server's coordinator and its presence |
| `getServerReceptionTick` | unchanged; the server host answers og-sim-net's `receptionTick()` with it |
| `releaseDelayedInputsForStep` | `[ReleaseBatch]`, then the server's `releaseForStep` (with the step's time since P5-37, §19.7) |
| `afterAuthorityBatch` | the server's `afterBatch` (the reap) |
| `forEachAuthorityCorrectionTick` | the walk of the registered channels |

**Still exempt from the seam** (`⛔G-11`): the release is typed on the game's input, and so is og-sim-net's server.

**The drain and the reap** keep their pattern: `onFrameStepsDue_Step` (PROBE A, then the server test, then PROBE 6;
§9.9), the drain per authority step or per frame batch, which prints `[ReleaseBatch] first=<tick> numSteps=<n>` on `LogOGNet`
at `Verbose` (its row is in `kOGAdapterLogRoutes`, `UESimulationLogRoutes-rationale.md` §6), and the reap once per frame. Where
each host calls them is the host's; the brawler's two hosts are its `OGBrawlerSimulationManager-rationale.md` §7.

**Threads, and one acknowledged wart.** The reception is game-thread-only: og-sim-net's server drains at the driver's
`PostTickDispatch` and is called by the hosts on the game thread. `getServerReceptionTick()` still reads the server clock,
which the physics thread writes, from the game thread with no synchronization; the tier average is insensitive to a one-tick
skew. *Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §1 ("One acknowledged wart").*

**The relay's schedule stamp is unchanged.** The coordinator still hands the relay sink `(captureTick, dA, input)`, where `dA`
is the effective input delay the authority held for that wire at receipt, so a peer derives the application tick as
`captureTick + dA`; og-sim-net's stager publishes the whole frame's staged entries once, at the flush, as the input relay's
`PreReplication` did, so two arrivals in one server frame both reach the wire.

### 9.8 The session knobs (task 26b)

Three ini keys are read once, at composition, in the owner's start-up (the owner's `BeginPlay` until task 27b). Task 26b
moved their statements here as three functions, which the owner calls at the positions of the statements they hold.
og-input-latency task 4 added two more keys, read by the same intake, and a fourth function that applies them:

| function | what it holds | where the owner calls it |
|---|---|---|
| `readSessionKnobs(worldIsAuthority)` | the intakes (`⛔G-14`, `⛔G-15`, `⛔G-16`, and since og-input-latency task 4 `⛔G-27`); it reaches no session state and returns the requests in a `SessionKnobs` value | after the latency member's begin, before the backend builds its world: both roles |
| `applySessionKnobs(knobs)` | the floor's clamp, set, ini line and advisory; the rotation width's clamp and set; the rotation width's proof line; the floor's publish through og-sim-net's server (`publishFloor`, since og-input-latency task P5-7; onto the timing relay before) | in the authority branch, after the neutral input and the og-sim-net server's start, before the tier consumer's start |
| `applyResimTriggerPolicy(knobs)` | the policy's parse, apply and proof line (`⛔G-17`) | after both role branches |
| `applyLatencyKnobs(knobs)` (og-input-latency task 4) | the two latency knobs' clamp and set, and their one proof line; since og-input-latency task 27 also the re-centre window's clamp, set and proof line | after both role branches, directly after `applyResimTriggerPolicy` |

The statements moved unchanged: the intake's locals are returned in a `SessionKnobs` value, and the two apply functions
read the same four fields from it (as task 26b left it; `applyLatencyKnobs` reads only the four fields og-input-latency
task 4 added, and the two og-input-latency task 27 added); the owner's `m_session.` prefix fell away. No log text, field,
level or category changed. The `[StateRotation] session` and `[ResimGate] session` lines are the start-up's identity lines,
and task 26b's harness sessions hold them, and the floor's lines, byte-identical.

**Every knob takes the same four steps:**

| step | what | why it is not optional |
|---|---|---|
| 1 INTAKE | read the ini once, before the manager exists where possible | a value that arrives after the first publish has to be absorbed as a *change* |
| 2 CLAMP / VALIDATE | out of range is **reported**, never silently corrected | a floor that big is a sizing mistake the operator needs to see |
| 3 SET | stamp the effective value into the one shared `TimeConfig` | every derivation site must read one number |
| 4 PROVE | an **unconditional** `Warning` line naming the value actually stored | see below |

**Step 4 is at `Warning`, not `Log`**, because a game runs `LogOGNet` at `Warning` (the brawler project's
`Config/DefaultEngine.ini` sets it there and calls `Warning` the default), and a `Log` line does not exist at `Warning`. ⚠
**Not every knob meets step 4:** the relay-delay floor's ini line is at `Log` and inside the key-present branch. Both ini
homes are accepted for every key, the game ini first, then the engine ini (`GGameIni`, then `GEngineIni`): the former is the
conventional place for a gameplay-tuning value, the latter is where the brawler keeps its other netcode settings. **And
it is unconditional**, because a line that is absent both when the key was read and when it was not cannot distinguish
those two cases, which is the whole question a proof line exists to answer. The rotation width's and the policy's proof
lines report values **read back from `TimeConfig`**, never the parsed request, so they cannot claim a setting the manager
did not store.

*R0, task 26b:* the moved text said these are "the only `GConfig` uses in this codebase"; the brawler's game module reads
`GConfig` in two other places (its join screen's recent address and its movement-scheme cvar), so they are the only ones in
the session's start-up. It said the reason for `Warning` was an ini setting that was false at the brawler's `b9f6d81`
(`Verbose` then, the brawler's §11 C17); the brawler's ini sets `Warning` again today. And it said every proof line
reports values read back from `TimeConfig`: the floor's ini line prints the clamped request (`clampedFloor`) it is about to
store. The setter's clamp is idempotent, so the stored value is the same number, but that line is not a read-back.

**None of these may become a cvar** (`⛔G-16`). The rotation width, because a run's probe output is read *against* the
cadence and a value that moves mid-run makes those readings unattributable. The resim policy, because it is pushed into
every correction cache and read on the game thread at the correction-landing site **with no synchronization**, which is
sound only because it is written once at composition, before any correction can land.

#### `RelayDelayFloorTicks`: the session floor

**Authority only, and that is correctness rather than economy** (`⛔G-14`). The floor is server-owned session state that
is *replicated* to clients. A client reading its own ini could disagree with the server, which is the two-ends-diverge
failure the whole tier ruling exists to avoid.

The clamped value is stamped into `TimeConfig` and then published through og-sim-net's server (`publishFloor`, since
og-input-latency task P5-7), which `beginServerSimNet` emplaces just before the knobs are applied, so the publish always has
a server (a `checkf` says so). The server sends the floor to every attached connection and to each connection that attaches
later; nothing publishes the floor after start-up. Before P5-7 the floor was written onto the timing relay, which the game's
composition root spawned first for that reason, and a missing relay meant no client learned a nonzero floor.

Two intakes share one clamp: this ini override, and the floor's receipt from the server (`onRelayDelayFloorReceived`; its replay
twin was deleted by og-input-latency task P5-9), where the value arrives off the wire as a `uint8` and is clamped rather than trusted (a
corrupt or version-mismatched byte must not schedule a read past the point where the delay line has already evicted the
capture). The manager's setter clamps again; the call sites exist so an out-of-range value is *visible* in the log rather
than silently corrected.

`logRelayDelayFloorAdvisory` is **advisory only, never an assert**, and is called from both intakes (§9.4, *The floor
advisory*). Floor `0` is the documented "scheduled regime OFF" mode, so og-simulation's `classifyRelayDelayFloor`
(`Network/ConnectionTierTable.h`) never flags it; that function carries the full classification table.

#### `CorrectionRotationK`: the state rotation width

Controls how many characters' correction-state buffers `SimulationNetSync::sendCorrectionAll` writes per tick, round-robin,
so each character's state replicates at the tick rate times K over N (`60 * K / N` Hz at the brawler's 60 Hz). A
character not written is not dirty and costs zero bytes, so this is a real wire saving rather than a deferred write. It
turns an emergent cadence into a decided number: at N characters the per-character correction rate should read that
figure in `[DivergenceProbe.Window]`.

**Authority only, but NOT for the floor's reason.** `K` is never replicated at all: only the authority sends corrections,
and a receiver reconciles against whatever arrives without needing to know the sender's cadence. A client read would have
no reader.

> ⚠ **The compiled default is `TimeConfig::correctionRotationK` and this document does not restate its value.** The
> brawler's `.cpp` once restated it, the value moved, and the comment did not (the brawler's §11, C1). Read it at the
> declaration.

**Sentinel collision, harmless and knowingly.** An ini that literally reads `CorrectionRotationK=-1` cannot be told from an
absent key, because `-1` is also the intake variable's own "not present" sentinel. The proof line reports the effective
number either way. (`-1 == absent` is the intake variable's own convention, not the field's.)

#### `ResimTriggerPolicy`: the resim-gate policy

Decides which landed corrections open the resim gate: `FrontierExact`, the compiled default, which reproduces the legacy
gate; or `OnDisagreement`, the designed trigger. The field lives on `TimeConfig` (`PCTimeManagement/TimeConfig.h`); the
mechanism is og-simulation's `ResimGatePolicy.h` and `StateCorrectionCache`. A game ships its configuration through the ini
key (the brawler's: `ResimTriggerPolicy=OnDisagreement`, in its `Config/DefaultEngine.ini`). **`TimeConfig::resimTriggerPolicy`'s
own declaration states the compiled default and the shipped configuration as two separate facts, and they differ: read them
there.**

**There is deliberately no `ResimCooldownTicks` key** (`⛔G-16`). A trigger-rate ceiling was built and removed on a user
ruling: it defers acting on a correction *already known to disagree*, which is the defect this mechanism repairs. The
throttle is structural instead. If you are here because a design document names that key, the code is right and the
document predates the ruling.

**Not authority-gated, unlike the two above, and that is the point rather than an oversight.** The resim gate exists only
on a predicting client: an authority allocates no correction caches and never rewinds. Gating this intake on authority
would read the ini on the one role that cannot use it and skip it on the role that can. Applying it on both roles keeps one
`TimeConfig` shape and costs nothing; on a server the value simply has no reader.

**Presence is a bool, not a sentinel value.** The value is a string, so there is no numeric sentinel to collide with, and
`GetString` already reports presence. The step-2 parse is case-insensitive because an ini is hand-written, and an
unrecognised policy string is **reported with the compiled default kept** (`⛔G-17`): a typo that quietly selected the
other value would change gate behaviour on a build nobody thinks they changed.

For this knob the proof line is also the **behaviour-neutrality receipt**. The gate ships defaulted to reproduce the old
one, so every claim made from a later run ("the ratios are indistinguishable from the baseline") is a claim about *which
policy was live*. Without that line in the log it is an assertion about the source rather than an observation about the
run. `depthPolicy` therefore carries `(inert under FrontierExact)` rather than falling silent, and `rateLimit = none
(structural)` is stated rather than omitted, because the absence of a cooldown is a ruling: a reader must be able to tell
"the ceiling is off" from "this build predates it".

*R0, task 26b:* verified: the compiled default is `FrontierExact` (`TimeConfig.h`), the brawler's ini sets
`OnDisagreement`, and og-simulation's reconciliation peer allocates no correction cache on the authority (its
`SimulationReconciliation.h` says so, and the manager gates its tick calls behind `runsPrediction`).

#### `RedundancyFrames` and `RelayResendEntries`: the latency knobs (og-input-latency task 4)

Two integer keys under `[OGNetcode]`, read by one intake and applied by `applyLatencyKnobs`. They feed
`TimeConfig::redundancyFrames` (the client's redundancy window, counted in frames) and `TimeConfig::relayResendEntries`
(already-flushed relay entries the authority re-sends per flush). What each one does, its range and why 0 is today's
behaviour are og-simulation's `TimeConfig-rationale.md` §12. In this change neither field has a reader.

**Not authority-gated** (`⛔G-27`), for the resim policy's reason rather than the floor's. `RedundancyFrames` acts only on
a predicting client, so the client must read its own ini. `RelayResendEntries` acts only on the authority, and reading it
on both roles keeps one `TimeConfig` shape.

**The four steps.** Presence is a `bool` per key, so there is no sentinel collision (unlike the rotation width's `-1`). An
out-of-range value is reported by a `[LatencyKnobs] ini [OGNetcode] … out of range, clamped to …` line, clamped by the
shared `latencyKnobs::` function and set through the manager's setter, which clamps again. The proof line,
`[LatencyKnobs] redundancyFrames=… relayResendEntries=… (ini|compiled)`, is unconditional, at `Warning`, read back from
`TimeConfig`, and printed once per session on every peer. Its trailing token is `ini` when either key was present and
`compiled` when neither was. **After both role branches**, like the policy's (`⛔G-24`'s reason): one call proves the
same thing on both roles.

#### `RecentreSettleSamples`: the clock's re-centre window (og-input-latency task 27)

One integer key under `[OGNetcode]`, read by the same intake and applied by `applyLatencyKnobs` after the two latency
knobs. It feeds `TimeConfig::recentreSettleSamples`, the number of clock advances with an RTT reading (a window of ticks since
og-input-latency P3-11; it counted measured RTT samples before) the client clock waits after a join
or a hard resync before its one-shot re-centre; 0 disables the re-centre. What the re-centre does is og-simulation's
`TimeConfig-rationale.md` §14. The key exists so the re-centre's A arm (0) runs on the same binary as its B arm (the
compiled default).

**Not authority-gated**, for `RedundancyFrames`' reason: the field acts only on a predicting client, which must read its
own ini, and reading it on both roles keeps one `TimeConfig` shape. A negative value is reported by a
`[ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=… out of range, clamped to 0` line and set to 0. The proof
line, `[ClockRecentre.Config] recentreSettleSamples=… (ini|compiled)`, is unconditional, at `Warning`, read back from
`TimeConfig`, once per session on every peer. It is a line of its own so the `[LatencyKnobs]` line's text is unchanged,
and its tag is not the clock's `[Clock.Recentre]`, which a client logs once per arm.

#### The retired fourth knob

There was a fourth key: a session-configurable **relay-ring retention depth**. The bare-C1 flush-on-poll write path
replaced the mechanism it sized (the stage capacity is `relayedInputRing::kMaxDepth`, a compile-time constant with no ini
key to feed; §9.7, *The relay tap*), and the key, its clamp intake, its setter and its `[RelayDepth]` startup proof line
were removed together. Guard **G-15** marks where the intake used to sit, in `readSessionKnobs` between the floor's intake
and the rotation width's. It replaces the two ⛔ RETIRED prose fences that marked the intake and the apply block. **It is an
absence fence:** the identifier it is about occurs nowhere in this tree, so there is no symbol to grep and the guard is the
only record.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §3 ("The session knobs"), made game-neutral, with the R0
above. Its history (§11 C1, C17, C18) stayed there.*

### 9.9 The frame probes: PROBE A and PROBE 6 (task 26b)

`onFrameStepsDue_Step(firstUpcomingSimTick, numSteps)` is the per-frame work before the steps: the host calls it on
the thread that runs the steps, once per frame that has steps due (on the authority loop once per stepping wake, on the
runner, §21.1; on the game thread otherwise), with the frame's first upcoming simulation tick and its step count. It was
`onFrameStepsDue_GameThread` until og-input-latency task P5-33 renamed it for that reason (the `_Step` suffix of
`afterStep_Step`). Its body is three pieces, in order:

1. **PROBE A**, frame health, on both roles;
2. **the server test** (`⛔G-18`, `m_simNetServer`; the coordinator test until og-input-latency task P5-7): a client has no
   og-sim-net server, and the function returns here; on the authority loop it returns next (§21.2);
3. on the authority, per client connection of the world's net driver: the one-shot `[PacketBudget]` line, then
   **PROBE 6**, the per-connection send budget.

Where each host calls it is the host's: the brawler's two hosts (the tick-driver binding at `TG_StartPhysics`, and the Chaos
host's per-frame input hook) are its `OGBrawlerSimulationManager-rationale.md` §7 and §8, in OGBrawlerUnreal's docs.

The body moved statement for statement; the task's notes hold the token comparison. The spellings changed where the owner
reached the session (`m_session.m_manager` is `m_manager`, its coordinator `m_receptionCoordinator`) and where the owner
reached itself as an actor (its world is `m_owner.GetWorld()`). No log text, field, level or category changed. Both probes
are members of this class since task 26b, declared where the owner declared them (§2).

All the frame probes are purely diagnostic: nothing reads them but the log lines they feed. **Volume convention:
per-window summaries at `Warning`, per-event detail at `Verbose`, and nothing per-tick or per-write at any verbosity.**

#### PROBE A: frame health (`m_frameHealthProbe`), both roles

Sim ticks per game-thread frame.

**What it settled on the server.** Clients measured a relay-ring arrival gap of about two capture ticks where the design
expected about one. Property replication runs once per server game-thread frame, and `NetServerMaxTickRate` is a **cap**
on that, not a floor, so a 60 Hz sim on a 30 fps server advances two sim ticks per replication, and a depth-1
replace-latest ring could only carry the newer one. If this ratio equals the clients' measured gap, the gap is a **host
performance artefact** rather than a netcode defect.

**Why the client needed it too.** `[RelayProbe.Frame]` was, until it gained a client role, the only wall-clock timing
instrument anywhere in the netcode surface, and it was server-only by construction, yet the server never resims. Every
client cost figure backing the shipped `ResimTriggerPolicy` was derived from `ResimGateProbe` window cadence, which sees
only the physics tick and is blind to game-thread or render hitching, which is precisely where a resim-driven cost would
show up.

**Why it lives here and not beside og-simulation's other probes** (`RelayReadProbe` in the input-resolution peer's
`InputResolutionTelemetry`, `RelayArrivalProbe` in `NetSyncTelemetry`, `ResimGateProbe` in `SimulationManager`): it is
measured on the game thread, in the host's per-frame call, from the tick the host hands it, and the only tick source legal
to read on that thread is the host's own (the brawler's Chaos host: its tick mapper's atomic offset; its tick-driver host:
the dispatch counter plus the published offset). og-simulation's peers have no access to it.

**Why this hook and not the owner's post-physics step, on either role:** that hook is game-thread on both roles too, but it
receives no physics step number, and therefore has no route to a sim tick through the only source safe on that thread.
Sampling there would have required either reading the physics-thread-written clock or inventing a second counter, both
ruled out. This hook already has both numbers: the step, and the frame's step count.

**Why reusing `firstUpcomingSimTick` is safe on the client,** even though a host's `+ 1` derivation may be proved under an
authority-only assumption (the brawler's Chaos host's is, its `ChaosTickBackendUImpl-rationale.md` §4): the probe never
consumes the value directly, only **deltas** between consecutive samples, and a constant additive skew cancels under
subtraction. A client-side departure from unconditional advance, a resim replay or a stall, surfaces either as an ordinary
sample, if the tick delta stays plausible, or as a counted discontinuity (`kFrameHealthDiscontinuityTicks`) if it does
not. Neither corrupts the measurement. What *would* be unsafe is a **different** tick source, such as reading the
prediction clock off the physics thread; the host's call hands the probe the same tick it hands the drain and the reap, so
no new thread-safety question is introduced.

**The ratio is hook-independent**, which is why the metric is defined against `GFrameCounter` rather than an invocation
count, and it is why the probe gained a second role without a second implementation. It additionally reports how its *own*
invocations distributed over frames (once per frame, more than once, or less often), so this hook's cadence is measured
and reported rather than assumed. On a resimming client that sub-step cross-check is the whole point:
`numStepsAboveOne > 0` reads as "resim or sub-stepping ran"; `== 0` with a ratio above 1 reads as "the game thread simply
hitched".

**Category decided per role, not inherited.** The server line keeps `[RelayProbe.Frame]` for the original reason (it pairs
with `[RelayProbe.Arrival]`). That reason does not transfer to the client, which never emits `[RelayProbe.Arrival]`
against its own frame rate; on the client the natural pairing is frame health against **resim cost**, so the client line
rides `[ResimProbe.Frame]`. Both lines go through `RouteOGMessage`, and each lands by its family's catch-all row in
`kOGAdapterLogRoutes` (`[RelayProbe` → `LogOGRelayProbe`, `[ResimProbe` → `LogOGResimProbe`;
`UESimulationLogRoutes-rationale.md`). The two tags are deliberately different families rather than one tag with a role
suffix: a prior review mis-assigned which process emitted `[RelayProbe.Frame]` and had to re-derive roles from other
receipts. Both lines additionally carry an explicit `role=` field, belt and braces.

The role test is `m_manager.has_value() && m_manager->runsPrediction()`, and `runsPrediction()` is `false` for **both** a
dedicated server and a standalone or listen-server-host manager, so a standalone session's frame-health line rides the
server tag, matching how its tick clock is treated everywhere else. A session with no manager yet also takes the server
tag.

**Window comparability with `ResimGateProbe`:** `kFrameHealthProbeWindowSamples` and `kResimGateProbeWindowSamples` are both
120 samples (the latter an alias of `kCorrectionVerdictProbeWindowSamples`), so a reader can line one `[ResimProbe.Frame]`
window up against the surrounding `[ResimProbe.Gate]` windows without a resim-tick count on this line. The function's first
statement asserts the two are equal, so the comparability is the compiler's to keep. They are **comparable, not
identical**: this window closes on `noteFrame`'s `GFrameCounter` cadence, that one on `noteCheck`, one call per
reconciliation `checkDivergenceAll` (from the manager's `onCheckIsSimilar`), which is a logically different event. The two
drift apart over a session exactly as far as their emitters' cadences do, which is itself diagnostic.

#### PROBE 6: per-connection send budget (`m_connectionBudgetProbe`), server only

*Site: `onFrameStepsDue_Step`, below its server test (`⛔G-18`, `m_simNetServer`) and the authority loop's return (§21.2), once
per frame with steps, for each client connection.*

**Why arithmetic was not enough.** The budget model is `allowance = CurrentNetSpeed / DesiredTickRate` bytes per tick,
with up to two ticks of bankable credit. Both halves are read from engine source, but **every term in it is derived**:
`CurrentNetSpeed` is what the server clamped the client's request to at runtime, on a path nobody here has watched
execute, and the modelled round counts two properties out of an unknown total. This probe measures all of them: the
negotiated net speed, the real bytes and packets on the wire, the ack-derived outgoing loss, and `notReady`, frames on
which the connection had no send credit, the state in which `UDataStreamChannel::Tick` returns having written
**nothing** (it returns when `IsNetReady()` is false).

*R0, task 26b:* the moved text defined `notReady` as frames on which `QueuedBits + SendBuffer > 0`, which is the engine's
own `IsNetReady()` test (`UNetConnection::IsNetReady`). The probe is handed `QueuedBits` alone and counts a sample as not
ready when it is **at or above zero** (og-simulation's `ConnectionBudgetProbe`), so its count is an approximation of the
engine's test that leaves out the send buffer's bits and counts exactly zero as not ready.

**Read before `TickFlush`, which is the right place.** `QueuedBits` is updated at the *end* of `UNetConnection::Tick`, so
the value sampled here is the credit this frame's replication write will actually be judged against.

**Cumulative counters, not `OutBytes`/`OutPackets`** (`⛔G-19`): the latter are `StatPeriod` accumulators the engine zeroes
on its own schedule, so differencing them across the window would silently drop whatever it reset mid-window.

The allowance denominator is `NetServerMaxTickRate`, what `GameEngine::GetMaxTickRate` clamps a dedicated server to and
therefore what `DesiredTickRate` resolves to when `MaxNetTickRate` (`BaseEngine.ini`) does not bind. It is read **from the
net driver** rather than hardcoded, so a config change cannot silently invalidate the occupancy figure. `netSpeedBps` is
printed rather than assumed, because if it is not the configured ceiling the whole derivation is wrong. `QueuedBits` is a
debt counter, so `min` is the *most* headroom and `max` the closest to saturation; `lost` is ack-derived, i.e. the
emulation's actual outgoing loss rather than a configured percentage. The line reports `occupancyPctX10=340` for 34.0 %
occupancy.

**The capacity pin.** A game's packet-budget tables budget against a **derived** usable single-bunch capacity computed from
`UNetConnection::GetMaxSingleBunchSizeBits()`, and an earlier published figure for the same quantity did not reproduce from
the formula. Budgeting against an unverified derived constant is precisely how an inert-ini defect happened in the brawler's
tree, so the running engine gets to be the referee, once, cheaply, from the real connection: the `[PacketBudget]` line.
`MaxPacketHandlerBits` is the term that can move it, and it can only move it **down** (encryption or any registered packet
handler reserves bits out of the same budget), so a game's pinned literal is an **upper bound**: if the value logged here is
ever smaller than that literal, the literal is optimistic and must be lowered (the brawler's literal and its test are its
`OGBrawlerSimulationManager-rationale.md` §8). **One shot per process, on the first client connection it sees, at `Warning`:**
the value is a property of the build and the handler stack, not of the connection.

*R0, task 26b:* the moved text said "one shot per session". The latch is a function-local `static` of
`onFrameStepsDue_Step`, so it is one per process (per instantiation in the one module that instantiates the session):
a process that runs two authority sessions in turn, or a listen server's two worlds, prints it once. That was already so in
the owner, where the same `static` sat in a non-template member.

#### The capacity pin's word rounding: `(usableBits / 32) * 4` ∴D-01

*The `[PacketBudget]` line in `onFrameStepsDue_Step`.*

The first argument of that line turns `UNetConnection::GetMaxSingleBunchSizeBits()` into usable **bytes**. It does not
divide by 8: it rounds **down to whole 32-bit words** first, because the channel writes a bunch in words, and then counts
four bytes per word. Replace it with the natural-looking `usableBits / 8` and the number no longer matches what a bunch can
really carry. A game's pinned literal is meant to be compared with this printed figure by hand, and it is an **upper
bound**: if the logged value is ever smaller than that literal, the literal must come down (*The capacity pin*, above).

*R0, task 26b:* verified in the engine: Iris's `UDataStreamChannel::WriteData` limits a write to
`(GetMaxSingleBunchSizeBits() / 32U) * 4U` bytes ("NetBitStreamWriter requires the number of bytes to be a multiple of 4").

The brawler's source banner over that block, carried:

> THE CAPACITY PIN - the packet budget, measured. §8
>
> ⛔ WHY THIS EXISTS: the DERIVED single-bunch capacity does not reproduce. The engine referees.
>
> ⛔ It only moves DOWN, so RoundVsPacketBudgetTest.cpp's literal is an UPPER BOUND.
>
> ONE-SHOT PER SESSION, at Warning: the value is a property of the build, not the connection. §3

(Its last line is the per-session claim the R0 above corrects: the latch is per process.)

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §8 ("PROBE A", "PROBE 6") and §13.8 (the word rounding,
its D-50, retired there; this heading carries D-01), made game-neutral, with the R0 above. The brawler's hosts' calls and
its packet-budget literal stayed in its §7 and §8.*

---

## §10 The teardown and the post-physics pass (task 27a)

**What moved.** The owner's `EndPlay` statements before its `Super` call became `endPlay`, and the owner's post-physics
step became `onPostPhysicsStep`, each one straight-line sequence in the owner's order. Task 27a's `transcribe.py` compares
both, statement by statement, with the owner's two functions as they stood before task 25, the hook inlined at its call
site (the task's notes show what it checked). A game's owner calls `endPlay` from its `EndPlay`, then its `Super`; a
backend calls `onPostPhysicsStep`.

**`endPlay`, in order** (game thread, once per world teardown):
1. `m_physicsBackend.onEndPlayBegin()` (`⛔G-20`). A driver-scheduled backend unregisters the frame host's two tick functions,
   so no new frame starts, and waits for every outstanding step task (`TickDriverBackendUImpl-guards.md` G-01); a
   self-scheduled backend's may be empty. Nothing moves above it.
2. `detachFromSimNetLanes` (since og-input-latency task P5-7, §16.6): the session unbinds from the og-net lanes it is still
   bound to.
3. og-sim-net's server's and its host's resets and the two registered maps' clears (`⛔G-21`), then the tier consumer's reset,
   while the manager still exists: the server and the tier consumer both hold the manager's `TimeConfig` (§2 asserts that
   member destruction takes them first too). Until P5-7 this step reset the session's own reception coordinator and cleared
   the delayed-input map.
4. The registry (§9.2): if this session holds the authority slot, the slot is cleared; then the client slot. The destructor
   repeats this as a backstop (`⛔G-10`). Until P5-7 each role's three relay listeners were unregistered here too.
5. If the owner still has a world: the latency member's `end` (it unbinds its post-tick-flush handler), then the backend's
   `onEndPlayWorld` (a self-scheduled backend removes its engine delegates there).

Nothing resets the manager, the composition or the backend: they live until the session is destroyed, with its owner.

**`onPostPhysicsStep`, in order** (game thread, once per frame, after the frame's physics, with or without steps):
1. The test hitch (not in Shipping; below).
2. `m_physicsBackend.beforePresent_GameThread()`: a driver-scheduled backend's presenter (the shadow restore and the render
   apply, `TickDriverBackendUImpl-rationale.md`); empty on a self-scheduled backend.
3. The manager's post-simulation pass, `onPostSimulationGameThread()` (§8).
4. The authority's server-clock publish, `publishServerClock_GameThread()` (§9.6).
5. For a session on its own detached lanes only (§16.2): the lanes' `afterDispatch` and `onTickFlush`.
6. `publishDisplaySnapshot_GameThread()` unless the session publishes after its batch on the step's thread
   (`publishesDisplayOnStep()`, since og-input-latency task P5-36), then `refreshDisplayView_GameThread()` (§17.2, §17.3).
   Until og-input-latency task P5-16 this step was the backend's visualization refresh, which copied every stored simulatable's state into its
   visualization copy on the game thread and overlaid the rendered poses there.
7. `Game::afterVisualization(*this)`, the game's hook, at the position of the game statement it replaced.
8. The latency member's `tick_GameThread`, with the backend's diagnostics view (`UELatencyBudgetDiagnostics-rationale.md`).

It is private: the backend hosts call it as the session's friends (§4), a driver-scheduled backend through the game's
tick-driver binding (`onPostPhysics`, at the frame's end of physics), a self-scheduled one from the engine's post-tick
delegate, bound to a weak lambda whose weak object is the owner.

**The game's hook, `afterVisualization(session)`.** The game's statement that sat between the visualization update and the
latency tick (the brawler's: its ring-out score push) is the hook's body, verbatim, and the session calls it with itself,
`const`. The hook reads the session through const reads only: `getManager()`, and the two task 27a added,
`getSystemsExec()` and the component map's read (`getRegisteredComponentsById()` since og-input-latency task P5-7), each a
const reference to the member it names. The latency
tick, which followed the game's statement, is the session's own statement after the hook. The brawler's hook is
`BrawlerSessionGameUImpl-rationale.md` §5, in OGBrawlerUnreal's docs.

**The test hitch, `og.Sim.TestGameThreadHitchMs`** (not in Shipping). When it is above 0, the next `onPostPhysicsStep`
sleeps the game thread that many milliseconds, once, and sets the variable back to 0 at console priority
(`ECVF_SetByConsole`), so a value set from the console, from `-ExecCmds` or from an ini is reset. It forces a catch-up
frame: on an inline-stepping server, 250 ms gives a frame of about 15 steps. The sleep is the first statement of the pass,
the one hitch site for every backend.

* **The delay, `og.Sim.TestGameThreadHitchAfterSeconds`** (og-input-latency task P5-32; not in Shipping; default 0). Set on
  a command line or in an ini, the hitch variable is already set at the process's first pass, so without a delay the hitch
  lands in that first frame, before any client has joined (P5-21's finding F1: `P519_hitchOff`'s hitch window closed
  15 s before its first client joined), and the Ph4 gate's hitch rows, which read the clients across a server hitch, could
  not be measured. When the delay is above 0, the accessor leaves the hitch variable set and returns 0 until that many
  seconds have passed since the process's first pass (a function-local `static` holding `FPlatformTime::Seconds()` at the
  accessor's first call), then fires as before. At 0 it fires at the next pass, as before the delay existed. Both
  variables are per process, so with two sessions in one process the first pass of either starts the delay and the first
  pass after it fires the hitch. The delay does not depend on the stepping mode: the pass runs on the game thread with the
  authority loop on and off (§19.5).
* **The marker, `[SimHost.TestHitch] ms=<n> afterSeconds=<s> frame=<f>`** (P5-32). When the hitch fires, the accessor
  logs this one Warning on `LogOGSimHost`, after the reset and before it returns, so the sleep follows it at once and the
  line's time stamp is the hitch's start; `frame` is the engine's `GFrameCounter`. It is new with the delay and prints at
  0 too; a session without the hitch prints no new line. Ph4's gate checker reads it first (P5-21's `check_ph4.py`, rows
  T2 and R1).
* **Where it lives.** The console variable is a namespace-scope object, registered when it is constructed; the class
  template's header has no translation unit of its own to define it in, so the plugin's `.cpp` does (the initiative's
  review, finding m-10), and the plugin module constructs it once. It is defined in `UESimulationSession.cpp`, under
  `#if !UE_BUILD_SHIPPING`, with its name, default, help text and flags unchanged (its help text still names the owner's
  `OnPostPhysicsStep`, verbatim). The template reads it through an exported function the header declares under the same
  condition, `OGSIMULATIONUNREAL_API int32 consumeTestGameThreadHitchMs()`: it reads the value on the game thread, sets it
  back to 0 when it is above 0, and returns what it read; when that is above 0, `onPostPhysicsStep` calls the second
  exported function, `sleepTestGameThreadHitch(hitchMs)`, whose one statement is the unchanged sleep. The read, the reset
  and the sleep run in their old order. The delay's variable sits beside it in the same `#if` and namespace, read by the
  same accessor (P5-32), so the header and its call site did not change.
* **Why the sleep is in the `.cpp` too.** Its argument is `static_cast<float>(hitchMs) / 1000.f`, and the engine sleeps
  `(uint32)(Seconds * 1000.0)` milliseconds. Compiled optimized in the template under `/fp:fast`, the division became a
  multiply by `0.001f`, one ulp off for most values, so the engine's truncation slept one millisecond more than before for
  about half of all values (250 itself was unchanged). In the `.cpp`'s unoptimized pair it is a division again, as it was
  in the owner (task 27a's notes, F1-b).
* **Off the step path** (the initiative's review, finding m-10). The accessor is called once per frame on the game
  thread, never from a step; in Shipping neither variable nor the accessor exists, and the call is compiled out
  with them. The delay's elapsed-time comparison (doubles) is the only floating point P5-32 added; it sits in the `.cpp`'s
  unoptimized pair and decides only when a test hitch fires.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §1 (the post-physics pass's row), §2 (the teardown), §16
(the latency member's `end` and `tick_GameThread` call sites) and §18 (the backend seam's teardown and post-physics call
sites, and the hitch variable), made game-neutral. R0, task 27a: the hitch paragraph said the reset catches "a value set
from the console or `-ExecCmds`"; console priority is above every ini priority too, and the harness sets the variable
through `-ini:Engine:[ConsoleVariables]` and sees it reset (one hitch per session). The brawler's call sites and its score
push stayed in its docs.*

---

## §11 The start-up (task 27b)

**What moved.** The owner's `BeginPlay` statements after its `Super` call became `beginPlay`, one straight-line sequence in
the owner's order, with the owner's file-local `bindCorrectionFieldDiffGate` as a private static member (§11.4). A game's
owner calls `beginPlay` from its `BeginPlay`, after `Super`. Task 27b's `transcribe.py` compares the sequence, statement by
statement, with the owner's `BeginPlay` as it stood before task 25, every helper and every game hook inlined at its call
site (the task's notes show what it checked).

**`beginPlay`, in order** (game thread, once per world, before any step):
1. The inline hook check, at compile time (§11.1).
2. The owner's world, `m_owner.GetWorld()`; a `checkf` if there is none.
3. `m_game.beforeWorldBuild(*uWorld, m_owner.GetNetMode(), compositionExists, m_staticData)`, where `compositionExists` is
   `m_manager.has_value() || m_integrationLayer.has_value()` (false here: nothing is emplaced yet). The game checks its
   static data against the world and writes what the level supplies into it, before anything reads it.
4. The step length `dt`, the engine's `UPhysicsSettings::AsyncFixedTimeStepSize`, and its check (§11.5).
5. The role: `worldIsAuthority` is the owner's net mode not being `NM_Client`, never the actor's `HasAuthority()` (§5).
6. The latency member's `begin` (`UELatencyBudgetDiagnostics-rationale.md`): its probe and two mailboxes, and its
   post-tick-flush binding, all before stepping starts.
7. `readSessionKnobs` (§9.8; from og-input-latency task P5-7 to P5-17 it also read `SimTrafficImmediate` and refused a nonzero
   value here; since P5-17 the lanes read and decide it, and `beginServerSimNet` or `beginClientSimNet` logs a refusal, §18.1),
   then the backend's `buildWorld` (the adapters, §3).
8. The role branch.
   * **Authority:** registry slot 0 (a `checkf` if it is taken) (§9.2); the router logger lambda (`RouteOGMessage`); the
     integration layer; `Game::emplaceSystems` (`UESimulationSession-guards.md` G-01); the manager, without prediction; the
     three peers' loggers; the neutral input, `Game::neutralInput()` (`⛔G-23`, §11.3); `beginServerSimNet` (§16.2);
     `applySessionKnobs` (§9.8); `beginTierConsumer()` (`⛔G-06`, §9.4); `simlog::setGlobal`;
     `Game::installGameLoggers(worldIsAuthority)`; `bindCorrectionFieldDiffGate()` (§11.4).
   * **Client:** registry slot 1; the router logger; the integration layer; `Game::emplaceSystems`; the manager, with
     prediction; the loggers; the neutral input; `beginTierConsumer()`; `beginClientSimNet` (§16.2); `simlog::setGlobal`;
     `Game::installGameLoggers(worldIsAuthority)`; `bindCorrectionFieldDiffGate()`; and the `[ResimProbe.Session]` line
     (compiled only `#if !NO_LOGGING`: its arguments call members a `NO_LOGGING` category does not have).
   * Until og-input-latency task P5-7 the authority also registered the three relay listeners and emplaced its own
     reception coordinator, and the client registered the listeners and pulled the latched tier and floor from the relays.
9. `applyResimTriggerPolicy` (`⛔G-24`), once, after both branches (§9.8); then `applyLatencyKnobs` (§9.8), once, the same
   way (og-input-latency task 4).
10. `m_game.afterSessionConfigured(worldNetMode)`: the game's checks that need the configured session.
11. The backend line (§11.6).
12. The backend's `startStepping`: no step runs before it, so everything above is in place when the first step reads it.
13. The end line, `SimulationManager: adapters, integration layer, and manager initialized` (`LogOGMgmt`, `Log`), its text
    unchanged; a game's start-up tooling may end its span at it (the brawler's harness tooling does).

### 11.1 The game's start-up hooks, and the inline check

| hook | kind | where `beginPlay` calls it | what the session passes |
|---|---|---|---|
| `beforeWorldBuild(world, netMode, compositionExists, staticData)` | member of the game object (`m_game`) | after the world check, before the step length | the world, the owner's net mode, `compositionExists`, `m_staticData` (mutable: the game writes into it) |
| `emplaceSystems(systemsExec, bodyAdapter, queryAdapter)` | static | each role branch, after the integration layer, before the manager | `m_systemsExec`, `*m_physAdapter`, `*m_queryAdapter` |
| `neutralInput()` | static, returns the input type | each role branch, as `setNeutralInput`'s argument | nothing |
| `installGameLoggers(isAuthority)` | static | each role branch, after `simlog::setGlobal` | the role |
| `afterSessionConfigured(netMode)` | member of the game object | after `applyResimTriggerPolicy` and `applyLatencyKnobs` | the role's net mode |
| `backendFingerprint()` | static, returns a `std::string_view` | in the backend line | nothing |

* **Each hook is at its statements' position.** The hook's body is the game statements that sat there, and the session
  calls it where they stood. Two hooks are members of the game object: they reach the game's own parts (the brawler's:
  its roster), which the session cannot name; the session holds the game object by reference (`m_game`, §2) and calls
  them on it. The rest are static, as `makeStaticData` and `afterVisualization` are (§10).
* **The inline check.** `beginPlay`'s first statement is a `static_assert` over a requires-expression that calls all six
  hooks with the argument types the session passes and checks the two return types. A game that lacks a hook, or gives one
  a signature the session cannot call, fails there with one message naming all six, beside the call's own error. It is in
  the member body, as the component binding's key-hook check is, so it is checked where the hooks are called (manager
  design §4.3). No member of the session shares a hook's name, so an unqualified call in the session can never reach a
  hook, and a hook call can never reach a session member; since task 28 the compiler checks that (§12.5), where task 27b
  checked it by grep. *Seen failing (task 27b):* with `emplaceSystems` deleted from the
  brawler's game class, the build fails with `C2338` on the check's message (and `C2039` at the two calls).
* **The arguments are evaluated before the hook's body runs.** `beforeWorldBuild`'s net mode and `compositionExists` were
  read inside the game's statements before the move; they are const reads with no side effect, and nothing between the
  two positions changes them. A captureless lambda's definition, the brawler's log sink, now sits inside
  `installGameLoggers` instead of beside the router lambda; defining it has no effect, and nothing between the two
  positions named it.

### 11.2 The registry writes are the session's own

The two slot tests and writes and the six listener registrations use the session's `this` (task 25 wrote `&m_session` in
the owner; the registry and the listeners hold the session since then, §9.2). The teardown clears them (`endPlay`, §10),
and the destructor is the backstop (`⛔G-10`).

### 11.3 The neutral input, on both roles

The input-resolution peer's neutral input fills the `[0, effectiveDelay)` window at session start and after a hard
resync. It is set on the **authority** branch too, and that is not a precaution: a dedicated server reads it on every
tick it substitutes an input for a remote character (the remote branch of `collectInputAll` on a queue underrun) and seeds
each character's replicated applied-input with it at `registerAuthorityOwner` (the value lives in the peer's per-type
`m_neutralInputs`). Deleting the call would make the authority simulate, and publish to every peer, the value-initialized
input for the whole of every join window; `registerAuthorityOwner` warns if the call has not run before it. **Ordering is
load-bearing: it precedes every `registerAuthorityOwner` call**, and registration happens per character, later, from the
registration path (`tryRegister`, the session's own since task 28, §12). `⛔G-23` guards the authority call. The value is the game's (`neutralInput()`), and a game's
neutral input is often not its value-initialized input (the brawler's aims at `(0,0,1)`); the guard on that value is the
game's (the brawler's: `BrawlerSessionGameUImpl-guards.md` G-01, in OGBrawlerUnreal's docs).

*R0, task 27b:* verified in og-simulation: `SimulationInputResolution::setNeutralInput` writes the per-type neutral and
injects it into the existing caches; `collectInputAll`'s provider-miss and underrun arms emplace it; `registerAuthorityOwner`
logs `[Warning][NeutralInput]` when `hasNeutralInput` is false.

### 11.4 The field-diff cost gate, `bindCorrectionFieldDiffGate`

Since og-netcode-v2-field-defects task 6 a correction line can carry a TAIL naming the first disagreeing FIELD and its
magnitude, and producing that tail costs a per-field walk of the whole composite, roughly ten times the boolean fold it
re-asks. **og-simulation ships that walk closed**: its enabling predicate is an empty `std::function`, so a host that
installs none walks no field on any correction, ever. That default is a property of the core, and it is what keeps every
game that does not ask paying nothing.

`bindCorrectionFieldDiffGate` opens it. `beginPlay` calls it on **both** branches, after the `setGlobal` sink installs, and
what it installs is:

```
correctionFieldDiff::setEnabledPredicate(
    []() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
```

**What it guarantees.** At `LogOGDivergenceProbe=Warning` the predicate answers false; `tryInsertingCorrectState` tests it
as the last of three `&&` operands, *before* the walk's arguments are formed, so `describeFirstDivergingField` is never
entered and `correctionFieldDiff::walkCount` stays at ZERO across a run of disagreeing corrections. The
`[DivergenceProbe.Correction]` line is then byte-for-byte its pre-task-6 self and the tail costs nothing. At `Verbose` the
walk runs once per disagreeing correction and the line gains `field=` and a magnitude. Both halves are asserted, not
argued, in og-simulation-tests' `Network/CorrectionFieldDivergenceTest.cpp`, including the anti-vacuity arm in which the
predicate counts its own invocations.

**A predicate, not a latched bool, and that is why it is a lambda.** `UE_LOG_ACTIVE` re-reads the category's CURRENT
verbosity, so `LogOGDivergenceProbe Verbose` typed into the console mid-session starts naming fields on the next
correction, and `Warning` stops it again. Reading the verbosity once in `beginPlay` would pin the session to whatever the
ini said. The lambda declares its return type, `-> bool`: under `NO_LOGGING` `UE_LOG_ACTIVE` expands to `(0)`, an `int`,
whose conversion inside `std::function<bool()>` is warning C4800, an error in a Shipping build (the brawler's
og-brawler-shrink-install task 9 found it).

⛔ **It must name the same category, at the same verbosity, that `RouteOGMessage` sends the `[DivergenceProbe` prefix
to.** Re-route that prefix without moving this predicate and the session either pays for a walk whose line is then
dropped, or drops a walk whose line is printed. This predicate, the margin predicate below and `RouteOGMessage`'s
`[DivergenceProbe` arm are the only three sites in code that USE this category: two read its verbosity, the other logs
to it. Guard **G-22** sits on the predicate and says so.

**The second predicate: the verdict margin (og-input-latency task 2h).** The same function installs a second predicate,
`correctionVerdictMargin::setEnabledPredicate`, with `UE_LOG_ACTIVE(LogOGDivergenceProbe, Log)`. og-simulation asks it
on every landed correction, agreeing or not, and when it answers true the cache measures how close the verdict came to
the tolerance; once per closed verdict window, `NetSyncTelemetry` prints a `[DivergenceProbe.Margin]` line per class
(og-simulation's `docs/SimulationComparison-rationale.md` §8 and `docs/Telemetry-rationale.md` §15). It is a second
predicate, not G-22's reused, because it answers at `Log`: a session run with `LogOGDivergenceProbe=Log` gets the
per-window margin lines without the per-correction `Verbose` lines and the field walk. At the shipped `Warning` both
predicates answer false.

- **Why `Log`.** The Margin line carries no severity token, so `RouteOGMessage` logs it at the `[DivergenceProbe`
  route's plain level, `Log`. A `static_assert` directly above the call checks that `[DivergenceProbe.Margin]` still
  routes to `OGLogRoute::DivergenceProbe` at `ELogVerbosity::Log`. The category that route logs to is a `switch` arm,
  not checkable at compile time, so guard **G-28** sits on this predicate for that half.
- **The same lambda shape as G-22's,** `-> bool`, for the `NO_LOGGING` reason above.
- **The function keeps its name.** `bindCorrectionFieldDiffGate` now binds both gates. Renaming it would change
  `beginPlay`'s two call sites and the docs that cite it, for no behaviour.

⚠ **Not yet confirmed in PIE.** The binding compiles and the gate is unit-tested against a stand-in predicate; that the
live `UE_LOG_ACTIVE` answers as expected in a running editor is unverified, so the first `Verbose` run should check that
`field=` actually appears before any conclusion is drawn from its absence.

*R0, task 27b:* verified: og-simulation's `correctionFieldDiff::enabled()` tests that a predicate is set before it calls
it, so it is false while the predicate is empty; the correction cache's gate is ahead of the walk's call;
the test file exists and counts walks with `walkCount`. The brawler's ini sets the category to `Verbose`
(`OGBrawlerSimulationManager-rationale.md` §4, in OGBrawlerUnreal's docs).

*R0, og-input-latency task 2h:* the last sentence above is false today: the brawler's `Config/DefaultEngine.ini` sets
`LogOGDivergenceProbe=Warning`, at `HEAD` and in the working tree. The margin predicate's claims were checked against
the code: the `static_assert` compiles in the Editor build, `correctionVerdictMargin::enabled()` is false while no
predicate is installed, and og-simulation-tests' `Network/VerdictMarginTest.cpp` counts the margin walks.

### 11.5 The step length

`dt` is the engine's physics setting `AsyncFixedTimeStepSize`, read once here, and `checkf`-ed to be one tick of
`TraitsT::NetConfig::tickFrequencyHz`: the manager's step dt and the tick rate the per-connection templates are compiled
for must agree. It is handed to the backend's `buildWorld` and `startStepping` and to the manager's constructor. It is the
only floating-point expression of `beginPlay`: the absolute difference of dt times the tick frequency from 1, against
1e-4, a fatal check (task 27b's notes compare
its instructions before and after the move).

### 11.6 The backend line

`[SimHost.Backend] backend=<name> fingerprint=<fingerprint>` on `LogOGSimHost` at `Warning`, once per session, after the
session is configured and before stepping starts: `EngineTraits::kName`, then the game's `backendFingerprint()`, so every
log names the configuration it came from. The category is OGSimulationUnreal's (`OGSimulationUnreal.h`).

### 11.7 The `checkf` texts

The three state checks (no world, an occupied registry slot) and the step-length check name `UESimulationSession` and
`TraitsT::NetConfig` instead of the owner and the game's net config (design Q7, ruled: neutral texts in moved, fatal-only
checks are equivalence). Their conditions and format arguments are unchanged. The game's own checks, in its hooks, keep
the game's text.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §2 (the start-up's order), §4 (the field-diff gate), §5 (the
neutral input on both roles) and §18 (the backend line and `dt`), made game-neutral. The brawler's hooks and its statements
are `BrawlerSessionGameUImpl-rationale.md` §6, in OGBrawlerUnreal's docs.*

---

## §12 The registration sequences (task 28)

**What moved.** The owner's `tryRegister` and `unregisterFromNewFramework`, after their first statements, became the
session's `tryRegister` and `unregisterFromNewFramework`; the owner's pending record, `PendingRegistration`, and its map
became the session's, and `TryRegisterStatus` moved to the session's header, at namespace scope, so a game's component
spells it unchanged. The owner keeps a wrapper for each: the brawler's turns its character id into the storage key
(`toStorageKey`), after a fatal check that the id is assigned (`tryRegister` only), and calls the session's sequence with
the key. Both run on the game thread, from the owner's component: `tryRegister` from its registration loop, once per
attempt until it returns `Ready`, and `unregisterFromNewFramework` from its `EndPlay`. **Since task 29** that loop is the
generic skeleton of the component's binding (`UESimulatableOwnerBinding-rationale.md` §9), which calls this
`tryRegister` (and `noteDelayedInputComponent`) itself, with the storage key its game's hook resolved; the brawler's
`tryRegister` and `noteDelayedInputComponent` wrappers had no caller since, and **task 36** removed them; its
`unregisterFromNewFramework` wrapper is still the component's `EndPlay` call. Task 28's `transcribe.py` compares
both sequences, statement by statement, with the owner's functions as they stood before task 25, every hook and helper
inlined (the task's notes show what it checked).

### 12.1 `tryRegister`: a two-phase contract

The first call creates bodies; later calls re-test resolvability and, once every body resolves, perform the registration
and return `Ready`. Every call before that returns `Pending`, and the component calls again on a later tick.

1. **The inline hook check,** at compile time (§12.5).
2. **The pending record.** The first call for an id records the simulatable, the input provider and the role; a later call
   for the same id finds the record and leaves its own simulatable and provider unused (the component builds a fresh pair
   on each attempt). The parameters are rvalue references, so the owner's wrapper moves its by-value arguments straight
   through, as the old function moved them into the record (since task 29 the binding's skeleton moves its two locals
   straight in, with no wrapper between).
3. **The first-call pass** (the record has no bodies yet):
   * the cast of the component's owner actor to `TraitsT::Character`, and a fatal check that it succeeded. The backend
     builds the bodies from that class's own components, so the cast IS the registration path, not an accessor: a game
     that changes its character's base class must keep this type the one its pawns are. `checkf` compiles out in
     Shipping, where a wrong owner type is a null dereference on the next line; that is why the text names the exact type;
   * the backend's `createBodies_GameThread(owner, character, simulatable, id)`; `false` returns `Pending` with the record
     unchanged, so the next call retries the whole pass;
   * under `DO_CHECK`, a walk of every physics declaration asserting that the creation fold populated its own and its
     parent body id;
   * the game's `afterBodiesCreated(simulatable, character, id, isAuthority, netMode)`, where a game seeds the new
     character from its bodies (the brawler: its capsule body id, its spawn pose and its ring-out spawn slot); the net mode
     is the owner's;
   * the record is marked, and the call returns `Pending`. The first-call pass never returns `Ready`.
4. **The resolvability gate:** the game's root body (`Game::rootBodyId(simulatable)`) is tested first, then every
   declaration's own body; any unresolvable body returns `Pending`. The named, order-first read says out loud which body a
   `Pending` waits on. When the root body is one of the declarations' own bodies, as the brawler's is, the walk tests it
   again and the first read adds no guarantee the walk does not give.
5. **The registration:** the authority adds the character to og-sim-net's server and registers the simulatable with the
   returned channel as both its prediction and its authority owner (a refused add returns `Refused`); a client registers it
   with the client session's channel and the input provider, wrapped by the latency member (`wrapInputProvider`, the capture
   stamp; `UELatencyBudgetDiagnostics-rationale.md`), and marks the id registered in the roster bindings (since
   og-input-latency task P5-7, §16.4; the component was both owners before). The wrap is the session's own statement. Then the line `tryRegister: registered simulatable id=%u isAuthority=%d` (`LogOGMgmt`, `Log`), which prints
   this call's role; a component passes the same role on every attempt (its world's net mode).
6. **The game's `afterRegistered(id, isAuthority)`** (the brawler: its pre-diet cap), then the systems executor's notify
   (`⛔G-25`, §12.4), then the backend's `onRegistered_GameThread`; the record is erased, and the call returns `Ready`.

**Generic creation, hand-written collision.** Each physics declaration names its own slice, so a game that adds a
body-owning sub-simulation edits neither this session nor a backend's creation fold to have the body created, bound,
captured, rewound and checksummed. That is the whole of the claim. Making the body collidable or queryable is still the game's
hand-written work: its collision category needs a row in the game's category table for each backend configuration (the
brawler's are the `categoryChannels` tables of its Chaos and Jolt bindings). *R0, task 28:* verified: both backends' creation
folds (the tick-driver host's `acquireAndBind` path and the brawler's self-scheduled host's `forEach` over the composite) and
the DO_CHECK walk name no declaration type; the brawler's two `categoryChannels` tables exist, one per configuration.

### 12.2 The pending record

`PendingRegistration` holds the simulatable (an `optional`, emptied by the move into storage), whether its bodies were
created, the input provider and the role. The map is keyed by the storage key. It is the session's last member (§2).

*An observation, routed, not fixed (equivalence):* `unregisterFromNewFramework` does not erase a pending record, and nothing
else does but the `Ready` path. A component that ends play while its registration is `Pending` leaves its record until the
session is destroyed; a later `tryRegister` for the same id would resume it. The brawler never issues an id twice (its
character-id allocator never reuses one), so for the brawler this is at most one stale record per abandoned id, bounded by
the id space. It is the owner's behaviour as it stood before the move.

### 12.3 `unregisterFromNewFramework`: a contract, not a garbage-collection hope

In order:
1. **The inline hook check,** at compile time (§12.5).
2. **The systems executor's notify, behind the storage test** (`⛔G-26`): it runs before `unregisterSimulatable` destroys
   the character, while it is still in storage, so a system's unregister hook can still reach it; and only for a character
   that is in storage, because one whose registration never reached `Ready` has nothing to drop, and a hook's
   `view.get<>(id)` would have nothing to return.
3. **`unregisterSimulatable`:** the owner callbacks, then the containers. The owner is the character's channel since
   og-input-latency task P5-7, or none for an id that never reached `Ready`.
4. **`forgetSimNetCharacter(id, isAuthority)`** (`⛔G-13`, §16.5): the registered maps and the slot drop the id; on the server
   og-sim-net's `removeCharacter` sends the roster Remove, drops the coordinator's claim and dedup watermark for this owner
   (`forgetOwner`) and destroys the channel and the stager; on a client the roster bindings mark the id unregistered. Promptly,
   rather than waiting for garbage collection: the coordinator's claim map is keyed by id. Until P5-7 this step was the
   session's own forget: its coordinator's `forgetOwner`, the delayed-input map's erase and PROBE 5's forget.
5. **The game's `onUnregistered(id)`** (the brawler: its roster's forget, then its input-history feed's forget).
6. **The latency member's forget,** the session's own statement.
7. **The backend's `onUnregistered_GameThread`,** then the line `NewFramework: unregistered simulatable id=%u`.

It is called for every component that ends play, registered or not. Since og-input-latency task P5-7 the work is
`unregisterCharacter`, and `unregisterFromNewFramework` returns first for an id a roster Remove or the lanes' stop already
unregistered (`⛔G-30`, §16.5), so each character is unregistered once.

### 12.4 The executor's notify runs on both roles

*Correction (R0, task 28).* The owner's §10 said that "on the authority path, registration also notifies the systems
executor". Both notifies are unconditional, on both roles: the manager forwards each to the systems executor with the
role, and the executor calls a system only where its role affinity fires (`SimulationSystemsExecutor`'s `firesOnRole`).
The brawler's hit-routing and hit-detection systems fire on every role; its ring-out score system on the authority only.
What the authority-only score system relies on, and why the notify must precede `Ready`, is `⛔G-25`.

### 12.5 The four registration hooks, their inline checks, and the name-clash row

| hook | kind | where the session calls it | what the session passes |
|---|---|---|---|
| `rootBodyId(simulatable)` | static, returns a `BodyId` | `tryRegister`, as the gate's first read | the record's simulatable (const) |
| `afterBodiesCreated(simulatable, character, id, isAuthority, netMode)` | member of the game object (`m_game`) | `tryRegister`'s first-call pass, after the bodies and the DO_CHECK walk | the record's simulatable (mutable: the game seeds it), the character, the key, the record's role, the owner's net mode |
| `afterRegistered(id, isAuthority)` | member of the game object | `tryRegister`, after the registration line, before the notify | the key, the record's role |
| `onUnregistered(id)` | member of the game object | `unregisterFromNewFramework`, after the reception forget, before the latency forget | the key |

* **Each hook is at its statements' position,** as §11.1's are. Three are members of the game object, because their
  statements reach the game's own parts (the brawler's roster and input-history feed); `rootBodyId` reads only the
  simulatable, so it is static.
* **The inline checks.** `tryRegister`'s first statement is a `static_assert` over a requires-expression that calls its
  three hooks with the argument types it passes and checks `rootBodyId`'s return type; `unregisterFromNewFramework`'s
  first statement checks `onUnregistered`. Each sits in the member body that calls the hooks, as `beginPlay`'s does.
  *Seen failing (task 28):* with `afterRegistered` deleted from the brawler's game class, the build fails with `C2338` on
  `tryRegister`'s check (and `C2039` at the call); with `onUnregistered` deleted, on `unregisterFromNewFramework`'s.
* **The name-clash row.** `compositionContractsHold()` asserts that no member of the session, public or private, own or
  inherited, of any kind, shares a name with one of the ten hooks it calls (§11.1's six and these four). The concept
  `ueSimulationSessionHookNames::SharesNoHookName` derives a probe class from the session and from a struct with one
  `int` member per hook name, and takes the address of each name through the probe: when the session has a member of that
  name, the lookup is ambiguous between the two bases, the expression is ill-formed, and the concept is false. A
  requires-expression inside the session itself could not test for a member it lacks without a hard error (task 27b's
  A8), and one that names the hook as a qualified member of the session sees only accessible members, and never a non-static member function; the probe
  sees every member, because ambiguity is decided by lookup, before access. *Seen failing (task 28):* a private non-static `rootBodyId()` added to
  the session fails the build with `C2338` on the row's message; so does a public static `neutralInput()` (the reviewer's
  task 27b arm); without either it builds. A scratch compile with MSVC (`/std:c++20 /permissive-`) also saw a private
  overloaded, a private template, a private data member and a public type alias of a hook's name fail it.
* **The hook order is checked by `transcribe.py` and the reviewer,** not by a test: the session does not compile over a
  fake owner without a fake composition, backend and character, so no automation test can record the order.

### 12.6 The `checkf` text

The cast's check names `UESimulationSession::tryRegister` and `TraitsT::Character` instead of the owner's component and
the game's character class (design Q7, ruled); its condition and second line are unchanged. The DO_CHECK walk's two
checks were already game-neutral and are unchanged. The `SimCharacterId::None` check was the owner's, in its wrapper
(since task 29 off the component's path: the binding's skeleton calls this function directly, and the game's
`resolveRegistrationKey` hook answers `retry()` for a null id before the call); task 36 removed it with the callerless
wrapper.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §10 and the generic part of its §13.4 (task 28), made
game-neutral, with the R0 lines above. The brawler's hooks, its statements and the source's own quoted comments are
`BrawlerSessionGameUImpl-rationale.md` §7, in OGBrawlerUnreal's docs.*

---

## §13 The core systems' spellings (task 34)

**What changed.** Until task 34 the session took its five core-system aliases from its game's composition:
`using Storage = typename Composition::Storage;`, and the same for `Reconciliation`, `InputResolution` and `NetSync`, and
`using ManagerType = typename Composition::Manager;`, where `Composition` is `typename TraitsT::Composition`. Go To
Definition stopped at the alias, and the way to og-simulation ran through the game's composition struct, found by hand.
Since task 34 the session spells them with og-simulation's own templates, over the composition's simulatable list:

| alias | spelling | og-simulation header |
|---|---|---|
| `Simulatables` (new) | `typename Composition::Simulatables` (a `SimulatableList`; `apply_t` unpacks it) | `SimulatableList.h` |
| `Storage` | `apply_t<SimulationObjectStorage, Simulatables>` | `SimulationObjectStorage.h` |
| `Reconciliation` | `apply_t<SimulationReconciliation, Simulatables>` | `SimulationReconciliation.h` |
| `InputResolution` | `apply_t<SimulationInputResolution, Simulatables>` | `SimulationInputResolution.h` |
| `NetSync` | `apply_t<SimulationNetSync, Simulatables>` | `SimulationNetSync.h` |
| `ManagerType` | `SimulationManager<IntegrationExec, NetSync, InputResolution, Reconciliation, SystemsExec, Storage, StaticData>` | `SimulationManager.h` |

F12 on a template name in the alias block now opens its primary template in og-simulation (task 34's notes list the
header and line of each). `SimulationObjectStorage` is also forward-declared in og-simulation's `StorageView.h`, so a
"Go To Declaration" may offer that line too.

**The ruling (user, 2026-10-24).** The session requires og-simulation's own core systems. A game supplies the simulatable
list, its systems (`SystemsExec`), its engine adapters (`IntegrationExec`), its static data, the backend, the hooks and
the net config; it does not substitute the storage, the reconciliation, the input resolution, the net sync or the core
manager. So `Simulatables`, `SystemsExec` and `IntegrationExec` stayed `typename Composition::…` (the two executors
until task 35, §14), and `StaticData` stays `typename TraitsT::StaticData`: they are the game's, and this MPL header names
no game type.

**The checks.** `compositionContractsHold()` (§2) holds one row per derived alias: the session's `Storage`,
`Reconciliation`, `InputResolution`, `NetSync` and `ManagerType` must each be the same type as the composition's
`Storage`, `Reconciliation`, `InputResolution`, `NetSync` and `Manager`. A composition that names another type there
fails the build with the row's message, which states the ruling. The rows sit beside the row that `ManagerType` is the
tick-driver binding's `Manager`, inside the function, where the class is complete; the constructor asserts the function.
They name only og-simulation templates and `TraitsT`-dependent types. The game's own side of the chain is its traits'
core-system map (the brawler's: `BrawlerSessionTraitsUImpl-rationale.md` §4, in OGBrawlerUnreal's docs). *Seen failing
(task 34):* `NetSync` spelled `apply_t<SimulationInputResolution, Simulatables>` fails the brawler's build with the
`NetSync` row's message, the manager row's and the tick-driver row's (the manager is over the wrong peer), and the
errors of the members that type uses (`C2661`, `C2672`).

**What did not change.** Every alias keeps its name and its place in the block; `Simulatables` is new, declared just
before `Storage`. No member declaration changed, and the types are the same ones, so no code changes: task 34's
per-function disassembly differs only in the `__LINE__` of seven `checkf` calls in `beginPlay` and `tryRegister`
(the header grew by 27 lines above them), the line a failed check reports.

**The includes.** The session includes the four og-simulation headers the new spellings name that it did not include
itself (`SimulatableList.h`, `SimulationObjectStorage.h`, `SimulationReconciliation.h`, `SimulationManager.h`), in its
block of og-simulation includes beside the two it did (`SimulationInputResolution.h`, `SimulationNetSync.h`). *R0:* the
plugin's two `.cpp` files that include this header, `UESimulationSession.cpp` and `SimulatableNetSyncComponent.cpp`,
already reached the first three through those two; `SimulationManager.h` is new to them. The game's files already reached
all four through its composition header.

---

## §14 The executors, the engine contract and `PhysicsBackend` (task 35)

**What changed.** Until task 35 the session took its two executors from its game's composition:
`using SystemsExec = typename Composition::SystemsExec;` and `using IntegrationExec = typename Composition::IntegrationExec;`.
Since task 35 it spells both with og-simulation's own executors, the way §13 spells the core systems:

| alias | spelling | og-simulation header |
|---|---|---|
| `IntegrationExec` | `apply_t<IntegrationExecFor, Simulatables>`, where the private member alias template `IntegrationExecFor<SimulatableTs...>` is `SimulationIntegrationExecutor<StaticData, typename EngineTraits::BodyAdapter, typename EngineTraits::QueryAdapter, SimulatableTs...>` | `SimulationIntegrationExecutor.h` |
| `SystemsExec` | `apply_t<SystemsExecFor, typename Composition::Systems>`, where the private member alias template `SystemsExecFor<SystemTs...>` is `SimulationSystemsExecutor<Simulatables, StaticData, SystemTs...>` (since task 36; task 35 spelled it through a partially specialized member struct) | `SystemsExecutor.h` |

`IntegrationExecFor` and `SystemsExecFor` are the composition's own pattern (the brawler's
`BrawlerCompositionUImpl::IntegrationExecFor` and `BrawlerCompositionUImpl::SystemsExecFor`), with `StaticData` and
`EngineTraits` taken from the traits. `SystemsExec` takes the systems from `Composition::Systems`, a list the game
declares in its firing order, so this MPL header names no game type. F12 on `SimulationIntegrationExecutor` or
`SimulationSystemsExecutor` opens og-simulation's template.

**The list type is og-simulation's `SystemList` (task 36).** og-simulation's `SimulatableList` is the simulatable pack:
the systems executor's primary template is left undefined so that the simulatables go through a `SimulatableList`, and
`IsSimulatableList` gates each system's `RequiredSimulatables`. A list of systems spelled as a `SimulatableList` would
model that concept and read as a list of simulatables. So the systems have their own marker, `SystemList`, declared in
og-simulation's `SimulatableList.h` beside `SimulatableList` with its own `apply` specialization, which is what lets
`apply_t` unpack it here; that header asserts `SystemList` does not model `IsSimulatableList`. A game declares its
`Systems` as a `SystemList` (the brawler's, `BrawlerCompositionUImpl-rationale.md` §4, in OGBrawlerUnreal's docs). Task 35
had each game declare its own minimal list and expanded any variadic class template through a partial specialization
(lead ruling, 2026-10-24); task 36 replaced both (lead ruling, 2026-10-24).

**The engine contract.** `compositionContractsHold()` asserts `PhysicsEngineTraits<EngineTraits>` (og-simulation's
`PhysicsEngineTraits.h`), so the requirement on `TraitsT::EngineTraits` is stated where the session reads the engine's
adapter types, its name (the `[SimHost.Backend]` line, §11.6) and its visualization types, and F12 on the concept opens
it. The engine stays the game's choice, made in its traits; the brawler also asserts the concept on its selected
`EngineTraits` (`PhysicsBackendUImpl.h`).

**The ruling (user, 2026-10-24), extending §13's.** A game also uses og-simulation's own integration executor and systems
executor. It chooses the systems and their order, its static data, its engine (through the traits) and its physics
backend.

**The checks.** Two rows beside §13's: the session's `IntegrationExec` and `SystemsExec` must each be the same type as the
composition's `IntegrationExec` and `SystemsExec`; then the `PhysicsEngineTraits` row. Firing order is template order, so
the `SystemsExec` row fails when the composition's `SystemsExec` is not the executor over its own `Systems` list in that
list's order (task 35's arm S2, a reordered executor, and its review's X1, a dropped system). A reorder of the list
itself changes both sides and passes the row: the order is the game's choice, and the brawler's `firesBefore` assert
is what rejects detection after routing (`BrawlerCompositionUImpl-rationale.md` §3, in OGBrawlerUnreal's docs).

**`PhysicsBackend` (task 35).** The backend's type and members are renamed so that the name says it is the physics backend:
the traits' `PhysicsBackend<SessionT>` and `PhysicsBackendWorlds` (were `Backend` and `BackendWorlds`), this class's alias
`PhysicsBackend` and its friend line, and the members `m_physicsBackend` and `m_physicsBackendWorlds` (were `m_backend` and
`m_backendWorlds`). It is the physics backend host: it owns and steps the physics worlds and updates the visuals (on Jolt
through the tick driver, on Chaos the game's own host), reached through the traits (§3). The rename is identifiers only:
the `[SimHost.Backend]` log tag and every other log text, cvar and config key are unchanged, and so are the game's selector
names (the brawler's `physicsBackendUImpl::Backend`, `BackendFor` and `BackendWorlds`), og-simulation's
`SimulationTickBackend` (whose `m_backend` is its render source's own reference) and the game hook `backendFingerprint`.
The tick-driver host reads the two members by name in its order row (`TickDriverBackendUImpl-rationale.md` §1), so they
renamed together.

**What did not change.** The types are the same ones, so no code changes; the header grew by 31 lines above every
`checkf`, which moves only the `__LINE__` a failed check reports (task 35's notes give the per-function disassembly).

**The includes.** `PhysicsEngineTraits.h`, `SimulationIntegrationExecutor.h` and `SystemsExecutor.h`, in the block of
og-simulation includes. *R0:* `SimulationIntegrationExecutor.h` already reached the plugin's two `.cpp` files that include
this header through `SimulationManager.h` (task 34); the other two are new to them, with `SystemRoleAffinity.h`, which
`SystemsExecutor.h` includes (an enum, no data).

---

## §15 The seams for the switch (og-input-latency task P5-5)

og-input-latency's switch to og-net (its task P5-7, P5 design §3.4) changes what the reap, the slot and the server's
`Handed` stamps run on. Task P5-5 gave the session four members that hold today's statements, so that the switch edits these
bodies instead of every host and the latency member. P5-5 is refactor-only: each member holds the statements its callers ran
before, and each caller calls it at the position of those statements. All four are private, and every caller is one of the
session's friends (§4).

| member | holds | callers |
|---|---|---|
| `afterAuthorityBatch(firstUpcomingSimTick)` | the reap: the coordinator's `reapConnections(firstUpcomingSimTick)`, when the session has a coordinator | the tick-driver game binding's `afterInlineBatch` (it tested `getReceptionCoordinator()` itself before); the brawler's Chaos host, `InjectInputs_External`, after its drain (it read `m_receptionCoordinator` directly before) |
| `physicsSlotFor(owner, key)` | the character's replicated slot: `GetPhysicsSlot()` on `owner` as the traits' `Character` | the game binding's `callWithSlotRequest`, as the slot it hands `slotCall` |
| `recordAuthoritativeSlot(owner, key, slot)` | the slot publish: `SetAuthoritativePhysicsSlot(slot)` on `owner` as the traits' `Character` | the setter `callWithSlotRequest` hands `slotCall` |
| `forEachAuthorityCorrectionTick(function)` | the walk of `m_delayedInputComponentsById`: it skips a stale weak pointer and an unbound component (`⛔G-29`), reads the component's correction-buffer tick, skips tick 0, and calls `function(id, tick)` | the latency member's server branch of `tick_GameThread`, which keeps the per-id `Handed` stamp (`UELatencyBudgetDiagnostics-rationale.md` §12) |

**The switch re-pointed all four** (og-input-latency task P5-7, §16.3, §16.4): the reap is og-sim-net's server's
`afterBatch`, the slot doors read and write the session's slot map on the authority and the roster entry on a client, and
the walk reads the registered channels. The table above is P5-5's state.

**No file but this header named the session's coordinator member since P5-5.** The Chaos host's test of it became
`hasServerTierWiring()` and its reap `afterAuthorityBatch`. The tick-driver binding never named the member: it called
`getReceptionCoordinator()`, which the component binding's RPC tail still called then (§9.7). Since og-input-latency
task P5-7 the accessor returns og-sim-net's server's coordinator (§16), and since P5-8 deleted the RPC tail no file in this
tree calls it.

**The slot doors.** Both take the owner as an `AActor&` and the host's slot key (`TickDriverBackendUImpl::slotKeyOf`), which
neither reads yet; the parameter is for the switch, which records the authority's slot in the session by that key and
answers a client from the roster. Each casts with `CastChecked` to `TraitsT::Character`, as `tryRegister` casts to it: the
session is a class template, so the cast and the member calls are compiled where the door is instantiated, through the game
binding's `callWithSlotRequest`, where the character is complete. The caller's `checkf` that the owner is that class runs
first and keeps its message, so the cast cannot fail on any path that reaches it. The doors name two members of the game's
character, `GetPhysicsSlot` and `SetAuthoritativePhysicsSlot`; the switch removes both reads.

**The walk's include.** The correction-buffer read names `correctionStateBuffer::kTickOffset`, which does not depend on
`TraitsT`, so this header includes `OGSimulation/CorrectionStateBufferCodec.h` itself. The latency member's header included
it for the same read and no longer does.

**The walk is compiled only where it is called.** It is a member template, and its one caller is compiled only while
`OG_LATENCY_BUDGET_DIAGNOSTICS` is 1 (`UELatencyBudgetDiagnostics-rationale.md` §3), so a Shipping build instantiates no
copy of it.

**What did not change.** No log text, level, category or order. The header grew by 50 lines, most of them above `checkf`s,
which moves only the `__LINE__` a failed check reports.

---

## §16 The switch: sim traffic on og-net, frame-locked (og-input-latency task P5-7)

Task P5-7 moved every sim payload off Unreal's replication onto og-net's sim lanes, in one build: inputs, corrections, relay
rings, the server clock, the connection tier, the relay-delay floor and (new) the roster. og-sim-net's sessions carry them
(`SimNetServer`, `SimNetClient`), over `UESimNetLanes`, the og-net driver's per-driver lanes object (`UESimNetLanes-rationale.md`),
with the Unreal hosts of `UESimNetHost.h` (`UESimNetHost-rationale.md`). The traffic is **frame-locked**: received at the
driver's `PostTickDispatch`, sent at its `TickFlush` (the P5 design, K3). The old carriers (the net-sync component's RPC and
correction property, the input, timing and connection relays, the three listener interfaces) are no longer written or
spawned by anything; later tasks delete them (P5-8, P5-9).

### 16.1 Who owns what

| piece | owner | why |
|---|---|---|
| the transport (`SimNetTransport` over the driver's erased endpoint) | the driver's lanes, from `InitBase` | og-net delivers from before the world exists (K4) |
| a client's `SimNetClient`, its roster mirror and its channels; the bindings (`SimNetBindings`) | the driver's lanes | the roster, floor and tier arrive before the client's world exists, and are latched there |
| the server's `SimNetServer` (it owns og-simulation's reception coordinator) and its host, `UESimNetServerHost` | this session: `m_simNetServer`, `m_simNetServerHost` | its lifetime is the world's, and it borrows the manager's `TimeConfig` |
| detached lanes, for a world with no og-net driver | this session: `m_detachedSimNetLanes` | one code path either way; sends are dropped and counted |

`m_simNetLanes` points at the lanes the session is bound to, the driver's or its own detached ones. It is cleared by the
lanes' stop handler and by `detachFromSimNetLanes`, so the session never keeps a pointer to lanes the driver destroyed.

### 16.2 `beginPlay`: finding the lanes, and the proof line

`findDriverSimNetLanes` casts the world's net driver to `UOGNetDriver` and takes its `GetSimLanes()`. The game registers one
factory (the brawler's `makeBrawlerSimNetLanes`), which makes `UESimNetLanes<TraitsT>` for the driver's role, so the
`static_cast` to this session's lanes type is the factory's own type; a `checkf` compares the lanes' role with the world's.

- **Authority** (`beginServerSimNet`, after the neutral input, before `applySessionKnobs`): with no lanes, a networked world
  logs `[SimNet.Host] refused role=server driver=<class>: simulation traffic needs the og-net driver` at `Error` on
  `LogOGNet` and shows a 15 s toast (the user's ruling Q1: no Fatal); a standalone world says nothing. Either way it uses
  detached server lanes. Then the host, then the server, with the manager's `TimeConfig` and og-sim-net's
  `correctionAllowanceBytes` for the endpoint's lane table and budget at `NetConfig::tickFrequencyHz`; then the lanes get the
  server's two frame functions (`bindServerSession`) and the session's stop handler.
- **Client** (`beginClientSimNet`, where `replayLatchedClientValues` stood): a `checkf` that the manager and the tier
  consumer exist (the bind replays into both; this replaced guard G-07); with no lanes, the same refusal line with
  `role=client`, the toast, and detached client lanes. Then the session's handler for a roster Remove
  (`setOnRegisteredRemoved`), its stop handler, and `bindClientSession(*this)`, which binds the host, replays the latched
  floor and tier, and seeds the bindings from the roster mirror before any registration (`SimNetBindings-rationale.md` §6).
- **The proof line**, once per session, from `logSimNetHostLine`:
  `[Warning][SimNet.Host] role=<server|client> immediate=<0|1> rttSource=EnginePing driver=<OGNet|detached> inbox=2048`.
  `immediate=0` is the frame-locked mode (the only one until og-input-latency task P5-17, which made the digit the lanes'
  decision, §18.1); `rttSource=EnginePing` is a fixed field that names WIRE's source (the connection's FNetPing, D13, which
  the hosts read in `readRoundTripMs` and `readServerRoundTrip`, `UESimNetHost.h`). It is **not** the clock's RTT source:
  that is `[OGNetcode] ClockRttSource` (§20), and the session reports it on its own line,
  `[ClockSource.Config] clockRttSource=<Engine|SimClock> …`. Under `ClockRttSource=SimClock` the line still reads `EnginePing`
  (og-input-latency task P3-10 part B F5).
- **`[OGNetcode] SimTrafficImmediate`** was read by `readSessionKnobs` on both roles until P5-17, and a nonzero value logged an
  `Error` and ran frame-locked. Since P5-17 the lanes read it and decide the mode at driver start (§18.1).
- **The floor** is published through the server, `publishFloor`, at the end of `applySessionKnobs`, where the timing relay's
  write stood; the server sends it to every attached connection and to each one that attaches later.
- **A detached session** is driven from `onPostPhysicsStep`: the lanes' `afterDispatch` and `onTickFlush`, after the clock's
  publish, so its counters and window lines behave as the driver's would. Nothing arrives, and every send is dropped.

### 16.3 The frame work

| session member | before P5-7 | since |
|---|---|---|
| `releaseDelayedInputsForStep` | the coordinator's release, delivered through the component map | `[ReleaseBatch]` kept, then the server's `releaseForStep` |
| `afterAuthorityBatch` | the coordinator's `reapConnections` | the server's `afterBatch` |
| `publishServerClock_GameThread` | the clock into the timing relay's buffer | the server's `publishClock(tick)`, sent at the next flush |
| `getReceptionCoordinator`, `hasServerTierWiring` | the session's own coordinator | the server's `coordinator()`; presence is the server's |
| `forEachAuthorityCorrectionTick` | the component map, skipping an unbound component (G-29) | the registered channels (`m_simChannelsById`), skipping a buffer shorter than its header and tick 0 |
| PROBE 6's test (`⛔G-18`) | the coordinator's presence | the server's presence |

**PROBE 5 is gone** (it was guard G-12): it counted the writes of the session's relay sink, and og-sim-net's server is the
relay sink now, staging into its own `RelayRingStager`; the stager reports its own window (`[RelayFlush]`). `[RelayProbe.Write]`
no longer appears in a server log.

**The client's `Handed` stamps** come from the channel's `setOnInputHandedCallback`, set at registration for a local
character, one stamp per bundle slot, as the component binding's send did. The other stamps are the hosts' (og-sim-net calls
them at the drain and the flush): `ServerReceived`, `ClientReceived`, the correction's, and the new `NetReceived` at the
datagram's receive time, which the latency member's masks now include (`kServerNetHops`, `kClientNetHops`).

### 16.4 Registration

**Server.** The slot door `recordAuthoritativeSlot` stores the slot the backend chose in `m_authoritativeSlotsByActor`, keyed by
the pawn actor (the tick-driver host's slot key is that actor's address, and a slot is requested before the pawn has a sim id:
the visualization volume's request comes from the registration's first loop), and `physicsSlotFor` reads it back; the character's replicated slot property is no longer written (P5-8
deletes it). Each `checkf` of the property's setter (authority only, a slot of the physics world, once per character) moved
into the door, and its message says so. At the `Ready` step, `addSimNetCharacter` calls the server's `addCharacter` with the sim
id, the recorded slot, the pawn's root connection's og-net id (`kInvalidSimConnection` for a server-local pawn, or a connection
of another driver), the pawn's local player index (`GetPlayerSlotForActor`) and the pawn's name. A refusal (`nullptr`, logged by
og-sim-net as `[Roster.Refused] role=server …`) returns `TryRegisterStatus::Refused`: the pending record is erased, an `Error`
line names the id, and the binding abandons. Otherwise the character registers with its channel as both owners, as it did with
the component.

**Client.** The binding gate is `clientBindStep(id, engineSaysLocal)`, which the game's key hook asks once the pawn's key is
known: `Wait` (no roster entry yet: the hook answers `Retry`, inside the binding's 600 attempts), `Ready`, or, on a role
mismatch, the `[Roster.BindMismatch]` line and a `checkf` (`Abandon` with checks off; P7's C-3). A detached client answers
`Abandon` with one `Error`, so it does not wait into the 600-attempt `checkf` (Q1: no Fatal). The slot door answers from the
roster entry (since og-input-latency task P5-17 through the bindings' copy of it, `verdict(...).physicsSlot`, under their lock:
the bindings hold every entry the mirror holds, fed by the same Add and Remove, so the answer is the same, and an immediate
client's mirror is the step's to read, §18.3): the game binding passes the pawn's storage key (its replicated sim id, 0 until that has arrived, which answers
"not yet" as the replicated slot property's absence did). At `Ready`, the channel is the client session's (`client.channel(simId)`); if the roster removed it between the
gate and the bodies, the call stays `Pending`. A `checkf` compares the input provider's presence with the roster's local index,
then the character registers, a local one gets the `Handed` callback, and the bindings mark the id registered.

**The registered maps.** `m_registeredComponentsById` (the game component per id, both roles; the brawler's ring-out push reads
it on the server through `getRegisteredComponentsById`) and `m_simChannelsById` (the channel per id) are written at `Ready` and
erased by `forgetSimNetCharacter`.

### 16.5 Unregistration: whichever comes first

`unregisterFromNewFramework` is the pawn's `EndPlay` path; `unregisterCharacter` is the work, in today's order: the executor's
notify (`⛔G-26`), `unregisterSimulatable` with the channel (both owners on the server, the prediction owner on a client; none
for an id that never reached `Ready`), the channel's `Handed` callback cleared, `forgetSimNetCharacter` (`⛔G-13`), the game's
`onUnregistered`, the latency member's forget, the backend's unregistration, and the `NewFramework: unregistered` line.
`forgetSimNetCharacter` erases the two maps and the slot, then on the server removes the character (`removeCharacter`, reason
`Left`: the roster Remove to every attached connection, the coordinator's `forgetOwner`, the channel's and the stager's
destruction), and on a client marks the bindings unregistered.

Two other paths run `unregisterCharacter` before the pawn's `EndPlay`, and each records the id in `m_simUnregisteredIds`, so
the later `EndPlay` returns at its first statement (`⛔G-30`) and the line prints once:
- **a client's roster Remove** of a registered id (`onSimNetRegisteredRemoved`, called by the client host inside og-sim-net's
  Remove handling, before the channel is erased; P7's C-4);
- **the lanes' stop** (`onSimNetLanesStopping`, from `beforeStop`): Unreal's map load shuts the world's net driver down before
  it destroys the pawns and ends the world's play (the engine's LoadMap), and the lanes, with a client's channels, are destroyed
  with the driver. So the stop unregisters every registered character while the channels and the endpoint still live, then
  releases the server and its host and forgets the lanes.

### 16.6 Teardown

`endPlay`, after the backend's `onEndPlayBegin` (`⛔G-20`), and, on an immediate client, its teardown (§18.6):
`detachFromSimNetLanes` (a server's frame functions unbound; a
client's registered ids marked unregistered in the bindings, its Remove handler cleared and the client session unbound),
then (`⛔G-21`) the server and its host reset, the two maps and the slot map cleared and the unregistered-id set cleared (since
og-input-latency task P5-9; the session dies with the world, so the set held nothing a later call reads), then the tier
consumer's reset and the registry. A
pawn that ends after the session finds no session in the registry and skips its unregistration, as before. The destructor
calls `detachFromSimNetLanes` too, as a backstop for an owner destroyed without `EndPlay` (`⛔G-10`): it touches the lanes
object only, never a `UObject`.

### 16.7 What the switch kept, and what it changed on purpose

- **The owner type** is og-sim-net's one channel, `UESimNetChannel<Simulatable>`, for both roles; `compositionContractsHold()`
  asserts it against both sessions' `Channel`, and the `TimeConfig` re-send clamp against the relay codec's bound (that
  assertion stood beside the input relay's spawn in the component binding before).
- **The reception policy** is unchanged: the coordinator is the same class, over og-sim-net's `SimNetConnectionHandle`
  (`NetConfig::Address`), with one RTT sample per bundle and the release per authority step.
- **The latched floor is applied as received** (`UESimNetHost-guards.md` G-05): it pays for an increase, as today's post-bind
  property arrival did, so the client's join keeps its `[RelayDelayFloor] floor = 6 ticks, UPWARD` stall line.
- **The `[Roster.*]` lines** og-sim-net writes without a level token go out at `Warning` through the hosts (P5-4's F1).

## §17 The display snapshot: what the game thread reads (og-input-latency task P5-16)

The game thread draws the client's simulation from a snapshot, never from the storage, the input resolution or the
reconciliation. The snapshot type is og-simulation's (`SimulationDisplaySnapshot-rationale.md`, in og-simulation's docs);
this section is the session's half: where it is published, what the game thread keeps of it, and the doors that read it.

### 17.1 The type and its sizes

`DisplaySnapshot` is `SimulationDisplaySnapshotT<Simulatable, kMaxSimulatableSlots, kSessionDisplayWindowTicks,
kSessionDisplayWindowTicks>`: one entry per stored simulatable, up to the body-slot capacity (`kMaxSimulatableSlots`,
`BodySlotOccupancy.h`), with a capture window and an applied window of `kSessionDisplayWindowTicks` ticks each.
`kSessionDisplayWindowTicks` is `kLocalInputCacheCapacityTicks`, the local input cache's capacity: the capture window can be
no wider (the snapshot's own `static_assert`), and a game names its narrower poll windows itself. The session names no game
constant, so a game asserts beside its reader that its windows fit (the brawler's input-history feed asserts its 64-tick
capture poll and its 60-tick applied poll, `InputHistoryDisplay-rationale.md` §8.7 in OGBrawlerUnreal's docs).

### 17.2 Publish: after the step batch, on the thread that ran it

**A predicting session on a driver-scheduled backend** (`publishesDisplayOnStep()`: the engine traits have a world host,
the manager exists and runs prediction) publishes in `publishDisplaySnapshot_Step`, which the tick-driver game binding's
`publishForDisplay()` calls once after each client batch, on the thread that ran the batch (a worker task, or the game
thread for a client stepping inline), under the world lock (`TickDriverConcepts-rationale.md` §11, in og-simulation's
docs). Nothing steps there, so the fill reads the storage, the input resolution, the reconciliation and the clock as the
batch left them. `onPostPhysicsStep` does not fill for such a session; it only acquires (§17.3).

Until og-input-latency task P5-36 every role filled in `onPostPhysicsStep`, on the game thread, on the claim that the tick
driver's post-physics ran once the frame's step tasks had completed. It did not: `FrameWait::EarlierFrames` (a frame-locked
client) waits only for earlier frames' tasks, and `ThisFramesSingleStep` (an immediate client) only for a one-step frame's,
so the fill could read the storage while the frame's own batch stepped on a worker (og-input-latency's design review P5-31,
L5; `SimulationTickDriverDisplayFillTest.cpp` in og-simulation-tests reproduces it).

**What the step-thread fill reads, per mode.**
- **Immediate** (§18): og-sim-net drains on the step's thread, so the relay stores, the arrival rings, the tier receipt and
  the registrations are written there too. The fill reads every source on its writer's thread. The tier comes from
  `m_displayTier`, the `TripleBuffer` the tier receipt (`recomputeAndPublishEffectiveInputDelay`) writes.
- **Frame-locked** (§16): the drain runs on the game thread, so four of the fill's sources are written there: the tier, the
  relay stores, the arrival rings and the registrations. The tier still comes from `m_displayTier`, whose writer is then the
  game thread; `beginClientSimNet` seeds it with the consumer's reading before any step, so the display before the first
  tier is the consumer's default reading, as before. The relay fields (each entry's last relayed input and arrival ring)
  come from a stage: `onFrameStepsDue_Step` (on the game thread, before the batch is dispatched) copies them for every
  stored character with og-simulation's `stageDisplayRelay` into `m_displayRelay`, a `TripleBuffer`, and the fill reads
  the newest stage (`SimulationDisplaySnapshot-rationale.md` §3, in og-simulation's docs). A stage always exists when a
  frame-locked batch fills (a `checkf`). What the game thread still writes under the fill (the correction landings, the
  relay stores the collect reads, a registration's storage insert) is what the step's own sweeps already read across
  threads, the netcode's accepted crossings (`ThreadingCrossings.md` rows 1, 2 and 11, in og-simulation's docs).

**Every other session** publishes in `publishDisplaySnapshot_GameThread`, in `onPostPhysicsStep` where the visualization
refresh ran before (§10, step 6):
- an authority stepping inline steps on the game thread inside the frame's start of physics, so its fill in the
  post-physics pass runs after its steps on the same thread: there is no race there;
- an authority on the loop never fills (below);
- the brawler's Chaos backend, which this initiative neither builds nor runs, calls `onPostPhysicsStep` from the engine's
  post-tick delegate while its physics thread may still step: the same unsynchronized read its visualization refresh made
  there before. It has no batch hook.

The fill is og-simulation's `fillSimulationDisplaySnapshot`; in `publishDisplaySnapshot_GameThread` the clock and the tier are
read first:
- **a predicting session** reads `readDisplayClock(getClientClock(), getNetworkEstimator())`, and, when the tier consumer
  exists, `readDisplayTier(consumer, getTimeConfig(), the published effective delay)`; without a consumer the tier reading
  stays default (no tier received);
- **an authority** has no prediction clock (the manager's `getClientClock` terminates on one), so the session writes only the
  simulation step: `predictionTick` and `stepDeltaSeconds` are the server clock's step, and every other clock field stays
  zero. Every reader of those other fields is gated on `runsPrediction()` (the brawler's input-history lanes, its G-09 and
  G-12), so the zeros are never shown.

It returns at once while the manager does not exist. A `FreeRunning` authority (og-input-latency's authority loop) reaches
`onPostPhysicsStep` on the frame thread while the runner steps, so this fill would read the storage across the runner. It
never runs there: `onPostPhysicsStep` returns before it on the loop (§19.5), and the fill's first statement is a `checkf` that
the loop is off (og-input-latency task P5-20, §21.4). The overlay below keeps the tick backend's `OG_CHECK`, which alone ran
too late (after the fill, and only once an entry existed: P5-16's F6).

**Heap.** The publisher's channel holds four snapshots, each up to `kMaxSimulatableSlots` entries with both relayed rings by
value, so the session holds it through a `std::unique_ptr`, as it holds the game-thread copies below. The session object
stays the size it was plus three pointers.

### 17.3 The game-thread view: one snapshot held, its states copied and overlaid

`refreshDisplayView_GameThread`, in the post-physics pass, acquires the newest snapshot and then releases the one it held, if
that is a different one: the channel marks a held snapshot with one bit, not a count, so releasing first and acquiring the
same slot again would leave the reader on an unmarked slot. Acquiring first holds two for the instant between the calls,
which is the reader's bound (`SimulationDisplayPublisher::kMaxHeldByReader`). On a Jolt client a publish may run in that
instant, on the step's thread: the channel's writer then recycles the oldest snapshot the reader does not hold, and with
two of four held one is always free (`SimulationDisplaySnapshot-rationale.md` §5). On every other session the publish runs
just before, on the game thread. Every reader in the frame shares the one held snapshot until the next pass.
`endPlay` releases it.

Then, for each entry, the session copies the entry's state into its own array of visualization states (index-aligned with
the entries) and calls the backend's `overlayRenderPoses_GameThread(id, state)`. On a driver-scheduled backend that writes
the presenter's rendered poses into every non-root body state (`TickDriverBackendUImpl-rationale.md`), exactly what the
presenter wrote into the storage's visualization copy before; the brawler's Chaos backend writes nothing, as its refresh
wrote only the plain copy. The snapshot itself stays as published: it is shared with the writer's channel, and the overlay is
the game thread's.

So the game thread now reads the state as of the end of the last step batch, plus this frame's poses. A reader that ran
after a frame's steps but before its post-physics pass read the live caches before; it now reads them as of the previous
pass, at most one step batch older (the task's DEV). On a Jolt client the pass reads the newest batch published before it
(since og-input-latency task P5-36): an immediate client's one-step frame waits for its batch, fill included, so it shows
that batch; a frame that does not wait shows the newest batch that finished before the pass.

### 17.4 The doors

| door | answers from |
|---|---|
| `getDisplaySnapshot()` | the held snapshot, or a null pointer before the first publish |
| `findDisplayEntry(id)` | the held snapshot's entry for `id`, or a null pointer |
| `findDisplayVizState(id)` | the game-thread copy of that entry's state, with the poses overlaid |
| `forEachDisplayVizState(f)` | `f(id, state)` for every entry, with the overlaid copies |
| `getLastRelayedInput(id)` | the entry's last relayed input (none without an entry) |
| `isLocallyControlledOnThisPeer(id)` | whether the entry has a local input cache (false without an entry) |
| `getVizSlice<SliceT>(id)` | one slice of the overlaid copy (`⛔G-02`) |

`getLastRelayedInput`, `isLocallyControlledOnThisPeer` and `getVizSlice` keep their names and signatures; only their source
changed. `getLocalInputCache`, `getRelayedReadObservations`, `getRelayedInputArrivals`, `getStorage` and
`editReconciliation` (both overloads) were deleted with their last game-thread callers: the entry carries the local
captures, both relayed rings by value and the applied-capture window. `editStorage` stays for the brawler's Chaos backend,
which steps on the physics thread. `getClientEffectiveInputDelayTicks` stays (the input resolution's atomic) but has no game
caller since this task: the feed reads the published delay from the snapshot's tier reading.

A game reads all of this through the doors, never the snapshot's channel: the session acquires and releases, so the hold
bound is the session's to keep.

## §18 Immediate mode on a client (og-input-latency task P5-17)

With `[OGNetcode] SimTrafficImmediate=1` a client's og-sim-net session is drained on the step's thread at each step and sends
its inputs at capture, and its registrations go through og-sim-net's lifecycle queue (`SimNetClient-rationale.md` §16–§18).
The game's shipped `Config/DefaultEngine.ini` sets the knob to 1 since og-input-latency task P5-22 (after Ph4's gate), so a Jolt
client is immediate by default. Frame-locked (the knob absent or 0, the default before P5-22) stays Chaos's mode and the
attribution arm (design Q11), and is unchanged: every branch below tests `m_immediateClient`, which is false there.

### 18.1 The mode and its lines

- **Decided by the lanes**, at driver start (`UESimNetLanes-rationale.md` §11), because og-sim-net fixes the mode when the
  transport is built. `beginClientSimNet` copies the lanes' `isImmediate()` into `m_immediateClient`, before any step runs and
  never again, so the steps read a value nothing writes while they run.
- **A refusal** (the server's lanes, a backend that does not step through the tick driver, `og.Sim.RunInline=1`, detached
  lanes) is one Warning on `LogOGNet` from `logSimTrafficImmediateRefusal`, in `beginServerSimNet` or `beginClientSimNet`, naming
  the knob's value and the reason. An absent or zero knob prints nothing. With the shipped knob (1, P5-22) the Warning is printed
  on every session of a world where immediate cannot run: a Chaos client, a server without the authority loop (a listen server,
  an in-editor PIE server), `og.Sim.RunInline=1`, detached lanes.
- **The proof line** carries the mode: `[SimNet.Host] role=client immediate=1 …`; a server's is `immediate=1` exactly when its
  authority loop runs (§19.1), else `immediate=0`.
- **The lifecycle binding.** An immediate client binds og-sim-net's lifecycle queue to the session's storage, reconciliation,
  input resolution and net sync (`bindLifecycle`) before `bindClientSession`, so before any registration. The server binds its
  own in `beginServerSimNet`, after the server exists. A frame-locked server enqueues nothing, so the bind changes nothing there;
  on the authority loop the server's `drain()` takes the lifecycle queue first (§19.3).

### 18.2 The step hooks: the drain at the step's start, the send at capture

An immediate, bound client's step reaches the session twice, on the step's thread. Both entries return at once on a
frame-locked client, on the server and before the world session is bound (`boundImmediateClient_Step()`).

**At the step's start, the drain.** The game binding's `receiveForStep` (og-simulation's `TickDriverReceiveGame` hook, called
from each client step's `beforeTick`, before the resimulation check and the collect; `BrawlerTickDriverGameUImpl-rationale.md`
§9) calls `receiveForClientStep_Step()`, which does, in this order:

1. **The queued unregistrations** the game thread asked for (§18.5).
2. **The drain**, `drainClient_Step()` (`UESimNetLanes-rationale.md` §11): the lifecycle queue's jobs first, then every record,
   then the disconnect request and the window line.

**After the capture, the send.** The game binding's `onStepStarted` (each step's `beforePhysics`, after `onGameSimulation`'s
capture and before the physics step) calls `afterClientCapture_Step()` after its latency stamp:
`afterCaptureTick_Step(m_netSync, currentIntegratedTick(), timeConfig)`: og-sim-net opens the input set, NetSync sends each
local character's bundle for the tick the step just captured, and the set goes to the endpoint at once
(`SimNetClient-rationale.md` §17). The tick is the one the frame-locked send read (`currentStep()`, which on this path names the
same step). `onPostSimulationGameThread` returns at once on an immediate client: the game thread's send would fail the
transport's input-set owner check (`SimNetTransport-guards.md` G-07).

**What the drain feeds.** Everything it delivers reaches the step it runs in: the step's resimulation check sees a correction
it delivered, and the step's clock advance and collect read the clock samples, relay rings and inputs it delivered. A
character it registers is in storage for that step's integrate, and one it unregisters is gone before it. This is the
frame-locked order per step: the frame-locked client drains at the frame's start, before the frame's first step, while the
immediate client drains before every step, so a step batch of several steps sees each step's own arrivals.

**Before og-input-latency task P5-30** the drain followed the send in `onStepStarted` (`onClientStepStarted_Step()`), after
the step's collect, clock advance and resimulation check, so everything it delivered reached the next step: H6c rose by one
tick and N6 rose too (P5-17's finding F2).

### 18.3 What the game thread no longer reads directly

The drain writes on the step's thread what it wrote on the game thread frame-locked. Each game-thread reader moved:

| state | the game thread's reader | since P5-17 on an immediate client |
|---|---|---|
| the server connection's round trip | the drain's `serverRttSeconds` read it | published each frame by the lanes (`publishServerRtt_GameThread`), read from atomics (`UESimNetHost-rationale.md` §5) |
| the WIRE sample | `onTimingInfoReceived`'s `noteWireSample` (a game-thread door) | `noteWireSample_Internal`, through the latency mailbox |
| the drain's latency stamps | the host's `stamp`, `stampAt`, `stampCorrectionReceived` | `stampClientReceived_Internal` (`UELatencyBudgetDiagnostics-rationale.md` §13) |
| the tier receipt's display state | `publishDisplaySnapshot_GameThread`'s `readDisplayTier` over the tier consumer and the time config | `recomputeAndPublishEffectiveInputDelay` writes the same `DisplayTierReading` into a `TripleBuffer` (og-sim `Mailbox.h`) after each recompute; the publish reads the newest (since og-input-latency task P5-36 a Jolt client's publish reads it in both modes, on the step's thread, §17.2) |
| the roster bindings | `clientBindStep`, `tryRegister`, `forgetSimNetCharacter` | the same calls, through the bindings' lock (`UESimNetHost-rationale.md` §12) |
| the roster mirror | `physicsSlotFor` | the bindings' copy of the entry, under the lock |

- **The display tier.** The recompute runs at every tier and floor receipt and at the tier consumer's start, so the buffer holds
  what the frame-locked read would have computed at that moment: the effective delay it reads is the value the recompute just
  published, and the client's floor is only written right before a recompute. A frame-locked client writes the buffer too and
  never reads it. Until the first recompute the snapshot's tier fields keep their defaults, as with no tier consumer.
- **`bindClientSession`'s seed** (`seedFrom(roster())`) still reads the mirror on the game thread. It runs in `beginClientSimNet`,
  before the backend starts stepping, while the game thread is the lanes' drain (`UESimNetLanes-rationale.md` §11), so it reads
  the mirror on its writer's thread.
- **The session-setup writers** of the state the drain and the send read (the tier consumer's start, `applySessionKnobs`'s
  floor on the server, `applyLatencyKnobs`' `setRedundancyFrames` and `setRelayResendEntries`, the resim policy) all run in
  `beginPlay` before `startStepping`, so before the first step and its drain. The backend's task dispatch orders them. A knob
  changed at run time while steps run would be a plain race; nothing in the tree does that.

### 18.4 Registration through the lifecycle queue

`tryRegister`'s client branch, on an immediate client, goes to `tryRegisterQueued` after the bodies exist and are resolvable:

1. **The gate** is the bindings' verdict, asked and marked in one hold (`markRegisteredIfReady`): `Wait` stays `Pending`, a role
   mismatch is the frame-locked `checkf`, `Ready` marks the id registered. The client session's channel map is the drain's, so
   the gate does not read it (the frame-locked branch still does).
2. **The enqueue**: `registerCharacter(simId, simulatable, provider, applied)` with the latency-wrapped provider. The record keeps
   no simulatable after this (`queued`), and every later call goes to `finishQueuedRegistration` first.
3. **The apply**, on the drain's thread: og-sim-net registers the character with its channel as the owner, then calls `applied`
   (`onQueuedRegisterApplied_Step`), which sets a local character's `Handed` callback (posting through the mailbox), notifies the
   systems executor (`⛔G-32`), and posts `RegisterApplied` to the game thread. A refusal (`noChannel`, `registered`) posts
   `RegisterRefused`.
4. **The finish**, on the game thread, at the binding's next attempt: `collectLifecycleOutcomes_GameThread` reads the posted
   outcomes (under a mutex; the session also collects them once a frame, in `onPostPhysicsStep`). On `RegisterApplied`, the
   frame-locked branch's host work: the registered-component map, the `tryRegister: registered` line, the game's `afterRegistered`
   and the backend's `onRegistered_GameThread`, then `Ready`. On a refusal, or a roster Remove before the finish, the bindings are
   marked unregistered, an `Error` names the id, and `Refused` (the simulatable died in the job; the pawn's `EndPlay` releases its
   bodies).

**The order, against frame-locked.** The executor's notify runs before the game's `afterRegistered`, on the step, right after the
character entered storage (G-25's condition, in storage and before `Ready`, holds). The channel map (`m_simChannelsById`) is not
written: the game thread never touches an immediate client's channel. A local character's input sample is published from the
frame after `Ready` (`sampleInput` walks the registered map), so its first one or two steps after the apply read the neutral
input; a frame-locked character's first step after `Ready` already had a sample.

### 18.5 Unregistration

- **The pawn's `EndPlay`** (`unregisterFromNewFramework`, after `⛔G-30`) calls `requestQueuedUnregister_GameThread`. An id that
  never reached the queue (no queued record, not registered) runs the frame-locked host work at once, bodies included
  (`unregisterCharacterHostOnly`); nothing of it is in og-sim. Otherwise the game thread erases the pending record, posts the id to
  the step, holds the component (`TStrongObjectPtr`) and runs the host work except the bodies' release (`⛔G-33`): the maps and
  the bindings (`forgetSimNetCharacter`), the game's `onUnregistered`, the latency forget and the `NewFramework: unregistered`
  line.
- **The step** takes the posted ids at its start, before its drain (`applyUnregisterRequests_Step`, in
  `receiveForClientStep_Step`): for each it notifies the systems executor while the character is still in storage (`⛔G-31`, the queue's form of `⛔G-26`) and enqueues `unregisterCharacter`, which the
  same step's drain applies at its top. `applied` clears the channel's `Handed` callback and posts `UnregisterApplied`; the game
  thread then runs the backend's `onUnregistered_GameThread` on the held component and lets it go. The bodies are released only
  after the character has left storage, so no step integrates a character whose bodies are unbound.
- **A queued registration not yet applied** is unregistered by the same request: the queue is FIFO, the register applies, and its
  `applied` sees the pending unregister and does not notify the executor, so the executor never holds the character.
- **A roster Remove** of a registered id: og-sim-net unregisters a queue-registered character inside the drain before it calls
  the host (`SimNetClient-guards.md` G-12), and the host's handler reaches `onSimNetRegisteredRemoved` on the step, which posts
  `RosterRemoved`. The game thread runs the host work with the release and records the id (`⛔G-30`), as frame-locked. If the
  pawn's request is already pending, its own completion does the release; if the registration has not finished, it ends
  `Refused`. **Known gap:** og-sim-net calls no host hook before its own unregister, so the systems executor is not notified for a
  character removed this way, and a system that keeps a pointer into storage (the brawler's hit routing) keeps it past the
  character's removal (the task's notes, finding F1).

### 18.6 Teardown

`teardownImmediateClient_GameThread` runs once, from whichever comes first: the lanes' stop handler (`onSimNetLanesStopping`,
before its own loop, which then finds nothing registered) or `endPlay` (after `onEndPlayBegin`, before `detachFromSimNetLanes`).

1. **The steps stop first** (`⛔G-34`): `m_physicsBackend.onEndPlayBegin()` stops the tick driver and waits for its step tasks;
   it is idempotent, so `endPlay`'s own call after it does nothing. From here the game thread is the only thread that touches the
   simulation.
2. **Everything queued or registered is unregistered through the queue**: the posted requests, every queued record and every
   registered character, each with its executor notify; then one game-thread drain (`drainClientWhileStopped_GameThread`) applies
   them. So og-sim-net's client holds no registered character and no pending job that points into this session's storage, and
   the session can die before the lanes (review_P2-17 N2).
3. **The game thread finishes**: the outcomes are collected, every held component's bodies released, every registered
   character's host work run with its id recorded (`⛔G-30`: the pawn's later `EndPlay` returns at once), and the queued records
   erased with their bindings unmarked. The session's later registrations stay `Pending` (`m_immediateStepsStopped`).

### 18.7 What frame-locked keeps

On a frame-locked client the only differences are the lock around the bindings, the display tier's unread buffer, and
`physicsSlotFor` reading the bindings' copy of the roster entry; the start-up lines are the same, `immediate=0` included.

## §19 The authority loop: the server on its runner (og-input-latency task P5-19)

With `og.Sim.AuthorityLoop=1` (the default since og-input-latency task P5-22) on a Jolt dedicated server the authority steps on
its own thread, `OGSimAuthority`, which also services og-net's server endpoint (P11, design K14; `UEAuthorityRunner-rationale.md`
§1 has the defaults). Every branch below tests `m_authorityLoop`, which is false on every other world, so the inline server (the
knob 0, the attribution arm), every client and Chaos are unchanged.

### 19.1 The decision

- **The lanes decide**, at the driver's `InitBase` (`UESimNetLanes-rationale.md` §12): og-net fixes the servicing thread at its
  first service, before any world exists. `beginPlay` copies the answer (`decideAuthorityLoop`, before the world build, only when
  the engine has a world host): the server's lanes' `authorityLoop()`. A detached world (no og-net driver) has no lanes; with the
  knob set it logs `[SimHost.Runner] og.Sim.AuthorityLoop=<n> refused: this world has no og-net sim lanes (detached); …` at
  Warning and steps inline. Chaos and listen servers are refused by the lanes (their Warning), so the session never sees the loop
  there.
- **Proof lines** with the loop on: `[OGNet.Driver] start … mode=hostServiced` (the driver), `[SimHost.Runner] priority=AboveNormal
  nice=… highResWait=…` and `[SimHost.Runner] startLatch waitMs=…` (the runner), `[SimHost.Mode] role=Authority stepping=runner
  viz=none` and `[SimHost.Frame] role=Authority stepping=runner …` (og-simulation), `[SimNet.Host] role=server immediate=1 …`.

### 19.2 Start: the session is configured on the runner

`beginPlay` keeps its order; only the thread changes. Its configuration, everything from the role branch (manager, systems,
og-sim-net server, knobs, tier consumer, loggers) to the `[SimHost.Backend]` line, is the lambda `configureSession`. Inline,
`beginPlay` calls it between `buildWorld` and `startStepping`, as before. On the loop it is the runner's `afterBuild`
(`TickDriverBackendUImpl-rationale.md` §15): the game thread imports the statics, starts the runner and waits on its latch; the
runner builds the world (the adapters exist only after that), runs `configureSession`, builds the driver and releases the latch.
The game thread is parked for all of it, so the configuration's writes race nothing, and the latch orders them before every
later game-thread read. Nothing in the configuration needs the game thread: it reads no console variable (`readSessionKnobs` ran
before) and touches no `UObject` but the world's driver, which it only reads.

### 19.3 og-sim-net on the runner

- **Immediate, and no frame points.** The lanes build an immediate transport for the loop, so og-sim-net's server is immediate
  (a `checkf` in `beginServerSimNet` compares the two). The session binds no frame points (og-sim-net refuses `dispatchFrame` and
  `flushFrame` of an immediate session); it binds the runner's two lanes functions instead (`bindAuthorityRunner`: the RTT publish
  and the join, `UESimNetLanes-rationale.md` §12).
- **The game binding's hooks** (`BrawlerTickDriverGameUImpl-rationale.md` §10):
  - `idleUntil(deadline)` is og-sim-net's `SimNetIdle` over the lanes' endpoint (`m_simNetIdle`), one `serviceUntil` per call:
    og-net's handlers, the SimClock answer and the relay at receipt run there, on the runner (P2-19);
  - `beforePump` runs the runner requests (§19.4), then the server's `drain()` (the lifecycle queue first, then the inbox);
  - `afterInlineBatch` writes the batch's corrections (`SimulationManager::onPostSimulationGameThread`, which on an authority
    only calls `sendCorrectionAll`), publishes the clock with the server tick, and calls og-sim-net's `afterInlineBatch` with the
    tick the driver passes (the corrections, the clock, the reap, the window; P2-19 F1);
  - `onStepEnded` publishes the server tick to the SimClock server after every authority step (`afterStep_Step`), on the loop and
    inline alike (P2-20 F2).
- **The first batch reports tick 0** (the driver publishes its offset after a tick ran, P2-19 F2), and a capture reaped against
  it would be refused by the coordinator's domain gate. On the loop it cannot happen: og-net is serviced only by the runner's
  idle, after its first batch, so no connection is attached at that batch. A `checkf` in `afterAuthorityBatch` states it.
- **The host off the game thread.** og-sim-net calls the server host from the runner: its `rttMs` reads the RTT table the game
  thread fills each frame, and its probe stamps go through the latency mailbox (`UESimNetLanes-rationale.md` §12; §21.5).
- **The frame probes on the runner** (PROBE A per stepping wake, PROBE 6 retired) are §21.1 and §21.2.

### 19.4 Registration and unregistration on the runner

The tick backend refuses binds, releases and registrations off the runner in `FreeRunning`, and the Jolt world checks its caller,
so on the loop a server character's registration crosses to the runner. The game thread keeps every `UObject` read and the game's
hooks; the runner does every simulation and world write. Requests travel in `m_runnerRequests` (closures under
`m_lifecycleBridgeMutex`), drained at the top of `beforePump`; outcomes come back as `LifecycleOutcome`s, collected on the game
thread in `onPostPhysicsStep` and at each `tryRegister` call.

`tryRegisterOnRunner`, one step per call (the component calls `tryRegister` each frame until it is not `Pending`):
1. **Bodies.** The simulatable moves into a shared pointer (the runner's requests hold it, so a pawn that leaves cannot free it
   under them). `makeBodyRequest_GameThread` captures the slot request; the runner's request binds the bodies
   (`createBodies_Runner`) and answers `BodiesBound` with the slot, or `BodiesPending` (no slot yet: the game thread asks again).
   The component's static data is read by pointer, and the component is held by `m_componentsHeldForRunner` until that answer
   arrives (the static data is not copyable).
2. **After the bodies,** on the game thread: the slot is recorded (`recordAuthoritativeSlot`), the fold check runs, and the game's
   `afterBodiesCreated`, as inline.
3. **Register.** The game thread reads the roster facts from the pawn (owner connection, local index, name); the runner's request
   checks that the bodies resolve (`bodiesResolvable_Runner`; `BodiesUnresolvable` makes the game thread ask again) and enqueues
   `SimNetServer::registerCharacter`, which the server's drain right after applies (`addCharacter`, then the facade with the
   channel as both owners). Its `applied` callback (`onRunnerRegisterApplied_Runner`) notifies the executor and records the id
   (G-36), and the tick backend's `onRegistered` (the occupancy push and the presenter's ring); on a refusal it releases the bodies.
4. **Finish,** on the game thread at the next call after `RegisterApplied`: the component map, the `tryRegister: registered` line,
   the game's `afterRegistered`, the render target. A refusal logs the same Error as inline and returns `Refused`.

`requestRunnerUnregister_GameThread` (the pawn's `EndPlay`, and the lanes' stop for every registered character): the game thread
does the host's half at once (the maps, the game's `onUnregistered`, the latency forget, the render target, the
`NewFramework: unregistered` line); when bodies may be held it posts a runner request that unregisters through og-sim-net's queue
once a registration was posted (G-37) and releases the bodies in its `applied` callback. og-sim-net calls the host's
`onLifecycleUnregistering` before the facade, which the session answers with the executor's notify while the character is in
storage (G-35; the same answer serves an immediate client's roster Removes, og-input-latency P5-17's F1). After the runner has
stopped nothing is posted: the simulation and its world are destroyed whole.

### 19.5 The game thread on the loop

`onPostPhysicsStep` (the frame thread's `endFrame` in `FreeRunning`) runs the test hitch, the presenter (`present` drains the
tick backend's registration ring J9, then the snapshot), the outcomes, the authority presentation (§21.3) and the latency
probe's tick, and returns. It does not run the correction write or the clock publish (both on the runner, §19.3), the
display-snapshot publish and refresh (they read storage, and the overlay is refused in `FreeRunning`: §17.2, §21.4), or the
game's `afterVisualization` (the brawler's ring-out score push reads the score system the runner writes; on the loop the push
reads the authority presentation snapshot instead, §21.3). The 200 ms test hitch therefore stalls only the game thread: the
runner keeps stepping (the gate's hitch arm, which delays the hitch into steady state with
`og.Sim.TestGameThreadHitchAfterSeconds`, §10).

### 19.6 Stop

- **`endPlay`:** `onEndPlayBegin` unregisters the tick functions, joins the runner and stops the driver
  (`TickDriverBackendUImpl-rationale.md` §15); `m_runnerStopped` is set; the rest is as before, with the idle and the clock-server
  pointer cleared before og-sim-net's server.
- **The driver first** (a map load destroys the net driver before the world's `EndPlay`): the lanes' `beforeStop` joins the runner
  first (`UESimNetLanes-guards.md` G-03, the session's `onAuthorityRunnerJoined_GameThread`), then calls the stop handler, which
  unregisters every character host-side (§19.4: nothing is posted once the runner stopped) and releases og-sim-net's server.
- A pawn whose `EndPlay` comes after either is unregistered host-side only.

### 19.7 The step's time on the runner's lines (og-input-latency task P5-37)

A dedicated server has no log thread. A line logged on another thread is queued, and it is stamped when the game thread
next flushes the queue, so on the loop the runner's `[AuthoritySimulation]` and `[Release]` lines carry stamps up to one
server frame late (the Ph4 gate measured 6.3 to 7.2 ms on average). Both lines therefore carry the step's own time:

- **`noteAuthorityStepTime_Step()`** reads `FPlatformTime::Seconds()` once, keeps it for the release that follows, and
  hands it to the manager for the step that follows (`SimulationManager-rationale.md` §18). The brawler's tick-driver
  binding calls it first in `releaseInputsForAuthorityTick`, on the thread that steps the authority (the runner on the loop,
  the game thread otherwise), immediately before `releaseDelayedInputsForStep(tick, 1)`.
- **`releaseDelayedInputsForStep`** takes the kept time (and clears it) before its server test, and passes it to og-sim-net's
  `releaseForStep`, which appends it to each `[Release]` line (`ServerReceptionCoordinator-rationale.md` §8).
- **The lines:** `[AuthoritySimulation] tick=<t> stepSeconds=<s>` and `[Release] id=… late=<n> stepSeconds=<s>`, the same
  value on both for one step; the clock is the latency probe's (`FPlatformTime::Seconds()`, the same on every process of
  one machine). Everything before the field is unchanged.
- **Loop on and loop off alike.** Off the loop the stamps are already prompt; the field is written anyway, so the gate tools
  read one timeline on every arm.
- **Not on Chaos.** The Chaos configuration releases a frame's batch from `InjectInputs_External` on the game thread and steps
  on the physics thread. It never calls `noteAuthorityStepTime_Step`, so its lines are as before, and nothing written on one
  thread is read on the other.

## §20 The clock's RTT source: SimClock samples, the knob, the shadow line (og-input-latency task P3-10)

- **Two feeds, one estimator, one thread.** The engine's round trip arrives in `onTimingInfoReceived` (once per drain that
  delivers a clock tick, §9.6) and goes to `NetworkTimeEstimator::noteEngineRttSample`. A SimClock sample arrives in
  `onSimClockSample`, from the client host's `onClockSample` (`UESimNetLanes-rationale.md` §13); it is copied field by field
  into og-simulation's `SimClockRttSample` (og-simulation names no og-sim-net type) and goes to `noteSimClockSample`. Both run on
  the client session's drain thread, the estimator's writer: the game thread frame-locked, the step's thread immediate. Which
  feed moves the estimate is the estimator's rule (og-simulation `TimeConfig-rationale.md` §15): the engine until the first valid
  SimClock sample under `ClockRttSource=SimClock`, which switches it once without a reset, and the SimClock after that.
- **The knob.** `[OGNetcode] ClockRttSource=Engine|SimClock` (case-insensitive, surrounding blanks trimmed), read by
  `readSessionKnobs` on both roles, as `RecentreSettleSamples` is (§9.8): it acts only on a predicting client, which reads its
  own ini. `applyClockRttSource` runs at the end of `applyLatencyKnobs` and calls `SimulationManager::setClockRttSource`. An
  unrecognised value is reported (`[ClockSource.Config] ini [OGNetcode] ClockRttSource='…' unrecognised (expected Engine or
  SimClock), keeping the compiled default`) and the compiled default kept, never read as either value. The proof line,
  `[ClockSource.Config] clockRttSource=<Engine|SimClock> (<ini|compiled>) jitterMultiplier engine=<m> simClock=<m> (<ini|compiled>)`
  (the `jitterMultiplier` field since og-input-latency task P3-13, below), is unconditional, at `Warning`, read back from
  `TimeConfig`, once per session on every peer. The setter runs before any sample can reach the estimator: `configureSession`
  runs before `startStepping`, so an immediate client's step drain has not started, and a frame-locked client drains on the
  game thread that runs it (the setter's one-shot rule, og-simulation `SimulationManager-rationale.md` §16).
- **The jitter knob** (og-input-latency task P3-13). `[OGNetcode] JitterMultiplier=<number>`, read by `readSessionKnobs` on
  both roles (`GConfig->GetDouble`, the game ini first, as every knob here) and applied in `applyLatencyKnobs` after
  `RecentreSettleSamples`, before `applyClockRttSource`. One value sets **both** sources' multipliers through
  `SimulationManager::setJitterMultiplier`: `TimeConfig::jitterMultiplier` (the engine ping's) and `simClockJitterMultiplier`
  (the SimClock's), whose compiled defaults differ (og-simulation `TimeConfig-rationale.md` §17). The key exists so the
  multiplier's measured choice ran every value on one binary. A value that is not a finite number is reported
  (`[ClockSource.Config] ini [OGNetcode] JitterMultiplier is not a finite number, keeping the compiled defaults`) and both
  compiled defaults kept; one outside `clockJitter::clampJitterMultiplier`'s 0 to 4 is reported as `… out of range, clamped
  to …` and clamped. The result is printed on the `[ClockSource.Config]` line rather than a line of its own, so no new tag
  joins the start-up lines; its trailing token is `ini` only when the key was applied. The setter runs where
  `setClockRttSource` runs, before `startStepping`, so before the clock reads the offset (the setter's one-shot rule,
  og-simulation `SimulationManager-rationale.md` §17).
- **The shadow line**, `[ClockSource.Window]` on `LogOGNet` at `Warning`, once per 10 s of `FPlatformTime`
  (og-sim-net's `kSimNetWindowSeconds`), checked from either feed on the drain's thread, on a predicting client only; the first
  call starts the window, so a client that receives neither feed logs nothing. Its fields:
  - `source=` the estimator's active source (`getActiveRttSource`);
  - `engineRttMs=` the session's own EMA of the engine readings above 0, at `rttSmoothingAlpha` and with no plausibility gate.
    While the engine is the active source this is the estimator's arithmetic without its gate; after the switch the estimator
    drops engine readings, so this is the only engine figure left to compare against. `-1.000` before the first reading above
    0 (the engine ping reads 0 for its first second, og-input-latency task P5-28);
  - `simClockRttMs=` the estimator's shadow EMA of the SimClock RTT (no gate); `-1.000` before the first sample;
  - `ackSrttMs=` og-net's smoothed ack round trip to the server, published by the lanes each frame (`UESimNetLanes-rationale.md`
    §13); a cross-check only: og-net's ack RTT includes the peer's ack delay. `-1.000` without a reading;
  - `minRttOffsetUs=` the offset of the minimum-RTT sample among the last 8 (NTP's clock filter): the server clock minus the
    client clock, which on a link with unequal one-way delays is off by half their difference, `(up − down) / 2`
    (`SimNetClock-rationale.md` §1);
  - `owdVarUs=<up>/<down>` the RFC 3550 interarrival jitter of `t1 − t0` (up) and `t3 − t2` (down), microseconds;
  - `smoothedRttMs=` the estimator's estimate, from the active source, through its gate;
  - `samples=` and `invalid=` the estimator's SimClock sample counts, and `pings=` and `pongs=` the clock client's, all
    cumulative.
- **Reading it.** SimClock leaves out the server's frame turnaround that the engine's ack round trip carries (og-input-latency
  task P5-27), so on one link `simClockRttMs` is expected at or below `engineRttMs` by about one server frame. That comparison is
  what the switch's flip rests on (og-simulation `TimeConfig-rationale.md` §15).

## §21 The game-thread paths off the authority (og-input-latency task P5-20)

With `og.Sim.AuthorityLoop=1` (§19) the game thread of a dedicated server touches no simulation state: every read of storage,
the systems, og-sim-net's channels or the probe's step-side state is on the runner, and what the game thread needs it reads
from a copy the runner publishes. The P11-6 entry checks (the Jolt world's caller check) and the `checkf`s below hold that
over a session. Every branch tests `m_authorityLoop`, so with the loop off every line and path is as before.

### 21.1 PROBE A per stepping wake

On the loop the tick driver calls `onFrameStepsDue` once per stepping wake, on the runner (P11 design §7). PROBE A's
"frame" is then a wake: `noteFrame` takes `++m_authorityWakes_Runner` (a runner-only counter) instead of `GFrameCounter`, which
is the game thread's frame count and has no meaning on the runner. Every sample is one wake after the last, so the cadence line
reads `dFrame0=0 dFrameGt1=0`, `meanTicksPerFrameX100` and the percentiles are sim ticks per stepping wake, and `meanFrameUs` is
the mean wall time between stepping wakes. The line keeps its text, `[RelayProbe.Frame] role=Server`; the server's
`[SimHost.Mode] … stepping=runner` line tells the two readings apart.

### 21.2 PROBE 6 retired on the loop

PROBE 6 (`[RelayProbe.Budget]`, `[PacketBudget]`) reads the `UNetDriver`'s client connections, which are game-thread state.
`onFrameStepsDue` returns
before it while the loop runs, and its first statement after that return is a `checkf(IsInGameThread())`, so a PROBE 6 that
ran on the runner (the return deleted) fails at its first wake instead of racing the driver. Inline Jolt and Chaos call
`onFrameStepsDue` on the game thread (the tick-driver binding at `TG_StartPhysics`, the Chaos host's `InjectInputs_External`),
so the check holds there and PROBE 6 is unchanged.

### 21.3 The authority presentation snapshot (the ring-out score)

The brawler's ring-out score is sim-derived presentation that only the authority computes (P7 design D2): its score system is
authority-only, and the replicated score property is the scoreboard's source. Inline, `Game::afterVisualization` reads the
score system on the game thread after the frame's steps (§10). On the loop the runner writes it, so:

- **The snapshot.** `AuthorityPresentation` is `entryCount` and up to `kMaxSimulatableSlots` entries, each a sim id and the
  game's `AuthorityPresentationSlice` (a game type; the brawler's is its score and whether the score system holds an entry,
  `BrawlerSessionGameUImpl-rationale.md` §8 in OGBrawlerUnreal's docs). It travels in a `TripleBuffer` (og-sim's
  `Mailbox.h`, one writer and one reader), held through a `std::unique_ptr` like the display tier's.
- **The runner fills it** in `publishAuthorityPresentation_Runner`, the last statement of `afterAuthorityBatch` on the loop:
  one entry per simulatable in storage, from the game's `authorityPresentationSlice_Step(systems, id)`, then `publish()`.
- **The game thread applies it** in `presentAuthority_GameThread`, from `onPostPhysicsStep`'s loop branch before the latency
  tick (where `afterVisualization` sits inline): the newest snapshot, one `Game::presentAuthoritySlice(session, id, slice)` per
  entry. The brawler's finds the registered component for the id and writes the score property, so clients see the score
  through the same `OnRep` as inline.
- **The two hooks are checked** by a `static_assert` in `publishAuthorityPresentation_Runner` and are in the name-clash probe
  (`ueSimulationSessionHookNames`, §12).
- **Why not the display snapshot.** The display snapshot (§17) is og-sim's, per entry the simulatable's state; the score lives
  in the systems executor, which og-sim's snapshot does not read, and a loop-on server publishes no display snapshot (§21.4).

### 21.4 The dedicated server's visualization gate

A free-running authority has no game-thread viz world (`viz=none`), and the tick backend refuses `registerVizVolume`,
`editVizQuery`, `getVizReader` and `overlayRenderPoses` (`TickDriverBackendUImpl-rationale.md` §15). The session answers the
question for every caller: `hasGameThreadVizWorld()`, public, is `!m_authorityLoop`. The character component asks it before it
registers its viz volume and before it draws (`OGBrawlerSimulatableComponent-rationale.md` §24, in OGBrawlerUnreal's docs).
The session itself publishes no display snapshot on the loop (§19.5), and `publishDisplaySnapshot_GameThread` opens with a
`checkf` that the loop is off: the fill reads storage, the reconciliation and the input resolution, all the runner's. So every
reader of the display snapshot (the component, the input-history feed, the scoreboard) sees none on a loop-on server.

### 21.5 The probe stamps through the mailbox

The latency probe's state is the game thread's; the step thread posts events through its mailbox (`UELatencyBudgetDiagnostics-
rationale.md` §6). On the loop the server's three game-thread stamp sites are the runner's, each with a mailbox door
(`UELatencyBudgetDiagnostics-rationale.md` §14):

- og-sim-net's `stampServerReceived` and `stampHanded`, which the lanes' server host forwards (`UESimNetLanes-rationale.md` §12);
- the corrections' `Handed`, which `tick_GameThread` stamps inline by walking `forEachAuthorityCorrectionTick`. That walk reads
  the session's channel map (the game thread's) and each channel's correction buffer (the runner's), so on the loop the game
  thread skips it, and the runner's `stampCorrectionsHanded_Runner`, after og-sim-net's `afterInlineBatch` has flushed the
  batch's corrections, reads each simulatable's channel from og-sim-net itself (`SimNetServer::channel`) and posts the tick it
  carries. Both walks read the tick with `correctionTickOf(channel)`.

The per-connection round trip comes the other way, from the game thread to the runner, through the lanes' RTT table, which the
game thread fills each frame (`UESimNetLanes-rationale.md` §12).

### 21.6 What the game thread still does on the loop

`onPostPhysicsStep`: the test hitch, the presenter's `present` (J9's registrations, then the render snapshot), the lifecycle
outcomes (§19.4), the authority presentation (§21.3) and the latency tick, whose server part on the loop is the mailbox drain,
the pending stamps and the window lines. The lanes' `afterDispatch` publishes the RTT table. Registration's game-thread half
reads only `UObject`s and the session's own maps (§19.4).
