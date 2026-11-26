<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: UEConnectionHandle.h -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: PreReplication -- Unreal Engine replication hook name, outside every scan root -->
<!-- lint-external-ref: noteStageOverflowDrop -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulatableOwnerBinding -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: InputRedundancyBundleBuilder.h -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: ServerReceiveRemoteMove -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: SimulatableNetSync -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: SimulatableNetSyncComponent-rationale.md -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: USimulatableNetSyncComponent -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: attachInputRelayHost -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: bindNetSync -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: getInputRelayHost -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: isNetSyncBound -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: makeRingFlushedCallback -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: noteRelayedRingArrived -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: onCorrectionStateAccepted -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: onRelayedInputRingArrived -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: receiveRemoteMoveBundle -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: relayStagingTarget -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: simulationKey -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: unbindNetSync -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: UESimulationSession::relayRemoteInput -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- ================= DECLARED LINT ESCAPES ==================================
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root -->
# `UESimulatableOwnerBinding.h` — rationale

Companion to `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulatableOwnerBinding.h`. The header carries only code,
assertion messages, log strings, the docs pointer and one-line tags `⛔G-nn` → an entry in
`UESimulatableOwnerBinding-guards.md`.

> **Licence.** MPL-2.0 (og-ue-simulationcomponentandmanagersplit ruling Q1). Created by that initiative's task 20a
> (2026-10-21) with the non-template base only; task 21 (2026-10-21) added the CRTP template and moved the typed half of
> the brawler's component into it, with its narrative (§3–§6, from the game component's rationale, made game-neutral).

**Since og-input-latency task P5-8 the binding is the registration skeleton and nothing else** (§10). Its net half — the
non-template interface the net-sync component called through, the owner-concept surface, the RPC tail, the stamp blocks,
the two per-packet lines and the staging write — had no caller since task P5-7 moved every sim payload to og-net, and P5-8
deleted it together with the net-sync component. §1 and §3–§6 are kept as the history of that half.

* **§1** is the non-template base, `ISimulatableOwnerBinding` (history: deleted by P5-8).
* **§2** is the template: its shape, its members, the key hook and the checks the compiler makes.
* **§3** is the RPC tail: primitives in, policy out (history: retired by P5-7, deleted by P5-8).
* **§4** is the five latency-budget stamp blocks (history: deleted by P5-8).
* **§5** is the two per-packet log lines (history: deleted by P5-8).
* **§6** is the staging write (history: deleted by P5-8).
* **§7** is how it is built and what that does to the code.
* **§8** is the provenance.
* **§9** is the registration skeleton (task 29): the two polling loops, the four registration hooks, the attempt budget,
  provider presence, the spawn order, the provider latch and the delivery route.
* **§10** is the deletion of the net half (og-input-latency task P5-8).

---

## §1 `ISimulatableOwnerBinding` — the one way the wire component reaches the game

> **History.** og-input-latency task P5-8 deleted this base with the net-sync component that called through it (§10). The
> text below describes it as it stood until then.

`USimulatableNetSyncComponent` (`SimulatableNetSyncComponent-rationale.md`) owns a character's reflected wire surface
and must stay free of game types. It reaches the typed game side only through this base, held as a pointer that the
owning actor attaches when it initializes its components. A non-template `UCLASS` can reach a template only through such
a non-template base; this is the same shape as the relay host's listener interfaces in this module.

**Its members** (three since task 20a, five since task 20b):

