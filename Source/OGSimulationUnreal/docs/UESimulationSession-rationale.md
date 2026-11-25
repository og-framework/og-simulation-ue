<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: hasServerTierWiring -- deleted by og-session-runtime task 7b (no caller after task 3 deleted the brawler's Chaos host); kept in the history -->
<!-- lint-external-ref: hasServerTierWiring() -- deleted by og-session-runtime task 7b (no caller after task 3 deleted the brawler's Chaos host); kept in the history -->
<!-- lint-external-ref: editStorage -- deleted by og-session-runtime task 7b (no caller after task 3 deleted the brawler's Chaos backend); kept in the history -->
<!-- lint-external-ref: applyTierTransitionStall -- retired by og-input-latency P6-2 (the client pays its effective delay's change, either sign); kept in the history -->
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
<!-- lint-external-ref: findTimingRelay -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: beginReceptionCoordinator -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: forgetReceptionOwner -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: getDelayedInputComponentsById -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: m_delayedInputComponentsById -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- lint-external-ref: m_backendWorlds -- RENAMED by og-ue-simulationcomponentandmanagersplit task 35 to m_physicsBackendWorlds; kept in the task-24a seen-failing record of §2 and in §14's record of the rename -->
<!-- lint-external-ref: beforeAuthorityPump_Runner -- RENAMED by og-time-dilation task 8 to beforePump_Owner; kept in §22.3's record of the rename -->
<!-- lint-external-ref: onClientStepStarted_Step -- SPLIT by og-input-latency task P5-30 into receiveForClientStep_Step and afterClientCapture_Step; kept in §18.2's record of the old order -->
<!-- lint-external-ref: onFrameStepsDue_GameThread -- RENAMED by og-input-latency task P5-33 to onFrameStepsDue_Step; kept in §7's provenance and in §9.9's record of the rename -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root (§9.9) -->
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
  PROBE 6 (§9.9; PROBE 6 retired by og-session-runtime task 14). One contiguous section.
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
  doors `physicsSlotFor` and `recordAuthoritativeSlot`, and the correction-tick walk (deleted by og-session-runtime task 14).
* **§16** is og-input-latency task P5-7, the switch: the sim traffic on og-net's sim lanes (frame-locked until og-session-runtime
  tasks 12 and 14); og-sim-net's server in this session and the lanes' client session; registration through the roster; and
  the two early unregistration paths.
  Where an earlier section describes the relays, the listeners, the session's own coordinator or its component map, §16 is
  what replaced it.
* **§17** is og-input-latency task P5-16: the display snapshot the game thread draws from, where it is published (after
  each client batch, on the step's thread, since og-input-latency task P5-36), the game-thread view of it, and the doors
  that read it.
* **§18** is og-input-latency task P5-17: the client's one mode (immediate; the frame-locked client was retired by
  og-session-runtime task 12), its lines, the step hooks, registration through og-sim-net's lifecycle queue, unregistration,
  teardown, and what frame-locked kept.
* **§19** is og-input-latency task P5-19: the authority loop, every authority since og-session-runtime task 14, the dedicated
  server stepping on its runner: its I/O mode, start, og-sim-net and registration on the runner, the game thread, and stop; and
  task P5-37's step time on the runner's `[AuthoritySimulation]` and `[Release]` lines (§19.7); and og-time-dilation task
  2b's publish of the server tick to the SimClock server with that same step time (§19.8).
* **§20** is og-input-latency task P3-10: the clock's RTT source, its two feeds, the `ClockRttSource` knob and the shadow line;
  and task P3-13's `JitterMultiplier` knob, on the same proof line.
* **§21** is og-input-latency task P5-20: the game-thread paths off the authority on the loop: PROBE A per stepping wake,
  PROBE 6 retired, the authority presentation snapshot, the visualization gate, the probe stamps, and what the game thread
  still does.
* **§22** is og-time-dilation task 8: clock dilation on a client in `Off` and `Shadow`: the server tick on the SimClock sample,
  the `ClockDilation` knob and its `[Dilation.Config]` line, `beforePump_Owner` and its two roles, the stage's threading and its
  two clocks, the `[Dilation.Window]` line, and why `Shadow` cannot change the rate; og-time-dilation task 11 accepts `On`
  (§22.6), adds the four dev doors and their clamps (§22.8) and the authority's `[Dilation.Authority]` line (§22.9);
  og-time-dilation task 14 accepts the `Common` setpoint and adds its lead's door (§22.8) and the smoke (§22.11).
* **§23** is og-session-runtime tasks 11 and 14: a world without a session: the start rule (task 14: a standalone, listen-server
  or foreign-driver world starts none), the retired knobs' refusals (task 11), the registered instance slot, the registration
  line, every public member with no session, and what a refused client and its server do.
* **§24** is og-session-runtime task 16: the threads, who calls what on which thread for both roles, the per-step
  order and what crosses; og-simulation's `ThreadingCrossings.md` points here for the callers. og-session-runtime
  task 22 moved its text to og-sim-session's `SessionThreads-rationale.md` §7; §24 keeps the pointers and the
  no-session world's note.

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
    → m_clockEngine → m_clock (og-sim-session's SessionClock, which holds the tier consumer since og-session-runtime task 24)
    → m_simNetLanes, m_simNetServerHost, m_simNetServer
    → m_frameHealthProbe
    → m_registeredComponentsById, m_authoritativeSlotsByActor, m_simUnregisteredIds
    → m_onTimingInfoReceivedCallback
    → m_pendingRegistrations
```

* **`m_owner` and `m_game` come first.** They are references, which have no destruction order, and the backend is
  constructed with the session, so both must be bound before it. No backend reads them during its construction today
  (the tick-driver host stores the session in its game binding; the former self-scheduled host, the brawler's Chaos host
  removed by og-session-runtime task 3, stored it too), but a reference
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
(task 26b) sit where the owner declared them, PROBE A after the coordinator and PROBE 6 after PROBE 5 (until
og-session-runtime task 14 deleted PROBE 6); both held plain counters, so their destruction is not observable either.
**Since og-input-latency task P5-7** the og-net members stand where the coordinator stood (§16): the lanes pointer, the
server's host and the server, which holds the reception coordinator (and, until og-session-runtime task 14, the session's own
detached lanes); the server is destroyed first among them (it references its host and the manager's `TimeConfig`), and one
row asserts it. The component map is the registered-component map, beside the slot map (keyed by the pawn actor) and the
early-unregistration set (the in-frame server's channel map stood between them until task 14); PROBE 5 and the timing relay
pointer are gone.

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
| `m_manager`, `m_inputResolution`, `m_clockEngine` < `m_clock` | destruction: the clock holds references to all three, and its tier consumer borrows the manager's `TimeConfig`, so it goes first (the `m_manager` < `m_replicatedTierConsumer` row, moved onto the clock with the consumer by og-session-runtime task 24; task 25; in the owner it was declared above the manager's owner, and only the owner's `EndPlay` reset protected it; that reset is `endPlay`'s since task 27a, `⛔G-21`) |
| `m_manager` < `m_simNetServer`, and `m_simNetServerHost` < `m_simNetServer` | destruction: og-sim-net's server (with the reception coordinator inside it) references the manager's `TimeConfig` and its host, so it goes first (og-input-latency task P5-7; the row was `m_manager` < the session's own coordinator from task 26a; the session's own detached lanes were in it until og-session-runtime task 14) |

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
*this, m_physicsBackendWorlds }`. The traits pick the host:

* **A driver-scheduled engine (Jolt)**: the worlds are `TickDriverBackendWorldsUImpl` over the engine's world host, and
  the backend is `TickDriverBackendUImpl` over the game's tick-driver binding, `TraitsT::TickDriverGame<UESimulationSession>`
  (`TickDriverBackendUImpl-rationale.md` §1, §10, §12).
* **A self-scheduled engine (Chaos)** was the other shape: the game's own host, with an empty worlds struct. It is the
  former Chaos configuration, removed by og-session-runtime tasks 3–6 (task 3 deleted the brawler's Chaos host and its
  docs; task 6 made og-simulation's `PhysicsEngineTraits` require a world host of every engine), so every host is now
  driver-scheduled.

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
`publishClientEffectiveInputDelayTicks` (og-sim-session's `SessionClock`'s own since og-session-runtime task 24), and two const reads for a game's parts, `getClientEffectiveInputDelayTicks` and
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
coordinator's type, `getReceptionCoordinator` (no caller left in code) and
`getServerReceptionTick` (the server host's `receptionTick()`), §16.3. `hasServerTierWiring`, the former Chaos host's
test, had no caller left in code after og-session-runtime task 3 deleted that host, and og-session-runtime task 7b
deleted it. og-input-latency task P5-9 deleted the two `Replayed`
overrides, so three listener overrides remain (`onConnectionTierReceived`, `onRelayDelayFloorReceived`,
`onTimingInfoReceived`), and task P5-26 deleted `getSyncedTimingBuffer`. The og-sim-net hosts also call
`onSimClockSample` (og-input-latency task P3-10, §20) and `onSimNetLifecycleUnregistering` (`⛔G-35`). The owner keeps its own
`getLatencyBudgetDiagnostics()` / `editLatencyBudgetDiagnostics()`, which forward here, so its latency lookups (`instanceFor`)
are unchanged. *R0, task 28:* the mutable one keeps one caller, the latency member's own `instanceFor`
(`UELatencyBudgetDiagnostics.h`), which reaches it through `TraitsT::SessionOwner::instanceFor`; the const one has no caller
left in code (the former self-scheduled host, removed by og-session-runtime task 3, read the session's own pair). No accessor
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
task 24a read the disassembly of the per-step release and the step stamps. The self-scheduled host this paragraph names
is the brawler's former Chaos host, which og-session-runtime task 3 deleted with every one of its calls; `editStorage` had
no caller left in code since, and og-session-runtime task 7b deleted it.

**Weak-lambda owners stay the actor.** A delegate's weak object must be a `UObject`, and this class is not one. Every
`AddWeakLambda` that took the owner still takes it, as `&m_owner.owner()`: the latency member's post-tick-flush binding,
the only one left since og-session-runtime task 3 deleted the former self-scheduled host and its two physics-scene bindings.

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
| `editStorage`, editReconciliation (mutable and const) | the storage and the reconciliation peer themselves |
| `getLastRelayedInput`, `getLocalInputCache`, `isLocallyControlledOnThisPeer` | the input-resolution peer and its diagnostic view |
| `getRelayedReadObservations`, `getRelayedInputArrivals` | the input-resolution peer's diagnostic view |
| `getVizSlice<SliceT>` | one slice of a simulatable's visualization copy (`⛔G-02`) |

⚠ **Since og-input-latency task P5-16 the visualization rows of this table are history.** `getLastRelayedInput`,
`isLocallyControlledOnThisPeer` and `getVizSlice` answer from the display snapshot, and editReconciliation,
`getLocalInputCache` and the two relayed-ring doors are deleted (§17.4); og-session-runtime task 7b deleted `editStorage`. The paragraphs below on `getVizSlice` and the
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
(`TickDriverBackendUImpl-rationale.md` §3; the brawler's former Chaos host, removed by og-session-runtime task 3, had its
own rationale until then).

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
| `instanceFor`, the static registry `s_instances`, the destructor | the owner registry: one slot per world, found by world and role (§9.2) |
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
* **`publishServerClock_GameThread`** was new (the initiative's review, finding m-14): the authority's server-clock write into
  the timing relay's buffer, extracted from the owner's post-physics step and called at the position of the block it
  replaced (in `onPostPhysicsStep` from task 27a, §10, until og-session-runtime task 14 deleted it with the in-frame server;
  the runner publishes the clock after each batch, §9.6).

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

**One session per world** (og-session-runtime task 16b). `instanceFor(world, isAuthority)` returns the session registered
for that world and role, or a null pointer. The registry is `kInstanceSlots` (128) atomic slots, and every session owner
that reaches its start rule takes one (`registerInstance`, a compare-exchange from null), so a caller finds only its own
world's session, also when one editor process holds several worlds (a single-process PIE session: the dedicated server
and every client, §23). A world that starts no session (a listen server, a standalone world, a refused one) still takes a
slot, with a no-session session (§23). A started session takes its slot where it did before: a client in
`configureSession` on the game thread, the authority in `configureSession` on its runner, before its first batch; that
write from the runner is why the slots are atomic. A refused one takes it in `refuseToStart`. `refuseToStart` writes the
world and the role (`m_registeredWorld`, `m_registeredAsAuthority`) on the game thread before either, and the runner starts
after it. `endPlay` and the destructor release the slot (`unregisterInstance`). All 128 taken is a `checkf` in
`registerInstance` (the editor's play settings offer up to 64 clients). The owner's own `instanceFor` forwards here and
returns the registered session's owner; its callers (the brawler's component, its HUD, the latency member, the component
binding) pass their own world. Until task 16b the registry was two slots, one per role (0 authority, 1 client), with a
`checkf` on a slot already taken: a second client world or a second standalone world in one process (a single-process PIE
session with two clients, or two standalone players) asserted, "UESimulationSession: unexpected state".

**One started session per role per process.** `s_startedSessions` (two entries, game thread only) names the session of
each role that passed the start rule, until its `endPlay`. A second world of that role in the same process starts no
session (§23's last refusal): the session still keeps per-process state per role, which a second session of one role would
share: the tick driver's thread-context masks have one bit per role (`TickDriverBackendUImpl-rationale.md`, "Why the
thread-local flags are keyed per role"), and `simlog::setGlobal`, the game's loggers (`Game::installGameLoggers`) and the
test hitch's variables (§10) are per process.

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

**The write crossing carries one scalar.** The recompute (og-sim-session's `SessionClock` since og-session-runtime task 24,
`SessionClock-rationale.md` §4; this class's `publishClientEffectiveInputDelayTicks` until then) writes a lone `std::atomic<int32>`
inside the input-resolution peer (`SimulationInputResolution`'s effective-delay atomic), which the step loads once per tick
in `collectInputAll` and once per resimulated tick in `collectResimInputAll`. Since og-session-runtime task 12 every
client's listener members are called by the client host from its drain at the step's start, on the step's thread (§18.2,
§24), so the receipts write the atomic on the thread that loads it; the start-up's publish (`beginTierConsumer`) runs on the
game thread before any step. The registry is written in the start-up and the teardown, on the game thread. Until
og-session-runtime task 12 the listeners ran from the og-net driver's PostTickDispatch on the game thread (the relays'
replication callbacks until og-input-latency task P5-7), and this scalar was the transport's one crossing: the worst a race
could do was apply a new delay one tick late. *Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §1 ("The
write crossing carries one scalar"); R0, task 25: the resimulation load was not listed.*

**Why the passthroughs are narrow.** `requestInputDelayTransition` (`requestInputDelayIncreaseStall` until og-input-latency
P6-2) and `publishClientEffectiveInputDelayTicks` are
one-purpose entry points onto objects otherwise driven only by the manager's tick loop (the client clock, the input
resolution's atomic). Handing game-thread code a general mutable handle to one of them would invite exactly the
cross-thread reach the entry point exists to bound, so do not widen one into an accessor. The transition request is private to the session since task 25 (only the tier and floor helpers call it, through
`payInputDelayTransition`); the publish was public, and its one caller is the recompute (`SessionClock-guards.md` G-03). Since
og-session-runtime task 24 both are private members of og-sim-session's `SessionClock`, and the publish is no longer a door of
this class. `requestInputDelayTransition` also guards on `runsPrediction()` (`SessionClock-guards.md` G-01): a server or standalone
manager has no client clock, and `editClientClock()` would `std::terminate`. *Moved from §1 of the brawler's rationale
("Why the passthroughs are narrow rather than `edit*()` accessors", these two names and its last paragraph).*

### 9.4 The client's effective input delay

og-sim-session's `SessionClock` since og-session-runtime task 24. The formula over the floor and the tier, the one recompute
over both inputs and its return value, both receipts' payment of the change either sign (the first arrival included), the floor
advisory and the tier consumer's lifetime are `SessionClock-rationale.md` §4, moved from this section with the code and
re-verified there; the prohibitions are `SessionClock-guards.md` G-01 to G-04 and G-07 (this file's G-03, G-04, G-05, G-09 and
G-40 until task 24).

What stays this class's:
- **The clock**, `m_clock` (`std::optional<Clock>`, `Clock = ogsession::SessionClock<ManagerType, InputResolution,
  UESessionClockEngine>`), emplaced right after the manager on both roles in `beginPlay`, over the manager, the input-resolution
  peer, `m_clockEngine` and the role's thread checks (the world host's thread context, the frame thread's id);
  `compositionContractsHold()` asserts `m_manager`, `m_inputResolution`, `m_clockEngine` < `m_clock` (§2).
- **Its start-up publish**, the two `m_clock->beginTierConsumer()` calls in `beginPlay` (`⛔G-06`), and **its reset**,
  `m_clock->endTierConsumer()` in `endPlay` (`⛔G-21`).
- **The forwards the client host calls**, `onConnectionTierReceived` and `onRelayDelayFloorReceived` (the clock's
  `onConnectionTier` and `onRelayFloor`, which return at once without a clock), and the const read `getReplicatedTierConsumer`.
- **The stopped drain's markers** around the teardown drain (`m_clock->beginStoppedDrain()`, `endStoppedDrain()`, §18.6;
  `SessionClock-rationale.md` §3).

### 9.5 The relay ring (retired host boundary)

From task 25 until og-input-latency task P5-7 the session bridged each input relay host (`ASimulationInputRelay`, which cannot
name a game's character) to its character's net-sync sibling, from the host's own calls in (`onInputRelayHostReady`, an
`ISimulationInputRelayListener` member). Since P5-7 no input relay is spawned or found: the server's stager
(`RelayRingStager`, one per character in og-sim-net's server) publishes the character's ring on og-net's SimRelay lane at
each bundle's receipt (once per server frame until og-session-runtime task 15), and the
client session stores it in that character's channel (`SimNetChannel::storeRing`), which fires the arrival callback og-sim's
`SimulationNetSync` bound on the channel at a remote character's registration (§16). A ring that arrives before its character's roster Add is held per sim id and stored at
the Add (`SimNetClient-rationale.md`).

### 9.6 The server clock and its callback

**The server clock.** The authority runner publishes the server-clock tick through og-sim-net's server
(`SimNetServer::publishClock`) after each batch, in `afterAuthorityBatch` (§19.3), and og-sim-net sends it there, as a
`ClockPublish` on the SimClock lane, only when the tick changed. Until og-session-runtime task 14 the in-frame server published
it from `onPostPhysicsStep` on the game thread (publishServerClock_GameThread) and og-sim-net sent it at the next `TickFlush`.
Until og-input-latency task P5-7 it was written into the timing relay's synced buffer, which a relay found once per world (and
logged `SimulationManager: timing relay found`).

**`onTimingInfoReceived`** first hands the round trip to the latency member's wire sample, then either to the callback set
by `setOnTimingInfoReceivedCallback`, if one is set, or, with a clock, to its `onAuthorityTick` (og-sim-session's
`SessionClock` since og-session-runtime task 24: the window line's EMA, the estimator's `noteEngineRttSample` and
`recordAuthorityTick`, then the window line; `SessionClock-rationale.md` §5).
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
| `ReceptionCoordinator`, `getReceptionCoordinator`, `hasServerTierWiring` | og-sim-net's server's coordinator and its presence (`hasServerTierWiring` deleted by og-session-runtime task 7b) |
| `getServerReceptionTick` | unchanged; the server host answers og-sim-net's `receptionTick()` with it |
| `releaseDelayedInputsForStep` | `[ReleaseBatch]`, then the server's `releaseForStep` (with the step's time since P5-37, §19.7) |
| `afterAuthorityBatch` | the server's `afterInlineBatch` (its corrections, clock, reap and window line; the reap was a member of its own until og-session-runtime task 15) |
| the correction-tick walk | the walk of the registered channels (deleted by og-session-runtime task 14 with the in-frame server) |

**Still exempt from the seam** (`⛔G-11`): the release is typed on the game's input, and so is og-sim-net's server.

**The release and the reap**, on the runner: `onFrameStepsDue_Step` (PROBE A, §9.9), the release once per authority step
(`releaseDelayedInputsForStep(tick, 1)`, from `releaseInputsForAuthorityTick`), which prints
`[ReleaseBatch] first=<tick> numSteps=<n>` on `LogOGNet` at `Verbose` (its row is in `kOGAdapterLogRoutes`,
`UESimulationLogRoutes-rationale.md` §6), and the reap once per batch, in og-sim-net's server's `afterInlineBatch` (§19.3,
§24).

**Threads.** Since og-session-runtime task 14 the reception runs on the authority's runner only: og-sim-net's server drains in
`beforePump_Owner` and releases per step, and `getServerReceptionTick()` reads the server clock on the runner, the thread
that advances it (§24). Until then the in-frame server drained at the driver's PostTickDispatch on the game thread and read
the server clock, which the physics thread wrote, with no synchronization (the acknowledged wart; the tier average is
insensitive to a one-tick skew). *Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §1 ("One acknowledged
wart").*

**The relay's schedule stamp is unchanged.** The coordinator still hands the relay sink `(captureTick, dA, input)`, where `dA`
is the effective input delay the authority held for that wire at receipt, so a peer derives the application tick as
`captureTick + dA`; og-sim-net's server relays each bundle's staged entries at once, at its receipt. Until og-session-runtime
task 15 the stager published a whole frame's staged entries once, at the frame's flush, as the input relay's `PreReplication`
did, so two arrivals in one server frame both reached the wire.

### 9.8 The session knobs (task 26b; og-sim-session's since og-session-runtime task 23)

The session's fourteen `[OGNetcode]` keys are read and applied by og-sim-session's `SessionKnobs.h`: its rationale
(`SessionKnobs-rationale.md`) has the keys, the four steps every knob takes, each knob's role rule, clamp and lines, and its
guards (`SessionKnobs-guards.md`) the intake's prohibitions. Task 26b moved the intake here from the brawler's owner as three
members, og-input-latency tasks 4, 27, P3-10 and P3-13 and og-time-dilation tasks 8, 11 and 14 added keys and a fourth member,
and og-session-runtime task 23 moved the members to og-sim-session, verbatim first and then adapted (`SessionKnobs-rationale.md`
§1). The guards that marked them here (G-14, G-15, G-16, G-17, G-27) retired with the move, each with its successor there.

What stays in this class is the binding, in `beginPlay`, at the positions the members had:

| call | where | what the session passes |
|---|---|---|
| `ogsession::readSessionKnobs(UEConfigSource{}, worldIsAuthority)` | after the latency member's begin, before the backend builds its world: both roles | the `[OGNetcode]` section through `UEConfigSource`: the game ini, then the engine ini, strings trimmed (`UEConfigSource-rationale.md`) |
| `ogsession::applySessionKnobs(knobLog, *m_manager, server, sessionKnobs, advisory)` | in the authority branch, after the neutral input and the og-sim-net server's start, before the tier consumer's start | the manager; og-sim-net's server as a pointer (`nullptr` when there is none, which the function's `OG_CHECK` refuses, as the member's `checkf` did); a lambda that calls `logRelayDelayFloorAdvisory` (§9.4) |
| `ogsession::applyResimTriggerPolicy(knobLog, *m_manager, sessionKnobs)` (`⛔G-24`) | after both role branches | the manager |
| `ogsession::applyLatencyKnobs(knobLog, *m_manager, sessionKnobs, m_clock->dilationController(), m_clock->dilationWindow())` | directly after it | the manager, and the client's dilation controller and window (§22.2) |

`knobLog` is a `UESessionLogSink` (`UESimulationLogRoutes.h`). A knob line carries its level as a `[Warning]` token, or none for
the floor's `Log` line; the sink takes the level from the token, the category from the line's tag (each of the seven tags,
`[RelayDelayFloor]`, `[StateRotation]`, `[ResimGate]`, `[LatencyKnobs]`, `[ClockRecentre.Config]`, `[ClockSource.Config]` and
`[Dilation.Config]`, has a `LogOGNet` row in the route table since task 23), and prints the line without the token. So each line
reads byte for byte, category and level included, as the member's `UE_LOG` did: a `static_assert` beside this header's includes
checks each of the 22 formats' category and level against the old `UE_LOG`, and the parity tests are `SessionKnobs-rationale.md`
§8.

The subsections that stood here moved with the code: `RelayDelayFloorTicks`, `CorrectionRotationK`, `ResimTriggerPolicy`, the
latency knobs, `RecentreSettleSamples` and the retired fourth knob are `SessionKnobs-rationale.md` §2.1 to §2.7.

### 9.9 The frame probes: PROBE A and PROBE 6 (task 26b)

`onFrameStepsDue_Step(firstUpcomingSimTick, numSteps)` is the per-frame work before the steps: the host calls it on
the thread that runs the steps, once per frame that has steps due (on the authority runner once per stepping wake, §21.1; on a
client's game thread otherwise), with the frame's first upcoming simulation tick and its step count. It was
`onFrameStepsDue_GameThread` until og-input-latency task P5-33 renamed it for that reason (the `_Step` suffix of
`afterStep_Step`). Its body is **PROBE A**, frame health, on both roles. Until og-session-runtime task 14 two more pieces
followed it: the server test (G-18, retired; the coordinator test until og-input-latency task P5-7), at which a client returned
and the authority loop returned next (§21.2), and on the in-frame server, per client connection of the world's net driver, the
one-shot `[PacketBudget]` line and **PROBE 6**, the per-connection send budget. Task 14 deleted both with the in-frame server.

The tick driver calls it from its pump through the brawler's tick-driver binding: a client's at `TG_StartPhysics`
(`OGBrawlerSimulationManager-rationale.md` §7 and §8, in OGBrawlerUnreal's docs), the authority's on its runner (§24); the
brawler's former Chaos host called it from its per-frame input hook until og-session-runtime task 3 deleted that host.

The body moved statement for statement; the task's notes hold the token comparison. The spellings changed where the owner
reached the session (`m_session.m_manager` is `m_manager`, its coordinator `m_receptionCoordinator`) and where the owner
reached itself as an actor (its world is `m_owner.GetWorld()`). No log text, field, level or category changed. PROBE A's probe
is a member of this class since task 26b, declared where the owner declared it (§2); PROBE 6's was too, until task 14.

All the frame probes are purely diagnostic: nothing reads them but the log lines they feed. **Volume convention:
per-window summaries at `Warning`, per-event detail at `Verbose`, and nothing per-tick or per-write at any verbosity.**

#### PROBE A: frame health (`m_frameHealthProbe`), both roles

Sim ticks per frame: a client's game-thread frame, the authority's stepping wake (§21.1).

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
measured in the host's per-frame call (on a client's game thread, on the authority's runner), from the tick the host hands
it, and the only tick source legal to read on that thread is the host's own (the brawler's tick-driver host: the dispatch counter plus the published offset;
the former Chaos host, removed by og-session-runtime tasks 3–4, read its tick mapper's atomic offset). og-simulation's peers
have no access to it.

**Why this hook and not the owner's post-physics step, on either role:** that hook runs on the game thread on both roles, but it
receives no physics step number, and therefore has no route to a sim tick through the only source safe on that thread.
Sampling there would have required either reading the physics-thread-written clock or inventing a second counter, both
ruled out. This hook already has both numbers: the step, and the frame's step count.

**Why reusing `firstUpcomingSimTick` is safe on the client,** even though a host's `+ 1` derivation may be proved under an
authority-only assumption (the brawler's former Chaos host's was, until og-session-runtime task 3 deleted it): the probe never
consumes the value directly, only **deltas** between consecutive samples, and a constant additive skew cancels under
subtraction. A client-side departure from unconditional advance, a resim replay or a stall, surfaces either as an ordinary
sample, if the tick delta stays plausible, or as a counted discontinuity (`kFrameHealthDiscontinuityTicks`) if it does
not. Neither corrupts the measurement. What *would* be unsafe is a **different** tick source, such as reading the
prediction clock off the step's thread; the host's call hands the probe the same tick it hands the drain and the reap, so
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

The role test is `m_manager.has_value() && m_manager->runsPrediction()`, taken once at the top since og-session-runtime task 14,
where it also picks the frame count (§21.1). `runsPrediction()` is `false` on the authority. A standalone or listen-server world
has no session since task 14; until then its in-frame authority's line rode the server tag, matching how its tick clock was
treated everywhere else. A session with no manager yet also takes the server tag.

**Window comparability with `ResimGateProbe`:** `kFrameHealthProbeWindowSamples` and `kResimGateProbeWindowSamples` are both
120 samples (the latter an alias of `kCorrectionVerdictProbeWindowSamples`), so a reader can line one `[ResimProbe.Frame]`
window up against the surrounding `[ResimProbe.Gate]` windows without a resim-tick count on this line. The function's first
statement asserts the two are equal, so the comparability is the compiler's to keep. They are **comparable, not
identical**: this window closes on `noteFrame`'s `GFrameCounter` cadence, that one on `noteCheck`, one call per
reconciliation `checkDivergenceAll` (from the manager's `onCheckIsSimilar`), which is a logically different event. The two
drift apart over a session exactly as far as their emitters' cadences do, which is itself diagnostic.

<!-- lint-anchor-ignore-begin: og-session-runtime task 14 deleted PROBE 6 and the [PacketBudget] line with the in-frame server; kept as history -->
#### PROBE 6: per-connection send budget (`m_connectionBudgetProbe`), server only

> **Retired by og-session-runtime task 14.** The session fed PROBE 6 only while the server stepped in-frame, on the game
> thread (§21.2), and task 14 retired that server; it deleted the probe's call, its member, the `[PacketBudget]` line and
> og-simulation's probe (`RelayProbes-rationale.md` §12), and retired G-18 and G-19. This subsection and the next record them
> as they stood.

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

#### ∴D-01 — RETIRED by og-session-runtime task 14 (2026-11-24): the `[PacketBudget]` line and its word rounding were deleted with PROBE 6

Was: *the capacity pin's word rounding, `(usableBits / 32) * 4`*. **Successor:** none: removed by og-session-runtime task 14.

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
<!-- lint-anchor-ignore-end -->

---

## §10 The teardown and the post-physics pass (task 27a)

**What moved.** The owner's `EndPlay` statements before its `Super` call became `endPlay`, and the owner's post-physics
step became `onPostPhysicsStep`, each one straight-line sequence in the owner's order. Task 27a's `transcribe.py` compares
both, statement by statement, with the owner's two functions as they stood before task 25, the hook inlined at its call
site (the task's notes show what it checked). A game's owner calls `endPlay` from its `EndPlay`, then its `Super`; a
backend calls `onPostPhysicsStep`.

**`endPlay`, in order** (game thread, once per world teardown):
1. `m_physicsBackend.onEndPlayBegin()` (`⛔G-20`). The driver-scheduled backend unregisters the frame host's two tick functions,
   so no new frame starts, and waits for every outstanding step task (`TickDriverBackendUImpl-guards.md` G-01); the former
   self-scheduled backend's was empty until og-session-runtime task 3 removed it. Nothing moves above it. Then a session that
   never started returns, its registry slot cleared (§23), and a client runs its teardown
   (`teardownImmediateClient_GameThread`, §18.6).
2. `detachFromSimNetLanes` (since og-input-latency task P5-7, §16.6): the session unbinds from the og-net lanes it is still
   bound to.
3. og-sim-net's server's and its host's resets and the two registered maps' clears (`⛔G-21`), then the tier consumer's reset,
   while the manager still exists: the server and the tier consumer both hold the manager's `TimeConfig` (§2 asserts that
   member destruction takes them first too). Until P5-7 this step reset the session's own reception coordinator and cleared
   the delayed-input map.
4. The registry (§9.2): if this session holds the authority slot, the slot is cleared; then the client slot. The destructor
   repeats this as a backstop (`⛔G-10`). Until P5-7 each role's three relay listeners were unregistered here too.
5. If the owner still has a world: the latency member's `end` (it unbinds its post-tick-flush handler). Until
   og-session-runtime task 5 the backend's world-teardown entry point followed it (the former self-scheduled backend
   removed its engine delegates there; the tick-driver backend's was empty).

Nothing resets the manager, the composition or the backend: they live until the session is destroyed, with its owner.

**`onPostPhysicsStep`, in order** (game thread, once per frame, after the frame's physics, with or without steps):
1. The test hitch (not in Shipping; below).
2. `m_physicsBackend.beforePresent_GameThread()`: a driver-scheduled backend's presenter (the shadow restore and the render
   apply, `TickDriverBackendUImpl-rationale.md`); it was empty on the former self-scheduled backend (removed by
   og-session-runtime task 3).
3. The lifecycle outcomes the step's or the runner's thread posted (`collectLifecycleOutcomes_GameThread`, §18.4, §19.4);
   on every session since og-session-runtime task 14 (it ran on a client and on the authority loop before).
4. **On the authority** (`!runsPrediction()`, which is the authority loop's since task 14): the authority presentation
   (`presentAuthority_GameThread`, §21.3), then the latency member's `tick_GameThread`, and the pass returns (§19.5).
5. On a client: `refreshDisplayView_GameThread()` (§17.3); the client published its display snapshot after its batch, on the
   step's thread (§17.2, og-input-latency task P5-36). Until og-input-latency task P5-16 this step was the backend's
   visualization refresh, which copied every stored simulatable's state into its visualization copy on the game thread and
   overlaid the rendered poses there.
6. `Game::afterVisualization(*this)`, the game's hook, at the position of the game statement it replaced.
7. The latency member's `tick_GameThread` (`UELatencyBudgetDiagnostics-rationale.md`). Until og-session-runtime task 5 it
   took the backend's diagnostics view, whose two latency-window hooks only the former Chaos host filled.

Until og-session-runtime task 14 the in-frame server (a listen server, a standalone or detached world, or a dedicated server
with og.Sim.AuthorityLoop=0) ran four more steps here, on the game thread, after the outcomes' place: the manager's
post-simulation pass (the correction write, behind a role `checkf`), the server-clock publish (§9.6), for a session on its own
detached lanes the lanes' two frame points (§16.2), and the display fill from storage (behind its own role `checkf`), before
the refresh. Task 14 deleted them with that server: on the authority the runner does the first two after each batch (§19.3),
and no authority publishes a display snapshot (§21.4).

It is private: the backend host calls it as the session's friend (§4), through the game's tick-driver binding
(`onPostPhysics`, at the frame's end of physics). Until og-session-runtime task 3 the former self-scheduled host (the
brawler's Chaos host) called it too, from the engine's post-tick delegate, bound to a weak lambda whose weak object was the
owner.

**The game's hook, `afterVisualization(session)`.** The game's statement that sat between the visualization update and the
latency tick (the brawler's: its ring-out score push) is the hook's body, verbatim, and the session calls it with itself,
`const`. The hook reads the session through const reads only: `getManager()`, and the two task 27a added,
`getSystemsExec()` and the component map's read (`getRegisteredComponentsById()` since og-input-latency task P5-7), each a
const reference to the member it names. The latency
tick, which followed the game's statement, is the session's own statement after the hook. The brawler's hook is
`BrawlerSessionGameUImpl-rationale.md` §5, in OGBrawlerUnreal's docs. The session calls the hook on a client only: the
authority returns before it (§21.3), so since og-session-runtime task 16b the brawler's hook is empty (its push ran on the
authority only).

**The test hitch, `og.Sim.TestGameThreadHitchMs`** (not in Shipping). When it is above 0, the next `onPostPhysicsStep`
sleeps the game thread that many milliseconds, once, and sets the variable back to 0 at console priority
(`ECVF_SetByConsole`), so a value set from the console, from `-ExecCmds` or from an ini is reset. It forces a catch-up
frame: on a client, 250 ms gives a frame of about 15 steps (on an inline-stepping server too, until og-session-runtime task 14). The sleep is the first statement of the pass,
the one hitch site for every backend.

* **The delay, `og.Sim.TestGameThreadHitchAfterSeconds`** (og-input-latency task P5-32; not in Shipping; default 0). Set on
  a command line or in an ini, the hitch variable is already set at the process's first pass, so without a delay the hitch
  lands in that first frame, before any client has joined (P5-21's finding F1: `P519_hitchOff`'s hitch window closed
  15 s before its first client joined), and the Ph4 gate's hitch rows, which read the clients across a server hitch, could
  not be measured. When the delay is above 0, the accessor leaves the hitch variable set and returns 0 until that many
  seconds have passed since the process's first pass (a function-local `static` holding `FPlatformTime::Seconds()` at the
  accessor's first call), then fires as before. At 0 it fires at the next pass, as before the delay existed. Both
  variables are per process, so with two sessions in one process the first pass of either starts the delay and the first
  pass after it fires the hitch. The pass runs on the game thread on both roles (on the authority beside its runner,
  §19.5).
* **The repeat, `og.Sim.TestGameThreadHitchEverySeconds`** (og-time-dilation task 7c; not in Shipping; a float, default 0).
  When it is above 0 the accessor does not reset the hitch variable when it fires; it fires again at the first pass at
  least that many seconds after its previous firing (a second function-local `static`, the `FPlatformTime::Seconds()` of
  the last firing), so the process keeps getting one long frame every interval until the hitch variable is set to 0.
  The delay still holds the first firing back. At 0 the hitch fires once and resets, as before. It exists so that a gate
  can inject isolated long frames of a few tens of milliseconds into a running client, the frames task 7c's frame-peak
  filter must ignore (`ClockDilationController-rationale.md` §5.1 in og-simulation): for example
  `og.Sim.TestGameThreadHitchMs=20`, `…AfterSeconds=60`, `…EverySeconds=3` on a 100 fps client.
* **The marker, `[SimHost.TestHitch] ms=<n> afterSeconds=<s> frame=<f> everySeconds=<r>`** (P5-32; `everySeconds`, the
  repeat's value, appended by task 7c). Each time the hitch fires, the accessor logs this one Warning on `LogOGSimHost`,
  after the reset (when there is one) and before it returns, so the sleep follows it at once and the line's time stamp is
  the hitch's start; `frame` is the engine's `GFrameCounter`. It is new with the delay and prints at 0 too; a session
  without the hitch prints no new line. A repeating hitch prints one line per firing. Ph4's gate checker reads it first (P5-21's `check_ph4.py`, rows
  T2 and R1).
* **Where it lives.** The console variable is a namespace-scope object, registered when it is constructed; the class
  template's header has no translation unit of its own to define it in, so the plugin's `.cpp` does (the initiative's
  review, finding m-10), and the plugin module constructs it once. It is defined in `UESimulationSession.cpp`, under
  `#if !UE_BUILD_SHIPPING`, with its name, default, help text and flags unchanged (its help text still names the owner's
  `OnPostPhysicsStep`, verbatim). The template reads it through an exported function the header declares under the same
  condition, `OGSIMULATIONUNREAL_API int32 consumeTestGameThreadHitchMs()`: it reads the value on the game thread; when it is
  above 0 and neither the delay nor the repeat's interval holds it back, it sets it back to 0 (unless the repeat is on)
  and returns it, and otherwise it returns 0; when that is above 0, `onPostPhysicsStep` calls the second
  exported function, `sleepTestGameThreadHitch(hitchMs)`, whose one statement is the unchanged sleep. The read, the reset
  and the sleep run in their old order. The delay's and the repeat's variables sit beside it in the same `#if` and
  namespace, read by the same accessor (P5-32, og-time-dilation task 7c), so the header and its call site did not change.
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
   value here; since og-session-runtime task 12 a client given 0 is refused by the start rule, §23),
   then the backend's `buildWorld` (the adapters, §3).
8. The role branch.
   * **Authority:** registry slot 0 (a `checkf` if it is taken) (§9.2); the router logger lambda (`RouteOGMessage`); the
     integration layer; `Game::emplaceSystems` (`UESimulationSession-guards.md` G-01); the manager, without prediction; the
     three peers' loggers; the neutral input, `Game::neutralInput()` (`⛔G-23`, §11.3); `beginServerSimNet` (§16.2);
     `applySessionKnobs` (§9.8); `m_clock->beginTierConsumer()` (`⛔G-06`, §9.4; the clock is emplaced right after the manager); `simlog::setGlobal`;
     `Game::installGameLoggers(worldIsAuthority)`; `bindCorrectionFieldDiffGate()` (§11.4).
   * **Client:** registry slot 1; the router logger; the integration layer; `Game::emplaceSystems`; the manager, with
     prediction (and the clock); the loggers; the neutral input; `m_clock->beginTierConsumer()`; `beginClientSimNet` (§16.2); `simlog::setGlobal`;
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

Since og-session-runtime task 14 the start rule (`refuseToStart`, §23) runs after step 2 and returns before anything is built
for a world that starts no session. On an authority steps 8 to 11 are `configureSession`, which the runner runs after it
builds the world, and step 12 is `runAuthorityLoop_GameThread` (§19.2).

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

> **Since og-session-runtime task 14** part of this section is history. The authority's half of 12.1 (steps 5 and 6 on the
> game thread) and the work of 12.3 (its steps 2 to 7) were the in-frame server's, and task 14 deleted them: an authority
> registers and unregisters on its runner (§19.4), a client through og-sim-net's lifecycle queue (§18.4, §18.5). What remains
> on the game thread is 12.1's first four steps for a client (and the first two for the authority, which then hands over to
> `tryRegisterOnRunner`), and 12.3's returns: no session (§23), an id already unregistered (`⛔G-30`), then the role's request.

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
   (G-25, retired by og-session-runtime task 14; §12.4), then the backend's `onRegistered_GameThread`; the record is erased, and
   the call returns `Ready`.

**Generic creation, hand-written collision.** Each physics declaration names its own slice, so a game that adds a
body-owning sub-simulation edits neither this session nor a backend's creation fold to have the body created, bound,
captured, rewound and checksummed. That is the whole of the claim. Making the body collidable or queryable is still the game's
hand-written work: its collision category needs a row in the game's category table for each backend configuration (the
brawler's is the `categoryChannels` table of its Jolt binding, the only one since og-session-runtime task 3 deleted the
Chaos binding's). *R0, task 28:* verified: both backends' creation
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
2. **The systems executor's notify, behind the storage test** (G-26, retired by og-session-runtime task 14): it runs before `unregisterSimulatable` destroys
   the character, while it is still in storage, so a system's unregister hook can still reach it; and only for a character
   that is in storage, because one whose registration never reached `Ready` has nothing to drop, and a hook's
   `view.get<>(id)` would have nothing to return.
3. **`unregisterSimulatable`:** the owner callbacks, then the containers. The owner is the character's channel since
   og-input-latency task P5-7, or none for an id that never reached `Ready`.
4. **`forgetSimNetCharacter(id, isAuthority)`** (G-13, retired by og-session-runtime task 14, which also dropped the role
   argument; §16.5): the registered maps and the slot drop the id; on the server
   og-sim-net's `removeCharacter` sends the roster Remove, drops the coordinator's claim and dedup watermark for this owner
   (`forgetOwner`) and destroys the channel and the stager; on a client the roster bindings mark the id unregistered. Promptly,
   rather than waiting for garbage collection: the coordinator's claim map is keyed by id. Until P5-7 this step was the
   session's own forget: its coordinator's `forgetOwner`, the delayed-input map's erase and PROBE 5's forget.
5. **The game's `onUnregistered(id)`** (the brawler: its roster's forget, then its input-history feed's forget).
6. **The latency member's forget,** the session's own statement.
7. **The backend's `onUnregistered_GameThread`,** then the line `NewFramework: unregistered simulatable id=%u`.

It is called for every component that ends play, registered or not. From og-input-latency task P5-7 until og-session-runtime
task 14 the work was the in-frame unregistration (unregisterCharacter), and `unregisterFromNewFramework` returns first for an id a roster Remove or the lanes' stop already
unregistered (`⛔G-30`, §16.5), so each character is unregistered once.

### 12.4 The executor's notify runs on both roles

*Correction (R0, task 28).* The owner's §10 said that "on the authority path, registration also notifies the systems
executor". Both notifies are unconditional, on both roles: the manager forwards each to the systems executor with the
role, and the executor calls a system only where its role affinity fires (`SimulationSystemsExecutor`'s `firesOnRole`).
The brawler's hit-routing and hit-detection systems fire on every role; its ring-out score system on the authority only.
What the authority-only score system relies on, and why the notify must precede `Ready`, was G-25 (retired by
og-session-runtime task 14); on the remaining paths it is `⛔G-36` (the runner) and `⛔G-32` (a client).

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
through the tick driver; on the former Chaos configuration, removed by og-session-runtime tasks 3–6, the game's own host),
reached through the traits (§3). The rename is identifiers only:
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
| the correction-tick walk (forEachAuthorityCorrectionTick, deleted by og-session-runtime task 14) | the walk of `m_delayedInputComponentsById`: it skips a stale weak pointer and an unbound component (`⛔G-29`), reads the component's correction-buffer tick, skips tick 0, and calls `function(id, tick)` | the latency member's server branch of `tick_GameThread`, which keeps the per-id `Handed` stamp (`UELatencyBudgetDiagnostics-rationale.md` §12) |

**The switch re-pointed all four** (og-input-latency task P5-7, §16.3, §16.4): the reap is og-sim-net's server's (in
`afterInlineBatch` since og-session-runtime task 15), the slot doors read and write the session's slot map on the authority and the roster entry on a client, and
the walk read the registered channels until og-session-runtime task 14 deleted it. The table above is P5-5's state.

**No file but this header named the session's coordinator member since P5-5.** The Chaos host's test of it became
`hasServerTierWiring()` and its reap `afterAuthorityBatch` (that host, the brawler's former Chaos host, was deleted by
og-session-runtime task 3; `hasServerTierWiring()` had no caller since, and og-session-runtime task 7b deleted it). The tick-driver binding never named the member: it called
`getReceptionCoordinator()`, which the component binding's RPC tail still called then (§9.7). Since og-input-latency
task P5-7 the accessor returns og-sim-net's server's coordinator (§16), and since P5-8 deleted the RPC tail no file in this
tree calls it.

**The slot doors.** Both take the owner as an `AActor&` and the host's slot key (`TickDriverBackendUImpl::slotKeyOf`), which
neither reads. At P5-5 each cast the owner to `TraitsT::Character` and called the game character's replicated-slot getter or
setter; the switch (P5-7, §16.4) replaced both: `physicsSlotFor` reads the session's slot map (keyed by the owner's address)
on the authority and the roster entry on a client, and `recordAuthoritativeSlot` writes that map. Neither casts or names a
member of the game's character now, and og-input-latency Ph3 (root edac6a0, OGSimulation 051f99b) deleted the brawler
character's two slot members.

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
with the Unreal hosts of `UESimNetHost.h` (`UESimNetHost-rationale.md`). The traffic was **frame-locked** at first: received
at the driver's `PostTickDispatch`, sent at its `TickFlush` (the P5 design, K3). The old carriers (the net-sync component's RPC
and correction property, the input, timing and connection relays, the three listener interfaces) are no longer written or
spawned by anything; later tasks deleted them (P5-8, P5-9). Since og-session-runtime tasks 12 and 14 nothing is frame-locked:
a client drains at each step's start and sends at capture (§18), and the authority receives, steps and sends on its runner
(§19, §24).

### 16.1 Who owns what

> **og-session-runtime task 14** deleted the in-frame server's paths this section describes: the detached lanes (16.1, 16.2),
> the server's frame-point bind and its game-thread clock publish (16.2, 16.3), the authority's game-thread registration
> through its channel map (16.4) and its game-thread unregistration (16.5). An authority registers and unregisters on its runner
> (§19.4), a client through og-sim-net's queue (§18.4, §18.5), and a world without og-net lanes starts no session (§23).

| piece | owner | why |
|---|---|---|
| the transport (`SimNetTransport` over the driver's erased endpoint) | the driver's lanes, from `InitBase` | og-net delivers from before the world exists (K4) |
| a client's `SimNetClient`, its roster mirror and its channels; the bindings (`SimNetBindings`) | the driver's lanes | the roster, floor and tier arrive before the client's world exists, and are latched there |
| the server's `SimNetServer` (it owns og-simulation's reception coordinator) and its host, `UESimNetServerHost` | this session: `m_simNetServer`, `m_simNetServerHost` | its lifetime is the world's, and it borrows the manager's `TimeConfig` |
| detached lanes, for a world with no og-net driver (until og-session-runtime task 14) | this session's own | one code path either way; sends were dropped and counted |

`m_simNetLanes` points at the driver's lanes the session is bound to (until task 14 also its own detached ones). It is cleared by the
lanes' stop handler and by `detachFromSimNetLanes`, so the session never keeps a pointer to lanes the driver destroyed.

### 16.2 `beginPlay`: finding the lanes, and the proof line

`findDriverSimNetLanes` casts the world's net driver to `UOGNetDriver` and takes its `GetSimLanes()`. The game registers one
factory (the brawler's `makeBrawlerSimNetLanes`), which makes `UESimNetLanes<TraitsT>` for the driver's role, so the
`static_cast` to this session's lanes type is the factory's own type; a `checkf` compares the lanes' role with the world's.

- **Authority** (`beginServerSimNet`, on the runner, after the neutral input, before `applySessionKnobs`): a `checkf` that the
  lanes exist and are host-serviced, which the start rule ensures (§23: with no og-net lanes the world logged
  `[SimNet.Host] refused role=server driver=<class>: simulation traffic needs the og-net driver` at `Error` on `LogOGNet`, showed
  a 15 s message and started no session; until og-session-runtime task 14 it then ran detached server lanes, and a standalone
  world did so silently). Then the host, then the server, with the manager's `TimeConfig` and og-sim-net's
  `correctionAllowanceBytes` for the endpoint's lane table and budget at `NetConfig::tickFrequencyHz`; then the lanes get the
  runner's two functions (`bindAuthorityRunner`, §19.3; the in-frame server's two frame functions until task 14) and the
  session's stop handler.
- **Client** (`beginClientSimNet`, where `replayLatchedClientValues` stood): a `checkf` that the manager and the tier
  consumer exist (the bind replays into both; this replaced guard G-07), and one that the lanes exist (the start rule's
  `role=client` refusal, §23; detached client lanes until og-session-runtime task 14). Then the session's handler for a roster Remove
  (`setOnRegisteredRemoved`), its stop handler, the lifecycle binding (`bindLifecycle`, §18.1), and `bindClientSession(*this)`,
  which binds the host, replays the latched
  floor and tier, and seeds the bindings from the roster mirror before any registration (`SimNetBindings-rationale.md` §6).
- **The proof line**, once per session, from `logSimNetHostLine`:
  `[Warning][SimNet.Host] role=<server|client> immediate=1 rttSource=EnginePing driver=OGNet inbox=2048`. The format is
  unchanged (`kSimNetHostLineFormat`, design Q7), and since og-session-runtime task 14 its two mode fields are constant:
  `immediate=0` was the frame-locked mode (the only one until og-input-latency task P5-17, which made the digit the lanes'
  decision, §18.1; a client printed `immediate=1` from og-session-runtime task 12, every session from task 14), and
  `driver=detached` the detached session's (until task 14); `rttSource=EnginePing` is a fixed field that names WIRE's source (the connection's FNetPing, D13, which
  the hosts read in `readRoundTripMs` and `readServerRoundTrip`, `UESimNetHost.h`). It is **not** the clock's RTT source:
  that is `[OGNetcode] ClockRttSource` (§20), and the session reports it on its own line,
  `[ClockSource.Config] clockRttSource=<Engine|SimClock> …`. Under `ClockRttSource=SimClock` the line still reads `EnginePing`
  (og-input-latency task P3-10 part B F5).
- **`[OGNetcode] SimTrafficImmediate`** was read by `readSessionKnobs` on both roles until P5-17, and a nonzero value logged an
  `Error` and ran frame-locked. Since P5-17 the lanes read it and decide the mode at driver start (§18.1); since
  og-session-runtime task 12 a client is always immediate, and a client given 0 is refused at `beginPlay` (§23).
- **The floor** is published through the server, `publishFloor`, at the end of `applySessionKnobs`, where the timing relay's
  write stood; the server sends it to every attached connection and to each one that attaches later.
- **A detached session** (until og-session-runtime task 14) was driven from `onPostPhysicsStep`: the lanes' `afterDispatch` and
  `onTickFlush`, after the clock's publish, so its counters and window lines behaved as the driver's would. Nothing arrived, and
  every send was dropped.

### 16.3 The frame work

| session member | before P5-7 | since |
|---|---|---|
| `releaseDelayedInputsForStep` | the coordinator's release, delivered through the component map | `[ReleaseBatch]` kept, then the server's `releaseForStep` |
| `afterAuthorityBatch` | the coordinator's `reapConnections` | the server's reap, in its `afterInlineBatch` since og-session-runtime task 15 |
| the server-clock publish | the clock into the timing relay's buffer | the server's `publishClock(tick)`, sent at the next flush (on the runner after each batch, and the game-thread publish deleted, since og-session-runtime task 14) |
| `getReceptionCoordinator`, `hasServerTierWiring` | the session's own coordinator | the server's `coordinator()`; presence is the server's |
| the correction-tick walk | the component map, skipping an unbound component (G-29) | the registered channels, skipping a buffer shorter than its header and tick 0 (deleted by og-session-runtime task 14) |
| PROBE 6's test (G-18) | the coordinator's presence | the server's presence (deleted by og-session-runtime task 14) |

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
into the door, and its message says so. At the `Ready` step the in-frame server (until og-session-runtime task 14; the runner's
`tryRegisterOnRunner` posts og-sim-net's `registerCharacter` with the same arguments, §19.4) called the server's `addCharacter`
with the sim id, the recorded slot, the pawn's root connection's og-net id (`kInvalidSimConnection` for a server-local pawn, or a connection
of another driver), the pawn's local player index (`GetPlayerSlotForActor`) and the pawn's name. A refusal (`nullptr`, logged by
og-sim-net as `[Roster.Refused] role=server …`) returns `TryRegisterStatus::Refused`: the pending record is erased, an `Error`
line names the id, and the binding abandons. Otherwise the character registers with its channel as both owners, as it did with
the component.

**Client.** The binding gate is `clientBindStep(id, engineSaysLocal)`, which the game's key hook asks once the pawn's key is
known: `Wait` (no roster entry yet: the hook answers `Retry`, inside the binding's 600 attempts), `Ready`, or, on a role
mismatch, the `[Roster.BindMismatch]` line and a `checkf` (`Abandon` with checks off; P7's C-3). A world with no session
answers `Abandon` with its registration line (§23; a detached client did so with one `Error` until og-session-runtime task 14),
so it does not wait into the 600-attempt `checkf` (Q1: no Fatal). The slot door answers from the
roster entry (since og-input-latency task P5-17 through the bindings' copy of it, `verdict(...).physicsSlot`, under their lock:
the bindings hold every entry the mirror holds, fed by the same Add and Remove, so the answer is the same, and a client's
mirror is the step's to read, §18.3): the game binding passes the pawn's storage key (its replicated sim id, 0 until that has arrived, which answers
"not yet" as the replicated slot property's absence did). At `Ready`, the channel is the client session's (`client.channel(simId)`); if the roster removed it between the
gate and the bodies, the call stays `Pending`. A `checkf` compares the input provider's presence with the roster's local index,
then the character registers, a local one gets the `Handed` callback, and the bindings mark the id registered. That unqueued
client registration was the frame-locked client's; og-session-runtime task 12 removed it, and every client now registers
through og-sim-net's lifecycle queue (§18.4), behind the same gate and slot door.

**The registered map.** `m_registeredComponentsById` (the game component per id, both roles; the brawler's ring-out push reads
it on the server through `getRegisteredComponentsById`) is written at `Ready` and erased by `forgetSimNetCharacter` (or, on the
runner's path, `requestRunnerUnregister_GameThread`). The channel map beside it was the in-frame server's, written at its
`Ready`, until og-session-runtime task 14.

### 16.5 Unregistration: whichever comes first

`unregisterFromNewFramework` is the pawn's `EndPlay` path. Until og-session-runtime task 14 the in-frame unregistration
(unregisterCharacter) was the work, in this order: the executor's notify (G-26, retired), `unregisterSimulatable` with the channel (both owners on the server, the prediction owner on a client; none
for an id that never reached `Ready`), the channel's `Handed` callback cleared, `forgetSimNetCharacter` (G-13, retired), the
game's `onUnregistered`, the latency member's forget, the backend's unregistration, and the `NewFramework: unregistered` line.
`forgetSimNetCharacter` erased the two maps and the slot, then on the server removed the character (`removeCharacter`, reason
`Left`: the roster Remove to every attached connection, the coordinator's `forgetOwner`, the channel's and the stager's
destruction), and on a client marks the bindings unregistered.

Two other paths unregister a character before the pawn's `EndPlay`, and each records the id in `m_simUnregisteredIds`, so
the later `EndPlay` returns at its first statement (`⛔G-30`) and the line prints once:
- **a client's roster Remove** of a registered id (`onSimNetRegisteredRemoved`, called by the client host inside og-sim-net's
  Remove handling, before the channel is erased; P7's C-4). It ran `unregisterCharacter` there until og-session-runtime task 12;
  since then og-sim-net has unregistered the queue-registered character itself, and the session posts the host work to the game
  thread, which records the id the same way (§18.5);
- **the lanes' stop** (`onSimNetLanesStopping`, from `beforeStop`): Unreal's map load shuts the world's net driver down before
  it destroys the pawns and ends the world's play (the engine's LoadMap), and the lanes, with a client's channels, are destroyed
  with the driver. So the stop unregisters every registered character while the channels and the endpoint still live (a
  client through its teardown, §18.6, after which a `checkf` states that nothing is left; the authority through its runner's
  queue, §19.4), then releases the server and its host and forgets the lanes.

### 16.6 Teardown

`endPlay`, after the backend's `onEndPlayBegin` (`⛔G-20`), and, on a client, its teardown (§18.6):
`detachFromSimNetLanes` (a server's runner functions unbound; a
client's registered ids marked unregistered in the bindings, its Remove handler cleared and the client session unbound),
then (`⛔G-21`) the server and its host reset, the two maps and the slot map cleared and the unregistered-id set cleared (since
og-input-latency task P5-9; the session dies with the world, so the set held nothing a later call reads; the channel map until
og-session-runtime task 14), then the tier
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

**A predicting session** (`publishesDisplayOnStep()`: the manager exists and runs prediction; until og-session-runtime
task 5 the engine traits' world-host flag was a first conjunct, true on Jolt, so the expression is the same on every
build since) publishes in `publishDisplaySnapshot_Step`, which the tick-driver game binding's
`publishForDisplay()` calls once after each client batch, on the thread that ran the batch (the client's worker task; the
game thread for a client stepping inline under og.Sim.RunInline=1, until og-session-runtime task 11), under the world lock (`TickDriverConcepts-rationale.md` §11, in og-simulation's
docs). Nothing steps there, so the fill reads the storage, the input resolution, the reconciliation and the clock as the
batch left them. `onPostPhysicsStep` does not fill for such a session; it only acquires (§17.3).

Until og-input-latency task P5-36 every role filled in `onPostPhysicsStep`, on the game thread, on the claim that the tick
driver's post-physics ran once the frame's step tasks had completed. It did not: `FrameWait::EarlierFrames` (a frame-locked
client) waits only for earlier frames' tasks, and `ThisFramesSingleStep` (an immediate client) only for a one-step frame's,
so the fill could read the storage while the frame's own batch stepped on a worker (og-input-latency's design review P5-31,
L5; `SimulationTickDriverDisplayFillTest.cpp` in og-simulation-tests reproduces it).

**What the step-thread fill reads.** og-sim-net drains on the step's thread (§18), so the relay stores, the arrival rings,
the tier receipt and the registrations are written there too. The fill reads every source on its writer's thread. The tier
comes from the clock's `displayTier()`, the `TripleBuffer` the recompute writes (og-sim-session's `SessionClock` since
og-session-runtime task 24; this class's `m_displayTier` until then).

Until og-session-runtime task 12 a frame-locked client (§16) filled here too, with four of its sources written on the game
thread, where its drain ran: the tier, the relay stores, the arrival rings and the registrations. Its tier buffer's writer was
the game thread, seeded by `beginClientSimNet` before any step. Its relay fields (each entry's last relayed input and arrival
ring) came from a stage that `onFrameStepsDue_Step` copied on the game thread before each batch was dispatched, with
og-simulation's relay-stage function, into a second triple buffer of the session's; the fill read the newest stage
(`SimulationDisplaySnapshot-rationale.md` §3, in og-simulation's docs), and a `checkf` refused a batch with none. Task 12
removed that stage, its buffer, the staging call, the staged arm of the fill and the game-thread seed.

**No other session publishes.** The authority publishes no display snapshot (§21.4): its storage is the runner's, and
`onPostPhysicsStep` returns on the authority before the display refresh (§10, §19.5). Until og-session-runtime task 14 the
in-frame server filled one in `onPostPhysicsStep` on the game thread (publishDisplaySnapshot_GameThread, after its steps on
the same thread), with only the server clock's step in its clock fields; until og-session-runtime task 3 the brawler's former
Chaos backend did too, while its physics thread might still step. The overlay below keeps the tick backend's `OG_CHECK`
(P5-16's F6).

**Heap.** The publisher's channel holds four snapshots, each up to `kMaxSimulatableSlots` entries with both relayed rings by
value, so the session holds it through a `std::unique_ptr`, as it holds the game-thread copies below. The session object
stays the size it was plus three pointers.

### 17.3 The game-thread view: one snapshot held, its states copied and overlaid

`refreshDisplayView_GameThread`, in the post-physics pass, swaps the held snapshot for the newest through the publisher's
`refreshView_GameThread(held)`, which releases the held one first and then acquires the newest
(`SimulationDisplaySnapshot-rationale.md` §5). The order is the channel's read rule: while the reader holds a snapshot, a
peek of the newest returns the held one, never a later commit. Until og-input-latency task P6-1 the session acquired first
and released the old pointer only if it differed, so the peek returned the held snapshot, nothing was released, and every
client's view stayed on the first snapshot it acquired, published before any character registered: the brawler's
`TickComponent` found no entry (`findDisplayVizState` null) and drew no aim indicator, attack or projectile visualization,
on immediate and frame-locked clients alike, from Ph4 on. Re-acquiring a slot just released marks it held again (the
channel's hold is one bit per slot), so the reader holds one snapshot between passes and none between the two calls. A
publish may run in that instant, on the step's thread: the writer's open slot is not listed, so the peek returns the newest
committed snapshot. Every reader in the frame shares the one held snapshot until the next pass. `endPlay` releases it.

Then, for each entry, the session copies the entry's state into its own array of visualization states (index-aligned with
the entries) and calls the backend's `overlayRenderPoses_GameThread(id, state)`. On a driver-scheduled backend that writes
the presenter's rendered poses into every non-root body state (`TickDriverBackendUImpl-rationale.md`), exactly what the
presenter wrote into the storage's visualization copy before; the brawler's former Chaos backend (removed by
og-session-runtime task 3) wrote nothing, as its refresh wrote only the plain copy. The snapshot itself stays as published: it is shared with the writer's channel, and the overlay is
the game thread's.

So the game thread now reads the state as of the end of the last step batch, plus this frame's poses. A reader that ran
after a frame's steps but before its post-physics pass read the live caches before; it now reads them as of the previous
pass, at most one step batch older (the task's DEV). A client's pass reads the newest batch published before it (since
og-input-latency task P5-36): a one-step frame waits for its batch, fill included, so it shows that batch; a frame that does
not wait shows the newest batch that finished before the pass.

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
editReconciliation (both overloads) were deleted with their last game-thread callers: the entry carries the local
captures, both relayed rings by value and the applied-capture window. `editStorage` stayed for the brawler's Chaos backend,
which stepped on the physics thread; it had no caller left in code after og-session-runtime task 3 deleted that backend,
and og-session-runtime task 7b deleted it.
`getClientEffectiveInputDelayTicks` stays (the input resolution's atomic) but has no game
caller since this task: the feed reads the published delay from the snapshot's tier reading.

A game reads all of this through the doors, never the snapshot's channel: the session acquires and releases, so the hold
bound is the session's to keep.

## §18 Immediate mode on a client (og-input-latency task P5-17)

A client's og-sim-net session is drained on the step's thread at each step and sends its inputs at capture, and its
registrations go through og-sim-net's lifecycle queue (`SimNetClient-rationale.md` §16–§18). Since og-session-runtime
task 12 this is every client: the session keeps no mode flag, and a client given `SimTrafficImmediate=0` is refused at
`beginPlay` (§23). Until then `[OGNetcode] SimTrafficImmediate=1` chose it (shipped from og-input-latency task P5-22) and the
frame-locked client was the attribution arm. The sections below name the frame-locked client only where it explains an order.

### 18.1 The mode and its lines

- **Fixed by the lanes**, at driver start (`UESimNetLanes-rationale.md` §11): a client's transport is always immediate, and
  og-sim-net fixes the mode when the transport is built. The session keeps no mode flag since og-session-runtime task 12 (until
  then `beginClientSimNet` copied the lanes' decision into one, before any step). Where it must tell a client's path from the
  authority's it asks the role, `runsPrediction()`, which the manager fixes at its construction, before any step.
- **The refusal** is the start rule's (§23): a client given `SimTrafficImmediate=0` starts no session, and a server does not
  read the knob. Until og-session-runtime tasks 11 to 14 the lanes refused immediacy here, with one Warning, wherever it could
  not run (a server without the authority loop, detached lanes, a client under og.Sim.RunInline=1).
- **The proof line** carries the mode: `[SimNet.Host] role=client immediate=1 …` on every client, and `role=server
  immediate=1 …` on every server since og-session-runtime task 14 (until then exactly when its authority loop ran, else
  `immediate=0`). The digit stays for the line's bytes (design Q7).
- **The lifecycle binding.** A client binds og-sim-net's lifecycle queue to the session's storage, reconciliation,
  input resolution and net sync (`bindLifecycle`) before `bindClientSession`, so before any registration. The server binds its
  own in `beginServerSimNet`, after the server exists; the server's `drain()` on the runner takes the lifecycle queue first
  (§19.3).

### 18.2 The step hooks: the drain at the step's start, the send at capture

A bound client's step reaches the session twice, on the step's thread. Both entries return at once on the authority
(`runsPrediction()` false) and before the world session is bound (`boundImmediateClient_Step()`).

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
same step). `SimulationManager::onPostSimulationGameThread` runs on the authority only, in `afterAuthorityBatch` on the runner (§24); a
client sends only here, at capture, and a send from another thread would fail the transport's input-set owner check
(`SimNetTransport-guards.md` G-07).

**What the drain feeds.** Everything it delivers reaches the step it runs in: the step's resimulation check sees a correction
it delivered, and the step's clock advance and collect read the clock samples, relay rings and inputs it delivered. A
character it registers is in storage for that step's integrate, and one it unregisters is gone before it. This is the
order the retired frame-locked client had per frame: it drained at the frame's start, before the frame's first step, while a
client now drains before every step, so a step batch of several steps sees each step's own arrivals.

**Before og-input-latency task P5-30** the drain followed the send in `onStepStarted` (`onClientStepStarted_Step`), after
the step's collect, clock advance and resimulation check, so everything it delivered reached the next step: H6c rose by one
tick and N6 rose too (P5-17's finding F2).

### 18.3 What the game thread no longer reads directly

The drain writes on the step's thread what the frame-locked client's drain wrote on the game thread. Each game-thread reader
moved:

| state | the game thread's reader | since P5-17 on an immediate client (every client since og-session-runtime task 12) |
|---|---|---|
| the server connection's round trip | the drain's `serverRttSeconds` read it | published each frame by the lanes (`publishServerRtt_GameThread`), read from atomics (`UESimNetHost-rationale.md` §5) |
| the WIRE sample | `onTimingInfoReceived`'s `noteWireSample` (a game-thread door) | `noteWireSample_Internal`, through the latency mailbox |
| the drain's latency stamps | the host's game-thread stamp doors (deleted by og-session-runtime task 14) | `stampClientReceived_Internal` (`UELatencyBudgetDiagnostics-rationale.md` §13) |
| the tier receipt's display state | the game-thread display fill's `readDisplayTier` over the tier consumer and the time config (that fill was deleted by og-session-runtime task 14) | the recompute (the clock's since og-session-runtime task 24) writes the same `DisplayTierReading` into a `TripleBuffer` (og-sim `Mailbox.h`) after each recompute; the publish reads the newest (on the step's thread since og-input-latency task P5-36, §17.2) |
| the roster bindings | `clientBindStep`, `tryRegister`, `forgetSimNetCharacter` | the same calls, through the bindings' lock (`UESimNetHost-rationale.md` §12) |
| the roster mirror | `physicsSlotFor` | the bindings' copy of the entry, under the lock |

- **The display tier.** The recompute runs at every tier and floor receipt and at the tier consumer's start, so the buffer holds
  what the frame-locked read would have computed at that moment: the effective delay it reads is the value the recompute just
  published, and the client's floor is only written right before a recompute. Until og-session-runtime task 12 a frame-locked
  client wrote the buffer too, from the game thread. Until the first recompute the snapshot's tier fields keep their defaults, as with no tier consumer.
- **`bindClientSession`'s seed** (`seedFrom(roster())`) still reads the mirror on the game thread. It runs in `beginClientSimNet`,
  before the backend starts stepping, while the game thread is the lanes' drain (`UESimNetLanes-rationale.md` §11), so it reads
  the mirror on its writer's thread.
- **The session-setup writers** of the state the drain and the send read (the tier consumer's start, `applySessionKnobs`'s
  floor on the server, `applyLatencyKnobs`' `setRedundancyFrames` and `setRelayResendEntries`, the resim policy) all run in
  `beginPlay` before `startStepping`, so before the first step and its drain. The backend's task dispatch orders them. A knob
  changed at run time while steps run would be a plain race; nothing in the tree does that.

### 18.4 Registration through the lifecycle queue

`tryRegister` sends every client registration to `tryRegisterQueued` once the bodies exist and are resolvable (the unqueued
client branch, the frame-locked client's, went with og-session-runtime task 12):

1. **The gate** is the bindings' verdict, asked and marked in one hold (`markRegisteredIfReady`): `Wait` stays `Pending`, a role
   mismatch is the `checkf` the frame-locked branch had, `Ready` marks the id registered. The client session's channel map is the
   drain's, so the gate does not read it (the frame-locked branch did).
2. **The enqueue**: `registerCharacter(simId, simulatable, provider, applied)` with the latency-wrapped provider. The record keeps
   no simulatable after this (`queued`), and every later call goes to `finishQueuedRegistration` first.
3. **The apply**, on the drain's thread: og-sim-net registers the character with its channel as the owner, then calls `applied`
   (`onQueuedRegisterApplied_Step`), which sets a local character's `Handed` callback (posting through the mailbox), notifies the
   systems executor (`⛔G-32`), and posts `RegisterApplied` to the game thread. A refusal (`noChannel`, `registered`) posts
   `RegisterRefused`.
4. **The finish**, on the game thread, at the binding's next attempt: `collectLifecycleOutcomes_GameThread` reads the posted
   outcomes (under a mutex; the session also collects them once a frame, in `onPostPhysicsStep`). On `RegisterApplied`, the host
   work the frame-locked branch did at once: the registered-component map, the `tryRegister: registered` line, the game's `afterRegistered`
   and the backend's `onRegistered_GameThread`, then `Ready`. On a refusal, or a roster Remove before the finish, the bindings are
   marked unregistered, an `Error` names the id, and `Refused` (the simulatable died in the job; the pawn's `EndPlay` releases its
   bodies).

**The order, against the retired frame-locked branch.** The executor's notify runs before the game's `afterRegistered`, on the
step, right after the character entered storage (G-25's condition, in storage and before `Ready`, holds). The session keeps no
channel map (the in-frame server's was the last, until og-session-runtime task 14): the game thread never touches a client's
channel. A local character's input sample is
published from the frame after `Ready` (`sampleInput` walks the registered map), so its first one or two steps after the apply
read the neutral input; a frame-locked character's first step after `Ready` already had a sample.

### 18.5 Unregistration

- **The pawn's `EndPlay`** (`unregisterFromNewFramework`, after `⛔G-30`) calls `requestQueuedUnregister_GameThread`. An id that
  never reached the queue (no queued record, not registered) runs the host work at once, bodies included
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
  `RosterRemoved`. The game thread runs the host work with the release and records the id (`⛔G-30`), as the frame-locked
  client did at once. If the
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
   the session can die before the lanes (review_P2-17 N2). The session is still bound, so a timing info, clock sample, tier or
   floor in that drain reaches the clock's receipt doors on the game thread: since og-session-runtime task 24 the drain is
   marked (`m_clock->beginStoppedDrain()` before, `endStoppedDrain()` after), and inside the marks those doors check the
   client's stopped-drain row of the thread table, `SessionOp::DrainStopped`, instead of the step's (`SessionClock-rationale.md`
   §3; `SessionThreads-rationale.md` §7.1).
3. **The game thread finishes**: the outcomes are collected, every held component's bodies released, every registered
   character's host work run with its id recorded (`⛔G-30`: the pawn's later `EndPlay` returns at once), and the queued records
   erased with their bindings unmarked. The session's later registrations stay `Pending` (`m_immediateStepsStopped`).

### 18.7 What frame-locked kept (retired by og-session-runtime task 12)

On a frame-locked client the only differences P5-17 made were the lock around the bindings, the display tier's buffer, and
`physicsSlotFor` reading the bindings' copy of the roster entry; its start-up lines were the same, `immediate=0` included. Task
12 retired the mode: every client is immediate, and `SimTrafficImmediate=0` is refused (§23).

## §19 The authority loop: the server on its runner (og-input-latency task P5-19)

Every dedicated server's authority steps on its own thread, `OGSimAuthority`, which also services og-net's server endpoint
(P11, design K14; `UEAuthorityRunner-rationale.md` §1). Since og-session-runtime task 14 it is the only server: a listen,
standalone or foreign-driver world starts no session, `og.Sim.AuthorityLoop=0` is refused (§23), and the branches ask the role
(`runsPrediction()`, or at `beginPlay` the world's role). Until then the loop was `og.Sim.AuthorityLoop=1`'s (the default
since og-input-latency task P5-22) and every other world ran the in-frame server.

### 19.1 The decision

- **The lanes' I/O mode**, at the driver's `InitBase` (`UESimNetLanes-rationale.md` §12): og-net fixes the servicing thread at
  its first service, before any world exists, and a dedicated server's lanes are host-serviced. `beginPlay` takes the runner
  path for every authority world the start rule lets through (§23), which is a dedicated server on the og-net driver, and
  `beginServerSimNet` `checkf`s that its lanes are host-serviced. Until og-session-runtime task 14 `beginPlay` copied a loop
  decision from the lanes; task 14 deleted it with the in-frame server.
- **Proof lines:** `[OGNet.Driver] start … mode=hostServiced` (the driver), `[SimHost.Runner] priority=AboveNormal
  nice=… highResWait=…` and `[SimHost.Runner] startLatch waitMs=…` (the runner), `[SimHost.Mode] role=Authority stepping=runner
  viz=none` and `[SimHost.Frame] role=Authority stepping=runner …` (og-simulation), `[SimNet.Host] role=server immediate=1 …`.

### 19.2 Start: the session is configured on the runner

`beginPlay` keeps its order; only the thread changes. Its configuration, everything from the role branch (manager, systems,
og-sim-net server, knobs, tier consumer, loggers) to the `[SimHost.Backend]` line, is the lambda `configureSession`. On a client
`beginPlay` calls it between `buildWorld` and `startStepping`, on the game thread. On an authority it is the runner's `afterBuild`
(`TickDriverBackendUImpl-rationale.md` §15): the game thread imports the statics, starts the runner and waits on its latch; the
runner builds the world (the adapters exist only after that), runs `configureSession`, builds the driver and releases the latch.
The game thread is parked for all of it, so the configuration's writes race nothing, and the latch orders them before every
later game-thread read. Nothing in the configuration needs the game thread: it reads no console variable (`readSessionKnobs` ran
before) and touches no `UObject` but the world's driver, which it only reads.

### 19.3 og-sim-net on the runner

- **No frame points.** og-sim-net's server has no frame drain or flush (since og-session-runtime task 15); the session binds
  the runner's two lanes functions instead (`bindAuthorityRunner`: the RTT publish and the join, `UESimNetLanes-rationale.md`
  §12).
- **The game binding's hooks** (`BrawlerTickDriverGameUImpl-rationale.md` §10):
  - `idleUntil(deadline)` is og-sim-net's `SimNetIdle` over the lanes' endpoint (`m_simNetIdle`), one `serviceUntil` per call:
    og-net's handlers, the SimClock answer and the relay at receipt run there, on the runner (P2-19);
  - `beforePump` runs the runner requests (§19.4), then the server's `drain()` (the lifecycle queue first, then the inbox): the
    authority path of the session's `beforePump_Owner` (§22.3);
  - `afterInlineBatch` writes the batch's corrections (`SimulationManager::onPostSimulationGameThread`, which on an authority
    only calls `sendCorrectionAll`), publishes the clock with the server tick, and calls og-sim-net's `afterInlineBatch` with the
    tick the driver passes (the corrections, the clock, the reap, the window; P2-19 F1);
  - `onStepEnded` publishes the server tick to the SimClock server after every authority step (`afterStep_Step`; P2-20 F2), with
    that step's start time since og-time-dilation task 2b (§19.8).
- **The first batch reports tick 0** (the driver publishes its offset after a tick ran, P2-19 F2), and a capture reaped against
  it would be refused by the coordinator's domain gate. On the loop it cannot happen: og-net is serviced only by the runner's
  idle, after its first batch, so no connection is attached at that batch. A `checkf` in `afterAuthorityBatch` states it.
- **The host off the game thread.** og-sim-net calls the server host from the runner: its `rttMs` reads the RTT table the game
  thread fills each frame, and its probe stamps go through the latency mailbox (`UESimNetLanes-rationale.md` §12; §21.5).
- **The frame probes on the runner** (PROBE A per stepping wake; PROBE 6, deleted by og-session-runtime task 14) are §21.1 and
  §21.2.

### 19.4 Registration and unregistration on the runner

The tick backend refuses binds, releases and registrations off the runner in `FreeRunning`, and the Jolt world checks its caller,
so an authority's character registration crosses to the runner. The game thread keeps every `UObject` read and the game's
hooks; the runner does every simulation and world write. Requests travel in `m_runnerRequests` (closures under
`m_lifecycleBridgeMutex`), drained at the top of `beforePump`'s authority path (§22.3); outcomes come back as `LifecycleOutcome`s, collected on the game
thread in `onPostPhysicsStep` and at each `tryRegister` call.

`tryRegisterOnRunner`, one step per call (the component calls `tryRegister` each frame until it is not `Pending`):
1. **Bodies.** The simulatable moves into a shared pointer (the runner's requests hold it, so a pawn that leaves cannot free it
   under them). `makeBodyRequest_GameThread` captures the slot request; the runner's request binds the bodies
   (`createBodies_Runner`) and answers `BodiesBound` with the slot, or `BodiesPending` (no slot yet: the game thread asks again).
   The component's static data is read by pointer, and the component is held by `m_componentsHeldForRunner` until that answer
   arrives (the static data is not copyable).
2. **After the bodies,** on the game thread: the slot is recorded (`recordAuthoritativeSlot`), the fold check runs, and the game's
   `afterBodiesCreated`.
3. **Register.** The game thread reads the roster facts from the pawn (owner connection, local index, name); the runner's request
   checks that the bodies resolve (`bodiesResolvable_Runner`; `BodiesUnresolvable` makes the game thread ask again) and enqueues
   `SimNetServer::registerCharacter`, which the server's drain right after applies (`addCharacter`, then the facade with the
   channel as both owners). Its `applied` callback (`onRunnerRegisterApplied_Runner`) notifies the executor and records the id
   (G-36), and the tick backend's `onRegistered` (the occupancy push and the presenter's ring); on a refusal it releases the bodies.
4. **Finish,** on the game thread at the next call after `RegisterApplied`: the component map, the `tryRegister: registered` line,
   the game's `afterRegistered`, the render target. A refusal logs an `Error` naming the id and returns `Refused`.

`requestRunnerUnregister_GameThread` (the pawn's `EndPlay`, and the lanes' stop for every registered character): the game thread
does the host's half at once (the maps, the game's `onUnregistered`, the latency forget, the render target, the
`NewFramework: unregistered` line); when bodies may be held it posts a runner request that unregisters through og-sim-net's queue
once a registration was posted (G-37) and releases the bodies in its `applied` callback. og-sim-net calls the host's
`onLifecycleUnregistering` before the facade, which the session answers with the executor's notify while the character is in
storage (G-35; the same answer serves a client's roster Removes, og-input-latency P5-17's F1). After the runner has
stopped nothing is posted: the simulation and its world are destroyed whole.

### 19.5 The game thread on the loop

`onPostPhysicsStep` (the frame thread's `endFrame` in `FreeRunning`) runs the test hitch, the presenter (`present` drains the
tick backend's registration ring J9, then the snapshot), the outcomes, the authority presentation (§21.3) and the latency
probe's tick, and returns. It does not run the correction write or the clock publish (both on the runner, §19.3), the
display-snapshot publish and refresh (they read storage, and the overlay is refused in `FreeRunning`: §17.2, §21.4), or the
game's `afterVisualization` (a client's hook; the authority's ring-out score comes from the authority presentation
snapshot, §21.3). The 200 ms test hitch therefore stalls only the game thread: the
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

- **`noteAuthorityStepTime_Step()`** forwards to the clock's `noteAuthorityStepStart` (og-sim-session's `SessionClock` since
  og-session-runtime task 24), which reads the engine's clock (`FPlatformTime::Seconds()`) once, keeps it for the release that
  follows, and hands it to the manager for the step that follows (`SimulationManager-rationale.md` §18). The brawler's tick-driver
  binding calls it first in `releaseInputsForAuthorityTick`, on the thread that steps the authority (the runner, §24),
  immediately before `releaseDelayedInputsForStep(tick, 1)`.
- **`releaseDelayedInputsForStep`** takes the kept time (`m_clock->takeReleaseStepSeconds()`, which clears it) before its
  server test, and passes it to og-sim-net's
  `releaseForStep`, which appends it to each `[Release]` line (`ServerReceptionCoordinator-rationale.md` §8).
- **The lines:** `[AuthoritySimulation] tick=<t> stepSeconds=<s>` and `[Release] id=… late=<n> stepSeconds=<s>`, the same
  value on both for one step; the clock is the latency probe's (`FPlatformTime::Seconds()`, the same on every process of
  one machine). Everything before the field is unchanged.
- Until og-session-runtime task 14 the in-frame server wrote the field too, though its stamps were prompt, so the gate tools
  read one timeline on every arm.
- **Not on Chaos.** The former Chaos configuration (removed by og-session-runtime tasks 3–6) released a frame's batch from
  its input hook on the game thread and stepped on the physics thread. It never called `noteAuthorityStepTime_Step`, so its
  lines were as before, and nothing written on one thread was read on the other.

### 19.8 The SimClock server's tick and its step's start time (og-time-dilation task 2b)

og-sim-session's `SessionClock` since og-session-runtime task 24: which tick and which time, the two kept stamps, why the
publish passes the step's start, how large the step's duration is and what it does not explain are `SessionClock-rationale.md` §6,
moved from this section with the code (the measurements carried from og-time-dilation task 2b); the prohibition is
`SessionClock-guards.md` G-06 (this file's G-39 until task 24). What stays this class's: the brawler binding's two calls, which
forward (`noteAuthorityStepTime_Step` to `noteAuthorityStepStart`; `afterStep_Step` to `publishServerTick` with the lanes' SimClock
server, `nullptr` on a client), and the release's take (§19.7). The former Chaos configuration (removed by og-session-runtime
tasks 3–6) never called `afterStep_Step` (§19.7), so it published nothing.

## §20 The clock's RTT source: SimClock samples, the knob, the shadow line (og-input-latency task P3-10)

- **The feeds and the shadow line** are og-sim-session's `SessionClock` since og-session-runtime task 24
  (`SessionClock-rationale.md` §5, moved from here: the engine's round trip in `onAuthorityTick`, the SimClock sample in
  `onClockSample`, one estimator on the drain's thread, and the `[ClockSource.Window]` line's due rule and fields). This
  class forwards both feeds (`onTimingInfoReceived` after its wire sample and callback, §9.6; `onSimClockSample`) with a
  `ClockLink` over its lanes (`ackSrttSeconds`, the SimClock client's `stats`).
- **The knobs.** `[OGNetcode] ClockRttSource=Engine|SimClock` (this task) and `[OGNetcode] JitterMultiplier=<number>`
  (og-input-latency task P3-13) are read and applied by og-sim-session's `SessionKnobs.h` since og-session-runtime task 23: its
  `SessionKnobs-rationale.md` §3 has their parse, clamps, unrecognised and non-finite lines, and the `[ClockSource.Config]` proof
  line. The setters run where this class calls `applyLatencyKnobs`, in `configureSession`, before `startStepping`, so before any
  sample reaches the estimator (og-simulation `SimulationManager-rationale.md` §16 and §17).
## §21 The game-thread paths off the authority (og-input-latency task P5-20)

The game thread of a dedicated server touches no simulation state (§19): every read of storage, the systems, og-sim-net's
channels or the probe's step-side state is on the runner, and what the game thread needs it reads from a copy the runner
publishes. The P11-6 entry checks (the Jolt world's caller check) and the `checkf`s below hold that over a session. Since
og-session-runtime task 14 every authority session runs the loop and the branches test the role (they tested the loop flag
before).

### 21.1 PROBE A per stepping wake

On the loop the tick driver calls `onFrameStepsDue` once per stepping wake, on the runner (P11 design §7). PROBE A's
"frame" is then a wake: `noteFrame` takes `++m_authorityWakes_Runner` (a runner-only counter) instead of `GFrameCounter`, which
is the game thread's frame count and has no meaning on the runner. Every sample is one wake after the last, so the cadence line
reads `dFrame0=0 dFrameGt1=0`, `meanTicksPerFrameX100` and the percentiles are sim ticks per stepping wake, and `meanFrameUs` is
the mean wall time between stepping wakes. The line keeps its text, `[RelayProbe.Frame] role=Server`; the server's
`[SimHost.Mode] … stepping=runner` line tells the two readings apart.

### 21.2 PROBE 6 retired on the loop

PROBE 6 (`[RelayProbe.Budget]`, `[PacketBudget]`) read the `UNetDriver`'s client connections, which are game-thread state, so
it ran only on the in-frame server: on the loop `onFrameStepsDue` returned before it, and its first statement after that
return was a `checkf(IsInGameThread())`, so a PROBE 6 that ran on the runner would have failed at its first wake instead of
racing the driver. og-session-runtime task 14 deleted the probe, the return and the check with the in-frame server, and
og-simulation's probe with them (`RelayProbes-rationale.md` §12): no dedicated server had fed it since og-input-latency P5-19.

### 21.3 The authority presentation snapshot (the ring-out score)

The brawler's ring-out score is sim-derived presentation that only the authority computes (P7 design D2): its score system is
authority-only, and the replicated score property is the scoreboard's source. On the in-frame server (until
og-session-runtime task 14) `Game::afterVisualization` read the score system on the game thread after the frame's steps (§10).
On the loop the runner writes it, so:

- **The snapshot.** `AuthorityPresentation` is `entryCount` and up to `kMaxSimulatableSlots` entries, each a sim id and the
  game's `AuthorityPresentationSlice` (a game type; the brawler's is its score and whether the score system holds an entry,
  `BrawlerSessionGameUImpl-rationale.md` §8 in OGBrawlerUnreal's docs). It travels in a `TripleBuffer` (og-sim's
  `Mailbox.h`, one writer and one reader), held through a `std::unique_ptr` like the display tier's.
- **The runner fills it** in `publishAuthorityPresentation_Runner`, the last statement of `afterAuthorityBatch` on the loop:
  one entry per simulatable in storage, from the game's `authorityPresentationSlice_Step(systems, id)`, then `publish()`.
- **The game thread applies it** in `presentAuthority_GameThread`, from `onPostPhysicsStep`'s authority branch before the
  latency tick (where `afterVisualization` sits on a client): the newest snapshot, one `Game::presentAuthoritySlice(session, id, slice)` per
  entry. The brawler's finds the registered component for the id and writes the score property, so clients see the score
  through the same `OnRep` as the in-frame server's did.
- **The two hooks are checked** by a `static_assert` in `publishAuthorityPresentation_Runner` and are in the name-clash probe
  (`ueSimulationSessionHookNames`, §12).
- **Why not the display snapshot.** The display snapshot (§17) is og-sim's, per entry the simulatable's state; the score lives
  in the systems executor, which og-sim's snapshot does not read, and an authority publishes no display snapshot (§21.4).

### 21.4 The dedicated server's visualization gate

A free-running authority has no game-thread viz world (`viz=none`), and the tick backend refuses `registerVizVolume`,
`editVizQuery`, `getVizReader` and `overlayRenderPoses` (`TickDriverBackendUImpl-rationale.md` §15). The session answers the
question for every caller: `hasGameThreadVizWorld()`, public, is `m_manager.has_value() && runsPrediction()` (since
og-session-runtime task 14; it tested the loop flag before, and its first term is task 11's: a world without a session has no
viz world either, §23). The character component asks it before it
registers its viz volume and before it draws (`OGBrawlerSimulatableComponent-rationale.md` §24, in OGBrawlerUnreal's docs).
The session publishes no display snapshot on the authority (§19.5): the fill reads storage, the reconciliation and the input
resolution, all the runner's. Until og-session-runtime task 14 the in-frame server's game-thread fill opened with a `checkf` that
the loop was off; task 14 deleted it. So every reader of the display snapshot (the component, the input-history feed, the
scoreboard) sees none on a dedicated server.

### 21.5 The probe stamps through the mailbox

The latency probe's state is the game thread's; the step thread posts events through its mailbox (`UELatencyBudgetDiagnostics-
rationale.md` §6). On the loop the server's three game-thread stamp sites are the runner's, each with a mailbox door
(`UELatencyBudgetDiagnostics-rationale.md` §14):

- og-sim-net's `stampServerReceived` and `stampHanded`, which the lanes' server host forwards (`UESimNetLanes-rationale.md` §12);
- the corrections' `Handed`, which the runner's `stampCorrectionsHanded_Runner` posts after og-sim-net's `afterInlineBatch` has
  flushed the batch's corrections: it reads each simulatable's channel from og-sim-net itself (`SimNetServer::channel`) and the
  tick it carries (`correctionTickOf(channel)`). Until og-session-runtime task 14 the in-frame server stamped it in
  `tick_GameThread` by walking the session's channel map, a walk the loop skipped (the map was the game thread's, each buffer
  the runner's).

The per-connection round trip comes the other way, from the game thread to the runner, through the lanes' RTT table, which the
game thread fills each frame (`UESimNetLanes-rationale.md` §12).

### 21.6 What the game thread still does on the loop

`onPostPhysicsStep`: the test hitch, the presenter's `present` (J9's registrations, then the render snapshot), the lifecycle
outcomes (§19.4), the authority presentation (§21.3) and the latency tick, whose server part on the loop is the mailbox drain,
the pending stamps and the window lines. The lanes' `afterDispatch` publishes the RTT table. Registration's game-thread half
reads only `UObject`s and the session's own maps (§19.4).

## §22 Clock dilation on a client: `Off`, `Shadow` and `On` (og-time-dilation tasks 8, 11 and 14)

og-simulation's `ClockDilationController.h` holds the rate controller (`ClockDilationController`), its per-frame stage
(`clockDilation::beforeClientPump`) and its window statistics (`ClockDilationWindow`); its rationale has the law, the sensor and
the stage's contract. This section is how the session runs them. `Off` runs nothing, `Shadow` runs the controller and logs what
it would do without changing the step rate, and `On` (og-time-dilation task 11) runs the same controller and applies its scale
through the tick driver's rate door. The compiled default stays `Off` until the user rules (og-time-dilation's design Q4).

### 22.1 The server tick on the SimClock sample

`SessionClock-rationale.md` §7.1 since og-session-runtime task 24 (moved from here): the clock's `onClockSample` passes the sample
with its `serverTick` and `serverTickAgeSeconds` to the estimator's server-tick sensor.

### 22.2 The knob and its proof line

`[OGNetcode] ClockDilation=Off|Shadow|On` is read and applied by og-sim-session's `SessionKnobs.h` since og-session-runtime task
23 (`applyClockDilation`, at the end of `applyLatencyKnobs`, after `applyClockRttSource`, so after both role branches and before
`startStepping`). Its rationale, `SessionKnobs-rationale.md` §4, has the authority's `[Dilation.Config] mode=n/a`, the client's
modes and unrecognised line, and the proof and doors lines (og-time-dilation's `lead_split.py` parses the proof line anchored at
its token).

**The controller and the window** (both `std::optional`) were this class's members until og-session-runtime task 24 and are
og-sim-session's `SessionClock`'s since (`SessionClock-rationale.md` §7.2); `beginPlay` passes them to `applyLatencyKnobs`
through the clock's `dilationController()` and `dilationWindow()`; they are emplaced there on every client session, `Off` included, and on
no authority. A client's `configureSession` runs on the game thread (`beginPlay` takes the runner path for an authority world
only), so they are built on the thread that later uses them.

### 22.3 `beforePump_Owner`: one hook, two roles

The tick driver calls the game binding's `beforePump(hostNowSeconds, rate)` on the scheduler's owning thread right before each
pump (og-simulation's `TickDriverConcepts-rationale.md`), and the brawler's binding forwards both arguments to
`beforePump_Owner(hostNowSeconds, rate)` (`BrawlerTickDriverGameUImpl-rationale.md` §10). The function was
`beforeAuthorityPump_Runner`, which took neither argument; the owning thread is the runner on the authority loop and the game
thread on a client, hence the new suffix.

- **On a predicting client** (the clock engaged and `runsPrediction()`), it runs the stage (the clock's `beforePredictionPump`;
  `runClockDilation_GameThread` until og-session-runtime task 24; §22.4) and returns.
- **Otherwise it is the authority path.** With the clock engaged it first notes the rate door for the `[Dilation.Authority]` line
  (the clock's `noteAuthorityRateDoor`, `noteAuthorityRateDoor_Owner` until og-session-runtime task 24, §22.9; og-time-dilation task 11). Then it runs the runner requests (§19.4) and og-sim-net's
  server `drain()` (§19.3). Until og-session-runtime task 14 it returned there unless the authority loop was on (the in-frame
  server pumped nothing here); task 14 deleted that return with the in-frame server. A client never reaches it: it returns
  after the stage (before og-time-dilation task 8 it returned at the loop test).

### 22.4 The stage on a client: the thread and the two clocks

`SessionClock-rationale.md` §7.2 since og-session-runtime task 24 (moved from here): the stage runs in the clock's
`beforePredictionPump` on the frame thread that owns the client's scheduler, with the driver's `hostNowSeconds` and the engine's
clock as `clockNowSeconds` (`SessionClock-guards.md` G-05, this file's G-38 until task 24).

### 22.5 The window line

`SessionClock-rationale.md` §7.2 since og-session-runtime task 24 (moved from here): the clock logs the window's `formatLine()`
when it is due and restarts it, through the engine's log routes (the Unreal adapter's `RouteOGLevelLine`) with the same text,
category (`LogOGNet`) and level (`Warning`). **Corrected (R0, task 24):** this section said the line was logged directly, not
through `RouteOGMessage`; it was a `UE_LOG` then and is a routed level line now.

### 22.6 Why `Shadow` cannot change the rate, and how `On` does

The controller sets `apply` only when its mode is `On` and the scale differs from the door's, and the stage calls
`setRateScale` only on `apply` (og-simulation's `ClockDilationController-guards.md` `G-05`). So in `Shadow` the door's scale
stays `1.0` and the window's `rateScale` reads `1.00000/1.00000` with `applied=0`, while `wouldScale` reports the scale `On`
would have set. (Until og-time-dilation task 11 this session also never built an `On` controller: it mapped `On` to `Shadow`.)

In `On` the stage applies each change of the controller's quantized scale through the door the driver handed to
`beforePump_Owner`, on the game thread that owns the client's scheduler (§22.4). The window's `applied` counts those
`setRateScale` calls and `rateScale` reports the door's own minimum and maximum. The session adds nothing between the
controller and the door: no second clamp (the controller's `maxDeviation` is the clamp, and the door's ceiling keeps it at
2 % at most, §22.8) and no role test (the stage `OG_CHECK`s a predicting manager, and the driver's door refuses an
authority's `setRateScale` and counts it, §22.9).

### 22.7 What `Off` keeps

In `Off` the stage is never called: no estimator read, no window, no line but the client's `[Dilation.Config]` lines (one until
og-time-dilation task 11, two since: the proof line and the doors line). The one runtime change on an
`Off` client is the copy of the two sample fields (§22.1), which feeds the sensor; the stage is its only production reader, so in
`Off` nothing reads it. Task 8's start-up
compare, the pre-task binary's warm-up (`T8_pre`) against the `Off` arm (`T8_off`), the same command lines: every one-shot LogOG*
line up to `[SimHost.Backend]` is byte-identical on all four processes but the new `[Dilation.Config]` line and og-net's per-run
connection ids and ports. Since task 11 the authority also prints `[Dilation.Authority]` (§22.9) in every client mode: the
authority has no mode, and the line only reads the door.

### 22.8 The dev doors and their clamps (og-time-dilation task 11)

Five `[OGNetcode]` keys (four since task 11, the fifth, `ClockDilationCommonLeadTicks`, since og-time-dilation task 14) set the
dilation config a client's controller and window are built from: the gain, the maximum deviation, the target offset, the setpoint
and the `Common` lead. They are og-sim-session's since og-session-runtime task 23: `SessionKnobs-rationale.md` §5 has their doors
(`clockDilationDoors`), their lines, the four `static_assert`s that keep each door inside the controller's constraints, and what
no door reaches.

### 22.9 The authority's refused rate changes: `[Dilation.Authority]` (og-time-dilation task 11)

`SessionClock-rationale.md` §7.4 since og-session-runtime task 24 (moved from here): the clock's `noteAuthorityRateDoor` reads
the door's refused count on the runner and logs `[Dilation.Authority] refusedRateChanges=<n> rateScale=<s>` at the first pump,
every 10 s of the driver's host time, and whenever the count changed.

### 22.10 What `On` measured (og-time-dilation task 11)

Editor build, shape (a) (og-net's shaper b0, 100 fps, two clients and a rejoin, floor 6), one lock hold, in this order: a
discarded `Off` warm-up, `Off`, `On`, `On` with `ClockDilationTargetOffsetTicks=0.5`, then the three again, then a short run
with every door out of range (`T11_*` under `Saved/Profiling`). Every number is og-time-dilation's `lead_split.py`.

- **`On` against `Off`** (`T11_on` / `T11_off`; the repeat `T11_on2` / `T11_off2`): `rateScale` within [0.9953, 1.0012] over
  every window; steady `|err|` p5/p95 at most 0.055 (0.073), `errAbsMax` 0.067 (0.087); the clients' lead split `|ΔL|` p95
  0.086 / 0.242 (0.178 / 0.159); 0 steady Skip, Stall or HardResync; steady `wouldScaleDevP99` at most 0.03 % (0.04 %);
  0.20–0.65 door changes per second; the mean lead 2.323 against `Off`'s 2.235 (2.360 against 2.342).
- **The controller holds its own target:** `target − stepLag` sits within 0.01 tick of each client's OwnRtt rule on every
  `On` client, with the guard never bound, as og-time-dilation's review of task 10c predicted for b0 at 100 fps. The execution
  lead sits 0.06–0.18 tick below it, about the sensor's constant `sHat` bias on b0 (−0.05 to −0.09).
- **The offset arm** moves the controller's target by +0.50 tick (`target − stepLag − rule`, 0.499 and 0.497) and the mean
  lead by +0.574 and +0.525 against the same build's `On`, +0.662 and +0.543 against `Off`.
- **The authority** prints `mode=n/a` and `[Dilation.Authority] refusedRateChanges=0 rateScale=1.00000` on every line of
  every run (18 in a 150 s run, the first 3 s after the server's log opens).
- **The doors:** gain 0.2, 5 %, −9 ticks and `Common` log the four Warnings and run 0.05, 2 %, −2 ticks and `OwnRtt`; at ten
  times the default gain the door changes 40 times a second (a dev door, not a setting).

### 22.11 What `Common` measured (og-time-dilation task 14)

A packaged smoke (Development server and clients of the task's tree, `Saved/DilationTD2_14/jolt`), og-time-dilation task 13's
launcher: three headless clients, each through its own og-net shaper with a fixed lag each way and b0's loss (2 % up, 1 %
down), a near (10 ms), a mid (30 ms) and a far client, 100 fps, `RelayDelayFloorTicks=6`, 150 s, one lock hold on a clean
host, a discarded warm-up first (`T14_*` under `Saved/Profiling`). Every number is og-time-dilation's `lead_split.py` and its
task 13 per-direction table (`e2e_join`'s late share, B ← A = B receiving A's inputs).

- **The door refused `Common` before this task:** the same tokens on the TD1b package (`T14_red`) log `[Dilation.Config] ini
  [OGNetcode] ClockDilationSetpoint='Common' is not accepted by this build (expected OwnRtt), running OwnRtt` and run
  `OwnRtt`. On this build the proof line reads `setpoint=Common` with `(ini)` and the doors line ends `setpoint=Common (ini)
  commonLeadTicks=3.650 (compiled)`.
- **The far client at 50 ms**, `Common` twice and `OwnRtt` twice, interleaved:

  | run | near ↔ mid (%) | near ↔ far (%) | mid ↔ far (%) | pooled late | guardBound (near / mid / far) |
  |---|---|---|---|---|---|
  | `Common` 1 | 0.00 / 0.00 | 0.00 / 0.00 | 1.01 / 0.13 | 0.19 % | 0 / 0 / 0 |
  | `Common` 2 | 0.01 / 0.00 | 0.00 / 0.03 | 1.29 / 0.11 | 0.24 % | 0 / 0 / 0 |
  | `OwnRtt` 1 | 0.00 / 0.00 | 0.00 / 0.03 | 0.03 / 30.03 | 5.02 % | 0 / 0 / 0 |
  | `OwnRtt` 2 | 0.00 / 0.00 | 0.00 / 0.00 | 0.00 / 100.00 | 16.68 % | 0 / 0 / 0 |

  Each pair cell is the first client's late share receiving the second's inputs, then the reverse (near ↔ far = near ← far
  / far ← near). Under `Common` every pair is within 1.2 percentage points across its two directions; under `OwnRtt` the far
  client receives the mid client's inputs 30 % and 100 % late while the reverse is on time. The clients' leads: `Common`
  3.58–3.69 ticks on every client, the window's target 3.950 (3.65 plus the 100 fps step lag, 0.300); `OwnRtt` 2.88–2.90 / 1.71–1.89
  / 3.00–3.01 (near / mid / far). In every run 0 steady Skips, Stalls and hard resyncs (G5) and G3 and G4 pass; G1 (the
  pair lead split) passes under `Common` (worst pair mean 0.107) and fails by design under `OwnRtt` (pair means up to 1.30).
- **The far client at 60 ms**, task 13's shape, `Common` once (`T14_c60_Common1`): guardBound 0 on every client, the far
  client's lead 3.565 against its low edge c − 0.5 = 3.30; the pooled late share 4.01 % (task 13 measured 16.8 % under
  `OwnRtt` on the same shape), with mid ← far 20.7 % and far ← mid 3.2 %: at 60 ms that pair's arrivals (101.9 and 102.7 ms)
  exceed the 100 ms an equal lead gives, so no common lead puts both on time (task 13's report §4.3).
- **The mid client's high edge**: under `Common` its lead minus its guard centre is 2.01 ticks on every run, the e that
  task 13's report predicted at 3.65 (+2.00), one tick inside the clock's band; it bound no call and the clock corrected
  nothing.
- **`OwnRtt` on this shape sits at the far client's low edge**: the 50 ms far client's `OwnRtt` lead, 3.00, is about its low
  edge's threshold (task 13's report §5 item 4 predicts X_low ≈ 3.0). On the TD1b run it bound 8,990 calls over 10 steady
  windows (its centre 3.45 that run); on this build's two `OwnRtt` runs none (centres 3.32 and 3.34). That is `OwnRtt`'s
  behaviour, unchanged by this task.

## §23 A world without a session (og-session-runtime tasks 11 and 14)

**When: the start rule** (og-session-runtime task 14). `beginPlay` asks `refuseToStart` first, after the world check and
before it builds anything. Its answers, in the order it tests them (the first refusal wins):

| net mode | driver | result |
|---|---|---|
| Standalone | — | one Warning on `LogOGSimHost`, `[SimHost.Session] none netMode=Standalone`; no session. The first registration logs `[SimHost.Refused] register id=<n> value=- reason=dedicated-server-only` and raises the message, once |
| ListenServer | any | `[SimHost.Refused] role=authority netMode=ListenServer value=- reason=dedicated-server-only` and the message; no session |
| DedicatedServer or Client | not og-net | today's `[SimNet.Host] refused role=<server\|client> driver=<class>: simulation traffic needs the og-net driver` Error on `LogOGNet` and its message; no session; a dedicated server exits with code 3 |
| DedicatedServer or Client | og-net | the retired knobs (task 11's `refuseRetiredKnobs`): `og.Sim.RunInline` other than 0; on a client `[OGNetcode] SimTrafficImmediate=0`, which its og-net lanes remembered at driver start (`UESimNetLanes-rationale.md` §11; task 12); on a dedicated server `og.Sim.AuthorityLoop=0`, `[SimHost.Refused] knob=og.Sim.AuthorityLoop value=0 reason=retired` (task 14) |
| DedicatedServer or Client | og-net | a second world of the same role in this process, while the first has a session (task 16b; `refuseSecondSessionOfRole`): `[SimHost.Refused] role=client netMode=Client value=pie reason=one-session-per-role-per-process` (`role=authority netMode=DedicatedServer` for a server; the value is `pie` in a Play-In-Editor world, `-` elsewhere) and the message; no session |
| DedicatedServer | og-net | the authority session, on its runner (§19) |
| Client | og-net | the client session |

`refuseNetModeOrDriver` holds the first three rows, `refuseRetiredKnobs` the fourth and `refuseSecondSessionOfRole` the
fifth; `refuseToStart` registers the slot (below) and asks a dedicated server to exit. The lines, the message and the exit are `UESessionRefusal-rationale.md`'s. The
driver refusal's message says what each role keeps: a server "runs no simulation session" (it said "simulates alone" until
task 14, when a detached session ran there), a client "simulates nothing". A standalone world is the packaged client's front
end before it joins (no pawn: nothing registers, and the join screen is unaffected) or an editor-launched standalone process
(a pawn: its registration refuses once, with the message). Local play is out of scope (design Q2, W5): it needs a dedicated
server on loopback, as `ogplaytest` and the onboarding doc's PIE advice use. Until task 14 these worlds ran the in-frame server
(§19): a listen server and a dedicated server with og.Sim.AuthorityLoop=0 on the game thread, a standalone or foreign-driver
world as a detached session.

**What a refused world has.** The session has no manager, no world, no tick functions, no og-net bind and no latency probe:
`getManager()` stays empty for the world's life. A dedicated server exits at once with code 3. Any other world keeps running
and simulates nothing: every character stays unsimulated on it. A listen server's og-net lanes are serviced by og-net's own
thread (`UESimNetLanes-rationale.md` §12) and no session binds them. Its og-net client lanes stay unbound and drain on the game thread before a bind, as every client's do
until its session binds (`UESimNetLanes-rationale.md` §11). Measured in task 11 (`SR2_t11_cli_knob2`, the Editor binary): the
client joined a default server and both ran 30 s with 0 asserts, ensures and fatals; the server printed only its usual
window lines. The same in task 12 for `SimTrafficImmediate=0` (`SR2_t12_cli_sti0`): one
`[SimHost.Refused] knob=SimTrafficImmediate value=0 reason=retired` on the client, the message shown, 0 on the server, 30 s with
0 asserts, ensures and fatals, and 0 `[InputGap]` on the server.

**The instance is registered.** The refused session still takes a slot in `s_instances` for its world, so `instanceFor` finds
it, and `endPlay` clears the slot. Without the slot the character binding (`UESimulatableOwnerBinding`) would retry every
frame while `instanceFor` is null and assert after `kMaxRegistrationAttempts` (600) attempts. Seen failing in task 11
(`SR2_t11_sf_P2`, the registration removed): `Assertion failed: false [...UESimulatableOwnerBinding.h] [Line: 104]`, "manager
never spawned after 600 attempts (netmode=3)", and the client was dead within 30 s.

**The registration line.** The first `tryRegister` or `clientBindStep` of such a world logs
`[SimHost.Refused] register id=<n> value=- reason=<reason>` once (`refuseRegistrationWithoutSession`,
`m_noSessionRegisterRefused`). The reason is `dedicated-server-only` in a standalone or listen-server world and `no-session`
after a knob or driver refusal (`m_noSessionReason`, which the start rule sets). It is the line alone where a refusal's message
is already on screen (a listen server's, a knob's, a driver's), so that message stays; in a standalone world, where only a
Warning came before, it raises the message too (`m_noSessionRegisterRaisesMessage`). `tryRegister` then returns
`Refused` (the binding logs its own `DeferredReg: id=… abandoned` Error and stops), and `clientBindStep` returns `Abandon`
(the binding stops without a line). A client asks `clientBindStep` before it registers, so on a client the line comes from
there and `tryRegister` is never reached.

**Every public member that reaches `m_manager`, with no session.** "`instanceFor()` null" is a world whose session owner
has not begun play or does not exist; "`getManager()` empty" is this section's world.

| member | how it reaches `m_manager` | callers | `instanceFor()` null | `getManager()` empty | task 11 |
|---|---|---|---|---|---|
| `getServerClock`, `editServerClock`, `getClientClock`, `runsPrediction` | `m_manager->`, unguarded | the session's own paths (`afterAuthorityBatch`, `getServerReceptionTick`, `onPostPhysicsStep`, `hasGameThreadVizWorld` behind its test, the client teardown after `endPlay`'s no-session return); `TickDriverBackendUImpl`'s `onRegistered_GameThread` and `onRegisteredHost_GameThread` (`runsPrediction`) | not reached | not reached: every caller runs inside a started session (a step hook, a tick function, a registration that succeeded) | none |
| `getServerReceptionTick` | through `getServerClock` | `UESimNetServerHost::receptionTick`, whose session pointer is set only when `beginServerSimNet` binds | not reached | not reached (nothing binds; a dedicated server has exited) | none |
| `tryRegister` | the body pass reaches `physicsSlotFor`'s `checkf(m_manager.has_value())`; the notify | `UESimulatableOwnerBinding::tryRegisterWithNewFramework`, through `instanceFor` | the binding retries, then asserts at 600 attempts (why the slot is registered) | an authority's first call would assert in `physicsSlotFor` (a client never gets here, below) | returns `Refused` at once, with the line once |
| `clientBindStep` | none (it reads `m_simNetLanes`) | `UOGBrawlerSimulatableComponent::resolveRegistrationKey`, on a client | not reached | `m_simNetLanes` is null, so it would log that the world has no og-net sim lanes (false: they exist, unbound) and abandon | returns `Abandon` at once, with the line once |
| `unregisterFromNewFramework` | the in-frame unregistration's notify, behind `has<>` (until og-session-runtime task 14; the role's request since) | `AOGBrawlerSimulationManager::unregisterFromNewFramework`, from the component's `EndPlay` through `instanceFor` | skipped by the caller | would run the backend's unregistration on an unbuilt session for an id it never registered | returns at once: nothing was registered |
| `endPlay` | none directly; it resets what `beginPlay` built (G-21) | `AOGBrawlerSimulationManager::EndPlay`, always | — | would unbind and reset state that never existed | after `onEndPlayBegin` (safe with nothing started, G-20), clears the slot and returns |
| `hasGameThreadVizWorld` | none before task 11 (it read the loop flag only); `runsPrediction()` since task 14, behind the test | the component's `initializeWithManager` and `TickComponent` | not reached | true on a client, so `initializeWithManager` registers a viz volume, which asserts in `physicsSlotFor`. Seen failing in task 11 (`SR2_t11_sf_P1`): `Assertion failed: m_manager.has_value() [...UESimulationSession.h] [Line: 2183]` | `m_manager.has_value()` first (task 11; `&& runsPrediction()` since task 14, the loop flag's test before): false, so the component neither registers nor draws |
| `onTimingInfoReceived`, `onSimClockSample`, `onConnectionTierReceived`, `onRelayDelayFloorReceived` | behind `m_clock.has_value()` (the clock is emplaced right after the manager; `m_manager.has_value()` until og-session-runtime task 24), or the tier consumer's `has_value()` | `UESimNetClientHost`, whose session pointer is null until `bindClientSession` (og-sim-net's client session holds a SimClock sample while its listener is unbound, which the lanes bind with the session) | — | not reached (nothing binds), and safe if reached | none |
| `getTimeConfigPtr`, `getManager`, `getSystemsExec`, `getReplicatedTierConsumer`, `getReceptionCoordinator` | they return the optional or a pointer | `BrawlerSessionGameUImpl`'s score push and the input-history feed, each testing `has_value()` first | — | empty or null, and every caller tests it | none |

The other public members never reach `m_manager` and are safe in such a world:
- the display readers (`getDisplaySnapshot`, `findDisplayEntry`, `findDisplayVizState`, `forEachDisplayVizState`,
  `getLastRelayedInput`, `isLocallyControlledOnThisPeer`, `getVizSlice`): no snapshot is held, so each answers null,
  `nullopt` or false, and their readers (the HUD, the scoreboard, the component, the input-history feed) return on that;
- the viz-world members (`editVizQuery`, `getVizReader`, `registerVizVolume`): reached only behind `hasGameThreadVizWorld()`;
- the latency diagnostics (`getLatencyBudgetDiagnostics`, `editLatencyBudgetDiagnostics`): never begun, and every stamp
  tests its probe first;
- `owner`, `getRegisteredComponentsById` (empty), `getClientEffectiveInputDelayTicks` (the input resolution's), and
  `setOnTimingInfoReceivedCallback` (`publishClientEffectiveInputDelayTicks` was one until og-session-runtime task 24 made it
  og-sim-session's `SessionClock`'s).

The friends (the physics backend, the tick-driver game, the latency diagnostics) reach private members only from a started
session: the tick functions, the step hooks and a registration that succeeded.

**Equivalence on the default path.** Every test above is `m_manager.has_value()`, which is true wherever `instanceFor` finds a
session that started: a client sets its slot and emplaces the manager in one game-thread pass, and an authority does both
on its runner while the game thread waits for the runner's start (§19.2). So the default path takes none of the new branches.
The start rule's tests pass on the default path (a dedicated server and a client, both on the og-net driver, no retired knob
set), so it starts the same two sessions; every branch task 14 deleted was a loop-off or detached branch the default path
never took (task 14's implementation notes, in og-session-runtime's workspace, list them).

**A refused client and its server** (og-session-runtime task 14, from the task-11 review). A client refused at `beginPlay`
(a knob) still joins: its og-net lanes stay unbound and drain at the driver's `afterDispatch` on the game thread
(`UESimNetLanes-rationale.md` §11), into the client session the lanes own, which nobody binds. The server does not know the
client refused: it registers the client's character as usual and keeps simulating it and sending its corrections, which the
client drains and drops. It receives no input from that client, so its reception never opens a gap (`[InputGap]` reports a
jump between delivered captures, and none is delivered). Measured in task 14 (the refusal matrix's row F, `og.Sim.RunInline=1`
on the client, 30 s): the client logged its knob line and the `register id=1 … reason=no-session` line, its `[SimNet.Window]
role=client` windows counted about 1,000 drains and 1,400 messages each and it logged about 1,560 `[ReceiveCorrectionState]`
lines; the server registered the character (`tryRegister: registered simulatable id=1 isAuthority=1`), kept stepping (1,680
ticks after the registration) and sending states (about 1,200), and logged 0 `[InputGap]`, with 0 asserts, ensures and fatals
on both. The build before task 14 measured the same.

### 23.1 Play In Editor (og-session-runtime task 16b)

The editor's "Play As Client" runs a dedicated server and N clients. With "Run Under One Process" on (the engine's
default) every one of them is a world of the editor process; with it off the editor starts the server, and every client but
the first, as its own process (`UnrealEditor.exe -game -PIEVIACONSOLE -Multiprocess [-server]`).

- **The server world in the editor process** is `NM_DedicatedServer` while `IsRunningDedicatedServer()` is false (that reads
  the process's `-server` switch). Its lanes decide on the driver's net mode (`UESimNetLanes-rationale.md` §12), so it gets
  host-serviced lanes and runs the authority session on its runner, as a server process does. Until task 16b the lanes read
  `IsRunningDedicatedServer()`: the world got lanes on og-net's own thread and `beginServerSimNet`'s
  `checkf(lanes->hostServiced())` fired, taking the editor down (since task 14; before it the world ran the in-frame server).
- **The first client world** in the editor process runs the client session beside it (the registry holds one slot per
  world, §9.2).
- **A second client world** in the editor process starts no session (the start rule's fifth refusal): it joins and renders,
  and its characters stay unsimulated. With two or more clients, turn "Run Under One Process" off (`docs/onboarding.md`).
- **A refused server world does not exit:** `requestServerExit` ends a dedicated-server PROCESS only
  (`UESessionRefusal-rationale.md` §3), never the editor.

Measured in task 16b (`impl/tasks/16b/pie_runs.ps1`, the editor's `-pie` switch with the play settings on its command line;
the runs and their lines are in the task's notes).

## §24 Threads: who calls what, on which thread (og-session-runtime task 16)

og-session-runtime task 22 moved this section's table and its notes, verbatim, to og-sim-session's
`SessionThreads-rationale.md` §7, so that the thread table is kept in one place: who calls what on which thread for both
roles (§24.1, now its §7.1), the per-step order (§24.2, now §7.2) and what crosses (§24.3, now §7.3). The runtime's own
table and its checks, `kPredictionThreads`, `kAuthorityThreads` and `SessionThreadChecks`, are that file's §3 and §4.

**A world without a session** (§23) holds its world's `s_instances` slot as a no-session session. The `instanceFor` callers'
safety there is run-time guarding (task 11's `m_manager.has_value()` tests, listed member by member in §23), not a type: a new
public member that reaches `m_manager` must test it too.

### 24.1 The table

Moved: `SessionThreads-rationale.md` §7.1.

### 24.2 The per-step order

Moved: `SessionThreads-rationale.md` §7.2.

### 24.3 What crosses

Moved: `SessionThreads-rationale.md` §7.3.
