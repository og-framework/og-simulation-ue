<!-- SPDX-License-Identifier: MPL-2.0 -->
# `USimulatableNetSyncComponent` — rationale

Companion to `Plugins/OGSimulation/Source/OGSimulationUnreal/SimulatableNetSyncComponent.h` and
`SimulatableNetSyncComponent.cpp`. Both files carry only code and one-line tags. **The prohibitions are in
`SimulatableNetSyncComponent-guards.md`, one id per site; this file carries the reasoning and the provenance.**
The non-template base the component calls the game through is `ISimulatableOwnerBinding`, documented in
`UESimulatableOwnerBinding-rationale.md`.

<!-- lint-external-ref: m_replicatedInputSyncedBuffer -- ABSENCE FENCE (Sec 5): the retired replicated correction-input property whose lifetime registration this component fences. It must NOT resolve -->
<!-- lint-external-ref: m_clientToServerInputSyncedBuffer -- RETIRED at og-syncedInput-rework task 9 (2026-10-03), dated history in Sec 5: the client->server input buffer member. It must NOT resolve -->
<!-- lint-external-ref: UNetActorFactory -- Unreal Engine class (the Iris actor factory), outside every scan root -->
<!-- lint-external-ref: SpawnActorAbsolute -- Unreal Engine UWorld method, outside every scan root -->
<!-- lint-external-ref: PostNetInit -- Unreal Engine AActor method, outside every scan root -->
<!-- lint-external-ref: EndReplication -- Unreal Engine UActorComponent method, outside every scan root -->

> **Licence.** MPL-2.0, as the code it describes (og-ue-simulationcomponentandmanagersplit ruling Q1). The code and
> this narrative moved here from the game module's `USimmableUpdateComponent` in tasks 20a and 20b (2026-10-21); the
> brawler's own side of the boundary stays in that component's rationale.

---

## §1 What this component is, and the four channels

One `USimulatableNetSyncComponent` per simulated character, created as a default subobject of the character's actor
beside the game's own per-character component. It is the character's **reflected wire surface**: the replicated
correction-state buffer with its notification, the client→server input RPC, and their wire-format fences. Since task
20b it also holds the character's **link to its relay host** with the two never-replicated fallback rings (§10), and
**sends the connection tier** (§9). It holds no game type and no netcode policy. Everything typed — reading the state,
delivering the inputs, stamping the relayed ring — is the game's, reached through the binding (§2).

UE forces the shape. Replicated properties, their notifications and RPCs must be declared in a `UCLASS` body, and a
`UCLASS` cannot be a template, so a typed game component cannot inherit these declarations from a generic template.
None of them names a game type, so a concrete, non-template class can own them. This is that class; the game's
component stops replicating (og-ue-simulationcomponentandmanagersplit rulings Q8 and Q11).

| channel | direction | mechanism | carrier |
|---|---|---|---|
| correction state | server → every client the character's actor replicates to | replicated property + `OnRep_CorrectionState` | this component |
| input bundle | owning client → server | unreliable server RPC `ServerReceiveRemoteMove` | this component |
| relayed input | server → every client except the owner | the relay host actor's ring | `ASimulationInputRelay` |
| connection tier | server → the owning connection | a per-connection relay actor's property | `ASimulationConnectionRelay` |

Only the first two are carried by this component. The other two have their own actors, each for a reason of its own
(scheduling and owner-skip for the ring, one wire one tier for the tier; §5); this component links the first of those
actors and writes the second (§9, §10).

⚠ **Correction (R0, task 20a).** The game component's table said the correction state goes "server → owning client".
It does not: its lifetime registration carries no condition, so it replicates to every client the actor replicates to.
Every peer predicts every character and corrects each one against its own correction state.

---

## §2 The binding, and why an unbound notification is fatal

The component reaches the game only through an attached `ISimulatableOwnerBinding` (`m_binding`), set by
`attachBinding` and cleared by `detachBinding`. Through it, the component calls the game at these points:

