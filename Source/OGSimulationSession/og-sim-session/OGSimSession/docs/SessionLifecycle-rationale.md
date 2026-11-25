<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionLifecycle.h` — rationale

Companion to og-sim-session's `OGSimSession/SessionLifecycle.h` (og-session-runtime task 29). Its prohibitions are in
`SessionLifecycle-guards.md`.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What it is, and where it came from

`ogsession::SessionLifecycle<PortT>` is the prediction role's registration and unregistration of characters: requests in from the
frame thread, applied on the step thread at the start of its drain, outcomes back to the frame thread (design §3.4, the
lifecycle queue). The authority's path (the runner's body creation and registration) is task 30's and is still in the Unreal
session; the constructor `OG_CHECK`s the prediction role.

Its bodies are the Unreal session's (`Source/OGSimulationUnreal/UESimulationSession.h`), moved by og-session-runtime task 29:
step (a) cut `onSimNetRegisteredRemoved`, `applyUnregisterRequests_Step`, `enqueueQueuedUnregister_Step`,
`onQueuedRegisterApplied_Step`, `tryRegisterQueued`, `finishQueuedRegistration`, `requestQueuedUnregister_GameThread` and
`teardownImmediateClient_GameThread` into this header unchanged (OGSimulation `6d424ec`); step (b) made them this engine-free
class, with the prediction arms of `collectLifecycleOutcomes_GameThread` and the prediction half of
`onSimNetLifecycleUnregistering`, which (a) left whole because both roles shared them. The members `m_unregisterRequests`,
`m_unregisterRequested_Step`, `m_immediateStepsStopped` and the prediction use of `m_systemsNotified_Step` moved with them.

---

## §2 The doors and their threads

Each door's first statement is its `SessionThreadChecks::checkOp`, against the prediction row of `SessionThreads.h`'s table. No
row was added: every door's thread was already in the table.

| door | what it does | row (thread) |
|---|---|---|
| `tryRegister(request, simulatable, provider)` | queue a registration whose bodies exist, or finish a queued one | `TryRegister` (frame) |
| `isQueued(simId)` | whether the last `tryRegister` queued it (the adapter drops its simulatable then) | `TryRegister` (frame) |
| `requestUnregister(simId, entityKey)` | post an unregistration, and say when the bodies may go (§5) | `RequestUnregister` (frame) |
| `applyRequests(client)` | enqueue the posted unregistrations into og-sim-net's client | `ApplyLifecycleRequests` (step) |
| `pollOutcomes(fn)` | take the posted outcomes; hand the adapter the ones it acts on | `PollOutcomes` (frame) |
| `onRegisteredRemoved(simId)` | the link's handler for a roster Remove of a registered id | `DrainInbox` (step), or `DrainStopped` inside the teardown's drain |
| `onUnregistering(simId)` | og-sim-net's unregistering hook, before the facade (`⛔G-05`) | as above |
| `teardown(fn)` | stop the steps, unregister everything through the queue, drain once on the frame | `DrainStopped` (frame) |
| `detach(bindings)` | unmark the queued registrations in the link's bindings (§7) | `RequestUnregister` (frame) |

- **`applyRequests` is the first thing the step's receive does, before the drain.** `SessionNetHooks::receiveForStep` calls it
  through its port (`applyUnregisterRequests`) and then drains, so an unregistration posted before the step began is applied by
  that step's own drain (`SessionNetHooks-rationale.md`). Drain first and it waits a step, while the frame thread may already
  have run the host work (`SessionLifecycle.AnUnregisterAppliesBeforeThisStepsDrain`; seen failing, task 29's poison P1).
- **The teardown's apply and drain run on the frame thread with the steps stopped**, which the `ApplyLifecycleRequests` and
  `DrainInbox` rows forbid. The teardown checks the client's stopped-drain row, `DrainStopped` (task 24's row for the same
  drain), and sets a flag for its drain's length so that the two drain hooks check that row instead of the inbox's
  (`SessionThreads-rationale.md` §7). The applied hooks of the jobs the teardown enqueues run in that drain and check the same
  way.

---

## §3 One request vector, one outcome vector, one mutex

As in the Unreal session (`m_lifecycleBridgeMutex` over `m_unregisterRequests` and `m_lifecycleOutcomes`): the frame thread pushes
unregistration requests, the step swaps them out; the step pushes outcomes, the frame swaps them out. The authority's runner
requests and its outcomes stay on the Unreal session's own mutex and vectors until task 30.

The outcomes are `SessionTypes.h`'s `LifecycleOutcome` variant (`SessionTypes-rationale.md` §6): `Registered{simId, 0}`,
`Refused{simId, SimNetRefused}`, `Unregistered{simId}`, `RosterRemoved{simId}`. `pollOutcomes` consumes the first two itself
(they finish a queued registration, §4) and hands the adapter only what it acts on:

- `Unregistered` for an id whose request answered `AfterApplied` (the adapter releases the held component's bodies), and
- `RosterRemoved` for a registered id (the adapter runs its host work with the release, and records the id for the pawn's later
  end of play).

A `RosterRemoved` for an id whose unregistration is pending is dropped (its own `Unregistered` does the release); for a queued id
it marks the registration as not held. This is the Unreal session's switch (`collectLifecycleOutcomes_GameThread` at `6d424ec`,
`UESimulationSession.h` :1698-1769), arm by arm. The frame thread's own state for it: the queued registrations (`m_queued`, each
with its applied / refused / roster-removed flags), the registered ids (`m_registered`) and the ids with a pending unregistration
(`m_unregisterPending`). Each mirrors one of the adapter's maps (its pending records' `queued` flag, `m_registeredComponentsById`,
`m_pendingUnregisters`), changed at the same points, so the answers are the ones the maps gave.

---

## §4 Registration

`tryRegister` is called each frame the adapter's registration is pending, after the adapter's own frame door has created the
bodies and found them resolvable (design Q12; the frame door stays in the adapter for this initiative).

1. **The gate** is the link's bindings, asked and marked in one hold (`markRegisteredIfReady`): `Wait` stays `Pending`; a role
   mismatch is the `OG_CHECK` the Unreal session's `checkf` was (the adapter's bind step checked the engine's role against the
   roster before); `Ready` marks the id registered. No client, a stopped session, or an id outside 1..255 stays `Pending`.
2. **The enqueue**: `registerCharacter(simId, simulatable, provider, applied)` with the port's wrapped provider (in Unreal the
   latency wrapper, which reads the component; the request's entity key is the component, §8). The simulatable and the provider
   are taken only here; on `Wait` they stay with the caller.
3. **The apply**, on the drain's thread: og-sim-net registers the character with its channel as the owner, then calls `applied`
   (`onRegisterApplied`), which installs a local character's input stamps, notifies the systems executor unless an
   unregistration is pending (`⛔G-02`), and posts `Registered`. A refusal (og-sim-net's `noChannel` or `registered`, which it
   reports as a null channel; its `[SimNet.Lifecycle]` line names which) posts `Refused`.
4. **The finish**, at the next `tryRegister` of a queued id, after the adapter collected the outcomes: on a refusal, or a roster
   Remove before the finish, the bindings are unmarked, the line `[Error]tryRegister: id=%u: the og-sim-net client's queued
   registration did not hold (%s); …` is logged and the answer is `Refused`; once applied, the line `tryRegister: registered
   simulatable id=%u isAuthority=0` is logged and the answer is `Ready`. The adapter then runs the host work (its registered map,
   the game's `afterRegistered`, the backend's `onRegistered_GameThread`).

**The lines.** Both were `UE_LOG(LogOGMgmt, …)` in `finishQueuedRegistration`, Error and Log. They are formatted with `snprintf`
(`sessionLifecycleLines`) and sent through the port's `logRouted`; in Unreal `RouteOGLevelLine` reads the `[Error]` token for the
level and routes `tryRegister:` to LogOGMgmt (its route-table row, plain level Log). The Unreal session asserts each line's route
and level, and `SessionLifecycleFormatParity.FStringPrintfEqualsSnprintfForBothLines` compares the two formattings byte for byte.
The order within the finish changed only in where the line is logged relative to the adapter's map insert (frame-thread state
with no reader in between).

---

## §5 Unregistration

`requestUnregister(simId, entityKey)` answers `UnregisterRelease::Now` or `AfterApplied`:

- **`Now`** when the id never reached the queue (not queued, not registered) or the session's steps have stopped: the adapter
  runs its host work with the bodies' release at once; nothing of it is in og-sim.
- **`AfterApplied`** otherwise (`⛔G-03`): the id is posted to the step, and the adapter holds the component and runs its host work
  without the release. The step takes the posted ids at its start (`applyRequests`, `⛔G-01`'s notify then the enqueue); the
  `applied` hook clears the channel's input stamps and posts `Unregistered`; the frame thread releases the bodies then.

A queued registration not yet applied is unregistered by the same request: the queue is FIFO, the register applies first, and its
`applied` sees the pending unregistration and does not notify the executor (`⛔G-02`), so the executor never holds the character.

A roster Remove of a registered id: og-sim-net calls the unregistering hook (`⛔G-05`, the executor's notify) and the facade, then
the link's host, whose handler reaches `onRegisteredRemoved` on the drain's thread, which posts `RosterRemoved`. *R0, task 29:*
the Unreal rationale's §18.5 called the hook missing ("og-sim-net calls no host hook before its own unregister"); it has been
there since og-input-latency task P5-19 (`SimNetClient::unregisterLifecycle`, `SimNetClient-guards.md` G-13).

**A second request for an id whose first is still pending** (a pawn that re-registers the same sim id and ends play again before
the first unregistration applied) answers `AfterApplied` again, and the adapter's map holds the newer component in place of the
older; the first `Unregistered` releases the newer one's bodies and the second finds nothing. That is the Unreal session's
behaviour before the move (`requestQueuedUnregister_GameThread`, `m_pendingUnregisters[id] = …`), kept; the randomized tests
count it (2 of 1,737 requests over the 1,000 serialized scripts) instead of hiding it.

---

## §6 Teardown

`teardown(fn)` runs once, from whichever comes first in the adapter: the og-net lanes' stop handler or `endPlay`. It answers
`false` when it already ran.

1. **The steps stop first** (`⛔G-04`): the port's `stopSteps` (in Unreal the backend's `onEndPlayBegin`, which waits for the step
   tasks). From here the frame thread is the only thread that touches the simulation, and later registrations stay `Pending`.
2. **Everything queued or registered is unregistered through the queue**: the posted requests, every queued registration and
   every registered id, each with its executor notify; then one frame-thread drain (the port's `drainWhileStopped`, which in
   Unreal marks the clock's stopped drain around the lanes' `drainClientWhileStopped_GameThread`). So og-sim-net's client holds no
   registered character and no job that points into this session.
3. **The outcomes are collected** through `fn`, the pending set and the registered set are cleared (the adapter releases every
   held component and runs every registered character's host work after), and the queued registrations' bindings are unmarked.

The enqueue order of the live ids follows this class's containers (the queued map, then the registered set) where the Unreal
session iterated its pending-record map and its registered map: the same ids, possibly in another order. Nothing at a teardown
depends on the order of characters (the brawler's systems erase by id), and the adapter's host-work loop, which logs
`NewFramework: unregistered`, still iterates the adapter's registered map. The transcription tests compare a teardown's events
as a multiset for this reason, with `stopSteps` required first.

---

## §7 Detach without a teardown (the destructor's path)

The Unreal session's destructor detaches from the og-net lanes without `endPlay` when the world never ended play (task 28's review,
lead note under task 29). The old detach unmarked only the registered ids; a registration still queued kept its id marked in the
link's bindings, and since task 28 the link's next attach seeds the bindings, whose `OG_CHECK` (a registered id would never see
its Remove) then fires. `detach(bindings)` unmarks the queued ids too (`SessionLifecycle.TheDetachWithoutTeardownUnmarksAQueuedRegistration`:
the old transcription leaves the id marked, the new code seeds cleanly). On `endPlay`'s path the teardown has
already unmarked them and `detach` finds none.

This is the one behaviour change of the task, on a path the smoke does not take: before task 28 that id's next unattached roster
Remove `checkf`ed; after it, the seed did. After this task neither does.

---

## §8 What the adapter keeps, and the port

The adapter keeps (design Q12): the frame door (`createBodies_GameThread` and the resolvable poll, before it calls
`tryRegister`), the component maps and strong references, the game's `afterRegistered` / `onUnregistered`, the backend's
`onRegistered_GameThread` / `onUnregistered_GameThread`, and the host work (`unregisterCharacterHostOnly`).

`SessionLifecyclePort` (one negative per requirement in `sessionLifecycleSelfCheck`): the client and the bindings (pointers, null
with no link), `wrapInputProvider(request, provider)`, the executor's two notifies, `inStorage(id)`, the input stamps' install
and clear on a channel, `stopSteps()`, `drainWhileStopped()`, `logRouted(line)`. The client models
`SessionLifecycleClient` (og-sim-net's `registerCharacter` / `unregisterCharacter`), the bindings `SessionLifecycleBindings`
(`markRegisteredIfReady`, `markUnregistered`).

**The entity key.** On the client the Unreal adapter's key is the simulatable component's address: the latency wrapper needs the
component, and the key is opaque to this class (`SessionTypes-rationale.md` §3). The authority's key (the pawn's address in the
design) is task 30's choice.

---

## §9 Tests

`tests/SessionLifecyclePredictionTest.cpp` drives the class on two real threads (the test's thread is the frame thread; a worker
runs each step inside a `StepScope` of the client's thread context), through og-sim-session's real `SessionNetHooks` for the
step's receive, against a fake og-sim-net client that transcribes `SimNetClient`'s lifecycle queue and roster removal, with
og-sim-net's real `SimNetBindings`. Every case runs the same script against a test-local transcription of the Unreal code at
task 29 (a) (each function with its file:line at `6d424ec`) and compares the event lists:

- register to `Ready`; refused by the roster verdict (`Wait`) and by the drain; a roster Remove after the apply, before the apply
  (the step thread paused between its queue and its inbox while the frame thread queues), and of a registered character; an
  unregistration that the same step's drain applies; an unregistration while the registration is queued; the teardown with the
  steps stopped; the detach without a teardown;
- 1,000 seeded serialized scripts equal the transcription event for event, with the totals asserted (4,011 registration
  notifies, 4,011 unregistration notifies, 3,790 `Ready`, 65 `Refused`, 1,735 releases, 2 replaced held components);
- 1,000 seeded concurrent scripts (the steps free-running beside the frame thread) lose no outcome: notifies balanced in every
  run, every held component released or replaced, og-sim-net's client and the bindings empty after the teardown, nothing left
  to poll.

Two hidden cases (`[SessionLifecycleDeath]`) show the thread checks fire: `applyRequests` on the frame thread, and the
roster-removed hook outside a drain. A hidden survey (`[SessionLifecycleSurvey]`) prints the totals.
