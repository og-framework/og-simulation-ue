<!-- SPDX-License-Identifier: MPL-2.0 -->
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
  post-visualization hook, and the test hitch cvar in the `.cpp`.
* **§11** is the start-up (task 27b): `beginPlay`, the game's six start-up hooks and their inline check, the neutral input
  on both roles, the field-diff gate, the step-length check and the backend line.
* **§12** is the registration sequences (task 28): `tryRegister` and `unregisterFromNewFramework`, the pending record, the
  game's four registration hooks and their inline checks, and the name-clash row over all ten hooks.

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
`TryRegisterStatus`, and calls the game's four registration hooks (§12). The owner keeps the wrappers that turn its game's
character id into the storage key, and reaches no private member any more (§4).

**The brawler** is the one user: `AOGBrawlerSimulationManager` holds `UESimulationSession<BrawlerSessionTraitsUImpl>` as its
member `m_session` (`OGBrawlerSimulationManager-rationale.md` §2, in OGBrawlerUnreal's docs).

**Not copyable.** The copy constructor and assignment are deleted: the members bind each other by reference (§2), and
the backend holds the session by reference.

**Polymorphic since task 25.** It derives from the three relay listener interfaces (§9), so it has a virtual destructor
and a vtable; it is never deleted through an interface pointer (the owner holds it as a member).

---

## §2 Member order

C++ constructs members in declaration order and destroys them in reverse, whatever the initializer lists say. The order
is today's relative order from the owner, where these members lived until task 24a, with the two references first:

```
m_owner&, m_game& → m_latencyBudgetDiagnostics → m_backendWorlds → m_physAdapter, m_physReaderAdapter, m_queryAdapter
    → m_storage → m_staticData → m_reconciliation → m_inputResolution → m_netSync
    → m_systemsExec → m_integrationLayer → m_manager → m_backend
    → m_replicatedTierConsumer, m_lastPublishedEffectiveInputDelayTicks
    → m_receptionCoordinator, m_frameHealthProbe, m_relayWriteProbe, m_connectionBudgetProbe, m_delayedInputComponentsById
    → m_onTimingInfoReceivedCallback, m_timingRelay
    → m_pendingRegistrations
```

* **`m_owner` and `m_game` come first.** They are references, which have no destruction order, and the backend is
  constructed with the session, so both must be bound before it. No backend reads them during its construction today
  (the tick-driver host stores the session in its game binding, the self-scheduled host stores it), but a reference
  declared after `m_backend` would still be unbound while the backend is built, and one that read it would read an
  unbound reference.
* **The latency member comes next, so it is destroyed last.** It was already declared before the worlds, the manager
  and the backend in the owner. Its initializer is `{ std::as_const(*this) }`: it holds a const reference to the session
  (`UELatencyBudgetDiagnostics-rationale.md` §4 says why the `std::as_const`).
* **`m_staticData{ Game::makeStaticData() }`** is the game's static data, built by its one hook that reads the game's
  static-data console variables once. The hook returns a prvalue, so the member is initialized in place (guaranteed copy
  elision): the static-data type need not be copyable or movable, and the brawler's is neither. The call runs after
  `m_storage` is built and before the peers, where the owner's cvar member and its static data were constructed before
  task 24a.

**The transport state comes last** (task 25), after `m_backend`, as manager design §4.5 placed it. It is emplaced in
the owner's start-up and reset in `endPlay` (the owner's `EndPlay` until task 27a, §10), so where it sits in construction is unobservable; in destruction
it now goes before the backend and the manager. The registry is a static member and has no place in the order. The
input reception's state (task 26a) sits inside it in the owner's relative order: the coordinator, PROBE 5 and the map,
after the delay publish mark. The map holds weak handles and the probe holds plain counters, so neither one's destruction
is observable; the coordinator borrows the manager's `TimeConfig` and is asserted below the manager. The two frame probes
(task 26b) sit where the owner declared them, PROBE A after the coordinator and PROBE 6 after PROBE 5; both hold plain
counters, so their destruction is not observable either.

**The pending registrations come last** (task 28), as manager design §4.5 placed them. In the owner the map was declared
after the session member, so it was destroyed before the whole session; here it is destroyed first among the session's
members, but after the destructor's body, which clears the registry slots (`⛔G-10`). A record holds a simulatable value
and the game's input provider (the brawler's: a lambda that captures its input component's pointer and the id), and
neither one's destruction touches the registry, so the change is not observable. Its declaration is not asserted: nothing
borrows from it.

**What is asserted.** `compositionContractsHold()` holds six `static_assert`s over `__builtin_offsetof` (four before
task 25, five before task 26a), the `ManagerType` and `getTimeConfigPtr` rows, and since task 26a the two reception sink
concepts (§9.7), and the
constructor asserts it, so every row is checked wherever the constructor is instantiated (a member function of a class
template is compiled only when it is used, and the constructor is the one member every owner uses):

| row | why |
|---|---|
| `m_storage` < `m_staticData` < `m_reconciliation` < `m_inputResolution` < `m_netSync` | construction: each peer binds the members above it in its initializer, and the static data's hook runs between the storage and the peers |
| `ManagerType` is the tick-driver game binding's `Manager` | `ManagerType` is private; the tick-driver host names the manager through the game binding (`TraitsT::TickDriverGame<UESimulationSession>::Manager`), so the two must be one `SimulationManager` instantiation |
| `m_backendWorlds` < `m_physAdapter` < {`m_physReaderAdapter`, `m_queryAdapter`} < `m_integrationLayer` | destruction: the backend's worlds outlive every adapter bound to them, and the integration layer goes before the adapters it holds |
| `m_manager` < `m_backend` | destruction: the backend holds the step driver, which unregisters its resync callback from the manager's client clock when it goes, so the backend goes first |
| `m_manager` < `m_replicatedTierConsumer` | destruction: the tier consumer borrows the manager's `TimeConfig`, so it goes first (task 25; in the owner it was declared above the manager's owner, and only the owner's `EndPlay` reset protected it; that reset is `endPlay`'s since task 27a, `⛔G-21`) |
| `m_manager` < `m_receptionCoordinator` | destruction: the reception coordinator borrows the manager's `TimeConfig`, so it goes first (task 26a; in the owner it too was declared above the manager's owner, protected only by the owner's `EndPlay` reset, `endPlay`'s since task 27a) |

