<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: m_replicatedInputSyncedBuffer -- ABSENCE FENCE (G-01, G-08): the retired replicated correction-input property. It must NOT resolve -->
<!-- lint-external-ref: OnRep_CorrectionInput -- ABSENCE FENCE (G-01, G-04): the retired notification of that property. It must NOT resolve -->
<!-- lint-external-ref: m_clientToServerInputSyncedBuffer -- RETIRED at og-syncedInput-rework task 9 (2026-10-03): the client->server input buffer member, quoted verbatim in G-01 and named in its dated Site history. It must NOT resolve -->
# `SimulatableNetSyncComponent.h` and `.cpp` — guards

Every prohibition with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/SimulatableNetSyncComponent.h`
and `SimulatableNetSyncComponent.cpp`. Each entry has an **opaque, stable id**. In the source a single line
`// ⛔G-nn  docs/SimulatableNetSyncComponent-guards.md` sits directly above the statement or declaration where the
forbidden edit would be typed. An **absence tag** — one with a blank line under it — marks the place where something
retired would be re-added.

**If this file and the source disagree, the source is authoritative and this file is stale.**

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, moves to §R and its number
is spent forever. A retired id may be **named** in prose or an assertion message; it may never again appear as a
`⛔G-nn` tag.

⛔ **Nothing in this file is a rationale.** The reasoning lives in `SimulatableNetSyncComponent-rationale.md`.

**Provenance.** G-01 … G-10 and G-12 … G-16 moved from `OGBrawlerSimulatableComponent-guards.md` (the game module's
component, BUSL-1.1 then) with their code, in og-ue-simulationcomponentandmanagersplit tasks 20a (G-01 … G-10) and 20b
(G-12 … G-16), 2026-10-21. The text is unchanged apart from its site, its new id, its cross-references, and the
component's name where it names the site. Each entry names the id it had there; that id is retired in that file's §R.
G-11 is new in task 20b. **Quoted fences** are the comment each entry replaced, as shipped in the game component; where
R0 found the shipped text false, the entry says so under **Correction**.

---

## G-01 — absence (header): the replicated correction-input property

**Site:** absence tag in the private section of `SimulatableNetSyncComponent.h`, after the `ServerReceiveRemoteMove`
declaration. (Was `OGBrawlerSimulatableComponent-guards.md` G-36, at the same position in `OGBrawlerSimulatableComponent.h`. Until
og-syncedInput-rework task 9, 2026-10-03, it followed `m_clientToServerInputSyncedBuffer`, which sat between the two.)

**As shipped:**
> [og-netcode-v2-input-relay T8] THE REPLICATED CORRECTION-INPUT PROPERTY IS
> GONE. `UPROPERTY(ReplicatedUsing = OnRep_CorrectionInput)
> FSimulationInputSyncBuffer m_replicatedInputSyncedBuffer;` and its
> `OnRep_CorrectionInput()` UFUNCTION stood here, registered in
> GetLifetimeReplicatedProps beside the state buffer. The server->client echo
> of "the input I applied for you" is retired; a remote character's input
> reaches peers on the relay ring below, and the correction state names which
> capture it applied via T4's ref.
>
> NOTE the surviving `m_clientToServerInputSyncedBuffer` above is a DIFFERENT
> member with the same type and the opposite direction — it is not replicated
> and it is not affected.

**Correction (R0).** "on the relay ring below" — the ring is no longer declared below; it is a property of
`ASimulationInputRelay` (G-02).

**Correction (R0, og-syncedInput-rework task 9, 2026-10-03).** The NOTE's "surviving
`m_clientToServerInputSyncedBuffer`" no longer survives: that member, its accessor, its typedef and the input sync buffer
type itself were deleted together with the core concept requirement that was their only reason to exist. The component
holds no input sync buffer in either direction now; the input leaves through `sendInputBundleToAuthority`.

---

## G-02 — absence (header): the relayed-input ring as a property of this component

**Site:** absence tag after G-01. (Was `OGBrawlerSimulatableComponent-guards.md` G-37.)

**As shipped (the retirement paragraph; the host-link paragraph that followed it is the game component's rationale
§6):**
> [og-netcode-v2-input-relay / T1] The outbound input relay ring stood here as
> `UPROPERTY(ReplicatedUsing = OnRep_RelayedInputRing) FRelayedInputRing
> m_relayedInputRing;` with its `OnRep_RelayedInputRing()` UFUNCTION, and was
> registered with a plain DOREPLIFETIME (no COND_) in GetLifetimeReplicatedProps.
>
> ⭐ [T39] ALL THREE MOVED TO ASimulationInputRelay, IN ONE EDIT — property,
> OnRep and registration together ...

**Consequence.** Two reasons, rationale §5: scheduling (a shared atomic Iris batch killed ring and state together) and
ownership (`COND_SkipOwner` on an actor owned by the character).

