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
hook's presence and the staging function's signature are compile-time checks in the header (rationale §1, §2, §6), so
they have no entry.

**Provenance.** G-01 … G-06 moved from `SimmableUpdateComponent-guards.md` (the brawler's game component, BUSL-1.1 then)
with their code, in og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): G-01 was G-13 there, G-02 … G-06 were
G-19 … G-23. The text is unchanged apart from its site (the template's spellings), its new id and its cross-references;
each id is retired in that file's §R. **Quoted fences** are the comment each entry replaced, as shipped in the game
component; where R0 found the shipped text false or stale, the entry says so under **Correction**.

---

## G-01 — a stage overflow is counted on the host, never absorbed

**Site:** `if (outcome.droppedOldest && host != nullptr) host->noteStageOverflowDrop();` in `stageRelayedInput`, where
`host` is the sibling's `getInputRelayHost()`. (Was `SimmableUpdateComponent-guards.md` G-13.)

**As shipped:**
> ⛔ COUNTED, NOT ABSORBED, and on the HOST -- the one input loss this side can see. §6

**Consequence.** A burst longer than `relayedInputRing::kMaxDepth` in one frame drops its oldest
entry; this is the only visible record of that input loss (rationale §6).

---

## G-02 — no netcode policy below the fence; the order is fence, one RTT sample, core loop

**Site:** `typename TraitsT::SessionOwner* authorityManager = TraitsT::SessionOwner::instanceFor(/*isAuthority=*/true);`,
the first statement of `receiveRemoteMoveBundle`. "The fence" is the sibling's: the RPC runs on
`USimulatableNetSyncComponent`, whose G-06 and G-07 are the two fences, and it calls this function only after them.
(Was `SimmableUpdateComponent-guards.md` G-19.)

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
(Was `SimmableUpdateComponent-guards.md` G-20.)

**As shipped:**
> ⛔ RESOLVE THE ROOT CONNECTION ONCE -- split-screen siblings collapse to their shared wire. §7

**Consequence.** A child connection, or a per-slot re-resolve, splits one wire into several tier and
slot keys.

---

## G-04 — the no-wire fallback is adapter-side; the core only ever sees a valid wire

**Site:** `if (coordinator == nullptr || rootConn == nullptr)`. (Was `SimmableUpdateComponent-guards.md` G-21.)

**As shipped:**
> ⛔ THE NO-WIRE FALLBACK IS ADAPTER-SIDE -- the core only ever sees a valid wire. §7

**Consequence.** Passing a null wire into `ServerReceptionCoordinator` hands the core a sentinel it is
written never to see.

---

## G-05 — one RTT sample per bundle, never per slot

**Site:** `coordinator->noteRttSample(handle, id, authorityManager->getServerReceptionTick(), readRoundTripMs(rootConn), *m_netSync);`
(Was `SimmableUpdateComponent-guards.md` G-22.)

**Correction (R0, task 21).** The site line there still ended `*this)`; since og-ue-simulationcomponentandmanagersplit
task 20b the last argument, the tier sink, is the sibling, `*m_netSync`.

**As shipped:**
> ⛔ ONE RTT SAMPLE PER BUNDLE, NEVER PER SLOT -- per-slot couples the tier EMA to depth. §7

**Consequence.** Sampling per slot feeds one reading into the tier EMA up to `kMaxSlots` times and
couples smoothing to redundancy depth.

---

## G-06 — the delay is applied exactly once, park-to-release, on the original capture tick

**Site:** `coordinator->template receiveInputBundle<typename TraitsT::Simulatable>(id, handle, playerSlot, bundle, *authorityManager, *authorityManager);`
(Was `SimmableUpdateComponent-guards.md` G-23.)

**As shipped:**
> ⛔ THE DELAY IS APPLIED EXACTLY ONCE (park-to-release) -- a released input keeps its ORIGINAL
> captureTick, and SimulationInputResolution::collectInputAll pops it in ARRIVAL order. §7

**Consequence.** Any offset or re-stamp added at this call applies the delay twice.

---

## §R — Retired ids

None.
