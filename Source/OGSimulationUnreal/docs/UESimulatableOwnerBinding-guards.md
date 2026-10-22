<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESimulatableOwnerBinding.h` — guards

Prohibitions with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulatableOwnerBinding.h`. Each
entry has an **opaque, stable id**. In the source a single line `// ⛔G-nn  docs/UESimulatableOwnerBinding-guards.md`
sits directly above the statement where the forbidden edit would be typed. The reasoning lives in
`UESimulatableOwnerBinding-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, moves to §R and its number
is spent forever. A retired id may be **named** in prose or an assertion message; it may never again appear as a
`⛔G-nn` tag.

**What is not here.** The interface's access (private members, one friend), its destructor's two properties, the key
hook's presence, the four registration hooks' presence and shapes, and the staging function's signature are compile-time
checks in the header (rationale §1, §2, §6, §9.1), so they have no entry. So is the rule that no hook shares a name and
signature with an interface member: the `final` overrides make it a compile error (rationale §9.1).

**Provenance.** G-01 … G-06 moved from `OGBrawlerSimulatableComponent-guards.md` (the brawler's game component, BUSL-1.1 then)
with their code, in og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): G-01 was G-13 there, G-02 … G-06 were
G-19 … G-23. G-07 … G-11 moved from the same file with the registration loop in task 29 (2026-10-22): they were G-02,
G-03, G-04, G-06 and G-07 there. The text is unchanged apart from its site (the template's spellings), its new id, its
cross-references and what a game-neutral entry cannot name (the brawler's own reasons stay in its component's docs);
each id is retired in that file's §R. **Quoted fences** are the comment each entry replaced, as shipped in the game
component; where R0 found the shipped text false or stale, the entry says so under **Correction**.

---

## G-01 — a stage overflow is counted on the host, never absorbed

**Site:** `if (outcome.droppedOldest && host != nullptr) host->noteStageOverflowDrop();` in `stageRelayedInput`, where
`host` is the sibling's `getInputRelayHost()`. (Was `OGBrawlerSimulatableComponent-guards.md` G-13.)

**As shipped:**
> ⛔ COUNTED, NOT ABSORBED, and on the HOST -- the one input loss this side can see. §6

**Consequence.** A burst longer than `relayedInputRing::kMaxDepth` in one frame drops its oldest
entry; this is the only visible record of that input loss (rationale §6).

---

## G-02 — no netcode policy below the fence; the order is fence, one RTT sample, core loop

**Site:** `typename TraitsT::SessionOwner* authorityManager = TraitsT::SessionOwner::instanceFor(/*isAuthority=*/true);`,
the first statement of `receiveRemoteMoveBundle`. "The fence" is the sibling's: the RPC runs on
`USimulatableNetSyncComponent`, whose G-06 and G-07 are the two fences, and it calls this function only after them.
(Was `OGBrawlerSimulatableComponent-guards.md` G-19.)

**As shipped:**
> ⛔ NO NETCODE POLICY BELOW THIS FENCE -- tier derivation, dedup, park/drain, the malformed-
> slot fence and the per-slot loop are all core. This side resolves engine primitives only. §7
>
> ⛔ ORDER IS LOAD-BEARING: fence, then ONE RTT sample, then the core per-slot loop. §7

**Consequence.** Policy added here is policy a second engine's adapter would silently lack, and the
core's tests do not cover (rationale §3).

---

## G-03 — resolve the root connection once

**Site:** `UNetConnection* rootConn = (coordinator != nullptr) ? GetRootNetConnection(derived().GetOwner()) : nullptr;`
(Was `OGBrawlerSimulatableComponent-guards.md` G-20.)

**As shipped:**
> ⛔ RESOLVE THE ROOT CONNECTION ONCE -- split-screen siblings collapse to their shared wire. §7

**Consequence.** A child connection, or a per-slot re-resolve, splits one wire into several tier and
slot keys.

---

## G-04 — the no-wire fallback is adapter-side; the core only ever sees a valid wire

**Site:** `if (coordinator == nullptr || rootConn == nullptr)`. (Was `OGBrawlerSimulatableComponent-guards.md` G-21.)

**As shipped:**
> ⛔ THE NO-WIRE FALLBACK IS ADAPTER-SIDE -- the core only ever sees a valid wire. §7

**Consequence.** Passing a null wire into `ServerReceptionCoordinator` hands the core a sentinel it is
written never to see.

---

## G-05 — one RTT sample per bundle, never per slot

**Site:** `coordinator->noteRttSample(handle, id, authorityManager->session().getServerReceptionTick(), readRoundTripMs(rootConn), *m_netSync);`
(the reception tick is the session's since og-ue-simulationcomponentandmanagersplit task 26a, reached through the owner's `session()`)
(Was `OGBrawlerSimulatableComponent-guards.md` G-22.)

**Correction (R0, task 21).** The site line there still ended `*this)`; since og-ue-simulationcomponentandmanagersplit
task 20b the last argument, the tier sink, is the sibling, `*m_netSync`.

**As shipped:**
> ⛔ ONE RTT SAMPLE PER BUNDLE, NEVER PER SLOT -- per-slot couples the tier EMA to depth. §7

**Consequence.** Sampling per slot feeds one reading into the tier EMA up to `kMaxSlots` times and
couples smoothing to redundancy depth.

---

## G-06 — the delay is applied exactly once, park-to-release, on the original capture tick

**Site:** `coordinator->template receiveInputBundle<typename TraitsT::Simulatable>(id, handle, playerSlot, bundle, authorityManager->session(), authorityManager->session());`
(the two sinks are the session's since og-ue-simulationcomponentandmanagersplit task 26a; they were the owner, `*authorityManager`, before)
(Was `OGBrawlerSimulatableComponent-guards.md` G-23.)

**As shipped:**
> ⛔ THE DELAY IS APPLIED EXACTLY ONCE (park-to-release) -- a released input keeps its ORIGINAL
> captureTick, and SimulationInputResolution::collectInputAll pops it in ARRIVAL order. §7

**Consequence.** Any offset or re-stamp added at this call applies the delay twice.

---

## G-07 — the role test is the world's net mode, never `HasAuthority()`

**Site:** `const bool isAuthority = (derived().GetNetMode() != NM_Client);` in `tryInitializeWithManager`, the registration
skeleton's first role test. (Was `OGBrawlerSimulatableComponent-guards.md` G-02.)

**As shipped:**
> ⛔ NEVER HasAuthority() HERE -- it is always true on a non-replicated actor and would
> disagree with the world-mode gate the manager picks its own role with. §3

**Correction (R0, carried from the game component's entry).** The stated mechanism is about the wrong actor. "Always true
on a non-replicated actor" describes the session owner (the brawler's sets `bReplicates = false`); the component's owner is
a replicated pawn. The prohibition stands for the other half of the sentence: `TraitsT::SessionOwner::instanceFor(isAuthority)`
returns the owner of the session that `UESimulationSession::instanceFor` finds in the registry slot for that role, and the
session's `beginPlay` fills that slot from `worldIsAuthority = (worldNetMode != NM_Client)`. Only the same world-level
predicate is guaranteed to pick the session owner of this component's own world; an actor-role test answers differently for
an actor spawned locally on a client.

**Consequence.** A component resolving the other role's session owner registers into the wrong storage — in PIE both
sessions live in one process, so the lookup succeeds and nothing fails loudly.

**Scope note.** The same predicate is computed again in `tryRegisterWithNewFramework`; a game component that reads the
role elsewhere (the brawler's `EndPlay` and `TickComponent`) computes it the same way. Only this site carried the fence,
and only this site is tagged.

---

## G-08 — absence: nothing tier-related is bound per character

**Site:** absence tag in `tryInitializeWithManager`, above the call of the game's `initializeWithManager` hook (the
per-character initialization). (Was `OGBrawlerSimulatableComponent-guards.md` G-03, which stood after the brawler's
input-collection cache, now inside that hook.)

**As shipped:**
> ⛔ NOTHING TIER-RELATED IS BOUND HERE -- the tier is a WIRE property, bound once per world. §5

**The prohibition.** Do not re-add a per-character tier consumer binding here, nor in the hook this call runs. The tier
consumer is the session's `m_replicatedTierConsumer`, one per world: the session's `beginPlay` emplaces it through
`beginTierConsumer` on both roles. The tier itself replicates from the per-connection `ASimulationConnectionRelay`.

**Correction (R0).** The game component's entry called it "the client-side consumer", emplaced "from the manager's
`BeginPlay`". `beginTierConsumer` is called in both of the session's role branches (the authority's and the client's),
and since og-ue-simulationcomponentandmanagersplit task 27b by the session's `beginPlay`, which the owner's `BeginPlay`
calls.

**Consequence.** A per-character binding reintroduces one consumer per sibling on a shared wire — the shape whose
disagreement the per-connection relay removed.

---

## G-09 — a simulated proxy must not register a local input provider

**Site:** `const bool isLocallyPredicted = !isAuthority && ownerActor != nullptr && ownerActor->GetLocalRole() == ROLE_AutonomousProxy;`
in `tryRegisterWithNewFramework`. (Was `OGBrawlerSimulatableComponent-guards.md` G-04.)

**As shipped:**
> ⛔ A SIMULATED PROXY MUST NOT REGISTER A PROVIDER -- absence is what buys it a relay store. §4

**Consequence.** Provider presence is the core's identity test (rationale §9.4). A provider on a simulated proxy selects
`registerLocalCharacter` instead of `registerRemoteCharacter` (`SimulationNetSync::registerPredictionOwner`): the proxy gets
no relay store and is predicted from nothing, and its captures are put on the wire as if local. The skeleton asks the game's
`makeInputProvider` hook for a provider only inside `if (isLocallyPredicted)`.

**What breaks if it moves.** The sibling's latched provider decision (`setHasLocalInputProvider`, read by its owner-skip
check, `SimulatableNetSyncComponent-guards.md` G-15) is this exact evaluation; a second, differently-worded role test would
split the two halves of the owner-skip check.

**Correction (R0).** The game component's entry said "`m_hasLocalInputProvider` latches this exact evaluation (G-15)":
that member and G-15 are the sibling's since og-ue-simulationcomponentandmanagersplit task 20b.

---

## G-10 — spawn the relay host BEFORE registering the delivery route (ordering)

**Site:** the `if (isAuthority)` block that calls `ASimulationInputRelay::spawnForCharacter`, above the
`noteDelayedInputComponent` call, in `tryRegisterWithNewFramework`. (Was `OGBrawlerSimulatableComponent-guards.md` G-06.)

**As shipped:**
> ⛔ SPAWN THE HOST BEFORE THE noteDelayedInputComponent ROUTE -- until that route exists,
> relayRemoteInput stages into m_detachedRelayStagingRing, unseen and unlogged. §6

**Correction (R0, carried).** The window is stated backwards. Until the route exists, `UESimulationSession::relayRemoteInput`
finds no component and returns — nothing is staged anywhere. The hazard is the opposite window: the route registered while
no host is linked. Then `relayRemoteInput` reaches `stageRelayedInput`, which falls back to the sibling's
`m_detachedRelayStagingRing` (`SimulatableNetSyncComponent-guards.md` G-13): unseen by every client and unlogged.

**The prohibition.** Do not move `noteDelayedInputComponent` above the host spawn. ⚠ This is an ordering fence (rationale
§9.5).

---

## G-11 — the delivery route is registered once, authority only

**Site:** `if (isAuthority) regManager->session().noteDelayedInputComponent(resolved.key, derived());` in
`tryRegisterWithNewFramework`. (Was `OGBrawlerSimulatableComponent-guards.md` G-07.)

**As shipped:**
> ⛔ ROUTE REGISTERED ONCE, erased in unregisterFromNewFramework. Authority only. §7

**The prohibition.** Register it here, once, under `isAuthority` exactly — not per parked slot, not on a client, and not
behind any further condition.

**Consequence.** The session resolves delivered and relayed input against this route: `deliverRemoteInput` (the core's
malformed-slot path and the release drain) and `relayRemoteInput` (the staging write). A missing route drops that input.
The erase is the session's `forgetReceptionOwner`, inside `unregisterFromNewFramework` (`UESimulationSession-guards.md`
G-13). A game may also walk the same map, through `getDelayedInputComponentsById`, as its list of registered characters:
narrow this registration and that list narrows with it (the brawler's case is in its component's docs).

---

## §R — Retired ids

None.