---

## G-03 — once a wire-format mismatch is latched, every later `OnRep_CorrectionState` no-ops

**Site:** `if (m_wireFormatMismatchDetected) return;` (Was `OGBrawlerSimulatableComponent-guards.md` G-09.)

**As shipped:**
> ⛔ ONCE A MISMATCH IS LATCHED EVERY LATER OnRep NO-OPS -- a mismatched peer must not correct. §8

**Consequence.** Without the latch a mismatched peer would drive correction logic with bytes it cannot parse (rationale
§4; the shipped "§8" is the game component's old section).

---

## G-04 — absence: `OnRep_CorrectionInput` stood here

**Site:** absence tag after `OnRep_CorrectionState`'s definition. (Was `OGBrawlerSimulatableComponent-guards.md` G-10.)

**As shipped:**
> ⛔ `OnRep_CorrectionInput` STOOD HERE, all of it retired -- see SimulationNetSync::sendCorrectionAll. §9

**The prohibition.** Do not re-add the server→client correction-input notification. The channel, its property (G-01),
its registration (G-08) and its core binding were retired together (rationale §5).

---

## G-05 — absence: `OnRep_RelayedInputRing()` stood here

**Site:** absence tag after G-04. (Was `OGBrawlerSimulatableComponent-guards.md` G-11.)

**As shipped:**
> ⛔ `OnRep_RelayedInputRing()` STOOD HERE -- it moved to the relay host with its property. §6

**The prohibition.** Do not re-add a ring notification on this component; the ring and its notification live on
`ASimulationInputRelay` (G-02, G-09, rationale §5).

---

## G-06 — an empty input bundle is idle traffic, not a version mismatch

**Site:** `if (bundle.wireBytes.Num() == 0) return;` in `ServerReceiveRemoteMove_Implementation`. (Was
`OGBrawlerSimulatableComponent-guards.md` G-17.)

**As shipped:**
> ⛔ AN EMPTY BUNDLE IS IDLE TRAFFIC, NOT A MISMATCH -- no wire header means no version byte. §8

**Consequence.** Removing or reordering it below the version test logs a false wire-format mismatch every idle frame.

---

## G-07 — refuse a mismatched bundle, but do not disconnect here

**Site:** the `return;` closing the version-mismatch branch of `ServerReceiveRemoteMove_Implementation`. (Was
`OGBrawlerSimulatableComponent-guards.md` G-18.)

**As shipped:**
> ⛔ REFUSE, BUT DO NOT DISCONNECT HERE -- the disconnect path is engine-managed. §8

**Consequence.** Disconnect policy is a dedicated-server validation concern owned by the engine's connection path, not
this fence (rationale §4).

---

## G-08 — absence: the `m_replicatedInputSyncedBuffer` lifetime registration

**Site:** absence tag in `GetLifetimeReplicatedProps`, after the state-buffer registration. (Was
`OGBrawlerSimulatableComponent-guards.md` G-32.)

**As shipped:**
> ⛔ THE `m_replicatedInputSyncedBuffer` REGISTRATION STOOD HERE -- removed in the SAME edit
> as the member: one half alone is a compile error, the other a silently dead field. §9
>
> ⛔ REMOVING A PROPERTY NEEDS NO VERSION BUMP -- registrations are lifetime entries. §9

---

## G-09 — absence: the ring's registration on this component

**Site:** absence tag after G-08. (Was `OGBrawlerSimulatableComponent-guards.md` G-33.)

**As shipped:**
> ⛔ `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` STOOD HERE, with NO COND_.
> It is now COND_SkipOwner on the ring's own per-character dependent object. §6
>
> ⛔ THE COND_ COULD NOT HAVE BEEN ADDED HERE -- the shared atomic batch is what forced it. §6

**Consequence.** Re-adding it here — even with the condition — re-couples the ring to the correction state's atomic
replication batch (rationale §5). `OGBrawlerSimulationManager-rationale.md` cites this fence as evidence the per-character
components no longer carry the ring.

---

## G-10 — absence: the `COND_OwnerOnly` tier registration

**Site:** absence tag after G-09. (Was `OGBrawlerSimulatableComponent-guards.md` G-34.)

**As shipped:**
> ⛔ THE COND_OwnerOnly TIER REGISTRATION IS GONE -- owner-only RELEVANCY narrows it now. §5

**Consequence.** The tier replicates from `ASimulationConnectionRelay` (`bOnlyRelevantToOwner`); a per-character
registration here restores one tier per sibling on a shared wire.

---

## G-11 — `EndPlay` detaches the relay host only when no binding is attached

**Site:** `if (m_binding == nullptr)` in `EndPlay`, above the `detachInputRelayHost` call. (New in task 20b.)

**The prohibition.** Do not drop the condition, and do not move the detach out from under it. While a binding is
attached, the game component's `EndPlay` drives the teardown (unregister, `detachInputRelayHost`, unbind; rationale
§10, "Teardown"). Do not rely on this `EndPlay` for the detach either: the order between two components' `EndPlay`
calls is the actor's set order, not a contract (rationale §3).