Every message names the row it replaced. The rows inside the backend and its worlds are their own assertions
(`TickDriverBackendUImpl-rationale.md` §1), and the tick-driver host asserts `m_backendWorlds` < `m_backend` itself.

**The reorder hazard, stated exactly.** There is no `-Wreorder`-as-error anywhere in this tree, so a member outside the
asserted chain is unprotected, and a reordered member inside it would still compile were it not for the assertions. It
has not bitten because every peer constructor is a trivial reference store: none calls into a sibling peer during
construction, so a reorder today would only bind a reference to a not-yet-constructed object that is not yet accessed.
It becomes load-bearing the day a peer constructor's body calls a method on one of those sibling references. *Moved from
`OGBrawlerSimulationManager-rationale.md` §2 ("The reorder hazard, stated exactly"), where task 11 of the brawler's comment
extraction first asserted it; that task poisoned three reorders and each fired its assertion.*

**Seen failing (task 24a).** Swapping the declarations of `m_manager` and `m_backendWorlds` fails the build with the
adapter row's message (the task's notes quote it). **Task 25:** declaring `m_replicatedTierConsumer` above `m_manager`
fails the build with the tier consumer's row. **Task 26a:** declaring `m_receptionCoordinator` above `m_manager` fails
it with the reception coordinator's row.

**The systems executor** (`⛔G-01`) is emplaced, not constructed: its body-reading systems hold the adapters, which are
emplaced in the role branches.

---

## §3 The backend's two members

`typename TraitsT::BackendWorlds m_backendWorlds` and `typename TraitsT::template Backend<UESimulationSession> m_backend{
*this, m_backendWorlds }`. The traits pick the configuration's host:

* **A driver-scheduled engine (Jolt)**: the worlds are `TickDriverBackendWorldsUImpl` over the engine's world host, and
  the backend is `TickDriverBackendUImpl` over the game's tick-driver binding, `TraitsT::TickDriverGame<UESimulationSession>`
  (`TickDriverBackendUImpl-rationale.md` §1, §10, §12).
* **A self-scheduled engine (Chaos)**: the game's own host, with an empty worlds struct (the brawler's
  `ChaosTickBackendUImpl-rationale.md`, in OGBrawlerUnreal's docs).

**Destruction order.** The backend is declared last, directly after `m_manager`, and the worlds above the adapters, so
`m_backend` is destroyed first (on Jolt the tick backend with its driver and presenter, then the render-target table,
the game binding and the frame host, while the manager still lives), then the manager, the integration layer and the
adapters, and last the worlds (on Jolt the world host and its engine). The two rows of §2 assert it, the same in both
configurations. *Moved from `OGBrawlerSimulationManager-rationale.md` §2 ("The backend's two members and the destruction
order", og-simulationscheduler-withjolt task 83).*

**The seam.** The owner calls the backend through one fixed set of entry points, each from exactly one site. The set is
a game concept (the brawler's `PhysicsBackendHost`, `PhysicsBackendHostUImpl.h`), so this class cannot assert it; the
game asserts it beside its traits, on the type this class holds (`BrawlerSessionTraitsUImpl.h` for the brawler).

---

## §4 Access: the friends, the accessors and the interim path

**The friends (ruled, Q-G).** The class befriends `typename TraitsT::template Backend<UESimulationSession>`,
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
`forgetReceptionOwner` and the backend's unregistration (with the line deleted, `C2248` on `m_backend`, `m_physAdapter`,
`m_storage`, the three peers, `m_manager` and `forgetReceptionOwner`, and nowhere else). The lead ruled the line kept until
those two moved. Task 28 moved them (§12) and deleted `friend SessionOwner;`; every configuration builds without it, so the
owner reaches only the public surface. Since then the class befriends exactly the three Q-G friends above.

**The public surface is narrow.** `owner()` (const and mutable), the latency member's `get`/`edit` pair, since task
24b the composition reads and doors of §8, and since task 25 the transport's public members (§9): `instanceFor`, the five
listener overrides, `getSyncedTimingBuffer` and `setOnTimingInfoReceivedCallback` (og-simulation's owner concept),
`publishClientEffectiveInputDelayTicks`, and two const reads for a game's parts, `getClientEffectiveInputDelayTicks` and
`getReplicatedTierConsumer`; and since task 26a the reception's public members (§9.7): the coordinator's type and its
three accessors, which the component binding's RPC tail calls, the route's registration, and the two sinks, which the
coordinator calls; and since task 27a `endPlay`, which the owner calls (public, so it still can once task 27b drops the
owner's friendship), and two const reads for the game's post-visualization hook, `getSystemsExec` and
`getDelayedInputComponentsById` (§10); and since task 27b `beginPlay`, which the owner calls after its `Super` (§11); and
since task 28 `tryRegister` and `unregisterFromNewFramework`, which the owner's wrappers call (§12; since task 29 the
component's registration skeleton, `UESimulatableOwnerBinding`, calls `tryRegister` and `noteDelayedInputComponent`
directly). The owner keeps its own
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
`onFrameStepsDue_GameThread`; task 27a re-pointed the post-physics pass, so the tick-driver binding's `onPostPhysics` and
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
  The knob functions and `onFrameStepsDue_GameThread` (task 26b) are defined at the end of the same pair, and compile
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
  `OGBrawlerSimulationManager.cpp`), and so do the game's parts they call.
* **The `.cpp` (task 27a)** holds what a header cannot: the test hitch console variable, a namespace-scope object that
  must be registered once, its exported accessor and the exported sleep (§10). The plugin compiles it on its own (the
  module is not unity), and the two functions, non-templates, sit in a pair of their own, so their statements keep the
  unoptimized state they had in the owner's post-physics step: the sleep's division compiles to `divss` by `1000.0f`, as
  in the owner, not to a multiply by its reciprocal. The game's post-visualization hook is likewise a non-template function in
  the game's own `.cpp` (the brawler's `OGBrawlerSimulationManager.cpp`, under its pair).
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
| `getTimeConfigPtr` | the manager's `TimeConfig`, or a null pointer |
| `editVizQuery`, `getVizReader`, `registerVizVolume` | the backend's three visualization doors |
| `onPostSimulationGameThread` | the manager's post-simulation pass |