| member | called by the sibling | what the binding supplies (§2) |
|---|---|---|
| `simulationKey` | the tier send's `check` and `[ConnectionTier]` line, and the owner-skip lines | the game's storage key, through the game's key hook |
| `onCorrectionStateAccepted` | `OnRep_CorrectionState`, after the wire-format fence, before the core's callback | the typed read of the correction state, `[ReceiveCorrectionState]`, and the `ClientReceived` stamp with the `H6L` heal by correction (§4) |
| `receiveRemoteMoveBundle` | `ServerReceiveRemoteMove_Implementation`, after its fences | the typed RPC tail (§3); since og-input-latency task P5-7 a one-shot `Warning` and nothing else, because inputs arrive on og-net |
| `noteRelayedRingArrived` | `onRelayedInputRingArrived`, after the owner-skip check, before the core's callback | the probe's `ClientReceived` stamps for a relayed ring |
| `makeRingFlushedCallback` | `attachInputRelayHost`, on the authority only | the callback the relay host calls after a flush that published entries (the probe's `Handed` stamps) |

All five are **private** pure virtuals, and only the sibling can call them: `friend class USimulatableNetSyncComponent;`
is the base's only friend. Nothing else in a game, the plugin or the core can call through the base by accident.

**The destructor is protected and non-virtual.** The base is never deleted through: the sibling only calls through its
pointer, and the game component that implements the base is a `UObject`, destroyed as itself by the engine. A
`static_assert` in the header pins both properties (not destructible through the base, no virtual destructor).

**No export macro.** The base and the template are header-only with no out-of-line non-template member, so there is
nothing to export; neither carries `OGSIMULATIONUNREAL_API`.

---

## §2 `UESimulatableOwnerBinding<OwnerT, TraitsT>` — the typed half, as a CRTP secondary base

**The shape.** A game component is a `UCLASS` and so cannot be a template, but it can *inherit* one: its base list is
the engine's component class first, then `UESimulatableOwnerBinding<OwnerT, TraitsT>` with `OwnerT` the component
itself and `TraitsT` the game's traits struct. The header tool takes the first base as the reflected super class and ignores a further base that is not reflected; the
build tool then compiles it as plain C++. Two cautions: the base sits **after** the `UCLASS` super, and **no `>>`**
appears in the base list (the design's caution: the header tool's tokenizer matches single `>` tokens; not tested
here). Task 21's first step built exactly
this, with the template empty, in the Jolt Editor (header tool and build), the Chaos Editor and the Development Game and
Server targets before anything moved into it (§8).

**What it supplies** (since og-input-latency task P5-8): the registration skeleton of §9, and a protected,
non-virtual destructor. The binding has no public member and no virtual member, and it is not the core's owner of the
character: the core's `SimulatableOwnerTraits` names the character's og-sim-net channel for both roles since task P5-7.

| part | members |
|---|---|
| the registration skeleton (task 29, §9) | the protected `beginRegistration()` (the game component's `BeginPlay` calls it); the private loops `tryInitializeWithManager` and `tryRegisterWithNewFramework`, their next-tick reschedulers, the two attempt counters and `kMaxRegistrationAttempts`; the game supplies four private hooks and the key hook |
| the destructor | protected and non-virtual: the game component is a `UObject`, destroyed as itself by the engine, and nothing holds it through the binding |

Until P5-8 it also derived from `ISimulatableOwnerBinding` (§1) and was the whole owner-concept surface the core expected of
the game component: the five interface overrides, the owner-concept aliases, the forwarders onto the net-sync sibling, the
typed owner-concept members, the sibling pointer, the five stamp blocks and the stage (§10 lists them).

**The key hook (review B-2, ruling Q-F).** The game component's `storageKeyOfOwner()` returns its storage key; loop two's
session-owner `checkf` names the owner by it. Until P5-8 the interface's `simulationKey()` override returned it, and every
override was `final`, so a game member named like an interface member was a compile error (`C3248`) rather than a silent
override; with the interface gone the binding has nothing a game member could override.

**The hook check is an inline requires-expression in a binding member body.** The hooks are private members of the game
component, which declares its binding specialization a friend. Access
in a requires-expression is checked in the context where it appears, so a namespace-scope `concept` cannot see a private
hook; the check is a `static_assert` of a requires-expression inside `tryRegisterWithNewFramework`'s body (inside
`simulationKey()`'s until P5-8), which the friendship covers, and it fires when that body is instantiated. Its message names
the hook, the ruling and the site it moved from. A game component without the hook fails there with that message, ahead of
the plain lookup error.

**The traits** are constrained by the concept `UESimulatableOwnerBindingTraits`: the types the template names
(`Simulatable`, `PlayerInput`, `State`, `SessionOwner`). `OwnerT` is incomplete where the base list instantiates the
class, so nothing about it is checked there: every member that reaches into `OwnerT` does so in a function body, through
`derived()` (a `static_cast` to `OwnerT&`). (Since P5-8 the binding itself names neither `PlayerInput`
nor `State`; the concept still requires both.)

**The session owner.** The registration skeleton reaches the session owner of its world's role through
`TraitsT::SessionOwner::instanceFor`, and from it the session (`session()`). Until og-input-latency task P5-7 the RPC tail
reached the authority's session the same way for the reception coordinator, the server reception tick and the two sinks
(§3), and until P5-8 the stamp blocks reached the latency member through `UELatencyBudgetDiagnostics<TraitsT>::withInstance`
(§4).

---

## §3 The RPC tail — primitives in, policy out

> **Retired by og-input-latency task P5-7.** Inputs reach the server on og-net's SimInput lane, where og-sim-net's server
> runs the same core calls (one `noteRttSample` per bundle, then `receiveInputBundle`) with the og-net connection and the
> roster's local index (`SimNetServer-rationale.md`). The net-sync RPC has no caller (the core's input sender is the
> character's og-sim-net channel), so `receiveRemoteMoveBundle` logs a one-shot `[ServerReceive]` `Warning` if a bundle
> arrives on it anyway and returned; its guards G-02 … G-06 are retired. The text below is the tail as it stood until P5-7;
> P5-8 deleted the function with the binding's net half (§10).

`ServerReceiveRemoteMove` is a thin transport adapter. The RPC is declared on `USimulatableNetSyncComponent`, which runs
its wire-format fences and then calls `receiveRemoteMoveBundle` here. Everything below the fence is engine-primitive
acquisition plus a forward into the engine-agnostic `ServerReceptionCoordinator`. **No netcode policy lives here**
(G-02): the tier derivation and its smoothing, capture-tick dedup, park and drain, the malformed-slot fence, and the
per-slot receive loop itself are all core. This side resolves only what a second engine would also have to supply — the
root connection, the player slot, the round-trip time, the server simulation tick — plus the wire buffer.

**The order is documented and load-bearing:** the wire-format fence, then one RTT sample, then the core per-slot loop.

**Resolve the root connection once** (G-03). Split-screen siblings collapse to the single wire entry their shared
connection deserves; both the tier and the slot key derive from it. A null connection is the documented "no wire
identity" sentinel (`UEConnectionHandle.h`): standalone, or a listen server's local pawn.

**The no-wire fallback boundary is adapter-side** (G-04). With no coordinator or no wire, every slot takes the undelayed
delivery path directly — the remote-move callback, with a `[ServerReceive]` line per slot (§5) — without the
coordinator, so **the core only ever sees a valid wire.**

**One RTT sample per bundle, never per slot** (G-05). A bundle is one datagram, one arrival event; sampling per slot
would feed the same reading into the tier smoothing up to `kMaxSlots` times and couple it to redundancy depth.
`noteRttSample` *drives the send itself*: it derives the tier and, on a change for this owner, calls
`sendConnectionTierToOwningClient` on the sink, which is the sibling (`SimulatableNetSyncComponent-rationale.md` §9).
The sibling satisfies `ConnectionTierSink`; the concept is compile-checked at the `noteRttSample` call site, and a
`static_assert` beside the sibling's definition makes a breaking signature change a legible assertion. The coordinator
calls the sink at once and does not keep it.

**The slot primitive is which local player on that wire.** The tier keys on the root connection, because latency is a
property of the link; input keys on the slot as well, because input is per character.

**The delay is applied exactly once, park-to-release** (G-06). The core's receive loop parks each slot in the
`ServerInputDelayQueue`; a released input keeps its **original** capture tick, and the authority's consumer —
`SimulationInputResolution::collectInputAll`, authority branch — pops the `RemoteMoveQueue` in **arrival order**, so it
contributes no offset of its own.

**The last two arguments are the delivery sink and the relay sink**, here the same session-owner object bound to two
boundaries: delivery routes an input *into* the simulation, the relay forwards it *out* to the other clients.

**The id → component delivery route** that the coordinator's drain and its malformed-slot deliver-now fallback resolve
against is the session owner's, registered once per character at registration and erased at unregistration, on the
authority only; the coordinator, the delay queue and the drain do not exist on a pure client. The game's registration
code owns that pairing (the brawler: its component's and manager's rationale).

---

## §4 The latency-budget stamps

> **History.** og-input-latency task P5-8 deleted all five blocks with the binding's net half (§10). Since task P5-7 the
> Unreal sim-net hosts stamp these points (`UESimNetHost.h`: `stampHanded`, `stampServerReceived`, `stampClientReceived`).
> The text below describes the blocks as they stood until P5-8. The diagnostics' game-thread doors it names (stamp and the
> correction's) were deleted by og-session-runtime task 14.
<!-- lint-external-ref: UELatencyBudgetDiagnostics::stampCorrectionReceived -- deleted by og-session-runtime task 14 (no caller since task 12); section 4 records the binding's blocks as they stood until og-input-latency task P5-8 -->

Five blocks in this header stamp the latency-budget probe. Each is a timestamp and nothing else: it reads values the
code beside it already has, writes only the probe, and changes no input, no wire byte and no ordering of the netcode it
sits next to. The template's own stamp points, their threads and errors are `UELatencyBudgetDiagnostics-rationale.md`
§6; the game's hop table is the game's documentation. This list only says what each block does here.

| member | stamp | role |
|---|---|---|
| `sendLocalInputToAuthority`, after the send | `Handed` for every capture tick in the bundle | client |
| `receiveRemoteMoveBundle`, which the sibling's RPC calls after its fences | none since og-input-latency task P5-7 (it was `ServerReceived` for every capture tick in the bundle, and one `WIRE` sample per bundle when there was a root connection; the server host stamps both since) | authority |
| `onCorrectionStateAccepted`, which the sibling's `OnRep_CorrectionState` calls; after the `[ReceiveCorrectionState]` line | `ClientReceived` for the correction's tick, and `Healed` for the character's pending relayed captures the correction contains, both through `UELatencyBudgetDiagnostics::stampCorrectionReceived` (og-input-latency task 6) | client |
| `noteRelayedRingArrived`, which the sibling's `onRelayedInputRingArrived` calls after the owner-skip check, before the core callback | `ClientReceived` for every capture tick in the ring other than the `kNoInputCaptureTick` sentinel | client |
| the callback `makeRingFlushedCallback` returns, which the sibling's `attachInputRelayHost` binds as the relay host's flush callback, authority only | `Handed` for every capture tick a flush published, other than the sentinel | authority |

All five ran on the game thread. Every stamp went through the diagnostics' game-thread stamp door, `stamp`, or, for the
correction, `UELatencyBudgetDiagnostics::stampCorrectionReceived` (both deleted by og-session-runtime task 14), each of which
`checkf`ed the game thread, and every block reached it through `UELatencyBudgetDiagnostics::withInstance`, which runs the
block on the role's instance (the session owner's `editLatencyBudgetDiagnostics()`) when there is one. Where
`OG_LATENCY_BUDGET_DIAGNOSTICS` is 0 (Shipping) `withInstance` is an empty function, so the owner lookup each block
makes only to stamp is not made (`UELatencyBudgetDiagnostics-rationale.md` §3). The relay host's flush callback is still
bound on the authority; its body is the one `withInstance` call. The callback holds the game component weakly
(a weak object pointer to `OwnerT`), so a flush after the component is gone stamps nothing. Repeated capture ticks (redundant
bundle slots, repeated relay entries) are expected: the probe counts them as duplicates (the duplicate field of
`[LatencyBudget.Window]`).

**G-02's order is unchanged by the stamps.** The `ServerReceived` loop and the `WIRE` sample sit after the fence and
before the tier's RTT sample and the core per-slot loop. They are measurement, not netcode policy: neither feeds the
tier, the delay or the queue, and the `WIRE` sample goes to the probe only (G-05's one-sample-per-bundle rule is about
the tier's sample; this is a separate one, also once per bundle).

---

## §5 The two per-packet log lines

> **History.** og-input-latency task P5-8 deleted both emitters with the binding's net half (§10). Since task P5-7 a
> client's `[ReceiveCorrectionState] id=%u tick=%u` is og-sim-net's client session's line (`SimNetClient.h`), with the same
> text. The text below describes the two lines as they stood here until P5-8.

Two lines in this header are logged with `OG_LOG_ROUTED`, not with `UE_LOG` at a fixed level:

* `[ReceiveCorrectionState] id=%u tick=%u` in `onCorrectionStateAccepted` (client, one per accepted correction);
* `[ServerReceive] id=%u tick=%u` on the no-wire path of `receiveRemoteMoveBundle` (authority, one per bundle slot), until
  og-input-latency task P5-7; since then that function writes one `[ServerReceive] id=%u: …` `Warning` at most, with
  `UE_LOG`.

Their level is their row's `plainLevel` in `kOGLogRoutes` (`UESimulationLogRoutes.h`): `Verbose` for both, so
`LogOGNet=Log` does not print them and `LogOGNet=Verbose` does. The rule and the table are
`UESimulationLogRoutes-rationale.md` §6. On the wire path the same `[ServerReceive]` tag is the core's: the
coordinator's per-slot loop logs it through its logger and the router, so a log carries the tag on both paths.

---

## §6 The staging write — `stageRelayedInput`

> **History.** og-input-latency task P5-8 deleted the function and its guard G-01 (§10). Since task P5-7 og-sim-net's
> server stages each character's relays in a per-character `RelayRingStager`, which counts a stage overflow as
> `stageOverflowDrops=` on its `[RelayFlush]` line. The text below describes the write as it stood until P5-8.

The session's relay tap (`UESimulationSession::relayRemoteInput`, the session owner's until og-ue-simulationcomponentandmanagersplit task 26a) calls `stageRelayedInput` on the
character's component. It takes the host and the staging target from the sibling (`getInputRelayHost`,
`relayStagingTarget`, whose choice is the sibling's G-13) and puts the arrival in the target. Two reads of the same weak
host pointer on the game thread, with no write between, agree. The host's `PreReplication` publishes the whole staged
burst once per replication poll, so two arrivals in one server frame both reach the wire instead of the second
overwriting the first.

It takes **no depth parameter**, and that absence is the fence: a depth passed down here would cap every round at one
entry and silently restore the replace-latest behaviour the staging exists to remove, with no compile error anywhere. A
`static_assert` at the top of its body pins its exact signature (as a member of this template); its message names the
prose fence and the game component's assertion it replaced. The return value's `droppedOldest` means the burst exceeded
`relayedInputRing::kMaxDepth` in one frame and has already been counted on the host.

A burst longer than the stage drops its oldest entry, and that drop is **counted on the host** rather than absorbed
(G-01): it is the one input loss this side of a zero-redundancy configuration can actually see. It is counted on the host
because the host is what a run inspects, and because a drop against the sibling's no-host fallback is already covered by
the much louder fault that no host exists at all.

---

## §7 How it is built

**Header-only, instantiated by the game.** The template's out-of-line members are defined in this header between an
`OGSIM_OPTIMIZE_OFF`/`ON` pair and are instantiated only in the game module (the one-module rule: no plugin object code
instantiates them). The brawler declares its one specialization with an explicit instantiation declaration in its component's header
and defines it once, with an explicit instantiation definition, in the component's `.cpp`, so the out-of-line members are compiled in
that one translation unit, where the session owner's type is complete, and in no other that includes the component's header
(its generated reflection code among them). Until og-input-latency task P5-8 those members included the interface's five
virtual overrides, and the in-class one-liners (the forwarders) compiled wherever they were used; the template has neither
since.

**Optimization (og-ue-simulationcomponentandmanagersplit lead ruling F1).** MSVC compiles a template instantiation with
the `#pragma optimize` state in force at the end of the instantiating translation unit, and every game `.cpp` ends with
`OGSIM_OPTIMIZE_ON`; so the out-of-line members compile optimized in Development, although the pair is there. The pair
documents intent. Task 21's notes list the functions whose Development state moved from unoptimized to optimized.

**Includes** (since og-input-latency task P5-8). The template needs the engine's timer manager (the next-tick
reschedules), the latency template (the `InputProvider` type loop two hands the session), the log routes (`LogOGMgmt`, the
category of loop two's lines), the session's header, for `TryRegisterStatus`, the namespace-scope enum the skeleton tests
the session's answer against (a non-dependent name, so it must be declared where the template is defined), and og-sim's
compiler-control header for the optimize pair.

Until P5-8 it also included the sibling, the redundancy bundle builder, the relay host (`noteStageOverflowDrop`, and from
task 29 to P5-7 its spawn and find), the connection helpers, the input relay's ring and og-sim's queue and codec headers,
for the net half. The sibling's `.cpp` included this header for the base's complete type, and so from task 29 on it also
compiled the session's header; that added no plugin object code, because no template member was instantiated there and the
three og-sim inline variables it carried (`simlog::g_sink`, `simulationLog::detail::t_integrateScope`,
`correctionFieldDiff::g_enabledPredicate`) were already in the plugin through the latency template's and the session's own
`.cpp`. With the sibling deleted, no plugin translation unit includes this header.

---

## §8 Provenance

* **Task 20a** created the header with `ISimulatableOwnerBinding` (three members); **task 20b** added the last two.
* **Task 21** (2026-10-21):
  * **Step 0, the spike:** the brawler's base `public ISimulatableOwnerBinding` became
    `public UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>` with the template empty; the
    Jolt Editor (header tool and build), the Chaos Editor and the Development Game and Server targets built.
  * Then the typed half moved in, statement for statement, from `OGBrawlerSimulatableComponent.h` and `.cpp`: the five
    overrides (now `final`), the forwarders (generic; the `const` buffer getter now reaches the sibling's `const`
    overload), the typed owner-concept members, the sibling pointer with `bindNetSync`/`unbindNetSync` and the new
    `netSync()`/`isNetSyncBound()`, the aliases, the five stamp blocks and the stage with its signature assertion. The
    brawler keeps `storageKeyOfOwner()` (its old `simulationKey()` body). Its guards G-13 and G-19 … G-23 are G-01 …
    G-06 here; its rationale §6 ("Staging, not writing"), §7, §14 and §15 are §6, §3, §4 and §5 here.
  * **R0 corrections made on the way:** the moved G-22 site line still named `*this` as the tier sink (the sibling
    since task 20b; G-05); the moved §7 said "Authority only" twice for the delivery route and placed its pairing in
    this code (it is the game's registration, §3); the moved §15 did not say the core logs the same `[ServerReceive]`
    tag on the wire path (§5); the moved §14 pointed at the game's hop table for the template's stamp points (§4 now
    points at the latency template's §6).
* **Task 29** (2026-10-22): the brawler component's registration loop became the skeleton of §9, statement for statement
  (`BeginPlay`'s next-tick call, `tryInitializeWithManager`, the two reschedulers, `tryRegisterWithNewFramework`, the
  counters and the bound), with the brawler's statements left in four hooks of the component. Its guards G-02, G-03,
  G-04, G-06 and G-07 are G-07 … G-11 here; its rationale §3, §4, the attempt-budget paragraph of §2, the spawn-order and
  provider-latch parts of §6 and the delivery-route part of §7 are §9.2 … §9.6 here, game-neutral (the brawler's own
  reasons stay in its component's rationale, §19 there).
  * **R0 corrections made on the way:** the tier consumer is emplaced on both roles, by the session's `beginPlay` (G-08;
    "client-side", "the manager's `BeginPlay`" were stale); the latched provider decision and its guard are the sibling's
    since task 20b (G-09); the delivery route is also what `relayRemoteInput` resolves the staging write against (G-11,
    §9.6); the role-test entry's slot is the session's registry, filled by the session's `beginPlay` (G-07).

---

## §9 The registration skeleton (og-ue-simulationcomponentandmanagersplit task 29, ruling Q-M6)

### 9.1 The shape: two loops, four hooks, three outcomes

The game component's `BeginPlay` calls the protected `beginRegistration()` after its super call; everything after that
is the binding's, on the game thread, one attempt per frame:

```
beginRegistration                schedule loop one for the next tick
tryInitializeWithManager         until the session owner exists (TraitsT::SessionOwner::instanceFor); then, if the
                                 component has an owner, the hook initializeWithManager(owner of the session, owner
                                 actor): Ready, or Retry (again next frame); then schedule loop two
tryRegisterWithNewFramework      until the session owner exists; then the hook resolveRegistrationKey(owner of the
                                 session, isAuthority): ready(storage key), retry() or abandon(); the local-prediction
                                 test; makeInputProvider(key) for a locally predicted owner only; makeSimulatable(); the
                                 session's tryRegister, again next frame while it answers Pending, and an Error and
                                 the end on Refused; then the `NewFramework: registered` line
```

**The four hooks** are private members of the game component, which declares its binding specialization a friend
(task 21). They hold only game statements, and each sits where those statements stood:

| hook | returns | what the brawler puts there (its component's rationale §19) |
|---|---|---|
| `initializeWithManager(manager, owner)` | `ueSimulatableOwnerRegistration::Initialization`: `Ready` or `Retry` | the input-collection cache, the target query volume (`Retry` while the Jolt slot waits) and the three visualization states |
| `resolveRegistrationKey(manager, isAuthority)` | `ueSimulatableOwnerRegistration::ResolvedKey`: `ready(key)`, `retry()` or `abandon()` | the character cast, the authority's id allocation (`abandon()` on a refusal) and the replicated id's wait (`retry()`) |
| `makeInputProvider(key)` | the latency member's `InputProvider` (`UELatencyBudgetDiagnostics`) | the provider lambda over the input collection and the id |
| `makeSimulatable()` | `TraitsT::Simulatable` | the simulatable built from the component's static data |

**The outcome types are the skeleton's vocabulary, not flags.** `Initialization` has no `Abandon`, so a game cannot
abandon loop one by accident; the loop it could not abandon before stays so. `ResolvedKey` carries its key only through
`ready`, and the skeleton reads `key` only after both other answers have returned. Every retry answer is handled by the
skeleton, with the same attempt check, `checkf` and reschedule the game code used to write at each site (§9.3).

**The hook checks are inline requires-expressions** in the two loop bodies, as for the key hook (§2): each loop's first
statement is a `static_assert` over the hooks it calls, with their parameter and return types; a namespace-scope concept
cannot see private hooks. Deleting a hook fails there, with the message naming the hooks and the ruling (task 29 saw both
fail: `makeSimulatable` deleted, and `initializeWithManager` deleted, `C2338` each).

**Hook names.** Until og-input-latency task P5-8 the interface's five members were the binding's `final` overrides, so a
game member with an interface member's name and signature was a compile error (`C3248`; task 29 saw it for
`simulationKey()` and `onCorrectionStateAccepted()`), and the four hook names were chosen unlike all five. Since P5-8 the
binding declares no virtual member, so no game member can override one of its members.

**Which session door the skeleton calls.** The session's own: `regManager->session().tryRegister(key, …)`, with the storage
key the hook resolved (and, until og-input-latency task P5-7, the session's delivery-route registration after it). The brawler's
session owner keeps its two wrappers that take the game's character id (task 28); the skeleton does not use them, because
a generic skeleton has no game id type. The wrapper's one extra statement, a `checkf` that the id is not the null id,
guarded a condition the hook's `retry()` already excludes on this path.

### 9.2 Reaching the session owner: two polling loops, and why both poll

The component's `BeginPlay` cannot do the work, because the session owner's `BeginPlay` and the character's race and
neither order is guaranteed. So `beginRegistration` schedules loop one for the next tick and returns.

**Loop one** waits for the session owner to exist, and for the component to have an owner. Then it runs the game's
per-character initialization (`initializeWithManager`); a hook that answers `Retry` is called again next frame, and the
skeleton schedules loop two only after a `Ready`. A game that registers a query volume through the session's
`registerVizVolume` waits here in the Jolt configuration, where the volume needs the character's physics slot and the
session answers "not yet" until it can acquire one. The wait is logged by the slot table of the session's Jolt host
(og-sim's `TickSlotTable`, a `[SimHost.Slot] wait=…` line naming the slot and its holder); the retry `checkf` keeps the
shipped phrase "the manager logs", the session owner owning that host.

**Loop two** waits for the session owner, then for the game's key (`retry()`), then for the character's bodies to become
resolvable — the session's `tryRegister` answers `Pending` until it has created them and the physics adapter resolves
every one — and on success logs `NewFramework: registered`. On a client the game's key hook also waits for the character's
og-sim-net roster entry (the session's `clientBindStep`, since og-input-latency task P5-7). A `Refused` (og-sim-net's server
refused the character) ends loop two with an `Error`. Until P5-7, success also latched the provider decision on the sibling,
spawned or found the relay host and registered the delivery route (§9.5, §9.6).

Every step of both loops can legitimately answer "not yet", repeatedly, and "not yet" is never an error. Only exhausting
`kMaxRegistrationAttempts` is. A game's `abandon()` is not "not yet": it ends loop two for good, with nothing logged by
the skeleton (the brawler's refusal is logged where it is decided).

### 9.3 The attempt budget

`kMaxRegistrationAttempts = 600` bounds both loops. Each attempt is one `SetTimerForNextTick`, i.e. one **game frame**,
so the budget is ten seconds only at 60 fps (2.5 s at 240 fps). Long enough for scene start-up on a slow load, short
enough that a genuine wiring bug fails loudly rather than hanging forever. Both loops count against it independently
(`m_initializationAttempts`, `m_registrationAttempts`), and every wait ends in a `checkf` rather than a silent give-up.
Within loop one the session-owner wait and the hook's retries share `m_initializationAttempts`; within loop two the
session-owner wait, the key's retries and the `Pending` wait share `m_registrationAttempts`.

The next-tick delegates are weak lambdas on the component (`&derived()`): a component destroyed before the next frame
cancels the call. The lambda captures the binding's `this`, the same component, and calls the binding's loop.

### 9.4 Provider presence, and what absence buys

A **local input provider** is a callable the component hands to the core at registration. It is made for exactly one
kind of owner: on a pure client, the locally controlled one (`ROLE_AutonomousProxy`). Every other owner — every simulated
proxy, and every owner on the authority — registers with an empty provider (G-09).

That is not a convenience null-check. **Provider presence is the identity test**, and the core branches on it in
`SimulationNetSync::registerPredictionOwner`:

- **provider present** → `SimulationInputResolution::registerLocalCharacter` builds the local delay line, and the core
  adds a sender-map entry so this owner's captures go on the wire.
- **provider absent** → `SimulationInputResolution::registerRemoteCharacter` builds a neutral-seeded remote input store
  for the id, and the core binds the relayed-input callbacks by id. The core then predicts that owner from the store
  through the scheduled read, instead of extrapolating the last correction.

The skeleton asks the game for a provider (`makeInputProvider`) only inside `if (isLocallyPredicted)`. The session wraps
it for the latency stamps (`SessionLatencyDiagnostics::wrapInputProvider`, og-sim-session's, which
`UELatencyBudgetDiagnostics` inherits since og-session-runtime task 37) and keeps it in the pending record until
the registration completes.

### 9.5 The spawn ordering, and the provider latch (retired by og-input-latency task P5-7)

Until P5-7 the authority spawned the character's input relay host at the moment registration completed, before it
registered the delivery route (guard G-10), handed the host the session's re-send count, and a client found its host; then
the skeleton latched its provider decision on the sibling. Since P5-7 og-sim-net's server stages and sends each character's
relays itself, reading the re-send count from `TimeConfig` at every flush, so there is no host to spawn, find or attach, and
the sibling's owner-skip check, which the latch fed, has no input traffic to skip. The assertion that kept the knob's clamp
equal to the codec's re-send bound moved to the session (`compositionContractsHold`).

### 9.6 The delivery route (retired by og-input-latency task P5-7)

Until P5-7 the skeleton registered the id → component route the session's two sinks resolved against, once, on the authority
(guard G-11). og-sim-net's server is both sinks since, and delivers through the character's channel; the session's
`tryRegister` records the registered component itself, for both roles, in the map a game may read as its list of registered
characters (`getRegisteredComponentsById`).

### 9.7 Equivalence (task 29)

Statement for statement and on the game thread, as the component ran it: `transcribe.py` (`map_29.json`, the loop's
units against the pre-task-25 pin) inlines each hook body at its call site and maps each outcome to the skeleton's
handler, and reports no difference. No log line changed (`NewFramework: registered`, and the session's own lines, keep
text, level, category and arguments). Three `checkf` texts became game-neutral (ruling Q7, fatal-only): loop one's two
(the session-owner wait names this class; the retry names the hook's answer and keeps the `[SimHost.Slot]` pointer) and
loop two's key wait (it names the hook's answer, not the brawler's id). **Optimization (lead ruling F1):** the moved
bodies are template members, so they compile optimized in Development, as §7 says; none does floating-point arithmetic
(task 29's notes list the functions and their disassembly reading). The hook bodies stay non-template members of the
component and keep its file's optimization state.

---

## §10 The net half deleted (og-input-latency task P5-8, 2026-11-18)

**What was deleted.** From this header: `ISimulatableOwnerBinding` with its five private pure virtuals, its friend and its
destructor `static_assert` (§1); the five `final` overrides, `simulationKey`, `onCorrectionStateAccepted`,
`receiveRemoteMoveBundle`, `noteRelayedRingArrived` and `makeRingFlushedCallback` (§3–§5); the owner-concept aliases
`SyncedCorrectionBufferType` and `RelayedInputRingType`; the forwarders (the correction-state and relayed-ring callback
pairs, both buffer getters, both ring getters, `attachInputRelayHost`); `sendLocalInputToAuthority`, the remote-move
callback pair, its member and `deliverDelayedRemoteInput`; `stageRelayedInput` and its signature `static_assert` (§6,
guard G-01); the sibling pointer with `bindNetSync`, `unbindNetSync`, `netSync()` and `isNetSyncBound()`; and the includes
only they needed. In the same task the sibling, `USimulatableNetSyncComponent`, and `InputRedundancyBundleBuilder.h` were
deleted, and the brawler's pawn lost its `SimulatableNetSync` subobject and the bind in its `PostInitializeComponents`.

**Why it was unreachable.** Since task P5-7 the core's owner of a character, on both roles, is the character's og-sim-net
channel (`UESimNetChannel`, `UESimNetLanes.h`), so the core called none of the forwarders or typed members. The sibling's
correction property was never written and its input RPC never called, so none of the five overrides ran: P5-7's
`receiveRemoteMoveBundle` was a one-shot `Warning` that never printed. The two lines and the stamps of §4 and §5 are
og-sim-net's client and the Unreal sim-net hosts' since P5-7 (§4, §5), and the stage is og-sim-net's server's (§6).

**What stays, and what moved.** The registration skeleton (§9) is unchanged statement for statement. The key hook's
`static_assert` moved from `simulationKey()`'s body to `tryRegisterWithNewFramework`'s, with its message re-worded for its
new site; the loops' two hook `static_assert` messages lost the clause about interface member names, which no longer has
a referent. No log line, no `checkf` text and no registration step changed.