**Consequence.** Unconditional, this component destroys the host (authority) or unbinds it (client) whenever it happens
to end play first, ahead of the game's unregister from the session, which the game ordered after the unregister.
Seen in task 20b: with the condition removed, the detach ran twice for every ending character, once here and once from
the game component; the session stayed clean only because `detachInputRelayHost` is idempotent.

---

## G-12 — on teardown the authority destroys the relay host; a client only unbinds it

**Site:** the inner `if (isAuthority)` in `detachInputRelayHost`. (Was `OGBrawlerSimulatableComponent-guards.md` G-08, in that
component's `EndPlay`.)

**As shipped:**
> ⛔ AUTHORITY DESTROYS, CLIENT ONLY UNBINDS -- a local destroy races the destruction bunch. §6

**Consequence.** A client-side `Destroy` of a replicated actor races the server's destruction bunch. The authority path
is `ASimulationInputRelay::detachFromParentAndDestroy`, which clears the callback, removes the Iris dependent-actor link
(Iris builds only) and then destroys. The link is cleared after the block on both roles, so a second call finds no host
(rationale §10, "Teardown").

---

## G-13 — the no-host staging fallback is `m_detachedRelayStagingRing`, never `m_detachedRelayRing`

**Site:** `FRelayedInputRing& stage = (host != nullptr) ? host->editRelayedInputStagingRing() : m_detachedRelayStagingRing;`
in `relayStagingTarget`. (Was `OGBrawlerSimulatableComponent-guards.md` G-12, in that component's `stageRelayedInput`.)

**As shipped (header, at `m_detachedRelayStagingRing`):**
> [T34] The same fallback, for the flush STAGE. `stageRelayedInput` must have somewhere to write when
> no host is linked, and it must NOT be the detached ring above: that one is what
> `getRelayedInputRing()` hands the core, so staging into it would let an unpublished burst read back
> as if it had replicated.

**Consequence.** As quoted: an unpublished burst would read back through the core's read accessor as if it had
replicated. The edit is typed here, which is why the tag sits at this statement and not at the member.

---

## G-14 — the host callback captures a weak pointer, never a raw `this`

**Site:** `host->setOnRelayedInputReceivedCallback([weakSelf](const FRelayedInputRing& ring) { ... });` in
`attachInputRelayHost`. (Was `OGBrawlerSimulatableComponent-guards.md` G-14.)

**As shipped:**
> ⛔ WEAK CAPTURE, NEVER A RAW `this` -- the host can outlive this component by a frame. §6

**Consequence.** A raw `this` is a dangling call from a replicated notification during teardown.

---

## G-15 — the owner-skip check stays gated on `num() > 0` and reads the LATCHED provider decision

**Site:** `if (m_hasLocalInputProvider && ring.num() > 0 && !m_loggedOwnerSkipDivergence)` in
`onRelayedInputRingArrived`. (Was `OGBrawlerSimulatableComponent-guards.md` G-15.)

**As shipped (two fences, merged because they govern the same condition):**
> ⛔ DO NOT DROP THE `num() > 0` GATE -- the host replicates to its owner, so empty is normal.

> ⛔ LATCHED, NEVER RECOMPUTED AT ARRIVAL -- onRelayedInputRingArrived's divergence check
> is only meaningful if it reads literally this evaluation. §6

(The second stood at the brawler component's provider latch in `tryRegisterWithNewFramework`, which since task 20b
writes this component's flag through `setHasLocalInputProvider`, and since task 29 is the binding's registration
skeleton (`UESimulatableOwnerBinding.h`); the edit it forbids — a fresh role test in place of the
latch — is typed here.)

**Consequence.** `ASimulationInputRelay` is `bAlwaysRelevant` and does reach its owner (only the property is
`COND_SkipOwner`), so an empty ring at a provider-present character is normal: dropping the gate fires on every clean
run. Recomputing provider presence here compares a different evaluation from the one the core registered with, so the
check no longer tests what it claims.

---

## G-16 — the owner-skip divergence is an `ensure`, never a `check`

**Site:** `ensureMsgf(false, TEXT("[InputRelay] relay ring arrived for a provider-present character (id=%u)"), ...)`
in `onRelayedInputRingArrived`. (Was `OGBrawlerSimulatableComponent-guards.md` G-16.)

**As shipped:**
> ⛔ `ensure`, NEVER `check`, one-shot -- a bandwidth regression must stay playable to diagnose.

**Consequence.** A `check` turns a bandwidth/correctness diagnosis into a crash; the one-shot latch
(`m_loggedOwnerSkipDivergence`) keeps it from spamming.

---

## §R — Retired ids

None.
