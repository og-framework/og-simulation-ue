<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: noteStageOverflowDrop -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: m_hasLocalInputProvider -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: receiveRemoteMoveBundle -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
# `UESimulatableOwnerBinding.h` — guards

Prohibitions with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/UESimulatableOwnerBinding.h`. Each
entry has an **opaque, stable id**. In the source a single line `// ⛔G-nn  docs/UESimulatableOwnerBinding-guards.md`
sits directly above the statement where the forbidden edit would be typed. The reasoning lives in
`UESimulatableOwnerBinding-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, moves to §R and its number
is spent forever. A retired id may be **named** in prose or an assertion message; it may never again appear as a
`⛔G-nn` tag.

**What is not here.** The key hook's presence and the three other registration hooks' presence and shapes are
compile-time checks in the header (rationale §2, §9.1), so they have no entry. Until og-input-latency task P5-8 the
net-sync interface's access, its destructor's two properties, the staging function's signature and the rule that no hook
shares a name and signature with an interface member were compile-time checks too; they went with the binding's net half
(rationale §10).

**Provenance.** G-01 … G-06 moved from `OGBrawlerSimulatableComponent-guards.md` (the brawler's game component, BUSL-1.1 then)
with their code, in og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): G-01 was G-13 there, G-02 … G-06 were
G-19 … G-23. G-07 … G-11 moved from the same file with the registration loop in task 29 (2026-10-22): they were G-02,
G-03, G-04, G-06 and G-07 there. The text is unchanged apart from its site (the template's spellings), its new id, its
cross-references and what a game-neutral entry cannot name (the brawler's own reasons stay in its component's docs);
each id is retired in that file's §R. **Quoted fences** are the comment each entry replaced, as shipped in the game
component; where R0 found the shipped text false or stale, the entry says so under **Correction**.

---

## G-07 — the role test is the world's net mode, never `HasAuthority()`

**Site:** `const bool isAuthority = (derived().GetNetMode() != NM_Client);` in `tryInitializeWithManager`, the registration
skeleton's first role test. (Was `OGBrawlerSimulatableComponent-guards.md` G-02.)

**As shipped:**
> ⛔ NEVER HasAuthority() HERE -- it is always true on a non-replicated actor and would
> disagree with the world-mode gate the manager picks its own role with. §3

**Correction (R0, carried from the game component's entry).** The stated mechanism is about the wrong actor. "Always true
on a non-replicated actor" describes the session owner (the brawler's sets `bReplicates = false`); the component's owner is
a replicated pawn. The prohibition stands for the other half of the sentence:
`TraitsT::SessionOwner::instanceFor(world, isAuthority)` returns the owner of the session that `UESimulationSession::instanceFor`
finds registered for this component's world and that role, and the session registers its role from
`netMode != NM_Client` of its world (`refuseToStart`). Only the same world-level predicate is guaranteed to pick the session
owner of this component's own world; an actor-role test answers differently for an actor spawned locally on a client.

**Consequence.** Since og-session-runtime task 16b the registry is keyed by world as well as role, so a component asking
for the other role finds nothing in its own world: it retries and asserts after `kMaxRegistrationAttempts`. Until then the
registry held one slot per role for the whole process, and in a single-process PIE the lookup found the OTHER world's
session and registered into the wrong storage with nothing failing loudly.

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
`beginTierConsumer` on both roles. The tier itself arrives once per connection, as og-sim-net's `ConnectionTier` Control
message since og-input-latency task P5-7 (the per-connection relay's property before).

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

**What breaks if it moves.** Since og-input-latency task P5-7 the session checks this evaluation against the character's
og-sim-net roster entry: the game's key hook asks `clientBindStep` with the same engine role, and `tryRegister` `checkf`s that
the provider's presence matches the entry's local index. A second, differently-worded role test would disagree with that
check. (Until P5-7 the sibling's provider latch was this exact evaluation too.)

**Correction (R0).** The game component's entry said "`m_hasLocalInputProvider` latches this exact evaluation (G-15)":
that member and G-15 are the sibling's since og-ue-simulationcomponentandmanagersplit task 20b.

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-01 — RETIRED by og-input-latency task P5-8 (2026-11-18): its statement was deleted

**Was:** *"a stage overflow is counted on the host, never absorbed"*. Its tag sat on the overflow count in
`stageRelayedInput`, above `if (outcome.droppedOldest && host != nullptr) host->noteStageOverflowDrop();`. (It was
`OGBrawlerSimulatableComponent-guards.md` G-13 before og-ue-simulationcomponentandmanagersplit task 21.) The shipped
fence read:

> ⛔ COUNTED, NOT ABSORBED, and on the HOST -- the one input loss this side can see. §6

**Why it retired.** Task P5-8 deleted the binding's net half, the staging write with it. The function had no caller since
og-input-latency task P5-7: og-sim-net's server stages each character's relays itself, in a per-character
`RelayRingStager`. The fact the entry protected, that a stage overflow is recorded rather than absorbed, is the stager's
now: it counts each drop and prints the count as `stageOverflowDrops=` on its `[RelayFlush]` line (rationale §10).

### G-02 — RETIRED by og-input-latency task P5-7 (2026-11-18): its statement was deleted

**Was:** *"no netcode policy below the fence; the order is fence, one RTT sample, core loop"*. Its tag sat on the first statement of `receiveRemoteMoveBundle`, the net-sync RPC's typed tail.

**Why it retired.** Since og-input-latency task P5-7 inputs arrive on og-net and og-sim-net's server runs the reception (one
RTT sample per bundle, then the coordinator's `receiveInputBundle`; `SimNetServer-guards.md` holds that path's guards). The
RPC has no caller, and the tail's body is a one-shot `Warning`; the coordinator's type no longer accepts the engine handle the
old body passed, so it could not have stayed. P5-8 deletes the function.

### G-03 — RETIRED by og-input-latency task P5-7 (2026-11-18): its statement was deleted

**Was:** *"resolve the root connection once"*. Its tag sat on that tail's root-connection read.

**Why it retired.** Since og-input-latency task P5-7 inputs arrive on og-net and og-sim-net's server runs the reception (one
RTT sample per bundle, then the coordinator's `receiveInputBundle`; `SimNetServer-guards.md` holds that path's guards). The
RPC has no caller, and the tail's body is a one-shot `Warning`; the coordinator's type no longer accepts the engine handle the
old body passed, so it could not have stayed. P5-8 deletes the function.

### G-04 — RETIRED by og-input-latency task P5-7 (2026-11-18): its statement was deleted

**Was:** *"the no-wire fallback is adapter-side; the core only ever sees a valid wire"*. Its tag sat on that tail's no-wire branch.

**Why it retired.** Since og-input-latency task P5-7 inputs arrive on og-net and og-sim-net's server runs the reception (one
RTT sample per bundle, then the coordinator's `receiveInputBundle`; `SimNetServer-guards.md` holds that path's guards). The
RPC has no caller, and the tail's body is a one-shot `Warning`; the coordinator's type no longer accepts the engine handle the
old body passed, so it could not have stayed. P5-8 deletes the function.

### G-05 — RETIRED by og-input-latency task P5-7 (2026-11-18): its statement was deleted

**Was:** *"one RTT sample per bundle, never per slot"*. Its tag sat on that tail's `noteRttSample` call.

**Why it retired.** Since og-input-latency task P5-7 inputs arrive on og-net and og-sim-net's server runs the reception (one
RTT sample per bundle, then the coordinator's `receiveInputBundle`; `SimNetServer-guards.md` holds that path's guards). The
RPC has no caller, and the tail's body is a one-shot `Warning`; the coordinator's type no longer accepts the engine handle the
old body passed, so it could not have stayed. P5-8 deletes the function.

### G-06 — RETIRED by og-input-latency task P5-7 (2026-11-18): its statement was deleted

**Was:** *"the delay is applied exactly once, park-to-release, on the original capture tick"*. Its tag sat on that tail's `receiveInputBundle` call.

**Why it retired.** Since og-input-latency task P5-7 inputs arrive on og-net and og-sim-net's server runs the reception (one
RTT sample per bundle, then the coordinator's `receiveInputBundle`; `SimNetServer-guards.md` holds that path's guards). The
RPC has no caller, and the tail's body is a one-shot `Warning`; the coordinator's type no longer accepts the engine handle the
old body passed, so it could not have stayed. P5-8 deletes the function.

### G-10 — RETIRED by og-input-latency task P5-7 (2026-11-18): its statement was deleted

**Was:** *"spawn the relay host BEFORE registering the delivery route (ordering)"*. Its tag sat on the input relay host's spawn in `tryRegisterWithNewFramework`.

**Why it retired.** og-sim-net's server stages and sends each character's relays and delivers through the character's
channel since og-input-latency task P5-7, so there is no relay host and no route to register (rationale §9.5, §9.6).

### G-11 — RETIRED by og-input-latency task P5-7 (2026-11-18): its statement was deleted

**Was:** *"the delivery route is registered once, authority only"*. Its tag sat on the session's delivery-route registration in `tryRegisterWithNewFramework`.

**Why it retired.** og-sim-net's server stages and sends each character's relays and delivers through the character's
channel since og-input-latency task P5-7, so there is no relay host and no route to register (rationale §9.5, §9.6).
