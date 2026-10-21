<!-- SPDX-License-Identifier: MPL-2.0 -->
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

* **§1** is the non-template base, `ISimulatableOwnerBinding`.
* **§2** is the template: its shape, its members, the key hook and the checks the compiler makes.
* **§3** is the RPC tail: primitives in, policy out.
* **§4** is the five latency-budget stamp blocks.
* **§5** is the two per-packet log lines.
* **§6** is the staging write.
* **§7** is how it is built and what that does to the code.
* **§8** is the provenance.

---

## §1 `ISimulatableOwnerBinding` — the one way the wire component reaches the game

`USimulatableNetSyncComponent` (`SimulatableNetSyncComponent-rationale.md`) owns a character's reflected wire surface
and must stay free of game types. It reaches the typed game side only through this base, held as a pointer that the
owning actor attaches when it initializes its components. A non-template `UCLASS` can reach a template only through such
a non-template base; this is the same shape as the relay host's listener interfaces in this module.

**Its members** (three since task 20a, five since task 20b):

| member | called by the sibling | what the binding supplies (§2) |
|---|---|---|
| `simulationKey` | the tier send's `check` and `[ConnectionTier]` line, and the owner-skip lines | the game's storage key, through the game's key hook |
| `onCorrectionStateAccepted` | `OnRep_CorrectionState`, after the wire-format fence, before the core's callback | the typed read of the correction state, `[ReceiveCorrectionState]` and the `ClientReceived` stamp |
| `receiveRemoteMoveBundle` | `ServerReceiveRemoteMove_Implementation`, after its fences | the typed RPC tail: reception, delay, delivery (§3) |
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

