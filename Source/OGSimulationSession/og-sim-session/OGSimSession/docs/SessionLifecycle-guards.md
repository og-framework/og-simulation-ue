<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionLifecycle.h` — guards

Prohibitions with a tagged site in og-sim-session's `OGSimSession/SessionLifecycle.h`. Ids are opaque, stable, and retired rather
than reused. The reasoning lives in `SessionLifecycle-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

Every entry here moved from `UESimulationSession-guards.md` (OGSimulationUnreal) with the code it marks, by og-session-runtime
task 29: the prediction role's registration and unregistration left the Unreal session for this engine-free header. Each entry
names its old id; those ids are retired there with this file's id as their successor. What the compiler or a check holds is not
here: each door's thread (its first statement, `SessionThreadChecks::checkOp` against `SessionThreads.h`'s table, the drain's
hooks against the inbox row or, inside the teardown's stopped drain, the stopped-drain row), the role (an `OG_CHECK` in the
constructor), the port (three concepts with one negative per requirement), and, in the Unreal adapter, each line's category and
level (a `static_assert` in `UESimulationSession.h`).

---

## G-01 — a queued unregistration notifies the executor before it enqueues, behind both tests

**Tag site:** `SessionLifecycle.h`, `enqueueUnregister`, directly above
`if (m_systemsNotified.erase(id) != 0u && m_port.inStorage(id))`, before `client.unregisterCharacter`.