- `OnRep_CorrectionState`, once the wire-format fence has passed: `onCorrectionStateAccepted` (the typed read of the
  state, and whatever the game logs and stamps there), then the correction callback the core bound
  (`setOnCorrectionStateReceivedCallback`);
- `ServerReceiveRemoteMove_Implementation`, once its fences have passed: `receiveRemoteMoveBundle`, the typed
  RPC tail (the reception coordinator, the delivery route);
- `onRelayedInputRingArrived`, after the owner-skip check: `noteRelayedRingArrived` (whatever the game stamps for a
  relayed ring), then the relayed-ring callback the core bound (§10);
- `attachInputRelayHost`, on the authority only: `makeRingFlushedCallback`, whose result the relay host calls after
  each flush that published entries (§10);
- `simulationKey`, the character's key in the game's simulation storage, which the owner-skip lines and the tier send
  log, and which the tier send's `check` compares with the coordinator's id (§9, §10).

**An OnRep or RPC with no binding attached is a `checkf`**, with game-neutral text, and it stays live in a Shipping
target that keeps checks (bUseChecksInShipping). A notification with no binding can only mean that the owning actor
never attached one, or attached it too late (§3); dropping it would hide a broken pawn and silently lose its corrections
or inputs.

**Why concrete, `final`, and neither `Blueprintable` nor `BlueprintSpawnableComponent`.** A game adds the component as
it is; nothing subclasses it, and plugin code can name it. An instance that no owning actor binds — dropped onto an
actor in the editor, say — would hit the `checkf` on its first notification, so the class offers no way to create one
outside C++. UHT accepts `final` after the class name.

---

## §3 Construction, the bind order, and the optimize pair