**What it supplies.** The template derives from `ISimulatableOwnerBinding`, so the sibling reaches it through §1's five
calls, and it is the whole owner-concept surface the core expects of the game component (`OwnerT` stays the core's
owner type; the core's `SimulatableOwnerTraits` names the game component, not the binding):

| part | members |
|---|---|
| the five interface members | `simulationKey`, `onCorrectionStateAccepted`, `receiveRemoteMoveBundle`, `noteRelayedRingArrived`, `makeRingFlushedCallback`: **private `final`** overrides |
| the owner-concept aliases | `SyncedCorrectionBufferType` (`FSimulationStateSyncBuffer`) and `RelayedInputRingType` (`FRelayedInputRing`); the core's `PredictionSyncedBufferOwnerConcept` reads them as `OwnerT::` names |
| the type-free forwarders | the correction-state callback pair, both `getSyncedCorrectionStateBuffer` overloads, both `getRelayedInputRing` overloads, the relayed-ring callback pair, `attachInputRelayHost`: one-liners onto the sibling |
| the typed owner-concept members | `sendLocalInputToAuthority` (builds the redundancy bundle, hands it to the sibling's `sendInputBundleToAuthority`, stamps), the remote-move callback pair and its member, `deliverDelayedRemoteInput`, `stageRelayedInput` (§6) |
| the sibling pointer | `m_netSync`, set by `bindNetSync` (public: the owning actor calls it) and cleared by `unbindNetSync` (protected: the component's teardown calls it); the protected `netSync()` accessor `checkf`s that the pointer is set; the public `isNetSyncBound()` says whether it is |

The `const` buffer and ring getters reach the sibling's `const` overloads through a `const` view of the sibling. (Task
20a's `const` buffer forwarder called the sibling's non-`const` overload; the buffer is the same object, so only the
overload changed.)

**The key hook, and why every override is `final` (review B-2, ruling Q-F).** `simulationKey()` returns the game
component's `storageKeyOfOwner()`, a hook with its **own** name. If the game's hook were also called `simulationKey()`,
with the same signature, it would *override* the interface member instead of serving it: the binding's override would be
dead, and with the hook deleted the binding's own inherited override would satisfy every lookup and recurse. With the
overrides `final`, a game member named like an interface member is a compile error (`C3248`), never a silent override.

**The hook check is an inline requires-expression in a binding member body.** The hooks are private members of the game
component, which declares its binding specialization a friend. Access
in a requires-expression is checked in the context where it appears, so a namespace-scope `concept` cannot see a private
hook; the check is a `static_assert` of a requires-expression inside `simulationKey()`'s body, which the friendship
covers, and it fires when that body is instantiated. Its message names the hook and the ruling. A game component
without the hook fails there with that message, ahead of the plain lookup error.

**The traits** are constrained by the concept `UESimulatableOwnerBindingTraits`: the types the template names
(`Simulatable`, `PlayerInput`, `State`, `SessionOwner`). `OwnerT` is incomplete where the base list instantiates the
class, so nothing about it is checked there: every member that reaches into `OwnerT` does so in a function body, through
`derived()` (a `static_cast` to `OwnerT&`).

**The session owner.** The RPC tail reaches the authority's session owner through `TraitsT::SessionOwner::instanceFor`,
and from its session (`session()`) the reception coordinator (`getReceptionCoordinator`) and the server reception tick
(`getServerReceptionTick`), and it hands that session to the coordinator as both sinks; the coordinator's type is
deduced. Until og-ue-simulationcomponentandmanagersplit task 26a the three were the owner's, as the game component had them on its own manager type. The
stamp blocks reach the latency member through `UELatencyBudgetDiagnostics<TraitsT>::withInstance`.

**`isNetSyncBound()`, and the diagnostics loop.** The latency template's server loop reads each delayed-input
component's correction buffer through the `const` getter, which dereferences the sibling pointer. It skips a component
whose binding is not bound (`UELatencyBudgetDiagnostics-guards.md` G-03). The component's own teardown uses the same
query before it detaches the relay host through `netSync()`.

---

## §3 The RPC tail — primitives in, policy out

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

Five blocks in this header stamp the latency-budget probe. Each is a timestamp and nothing else: it reads values the
code beside it already has, writes only the probe, and changes no input, no wire byte and no ordering of the netcode it
sits next to. The template's own stamp points, their threads and errors are `UELatencyBudgetDiagnostics-rationale.md`
§6; the game's hop table is the game's documentation. This list only says what each block does here.

| member | stamp | role |
|---|---|---|
| `sendLocalInputToAuthority`, after the send | `Handed` for every capture tick in the bundle | client |
| `receiveRemoteMoveBundle`, which the sibling's RPC calls after its fences | `ServerReceived` for every capture tick in the bundle, and one `WIRE` sample from `readRoundTripMs` per bundle when there is a root connection | authority |
| `onCorrectionStateAccepted`, which the sibling's `OnRep_CorrectionState` calls; after the `[ReceiveCorrectionState]` line | `ClientReceived` for the correction's tick | client |
| `noteRelayedRingArrived`, which the sibling's `onRelayedInputRingArrived` calls after the owner-skip check, before the core callback | `ClientReceived` for every capture tick in the ring other than the `kNoInputCaptureTick` sentinel | client |
| the callback `makeRingFlushedCallback` returns, which the sibling's `attachInputRelayHost` binds as the relay host's flush callback, authority only | `Handed` for every capture tick a flush published, other than the sentinel | authority |

All five run on the game thread. Every stamp goes through `UELatencyBudgetDiagnostics::stamp`, which `checkf`s the game
thread, and every block reaches it through `UELatencyBudgetDiagnostics::withInstance`, which runs the block on the
role's instance (the session owner's `editLatencyBudgetDiagnostics()`) when there is one. Where
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

Two lines in this header are logged with `OG_LOG_ROUTED`, not with `UE_LOG` at a fixed level:

* `[ReceiveCorrectionState] id=%u tick=%u` in `onCorrectionStateAccepted` (client, one per accepted correction);
* `[ServerReceive] id=%u tick=%u` on the no-wire path of `receiveRemoteMoveBundle` (G-04; authority, one per bundle
  slot).

Their level is their row's `plainLevel` in `kOGLogRoutes` (`UESimulationLogRoutes.h`): `Verbose` for both, so
`LogOGNet=Log` does not print them and `LogOGNet=Verbose` does. The rule and the table are
`UESimulationLogRoutes-rationale.md` §6. On the wire path the same `[ServerReceive]` tag is the core's: the
coordinator's per-slot loop logs it through its logger and the router, so a log carries the tag on both paths.

---

## §6 The staging write — `stageRelayedInput`

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
and defines it once, with an explicit instantiation definition, in the component's `.cpp`, so the virtual members are compiled in that
one translation unit, where the session owner's type is complete, and in no other that includes the component's header
(its generated reflection code among them). The in-class one-liners are inline and compile wherever they are used.

**Optimization (og-ue-simulationcomponentandmanagersplit lead ruling F1).** MSVC compiles a template instantiation with
the `#pragma optimize` state in force at the end of the instantiating translation unit, and every game `.cpp` ends with
`OGSIM_OPTIMIZE_ON`; so the out-of-line members compile optimized in Development, although the pair is there. The pair
documents intent. Task 21's notes list the functions whose Development state moved from unoptimized to optimized.

**Includes.** The template needs the sibling, the redundancy bundle builder, the relay host (`noteStageOverflowDrop`),
the connection helpers, the latency template, the log routes and og-sim's queue and codec headers. The sibling's own
header forward-declares the base; the sibling's `.cpp` includes this header for the base's complete type.

---

## §8 Provenance

* **Task 20a** created the header with `ISimulatableOwnerBinding` (three members); **task 20b** added the last two.
* **Task 21** (2026-10-21):
  * **Step 0, the spike:** the brawler's base `public ISimulatableOwnerBinding` became
    `public UESimulatableOwnerBinding<USimmableUpdateComponent, BrawlerSessionTraitsUImpl>` with the template empty; the
    Jolt Editor (header tool and build), the Chaos Editor and the Development Game and Server targets built.
  * Then the typed half moved in, statement for statement, from `SimmableUpdateComponent.h` and `.cpp`: the five
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