**The prohibition.** ⛔ Keep the systems executor's notify here, before the enqueue, and keep both conditions. Do not move the
notify into the unregister's `applied` hook, and do not drop the notified-set test or the storage test. The enqueued job runs at
the top of the drain that follows (og-sim-net's `SimNetClient::drain` applies its lifecycle queue before the inbox), and
og-sim-net's `unregisterSimulatable` takes the character out of storage before the `applied` hook runs.

**What breaks if the edit is made.**
* Moved into `applied`, or after the drain: a system's `onCharacterUnregistered` reaches a character storage has already freed.
  The brawler's hit routing takes `&view.get<SimulatableBrawler>(id)` there (`BrawlerHitRoutingSystem.h`), so it reads a
  destroyed object.
* The notified-set test dropped: the executor is told of the removal of a character it was never told of (one whose register
  applied while an unregister was pending, `⛔G-02`). The brawler's systems erase by id or by pointer and tolerate it; the pair
  of notifies no longer matches (the tests' balance checks count it).
* The storage test dropped: the notify runs for an id og-sim-net already removed (a roster Remove in an earlier drain), and the
  hook's `view.get<>(id)` has nothing to return.

**What breaks if the tag moves.** Nothing mechanical; `SessionLifecyclePredictionTest.cpp`'s balance checks fail on the first two
edits.

*R0, og-session-runtime task 29:* verified on the tree: `SimNetClient::drain` runs `m_lifecycle.applyPending()` before the
inbox; `brawlerHitRouting::System::onCharacterUnregistered` (`BrawlerHitRoutingSystem.h`) takes `&view.get<SimulatableBrawler>(id)`.

*Was `UESimulationSession-guards.md` G-31 (og-input-latency task P5-17), moved by og-session-runtime task 29.*

---

## G-02 — the registration's executor notify is in its `applied` hook, and not when an unregister is pending

**Tag site:** `SessionLifecycle.h`, `onRegisterApplied`, directly above
`if (m_unregisterRequested.find(id) == m_unregisterRequested.end())`, which guards `m_port.notifyRegistered`.

**The prohibition.** ⛔ The queued registration's executor notify stays here: after og-sim-net's `registerSimulatable` put the
character in storage, on the drain's thread, and only when no unregister request for the id is pending. Do not move it to the
frame thread's finish (`tryRegister` on a queued id), and do not drop the pending-unregister test.

**What breaks if the edit is made.**
* Moved to the frame thread: the notify writes the systems' maps (the hit routing's root-body map, the score map) while a step
  may be reading them on a worker, and every step between the apply and the finish integrated the character with no
  hit-routing or hit-detection entry, so hits on it went unrouted.
* The pending-unregister test dropped: a character unregistered before its registration applied (its pawn ended play in
  between) is registered with the executor by its register job and removed from storage by the unregister job that follows it in
  the same drain, with no notify between (`⛔G-01` found it not notified). The executor keeps a pointer to a destroyed character.
  `SessionLifecycle.AnUnregisterWhileARegisterIsQueued` fails (seen: og-session-runtime task 29's poison P2b).

**What breaks if the tag moves.** Nothing mechanical.

*Was `UESimulationSession-guards.md` G-32 (og-input-latency task P5-17), moved by og-session-runtime task 29.*

---

## G-03 — a queued or registered character's bodies are released only after the step applied its unregistration

**Tag site:** `SessionLifecycle.h`, `requestUnregister`, directly above `return UnregisterRelease::AfterApplied;`.

**The prohibition.** ⛔ When the id is queued or registered and the steps run, the request is posted to the step and the answer is
`AfterApplied`. Do not answer `Now` here. The adapter runs its host work with the release when the answer is `Now` and without it
otherwise (`UESimulationSession::unregisterFromNewFramework` passes `release == UnregisterRelease::Now` as `releaseBodiesNow`),
holds the component, and releases its bodies on the `Unregistered` outcome (`pollOutcomes`) or at the teardown.

**What breaks if the edit is made.** The backend's unregistration releases the character's physics slot, unbinding its bodies from
the bind table on the frame thread (`JoltTickWorldHost::release`). The character is still in storage until a step's drain
applies the queued unregister, and that drain runs at the start of the first step that begins after the request is posted
(`SessionNetHooks::receiveForStep`, which applies the requests before it drains). The frame thread does not wait for the steps: a
step that began before the request was posted, past its drain, can still be running, and it integrates a character whose bodies
are no longer bound. (Before og-input-latency task P5-30 the drain ran after the integrate, so the first step after the post
integrated the character too.)

**What breaks if the tag moves.** Nothing mechanical; a leave in a smoke may survive it when no step falls in the window.

*R0, og-session-runtime task 29:* verified on the tree: `JoltTickWorldHost::release` calls `bindTable().releaseSlot(*slot)`;
`TickDriverBackendUImpl::onUnregistered_GameThread` reaches it on the frame thread.

*Was `UESimulationSession-guards.md` G-33 (og-input-latency task P5-17), moved by og-session-runtime task 29. Its site was the
adapter's `unregisterCharacterHostOnly(id, &owner, /*releaseBodiesNow=*/false)`; the decision is now this answer, which the
adapter's one call follows.*

---

## G-04 — the teardown stops the steps before it unregisters anything

**Tag site:** `SessionLifecycle.h`, `teardown`, directly above `m_port.stopSteps();`.

**The prohibition.** ⛔ The teardown's first statement after its once-only test stops the steps and waits for them. Do not move
anything above it, and do not drop it on the grounds that the adapter's `endPlay` already stopped them: the og-net lanes' stop
handler (`onSimNetLanesStopping`) reaches the teardown with the steps still running.

**What breaks if the edit is made.** The teardown's frame-thread apply, its drain and its unregistrations then run while a step
drains the same og-sim-net client and runs the same simulation on a worker: two drains (og-sim's `SimInbox::drain` `OG_CHECK`s a
second consumer; a lifecycle queue applied by both is a data race), and storage changed under a running step.

**What breaks if the tag moves.** Nothing mechanical; a client that leaves while its steps run hits it.

*R0, og-session-runtime task 29:* the old entry named "og-sim-net's in-drain flag"; the check is og-sim's inbox
(`SimInbox::drain`, "one consumer at a time").

*Was `UESimulationSession-guards.md` G-34 (og-input-latency task P5-17), moved by og-session-runtime task 29.*

---

## G-05 — og-sim-net's unregistering hook notifies the executor only for a notified id still in storage

**Tag site:** `SessionLifecycle.h`, `onUnregistering`, directly above
`if (m_systemsNotified.erase(id) != 0u && m_port.inStorage(id))`.

**The prohibition.** ⛔ Keep both tests, and keep the notify in this hook. Do not drop the `erase` test, do not drop the storage
test, and do not move the notify after the hook returns.

**The consequence.** og-sim-net calls the hook before the facade of every queue-registered character's unregister (a roster
Remove, a queued unregister; `SimNetHost-rationale.md` §4a), on the drain's thread. Without the `erase` test the executor hears of
one unregistration twice (a queued unregister already notified at its enqueue, `⛔G-01`). Without the storage test the brawler's
`brawlerHitRouting::System::onCharacterUnregistered` takes `view.get<SimulatableBrawler>(id)` of a character that is not there.
Moved out of the hook, the notify runs after the facade destroyed the storage entry, or not at all for a roster Remove: the
hit-routing system then keeps a pointer to the destroyed brawler (og-input-latency task P5-17's F1).

**What breaks if the tag moves.** Nothing mechanical. The og-sim-net cases `SimNetLifecycleUnregisteringHook.*` pin that the hook
is called before the facade; `SessionLifecycle.ARosterRemovalBeforeAndAfterTheApply` pins the notify's place
(`notifyUnregistered` before `facade.unregister`).

*Was the prediction half of `UESimulationSession-guards.md` G-35 (og-input-latency task P5-19), split and moved by
og-session-runtime task 29; the authority's half is `UESimulationSession-guards.md` G-41.*