**Construction.** `SetIsReplicatedByDefault(true)`; the tick is off (`bCanEverTick = false`, which is also
`UActorComponent`'s default, stated so the "never ticks" property is visible at the class).

**The binding is attached in the owning actor's `PostInitializeComponents`, never in a `BeginPlay`.** On an Iris client
a replicated actor is spawned undeferred (`UNetActorFactory`, `SpawnActorAbsolute`), which runs
`PostInitializeComponents` and holds `BeginPlay` back for a remote-owned actor. The initial state's notifications then
fire, and only afterwards does `PostNetInit` run `BeginPlay`. An actor that arrives with a written buffer — every
existing character, for a client that joins or rejoins a running session — therefore receives its first
`OnRep_CorrectionState` before any `BeginPlay` of its components. A bind in `BeginPlay` would let that notification hit
the `checkf` of §2. The order between sibling components' `BeginPlay` and `EndPlay` is not a contract either: the actor
walks its owned components in set order.

**The optimize pair.** Every definition in the `.cpp` sits inside one `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair, the
`ON` at true end-of-file. These are non-template definitions in the plugin's own translation unit (the plugin module
compiles without unity), so they compile with the `OFF` state, as they did in the game component; nothing that moved
here changed its Development optimization state.

---

## §4 The wire-format compat fences

Both directions carry a version byte and both refuse rather than guess.

**Client side, `OnRep_CorrectionState`.** The sender's wire-format version is the first byte of the payload, which the
buffer's serializer reads into `getReceivedWireFormatVersion` before it trusts the rest. On disagreement with
`FSimulationStateSyncBuffer::kWireFormatVersion` the handler logs an Error, raises an on-screen build-mismatch toast,
latches `m_wireFormatMismatchDetected`, and returns. **Once latched, every later notification no-ops**, so a mismatched
peer can never drive correction logic with bytes it cannot parse. The latch fires on any disagreement with the version
constant, not on one particular pair of builds. The toast uses one stable key (`kWireFormatMismatchToastKey`), so a
repeat call replaces the message rather than stacking it; each character's component latches on its own, so a
mismatched client raises it once per character and still shows one message.

**Server side, `ServerReceiveRemoteMove`.** Same test, one extra case first: **an empty bundle is idle traffic, not a
mismatch.** The bundle builder writes its version byte lazily, with the first slot, so a client with no input in the
redundancy window sends a bundle with no bytes at all, and `getWireFormatVersion` reads that as version 0. Treating
it as a mismatch would log a false alarm every idle frame. On a genuine mismatch the server logs and returns. **It does
not disconnect the client**: disconnecting is the engine's connection path's business, not this fence's.

**The wording is the brawler's, on purpose.** Both Errors and the toast name the playtest README and the
archive folder of the one game that uses this component today. They moved verbatim with the code
(og-ue-simulationcomponentandmanagersplit ruling Q6); a game hook for the wording is a follow-on.

**The state channel's version is `kWireFormatVersion` in `CorrectionStateBufferCodec.h`, 6 today.** Its history: 1 → 2
for the applied-capture-tick reference; 2 → 3 when the ring-out sub-simulation joined the brawler's state composite;
3 → 4 and 4 → 5 when fields left the middle of the composite (og-netcode-v2-field-defects tasks 9 and 17); 5 → 6 when a
field left the composite's first slice. ⚠ **Correction (R0, task 20a).** The game component's rationale said **5**; the
constant is 6. The reasoning for each bump is written at the constant. The input bundle's version is
`FInputRedundancyBundle::kWireFormatVersion`, from the bundle codec.

---

## §5 Retired channels — what stood here, and why the absence is fenced

Seven absence tags in the two files mark where something retired stood, so that a reader about to re-add it meets the
fence at the line where it would be typed (guards G-01, G-02, G-04, G-05, G-08, G-09, G-10). They cannot live only in a
document: a reader about to *add* a line has no symbol to look up.

**`OnRep_CorrectionInput`.** It peeked the replicated input for a trace and forwarded the buffer to the core's
correction-input binding. The property, the notification, the trace and the binding were all retired together; the
reasoning lives at `SimulationNetSync::sendCorrectionAll`, where the writer half was.

**The `m_replicatedInputSyncedBuffer` lifetime registration.** Removed in the same edit as the member, and the pairing
is the point: a registration naming a member that no longer exists is a compile error and so cannot be forgotten, while
keeping the member and dropping only the registration leaves a silently-never-replicated field. Both halves go together.

*Fence coherence, which is the part a wire-format editor needs:* removing a property leaves **no hole and no stale
version expectation.** Lifetime registrations are per-property entries, not fixed wire offsets, and the surviving
property carries its own self-describing serializer (version byte, then a length, then only the used bytes). That is
why the state channel's version was deliberately **not** bumped for this removal. The input sync buffer type survived
that retirement in a client→server role as `m_clientToServerInputSyncedBuffer`; that role was retired too in
og-syncedInput-rework task 9 (2026-10-03), when nothing had called its accessor since the redundancy-bundle RPC replaced
it, and the member, its accessor, its typedef and the type went together with the core concept requirement that kept them.

**The relayed-input ring and the connection tier.** The ring was once a replicated property of the per-character
component, registered with a plain lifetime entry. It moved, with its notification and registration, to
`ASimulationInputRelay`, a per-character actor that is its own replication root: on the component the ring and the
correction state shared one atomic replication batch, so a round dropped under packet pressure lost both, and a
condition added on the component would have bought the skipped owner echo without removing that coupling. The tier
replicates from `ASimulationConnectionRelay`, whose owner-only relevancy does the narrowing an owner-only lifetime
condition used to do, one property per wire. The full accounts are §9 and §10, since task 20b moved the tier send and
the relay-host link here.

---

## §6 Mixed builds: no toast across this change

This component is a wire-format change in its own right, separate from the version bytes of §4: the replicated
property and the RPC moved from the game's component to this new default subobject (`SimulatableNetSync` on the
brawler's pawn; og-ue-simulationcomponentandmanagersplit ruling Q-D). **A client built before that change, connected to
a server built after it, shows no wire-format toast**, because no correction-state notification reaches its old
component: the server no longer replicates that component, and the client cannot bind the subobject it does not know
[I]. The opposite pair fails the same silent way. The matching-builds policy already covers both; the version bytes
guard only peers that share this layout.

---

## §7 Teardown under Iris

`UActorComponent::EndPlay` ends the component's replication (`EndReplication`, in Iris builds, unless the reason is the
editor's end of play or a quit). The order between this component's `EndPlay` and the game component's is the actor's
set order, not a contract (§3); before task 20a the correction state belonged to the game component and ended with it.
The game component's `EndPlay` unregisters from the session, then calls this component's `detachInputRelayHost`, then
its unbind, then its `Super`; this component's own `EndPlay` detaches the relay host only when no binding is attached
(§10, "Teardown"). Nothing reads the correction state between the two `EndPlay` calls, and they run in one actor
teardown with no network dispatch in between, so the order is unobservable [I].

---

## §8 Provenance (task 20a)

Moved verbatim from `USimmableUpdateComponent` (`Source/OGBrawlerUnreal/`, BUSL-1.1 then) by
og-ue-simulationcomponentandmanagersplit task 20a: the replicated buffer, `OnRep_CorrectionState`, the mismatch latch and
the toast key, `ServerReceiveRemoteMove` and its two fences, `GetLifetimeReplicatedProps`, the correction-state callback
pair and both buffer getters. Two statements replaced typed code at its position: the call to `onCorrectionStateAccepted`
where the OnRep read the state, and the call to `receiveRemoteMoveBundle` where the RPC's typed tail began. The two
`checkf`s and `sendInputBundleToAuthority` (the public send the game component calls in place of the RPC) are new. This
file's §1, §4 and §5 are that component's rationale §1, §8 and §9, R0-checked and made game-neutral.

| guard here | was (`SimmableUpdateComponent-guards.md`) |
|---|---|
| G-01, G-02 | G-36, G-37 |
| G-03, G-04, G-05 | G-09, G-10, G-11 |
| G-06, G-07 | G-17, G-18 |
| G-08, G-09, G-10 | G-32, G-33, G-34 |

---

## §9 The connection tier send (task 20b; was the game component's rationale §5)

The tier is a **wire** property, not a character property. Split-screen siblings resolve to the same root connection
(`GetRootNetConnection` walks a child connection to its parent) and therefore to the same `ASimulationConnectionRelay`
(`findOrSpawnForConnection` matches on the root connection): one wire, one tier property. That makes sibling starvation
structurally impossible on the transport side rather than merely guarded against.

**The owner-only condition is gone from the lifetime registrations.** The tier no longer replicates from a per-character
component at all; it replicates from `ASimulationConnectionRelay`, whose **owner-only relevancy**
(`bOnlyRelevantToOwner`) performs the narrowing the replication condition used to perform. The absence fence at that
site is G-10, in this component's `GetLifetimeReplicatedProps` (§5). ⚠ **Correction (R0, task 20a).** The game
component's account said another file in the tree cites that fence as evidence the condition is gone; the file that
cites a fence here (`SimulationManagerUImpl-rationale.md`) cites the ring's (G-09), not the tier's.

**`sendConnectionTierToOwningClient` is pure transport.** The core (`ServerReceptionCoordinator::noteRttSample`) has
already skipped a "no reading yet" sample (a negative round-trip time) and deduplicated per owner, and it calls this
method only when the tier for that owner actually changed. So there is deliberately no sentinel check and no
changed-versus-current test here. The null-root-connection early-out is defensive only: the one caller of
`noteRttSample` is the game's RPC tail (in the brawler, `receiveRemoteMoveBundle`), which takes its no-wire path on a
null root connection before it can reach the coordinator.

**This component is the sink.** The game's RPC tail passes this component to `noteRttSample` as its
`ConnectionTierSink`. The coordinator calls the sink at once and does not keep it. The `static_assert` beside the
definition makes a breaking signature change a legible assertion rather than an opaque template error at the call site.
The sink is per character because it must resolve id → owning wire, and this object reaches that wire through its
owner unconditionally. A manager's id → component map is filled at registration, several frames after the first input
RPC can arrive, and the core records a tier as published **before** it calls the sink, so a publish the sink could not
route would never be retried.

**The key.** The `check` compares the coordinator's `id` with the binding's `simulationKey`, and the `[ConnectionTier]`
line logs that key. Both were `toStorageKey(simCharacterId())` in the brawler component, which is what the binding's
`simulationKey` returns (since task 21 through the brawler's `storageKeyOfOwner` hook), so the line is unchanged.

---

## §10 The relay host link, the rings and the staging target (task 20b; was the game component's rationale §6)

### Why the ring is on its own actor

The relayed-input ring is a property of `ASimulationInputRelay`, one per character, registered
`DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing, COND_SkipOwner)`. It was once a replicated property
of the game's per-character component, registered with a plain lifetime entry and no condition (fenced here at G-02 and
G-09, §5).

**Two separate gains, and the smaller one is the famous one.** The condition skips the echo of a character's own input
back to its owner (about 85 bytes per connection per round, the figure recorded when the ring moved). But the *reason
for the move* is the other gain: on the component the ring and the correction state shared one atomic replication
batch, so a round the replication system dropped under packet pressure killed both together. On its own dependent
object the ring is scheduled and prioritised independently. **Adding the condition on the component would have bought
the bytes and left the coupling in place**, which is why the ring's absence fence (G-09) says so explicitly.

### The link

This component holds the character's link to its host: `m_inputRelayHost`, a weak pointer, because the host is
destroyed with the character and the teardown order between the two is not guaranteed. `attachInputRelayHost` sets it
and `detachInputRelayHost` clears it; `getInputRelayHost` reads it for a game that counts on the host.

`attachInputRelayHost` is reached from every path on which a game meets the host. In the brawler there are three (the
authority's own spawn and the client's pull at registration, and the manager's listener when the host replicates in),
each through the forwarder the brawler component inherits from its binding base (`UESimulatableOwnerBinding-rationale.md`
§2, since task 21). It is idempotent: a null host or the host already linked returns at
once, and the repeats are the design, because the host and the character come up in an order nothing controls. On
link it installs a **weak** capture of this component as the host's arrival callback (G-14), since the host can
outlive this component by a frame during teardown and a raw `this` would then be a dangling call from a replicated
notification [I]. On the authority it then installs the binding's ring-flush callback (`makeRingFlushedCallback`),
which the host calls from its `PreReplication` after a flush that published entries, and only if the callback is set.
Last, it **replays the current ring** to the core's callback, which covers the ordering where the host replicated and
dropped one or more notifications before the link existed (the host counts those as unrouted). A never-written ring
reads as version 0 and the core's ingest no-ops, so the replay is free on the authority and on a freshly spawned host.

### The two fallback rings, and the staging target

Both are `UPROPERTY(Transient)` members of this component, never replicated.

- `m_detachedRelayRing` is the **read** fallback. The core reads the ring once at registration and its concept types
  the read as a reference, so `getRelayedInputRing` returns this never-written, always-empty ring when no host is
  linked (version 0, so the bind-time ingest no-ops).
- `m_detachedRelayStagingRing` is the **stage** fallback. `relayStagingTarget` returns the host's staging ring, or this
  one when no host is linked, and never the read fallback (G-13): the core reads the read fallback, so staging into it
  would let an unpublished burst read back as if it had replicated.

The game stages its typed arrival into the target and counts an overflow on the host; in the brawler that is
`stageRelayedInput`, a member of the binding template since task 21 (`UESimulatableOwnerBinding-rationale.md` §6).

### Teardown: who detaches, and why twice is safe

`detachInputRelayHost(isAuthority)` has one rule (G-12): **the authority destroys the host; a client only unbinds it.**
The authority calls `ASimulationInputRelay::detachFromParentAndDestroy`, which clears the arrival callback, removes the
Iris dependent-actor link (Iris builds only) and destroys the host. A client clears the host's arrival callback only,
because a replicated actor is removed on clients by the destruction bunch and destroying it locally would race that.
Either way the link is then cleared unconditionally, so **a second call finds no host and does nothing**: the detach
is idempotent.

The host needs no timer to find its end, unlike the connection relay, which polls a one-second reap timer because the
death of its owning `PlayerController` does not destroy it: this host has a synchronous signal, its character ending
play.

**Sibling order is not a contract (§3), so an attached binding drives the teardown.** The game component's `EndPlay`
unregisters from the session, then calls `detachInputRelayHost`, then unbinds. This component's own `EndPlay` detaches
only when **no binding is attached** (G-11). That covers an instance no owner ever bound, and an instance that ends
after its binding unbound, where the call finds the host already cleared. An instance that ends **first** leaves the
host to its binding, so the host outlives the game's unregister, as it did before task 20b.

Measured in task 20b (the G-11 POISON session, with a temporary log line in both functions): at the harness's
leave, on the server and on the remaining client alike, this component ended play **first**, still bound. With the
check removed it detached the live host there (the server destroyed it ahead of the game's unregister), and the game
component's own call then found the host cleared and did nothing; the session stayed clean (0 asserts or ensures) only
because the detach is idempotent. So in the default shape the check is what defers the detach to the binding.

### The owner-skip precondition

The owner-echo removal rests on an equivalence that nothing checked: **"owning connection"** — the set the replication
condition narrows on, decided server-side from the host's owner chain — and **"provider present"** — the set that
decides whether the core builds a relay store, decided client-side from the local role. If those two ever name
different sets the failure is silent in *both* directions: a character in the first set but not the second still pays
the echo, and a character in the second but not the first loses its relayed input entirely.

`onRelayedInputRingArrived` asserts the observable half. A ring with resident entries arriving at a character that
**has** a local input provider means the skip did not apply where this build believes it does — most likely an unset
or wrong owner on the host. Nothing consumes the payload either way (the core binds the relayed-ring callback only for
a provider-absent character), so this is pure diagnosis. The game supplies the provider decision through
`setHasLocalInputProvider`, at the point where it registers the character; the check reads that member.

Three deliberate choices in that check, each with a guard at the site:

- **Gated on `num() > 0`** (G-15). An empty ring is not evidence. The host actor is `bAlwaysRelevant` and *does*
  replicate to the owning client — only the property is skipped — so an owner legitimately holds a host carrying a
  never-written ring. Asserting on that would fire on every clean run.
- **`ensure`, not `check`** (G-16). This is a bandwidth and correctness regression, not a memory-safety fault, and a
  session that hits it should stay playable enough to be diagnosed. `m_loggedOwnerSkipDivergence` makes it one-shot.
- **The provider decision is latched at registration, not recomputed at arrival** (G-15), so the two halves of the
  comparison are literally the same evaluation taken once.

After the check the component calls the binding's `noteRelayedRingArrived`, then the core's callback, in that order.

---

## §11 Provenance (task 20b)

Moved verbatim from `USimmableUpdateComponent` by og-ue-simulationcomponentandmanagersplit task 20b: `m_inputRelayHost`,
the two `UPROPERTY(Transient)` rings, the provider flag and the divergence latch, both `getRelayedInputRing`
overloads, the relayed-ring callback pair and its member, `attachInputRelayHost`, `onRelayedInputRingArrived`, the
G-12 staging choice (now `relayStagingTarget`), the relay detach (now `detachInputRelayHost`), and
`sendConnectionTierToOwningClient` with its `ConnectionTierSink` `static_assert`. Four statements replaced typed code
at its position: `m_binding->makeRingFlushedCallback()` where the flush lambda was written, `m_binding->noteRelayedRingArrived(ring)`
where the relayed-ring stamp block was, and `m_binding->simulationKey()` for `toStorageKey(simCharacterId())` in the
tier send's `check` and log and the owner-skip lines. `getInputRelayHost`, `setHasLocalInputProvider` and the `EndPlay`
override are new. This file's §9 and §10 are that component's rationale §5 and §6, R0-checked and made game-neutral.

| guard here | was (`SimmableUpdateComponent-guards.md`) |
|---|---|
| G-11 | — (new: the unbound-only detach in `EndPlay`) |
| G-12 | G-08 |
| G-13 | G-12 |
| G-14, G-15, G-16 | G-14, G-15, G-16 |