Two members are new, and read-only: `getManager()` (the manager's `std::optional`, by const reference) and
`getStorage()` (the storage, by const reference). They exist for a game's parts, which read through a const reference
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
* **`onInputRelayHostReady` attaches the host to the character's `USimulatableNetSyncComponent`.** The owner found the
  game's component and called its `attachInputRelayHost`, which the component's binding forwards to that same sibling (the
  character binds the two in its `PostInitializeComponents`), so the call lands on the same object. The cast of the host's
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

**The session implements the three listener interfaces,** and the owner registers the session (`&m_session`) in each
interface's registry at the positions where it registered itself: the timing listener directly after writing the
registry slot, the connection and input listeners after the tier consumer's start (`beginTierConsumer`) on both roles,
and, on the client, before the latched values are pulled (`⛔G-07`). The relays call through the interface pointer, so
nothing on their side changed.

**The destructor** clears the registry slot and the three listener slots if they still name this session (`⛔G-10`). It is
a backstop: `endPlay`, which the owner's `EndPlay` calls, clears them first in every world teardown. It runs inside the owner's destructor, during
the garbage collector's purge, which is why it touches no `UObject`. The owner's destructor, whose whole body this was, is
gone.

**One listener per WORLD, not per CHARACTER.** A tier is a property of the wire, and every character on a machine shares
one wire. The retired per-character shape had N characters each driving the same effective-delay atomic and each
requesting its own tier-transition stall, so for a couch co-op client one wire transition produced two stall requests
against a clock that accumulates debt. One listener per world makes the request count match the transition count.

### 9.3 Threads: the transport's one crossing

**The write crossing carries one scalar.** `publishClientEffectiveInputDelayTicks` writes a lone `std::atomic<int32>`
inside the input-resolution peer (`SimulationInputResolution`'s effective-delay atomic), which the physics thread loads
once per tick in `collectInputAll` and once per resimulated tick in `collectResimInputAll`. The worst a race can do is
apply a new delay one tick late. Everything else in the transport is game-thread-only: the listeners are called from the
relays' replication callbacks, and the registry, the tier consumer and the timing relay pointer are read and written only
there, in the owner's start-up, and in the session's teardown and post-physics pass (§10; the owner's until task 27a). *Moved from the brawler's
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

The **floor** rides the session relay (`ASimulationTimingRelay`); the **tier** rides the per-connection relay
(`ASimulationConnectionRelay`). The two `OnRep`s are genuinely independent and can land in either order, which is exactly
why both call **one** recompute holding **both** cached inputs rather than each writing the atomic on its own (`⛔G-05`):
two writers each holding half the formula would answer with a stale other half whenever their `OnRep`s interleaved. The
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
(og-simulation's `ServerReceptionCoordinator`) and replicates the result to the owning client as
`ASimulationConnectionRelay`'s tier property. The client never computes a tier and never samples the round trip for tier
purposes, which is what makes a client/server tier disagreement impossible by construction rather than merely unlikely.

**The tier consumer is emplaced on both roles** (`⛔G-06`). A dedicated server never reads the value it publishes there,
but a listen-server host does: its local player's input delay comes from that cache, and an empty cache would leave the
atomic at `0` where the no-tier fallback is correct. No tier ever reaches an authority world, because the server writes the
relay property and `OnRep` is a non-authority callback, so the authority cache stays at that fallback for the whole
session.

**The tier consumer is destroyed before the manager.** It borrows the manager's `TimeConfig`. It is declared after
`m_manager`, so member destruction takes it first, and a `compositionContractsHold()` row asserts that order (§2).
`endPlay` also resets it while the manager still exists, as the owner's `EndPlay` did before task 27a (`⛔G-21`).

#### The preserved tier-0 quirk, and its two consequences

**The server never publishes tier 0 as a FIRST value:** the coordinator's last published tier for an owner is taken as 0
until the first publish. A wire that never leaves tier 0 therefore produces no publish, no connection relay (the relay is
spawned by the first send) and no call into `onConnectionTierReceived` at all: the client sits on the pre-arrival no-tier
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

`onConnectionTierReplayed` and `onRelayDelayFloorReplayed` apply and republish but **do not** stall. For the tier, it is the
first tier ever applied, so no tick was predicted against a previous tier's delay. For the floor, the pull happens inside
the owner's start-up, before the first prediction tick.

A floor **rise** on the live channel is indistinguishable from an upward tier transition to the client: the frontier must
fall back by the difference, which the clock pays down as stall ticks.

Both listeners **bind, then PULL** (`⛔G-07`). Each property dirties only on change, so an `OnRep` that fired before the bind
would never be re-notified; the relay latches it and hands it over at the pull. The timing relay is always relevant, so on
a client it may legitimately already exist or not: a missing relay simply means no floor has arrived, and the relay's own
client `BeginPlay` replays it if it turns up later.

#### The floor advisory

`logRelayDelayFloorAdvisory` reports what og-simulation's `classifyRelayDelayFloor` says about the session floor, from every
intake: the owner's ini override at start-up and the floor listener's two entry points. It is advisory only (`⛔G-04`,
`⛔G-09`): floor `0` is the documented off mode and is never flagged, and floor `1` is a legal configuration.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §5 (all but "The zero input" and "Inbound-hit routing",
which are the brawler's and stayed), made game-neutral, with the R0 corrections above.*

### 9.5 The relay ring: the host boundary

`ASimulationInputRelay` cannot name a game's character or component, so it cannot resolve its own owner to the object that
consumes relayed input. The session is the bridge, and **only** the bridge: it holds no state for this channel and no
per-arrival traffic passes through it. Once the host is attached to the character's `USimulatableNetSyncComponent`, ring
`OnRep`s go straight from the host to that sibling (`onRelayedInputRingArrived`) and on to the ring callback the game's
component bound there.

**No map, deliberately.** The resolution is host → `GetOwner()` → character → sibling: lookups on data that already
exists, because the actor's owner replicates and the authority spawns the host with the character as its owner
(`ASimulationInputRelay::spawnForCharacter`). A registry keyed by character would be a second structure to keep in step
with every spawn, death and travel, for nothing.

**Idempotent.** The host calls in from its `BeginPlay`, from `OnRep_Owner`, and again from any ring `OnRep` that arrives
while still unlinked; the sibling's `attachInputRelayHost` absorbs the repeats (it returns when the host is already its
own). Every hop can legitimately fail during the join window, and every failure is a plain "not yet".

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §6 ("The host boundary"); R0, task 25: the moved text
named the brawler component as the attach target and its callback as the arrival's destination; the host has been the
sibling's since og-ue-simulationcomponentandmanagersplit task 20b, and since task 25 the session attaches to the sibling
directly. The relay tap ("stage, do not write") describes `relayRemoteInput`, a reception sink that was the owner's until
task 26a; it moved with it, to §9.7.*

### 9.6 The timing relay, its callback and the server clock

**The relay is found once.** `findTimingRelay` iterates the owner's world for the timing relay on first use and caches the
pointer; the "timing relay found" line logs that first find. The owner uses it at start-up (to publish the session floor on
the authority, to pull the latched floor on a client), and the server-clock publish uses it every frame.

**`publishServerClock_GameThread`** writes the authority's server clock into the relay's synced buffer, from
`onPostPhysicsStep` (§10; the owner's post-physics step until task 27a), between the post-simulation pass and the backend's visualization update, only on a manager that does not
predict. Clients read the clock from that buffer through the relay's replication; the round trip and the authority tick come
back through `onTimingInfoReceived`.

**`onTimingInfoReceived`** first hands the round trip to the latency member's wire sample, then either to the callback set
by `setOnTimingInfoReceivedCallback`, if one is set, or to the manager's network estimator (round trip and authority tick).
No caller in the tree sets the callback; it and `getSyncedTimingBuffer` are og-simulation's
`SimulationManagerOwnerConcept`, which the session models (the brawler's concept test asserts it on the session since
task 25, where it asserted it on the owner before).

**`getSyncedTimingBuffer`** returns the relay's buffer and is a fatal check when no relay exists yet.

*New text, task 25: the owner's rationale described these members only in its thread table and its carried header
comments (§12, history).*

### 9.7 Input reception (task 26a): exempt from the seam

The server's input reception: the reception coordinator and its emplace, the delayed-input map, the two sinks with PROBE 5,
the per-step release, the reap and the forget. Task 26a moved these from the owner. **The section is exempt from the seam**
(`⛔G-11`): it is typed on the per-step release, where erasing the game's input type would put an indirect call on the step
path (manager design §9, step 5), so it stays in this class under M4, and the seam script does not list it.

| member | what it is |
|---|---|
| `ReceptionCoordinator`, `m_receptionCoordinator`, `beginReceptionCoordinator` | og-simulation's `ServerReceptionCoordinator` over the engine's connection handle (`FUEConnectionHandle`) and `TraitsT::Simulatable`; its emplace and logger, extracted from the owner's start-up and called at the position of the two statements it holds |
| `hasServerTierWiring`, `getReceptionCoordinator`, `getServerReceptionTick` | the coordinator's presence, the coordinator itself, and the server's reception tick |
| `m_delayedInputComponentsById`, `noteDelayedInputComponent` | the id → component delivery route, and its registration |
| `deliverRemoteInput`, `relayRemoteInput` | the delivery sink and the relay sink; the relay sink holds PROBE 5's write |
| `m_relayWriteProbe` | PROBE 5 (`⛔G-12`) |
| `releaseDelayedInputsForStep` | the per-step release (the drain) |
| `forgetReceptionOwner` | the reap and the forget: the coordinator's `forgetOwner`, the map's erase (`⛔G-13`) and PROBE 5's `forgetOwner`, extracted from the owner's unregistration and called at their position (since task 28 the unregistration is the session's own, §12.3) |

Every body moved statement for statement; the task's notes hold the token comparison. The spellings changed where the
owner reached the session (`m_session.m_manager` is `m_manager`), where the owner named the brawler's types (the map's
element is `TraitsT::Component`, the input is `TraitsT::PlayerInput`, and the release names `TraitsT::Simulatable`, with the
`template` keyword a dependent call needs), and at the registration: the owner keeps a wrapper that converts its game's
character id to the storage key and passes that (the brawler's `noteDelayedInputComponent`). No log text, field, level or
category changed. The coordinator, PROBE 5 and the map keep the owner's relative order, after the delay publish mark.

**The callers reach the session.** The hosts call the release and the reap on the session they hold, as its friends: the
tick-driver game binding's `releaseInputsForAuthorityTick` and `afterInlineBatch`, and a self-scheduled host's per-frame input
hook (the brawler's Chaos host). The component binding's RPC tail finds the authority's session through its owner,
`TraitsT::SessionOwner::instanceFor(true)`, then `session()`, for the coordinator, the reception tick and both sinks. The
latency member reads the map directly. Until task 27a the owner's post-physics step handed the map to its game's score
push, and the owner's `EndPlay` cleared the map and reset the coordinator; since task 27a `onPostPhysicsStep` hands the
session to the game's hook, which reads the map through `getDelayedInputComponentsById`, and `endPlay` clears the map and
resets the coordinator (`⛔G-21`, §10). The per-step release
and the delivery stay direct calls on the concrete session, so the step path gained no indirect call (the task's notes read
the disassembly of the release call site).

#### The coordinator and the transport adapter

The whole server reception subsystem (the tier table, the delay queue, the claim map, and the orchestration over them) lives
in og-simulation's engine-agnostic `ServerReceptionCoordinator`. The session owns **one** instance of it and is a thin
transport adapter: it acquires engine primitives (address, player slot, round trip, sim tick, wire decode) and forwards them
in. **Authority role only:** `std::nullopt` on a pure client, because the coordinator borrows `const TimeConfig&` from the
manager's owned config. It is declared after `m_manager`, so member destruction takes it first, and a
`compositionContractsHold()` row asserts that order (§2); `endPlay` also resets it while the manager exists, as the
owner's `EndPlay` did before task 27a.

**Reception policy is in the core; primitive acquisition is at the RPC boundary** (`USimulatableNetSyncComponent`'s server
RPC and its typed tail, `UESimulatableOwnerBinding::receiveRemoteMoveBundle`), where the bundle and the owning actor live.
The tail forwards straight into `ServerReceptionCoordinator::noteRttSample` (once per bundle) and
`ServerReceptionCoordinator::receiveInputBundle` (the whole per-slot loop). The session supplies three accessors (the
coordinator, the server's reception tick and the route's registration) and the two sinks, and none of them carries netcode
policy. A fourth accessor, `hasServerTierWiring`, has no caller in the tree.

*R0, task 26a:* the moved text said the route is registered "from the RPC adapter". It is registered by the game
component's own registration, once, on the authority (the binding's registration skeleton,
`UESimulatableOwnerBinding::tryRegisterWithNewFramework`, since task 29; the brawler component's own loop before).

#### The two sinks

The session satisfies both `RemoteInputDeliverySink` and `RemoteInputRelaySink`. Both are checked at the
`receiveInputBundle` call site (its `requires` clause), and both are asserted on the session in `compositionContractsHold()`,
so a breaking signature change surfaces legibly wherever the session is constructed. Before task 26a they were asserted on
the owner, beside its two definitions; task 26a saw both assertions fail on the owner's type.

`deliverRemoteInput` is the **one** delivery method that both the per-id drain (built in `releaseDelayedInputsForStep`) and
the core receive loop's fallback (a malformed slot in `receiveInputBundle`) route through. It resolves id → component
through `m_delayedInputComponentsById` and hands the input to the inbound path the RPC tail's no-coordinator branch uses
(the binding's `deliverDelayedRemoteInput`, which calls the component's remote-move callback), with the **original**
`captureTick`. A stale weak handle means the component was garbage-collected without an unregister: the entry is dropped
and the input discarded, because a dead component has nothing to receive it.

`m_delayedInputComponentsById` exists because the core claim map is **id-keyed** (an engine-free container cannot hold a
`TWeakObjectPtr`), so the coordinator's `deliver` callback hands back an id and this map resolves it. The id is the storage
key. The map is filled by `noteDelayedInputComponent`; pruned by both sinks and by the drain's callback when a weak handle
has gone stale; erased in `forgetReceptionOwner`, inside the unregistration (the session's own since task 28, §12.3; the owner's before) and so before the component unbinds
from its sibling (`⛔G-13`); and cleared in `endPlay` (the owner's `EndPlay` until task 27a).

*R0, task 26a:* the moved text listed only the drain's prune and the unregistration's erase; both sinks prune too, and
`EndPlay` clears the map.

The per-id `deliver` callback answers "is this owner still alive", so the coordinator can drop a stale claim, and routes a
live delivery through `deliverRemoteInput`. **The liveness check stays on the adapter side,** because the drain's prune
contract is a `bool` return while the sink method returns `void`, as the concept requires.

*Carried from the owner's source, its note at the route's registration:* "Routing registration for the `deliver` callback.
⛔ A plain overwrite, ONCE at register." The store is a plain assignment, so a second registration of an id replaces the
first; the game component registers once, on the authority.

#### The drain and the reap

A frame's game-thread reception work is three pieces (og-simulationscheduler-withjolt task 51):
* `onFrameStepsDue_GameThread`, the per-frame work before the steps: PROBE A, then the coordinator test, then PROBE 6
  (§9.9; the owner's until task 26b);
* `releaseDelayedInputsForStep(firstTick, numSteps)`, the drain only. It tests the coordinator itself, prints one
  `[ReleaseBatch] first=<tick> numSteps=<n>` line on `LogOGNet` at `Verbose` (the category and level of the coordinator's
  `[Release]` lines; its row is in `kOGAdapterLogRoutes`, `UESimulationLogRoutes-rationale.md` §6), and calls
  `ServerReceptionCoordinator::releaseDelayedInputs` with the per-id callback;
* the reap, `ServerReceptionCoordinator::reapConnections`, which the host calls on the coordinator.

The drain hands over only the game-thread-safe upcoming sim tick and the per-id callback. Where each host calls the three
pieces is the host's: the driver-scheduled host drains per authority step and reaps once per frame after the inline steps;
the self-scheduled host drains the whole frame's batch up front and then reaps, once per frame. The brawler's two hosts and
their call patterns are its `OGBrawlerSimulationManager-rationale.md` §7, in OGBrawlerUnreal's docs.

*R0, task 26a:* the moved text named the router's table `kOGLogRoutes`; it is `kOGAdapterLogRoutes` since
og-ue-simulationcomponentandmanagersplit task 19.

#### Threads, and one acknowledged wart

Everything in the reception is game-thread-only and has no internal synchronization: the coordinator, PROBE 5 and the map
are read and written by the RPC tail (the engine's receive dispatch), the hosts' per-frame and per-step calls (on the game
thread: a driver-scheduled authority steps inline there), the owner's start-up and unregistration, and the session's
post-physics pass and teardown (the owner's until task 27a).

`getServerReceptionTick()` reads the server clock through `getServerClock()`, which the physics thread writes, from the game
thread with no synchronization. The RTT sample path has always tolerated this, and the tier average is insensitive to a
one-tick skew. It was left as it is so that relocating the reception subsystem into the core stayed a pure relocation; a
follow-up may source it from the host's mapped tick, the way the drain's tick already is. *Moved from the brawler's
`OGBrawlerSimulationManager-rationale.md` §1 ("One acknowledged wart").*

#### The relay tap: stage, do not write

`relayRemoteInput` is the **outbound** half of the receipt path. Where `deliverRemoteInput` routes an input *into* the
simulation for the character that sent it, this stages the same `(captureTick, dA, input)` for that character's replicated
relay ring, which carries it to the **other** clients, so each peer simulates that character with its real input instead
of extrapolating. `dA` is the **schedule stamp**, the effective input delay the authority held for that wire at receipt, so
a peer derives the application tick as `captureTick + dA`.

Under bare-C1 flush-on-poll the arrival is **staged**: the binding's `stageRelayedInput` puts it in the sibling's staging
target, and the relay host's `PreReplication` publishes the whole staged burst once per replication poll, so two arrivals
in one server frame both reach the wire instead of the second overwriting the first.

**No depth is read at that site any more, and that is the point.** It used to pass a session-configurable retention depth
into `writeLatest`. On the flush path that same value would make every staged entry after the first supersede its
predecessor, the ring would carry exactly one entry per round, and bare C1 would silently become replace-latest again, with
no compile error and no warning. `stageRelayedInput` has no depth parameter (the binding asserts its signature), and the
capacity is `relayedInputRing::kMaxDepth`, taken as a constant inside the codec; og-simulation-tests' relay redundancy depth
test is the machine-checked fence that replaced the retired start-up proof line.

The staging outcome is deliberately unchecked: `accepted` can only be false on the stale-write arm, which the
coordinator's monotonic `acceptedNew` gate makes unreachable from this call site, and `droppedOldest` is already counted on
the host (the binding counts each overflow drop there).

> ⚠ **Which ring.** The replicated ring is `ASimulationInputRelay::m_relayedInputRing`, registered
> `DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing, COND_SkipOwner)`. The tap stages into the host's
> staging ring, which the flush publishes into it. With no host linked, the tap stages into the sibling's
> `m_detachedRelayStagingRing`, whose contents nothing replicates.

*R0, task 26a:* the moved text named the sibling's `m_detachedRelayRing` as the tap's no-host fallback. That ring is the
read fallback (`getRelayedInputRing` with no host); the staging fallback is `m_detachedRelayStagingRing`
(`USimulatableNetSyncComponent::relayStagingTarget`).

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §6 ("The relay tap: stage, do not write"), with the R0
above. Its history of the retired dual write, and the brawler's visualization read of the relayed input, stayed there.*

#### PROBE 5: relay writes per frame (`m_relayWriteProbe`), server only

All the frame probes are purely diagnostic: nothing reads them but the log lines they feed. Per-window summaries go out at
`Warning`, and nothing is emitted per tick or per write at any verbosity.

**What it settles.** Replication polls a replicated property once per server game-thread frame and compares the live value
against its shadow. **When this probe was built** the ring shipped at depth 1 (replace-latest), so a second write inside the
same frame overwrote the first in server memory, and no client and no client-side probe could tell that apart from a
send-path drop: both surface as `[RelayProbe.Arrival] gapCaptureTicks > 1`. Flush-on-poll has since removed that mechanism
(the relay tap above); the probe stayed because the **quantity**, writes per frame, is still the one nobody had measured.

The relay-loss elimination chain does not cover this. It eliminated the server's own write with "writes on every accepted
receipt, and receipts are complete", which is true and is about the wire *into* the server; and the server frame rate with
"about one sim tick per frame", which is true and about a different clock: these writes are paced by **packet arrival**, not
by the sim.

**`GFrameCounter`, not an invocation count.** The frame is the unit replication polls on, so the engine's own frame identity
is the only correct key; a local counter incremented at the tap would measure the tap's call rate instead.

**Three fractions, never collapsed.** `receivedX1000` is upstream completeness (only input redundancy raises it).
`observableX1000` is the coalescing ceiling (depth raises exactly this one), and it reports the **flush** ceiling since the
write path changed, which is why the stage capacity is *injected* into the probe from the codec constant rather than assumed
(`⛔G-12`). `deliverableX1000` is their product and is the only one comparable to the client's arrival rate. A single merged
"loss" number would decide the remedy on merged evidence. `replaceLatestObservableX1000` recomputes the same window the way
the retired write path imposed it, so archived windows stay directly comparable, and the improvement is two numbers on one
line rather than a claim.

Line 2 carries the run-length shape plus the capture-tick range, so a server window can be aligned against a client window
by its capture ticks, and by its owner id.

*R0, task 26a:* the moved text said owner ids "are per-process and do not match across logs". The id is the storage key,
which for the brawler has been its pawn's replicated character id since the sim-id change (the brawler's
`OGBrawlerSimulationManager-rationale.md` §15), the same number on every peer.

**Volume: two `Warning` lines per 120 writing frames per relayed character** (og-simulation's `kRelayWriteProbeWindowRuns`).
Nothing per write is emitted at any verbosity, deliberately: a per-write line is a per-tick line.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §8 ("PROBE 5") with the R0 above. PROBE A and PROBE 6
followed in task 26b (§9.9); the resim-gate feeds, the rewind-push probe and the Jolt host's window line stayed in the
brawler's §8.*

### 9.8 The session knobs (task 26b)

Three ini keys are read once, at composition, in the owner's start-up (the owner's `BeginPlay` until task 27b). Task 26b
moved their statements here as three functions, which the owner calls at the positions of the statements they hold:

| function | what it holds | where the owner calls it |
|---|---|---|
| `readSessionKnobs(worldIsAuthority)` | the three intakes (`⛔G-14`, `⛔G-15`, `⛔G-16`); it reaches no session state and returns the requests in a `SessionKnobs` value | after the latency member's begin, before the backend builds its world: both roles |
| `applySessionKnobs(knobs)` | the floor's clamp, set, ini line and advisory; the rotation width's clamp and set; the rotation width's proof line; the floor's publish onto the timing relay | in the authority branch, after the neutral input, before the tier consumer's start |
| `applyResimTriggerPolicy(knobs)` | the policy's parse, apply and proof line (`⛔G-17`) | after both role branches |

The statements moved unchanged: the intake's locals are returned in a `SessionKnobs` value, and the two apply functions
read the same four fields from it; the owner's `m_session.` prefix fell away. No log text, field,
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

The clamped value is stamped into `TimeConfig` and then published on the session's timing relay, which the game's
composition root must spawn **before** the session owner, precisely so that write has somewhere to land (the brawler's
`USimulationManagerSubsystem::OnWorldBeginPlay` spawns the relay first, on every non-client world). Reaching the failure
branch, the `[RelayDelayFloor] no timing relay at manager BeginPlay` line, means no client will ever learn a nonzero
floor: nothing publishes the floor after start-up.

Two intakes share one clamp: this ini override, and the floor listener (`onRelayDelayFloorReceived` and
`onRelayDelayFloorReplayed`), where the value arrives off the wire as a `uint8` and is clamped rather than trusted (a
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

`onFrameStepsDue_GameThread(firstUpcomingSimTick, numSteps)` is the per-frame work before the steps: the host calls it on
the game thread, once per frame that has steps due, with the frame's first upcoming simulation tick and its step count. Its
body is three pieces, in order:

1. **PROBE A**, frame health, on both roles;
2. **the coordinator test** (`⛔G-18`): a client has no reception coordinator, and the function returns here;
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

*Site: `onFrameStepsDue_GameThread`, below its coordinator test, once per frame with steps, for each client connection.*

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
`onFrameStepsDue_GameThread`, so it is one per process (per instantiation in the one module that instantiates the session):
a process that runs two authority sessions in turn, or a listen server's two worlds, prints it once. That was already so in
the owner, where the same `static` sat in a non-template member.

#### The capacity pin's word rounding: `(usableBits / 32) * 4` ∴D-01

*The `[PacketBudget]` line in `onFrameStepsDue_GameThread`.*

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
1. `m_backend.onEndPlayBegin()` (`⛔G-20`). A driver-scheduled backend unregisters the frame host's two tick functions,
   so no new frame starts, and waits for every outstanding step task (`TickDriverBackendUImpl-guards.md` G-01); a
   self-scheduled backend's may be empty. Nothing moves above it.
2. The delayed-input map's clear, the reception coordinator's reset (`⛔G-21`) and the tier consumer's reset, while the
   manager still exists: both borrowers hold the manager's `TimeConfig` (§2 asserts that member destruction takes them
   first too).
3. The registry (§9.2): if this session holds the authority slot, the slot is cleared and the role's three relay listeners
   are unregistered; then the same for the client slot. The destructor repeats this as a backstop (`⛔G-10`).
4. If the owner still has a world: the latency member's `end` (it unbinds its post-tick-flush handler), then the backend's
   `onEndPlayWorld` (a self-scheduled backend removes its engine delegates there).

Nothing resets the manager, the composition or the backend: they live until the session is destroyed, with its owner.

**`onPostPhysicsStep`, in order** (game thread, once per frame, after the frame's physics, with or without steps):
1. The test hitch (not in Shipping; below).
2. `m_backend.beforePresent_GameThread()`: a driver-scheduled backend's presenter (the shadow restore and the render
   apply, `TickDriverBackendUImpl-rationale.md`); empty on a self-scheduled backend.
3. The manager's post-simulation pass, `onPostSimulationGameThread()` (§8).
4. The authority's server-clock publish, `publishServerClock_GameThread()` (§9.6).
5. `m_backend.updateVisualization_GameThread()`.
6. `Game::afterVisualization(*this)`, the game's hook, at the position of the game statement it replaced.
7. The latency member's `tick_GameThread`, with the backend's diagnostics view (`UELatencyBudgetDiagnostics-rationale.md`).

It is private: the backend hosts call it as the session's friends (§4), a driver-scheduled backend through the game's
tick-driver binding (`onPostPhysics`, at the frame's end of physics), a self-scheduled one from the engine's post-tick
delegate, bound to a weak lambda whose weak object is the owner.

**The game's hook, `afterVisualization(session)`.** The game's statement that sat between the visualization update and the
latency tick (the brawler's: its ring-out score push) is the hook's body, verbatim, and the session calls it with itself,
`const`. The hook reads the session through const reads only: `getManager()`, and the two task 27a added,
`getSystemsExec()` and `getDelayedInputComponentsById()`, each a const reference to the member it names. The latency
tick, which followed the game's statement, is the session's own statement after the hook. The brawler's hook is
`BrawlerSessionGameUImpl-rationale.md` §5, in OGBrawlerUnreal's docs.

**The test hitch, `og.Sim.TestGameThreadHitchMs`** (not in Shipping). When it is above 0, the next `onPostPhysicsStep`
sleeps the game thread that many milliseconds, once, and sets the variable back to 0 at console priority
(`ECVF_SetByConsole`), so a value set from the console, from `-ExecCmds` or from an ini is reset. It forces a catch-up
frame: on an inline-stepping server, 250 ms gives a frame of about 15 steps. The sleep is the first statement of the pass,
the one hitch site for every backend.

* **Where it lives.** The console variable is a namespace-scope object, registered when it is constructed; the class
  template's header has no translation unit of its own to define it in, so the plugin's `.cpp` does (the initiative's
  review, finding m-10), and the plugin module constructs it once. It is defined in `UESimulationSession.cpp`, under
  `#if !UE_BUILD_SHIPPING`, with its name, default, help text and flags unchanged (its help text still names the owner's
  `OnPostPhysicsStep`, verbatim). The template reads it through an exported function the header declares under the same
  condition, `OGSIMULATIONUNREAL_API int32 consumeTestGameThreadHitchMs()`: it reads the value on the game thread, sets it
  back to 0 when it is above 0, and returns what it read; when that is above 0, `onPostPhysicsStep` calls the second
  exported function, `sleepTestGameThreadHitch(hitchMs)`, whose one statement is the unchanged sleep. The read, the reset
  and the sleep run in their old order.
* **Why the sleep is in the `.cpp` too.** Its argument is `static_cast<float>(hitchMs) / 1000.f`, and the engine sleeps
  `(uint32)(Seconds * 1000.0)` milliseconds. Compiled optimized in the template under `/fp:fast`, the division became a
  multiply by `0.001f`, one ulp off for most values, so the engine's truncation slept one millisecond more than before for
  about half of all values (250 itself was unchanged). In the `.cpp`'s unoptimized pair it is a division again, as it was
  in the owner (task 27a's notes, F1-b).
* **Off the step path** (the initiative's review, finding m-10). The accessor is called once per frame on the game
  thread, never from a step; in Shipping neither the variable nor the accessor exists, and the call is compiled out
  with them.

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
7. `readSessionKnobs` (§9.8), then the backend's `buildWorld` (the adapters, §3).
8. The role branch.
   * **Authority:** registry slot 0 (a `checkf` if it is taken) and the timing-relay listener (§9.2); the router logger
     lambda (`RouteOGMessage`); the integration layer; `Game::emplaceSystems` (`UESimulationSession-guards.md` G-01); the
     manager, without prediction; the three peers' loggers; the neutral input, `Game::neutralInput()` (`⛔G-23`, §11.3);
     `applySessionKnobs` (§9.8); `beginTierConsumer()` (`⛔G-06`, §9.4); the connection and input relay listeners;
     `beginReceptionCoordinator` (§9.7); `simlog::setGlobal`; `Game::installGameLoggers(worldIsAuthority)`;
     `bindCorrectionFieldDiffGate()` (§11.4).
   * **Client:** registry slot 1 and the timing-relay listener; the router logger; the integration layer;
     `Game::emplaceSystems`; the manager, with prediction; the loggers; the neutral input; `beginTierConsumer()`; the
     connection and input relay listeners, then `replayLatchedClientValues` (`⛔G-07`, §9.4); `simlog::setGlobal`;
     `Game::installGameLoggers(worldIsAuthority)`; `bindCorrectionFieldDiffGate()`; and the `[ResimProbe.Session]` line
     (compiled only `#if !NO_LOGGING`: its arguments call members a `NO_LOGGING` category does not have).
9. `applyResimTriggerPolicy` (`⛔G-24`), once, after both branches (§9.8).
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
| `afterSessionConfigured(netMode)` | member of the game object | after `applyResimTriggerPolicy` | the role's net mode |
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
dropped, or drops a walk whose line is printed. This predicate and `RouteOGMessage`'s `[DivergenceProbe` arm are the only
two sites in code that USE this category: one reads its verbosity, the other logs to it. Guard **G-22** sits on the
predicate and says so.

⚠ **Not yet confirmed in PIE.** The binding compiles and the gate is unit-tested against a stand-in predicate; that the
live `UE_LOG_ACTIVE` answers as expected in a running editor is unverified, so the first `Verbose` run should check that
`field=` actually appears before any conclusion is drawn from its absence.

*R0, task 27b:* verified: og-simulation's `correctionFieldDiff::enabled()` tests that a predicate is set before it calls
it, so it is false while the predicate is empty; the correction cache's gate is ahead of the walk's call;
the test file exists and counts walks with `walkCount`. The brawler's ini sets the category to `Verbose`
(`OGBrawlerSimulationManager-rationale.md` §4, in OGBrawlerUnreal's docs).

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
`tryRegister` and `noteDelayedInputComponent` wrappers have no caller since, and its `unregisterFromNewFramework`
wrapper is still the component's `EndPlay` call. Task 28's `transcribe.py` compares
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
5. **The registration:** the authority registers the simulatable with the component as both its prediction and its
   authority owner; a client registers it with the component and the input provider, wrapped by the latency member
   (`wrapInputProvider`, the capture stamp; `UELatencyBudgetDiagnostics-rationale.md`). The wrap is the session's own
   statement. Then the line `tryRegister: registered simulatable id=%u isAuthority=%d` (`LogOGMgmt`, `Log`), which prints
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
3. **`unregisterSimulatable`:** the owner callbacks, then the containers.
4. **`forgetReceptionOwner(id)`** (§9.7): the reception coordinator drops this owner's claim and its dedup watermark, the
   delayed-input map drops the id (`⛔G-13`: this erase stays here, ahead of the component's unbind from its sibling), and
   PROBE 5 drops its per-owner state. Promptly, rather than waiting for garbage collection to make an engine handle stale:
   the coordinator's claim map is keyed by id and cannot hold an engine weak pointer. PROBE 5's map then stays bounded by
   live ids, as the coordinator's does; a half-open run belonging to a dead owner is discarded rather than reported, which
   is correct, since its length is unknowable. The coordinator is a no-op on a pure client.
5. **The game's `onUnregistered(id)`** (the brawler: its roster's forget, then its input-history feed's forget).
6. **The latency member's forget,** the session's own statement.
7. **The backend's `onUnregistered_GameThread`,** then the line `NewFramework: unregistered simulatable id=%u`.

It is called for every component that ends play, registered or not.

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
checks were already game-neutral and are unchanged. The `SimCharacterId::None` check is the owner's, in its wrapper
(since task 29 off the component's path: the binding's skeleton calls this function directly, and the game's
`resolveRegistrationKey` hook answers `retry()` for a null id before the call).

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §10 and the generic part of its §13.4 (task 28), made
game-neutral, with the R0 lines above. The brawler's hooks, its statements and the source's own quoted comments are
`BrawlerSessionGameUImpl-rationale.md` §7, in OGBrawlerUnreal's docs.*
