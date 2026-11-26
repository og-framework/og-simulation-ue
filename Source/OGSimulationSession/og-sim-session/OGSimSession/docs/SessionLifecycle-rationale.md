<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: forgetSimNetCharacter -- moved by og-session-runtime task 31 (the session's composition, start and stop are og-sim-session's SessionRuntime; UESimulationSession is the thin Unreal adapter); kept in the history -->
<!-- lint-external-ref: slotKeyOf -- moved by og-session-runtime task 31 (the world build and the stepping to og-sim-session's SessionRuntime: buildWorld, startStepping, stopStepping; the static import to ueSessionEngine::importStatics; the present to UESessionPresentation; the world config to TickDriverBackendUImpl::worldConfig; slotKeyOf is slotKeyFor); kept in the history -->
# `SessionLifecycle.h` — rationale

Companion to og-sim-session's `OGSimSession/SessionLifecycle.h` (og-session-runtime tasks 29 and 30). Its prohibitions are in
`SessionLifecycle-guards.md`.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What it is, and where it came from

`ogsession::SessionLifecycle<PortT>` is the prediction role's registration and unregistration of characters: requests in from the
frame thread, applied on the step thread at the start of its drain, outcomes back to the frame thread (design §3.4, the
lifecycle queue). The authority's path (the runner's body creation and registration) is the second class of the header,
`SessionAuthorityLifecycle<PortT>` (§10, og-session-runtime task 30): a role-split pair, one class per role, each constructor
`OG_CHECK`ing its role.

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
requests and its outcomes are `SessionAuthorityLifecycle`'s own vectors and mutex since task 30 (§10.2): a session has one role,
so it has one request vector and one outcome vector, of that role's class.

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
component, and the key is opaque to this class (`SessionTypes-rationale.md` §3). The authority's key is the same component's
address (§10.4).

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

---

## §10 The authority's path: `SessionAuthorityLifecycle`

### §10.1 Where it came from

og-session-runtime task 30 moved the authority's registration and unregistration out of the Unreal session. Step (a) cut
`postRunnerRequest`, `runRunnerRequests_Owner`, `onSimNetLifecycleUnregistering` (its authority half, after task 29's split),
`physicsSlotFor`, `recordAuthoritativeSlot`, `forgetSimNetCharacter`, `collectLifecycleOutcomes_GameThread`,
`tryRegisterOnRunner`, `bodiesResolvable_Runner`, `onRunnerRegisterApplied_Runner`, `requestRunnerUnregister_GameThread` and
`releaseOnRunner_Runner` into this header unchanged (OGSimulation `221b701`); step (b) made the engine-free half of them this
class, with the members `m_runnerRequests`, `m_lifecycleBridgeMutex`, the runner fields of `m_pendingRegistrations`,
`m_runnerBodyKeysById`, `m_authoritativeSlotsByActor` and the authority's use of `m_systemsNotified_Step`.

### §10.2 The sequence, its doors and their threads

The order is the Unreal session's (design §3.4, the authority's lifecycle queue), unchanged:

1. **Frame, `tryRegister(request, simulatable, hooks)`** (`TryRegister`): the first call keeps the simulatable in a shared record;
   with the runner free, it asks the bodies for a request (`makeRequest`, `SessionBodies`), tells the adapter (`hooks.bodiesRequested()`:
   the Unreal adapter holds the component strongly until the bodies answer) and posts a `CreateBodies` request; `Pending`.
2. **Runner, `applyRequests()`** (`ApplyLifecycleRequests`, the authority's Step row: the runner), the first thing of every wake
   (`SessionNetHooks::beforePump`, before og-sim-net's drain): `create` and `slotOf` → the outcome `BodiesBound{slot}`, or
   `BodiesPending` when the bodies could not be bound.
3. **Frame, `pollOutcomes(fn)`** (`PollOutcomes`): the adapter sees every outcome (it lets go of the component on `BodiesBound` and
   `BodiesPending`), then the record takes it. On the next `tryRegister` with the bodies bound: the slot is recorded (§10.5) and the
   adapter runs the game's `afterBodiesCreated` (`hooks.bodiesCreated(simulatable)`); `Pending`. The next call confirms: it posts a
   `Register` request with the engine's facts (`RegisterRequest`'s owner connection, local index and name, read by the adapter in
   that call) and the bound slot.
4. **Runner**: the bodies resolvable? (the port's `bodiesResolvable`; else `BodiesUnresolvable`, and the frame posts the
   `Register` again) → `SimNetServer::registerCharacter`, applied in the same wake's drain: on a channel the executor is told
   (`⛔G-06`), the bodies' `onRegistered`, the outcome `Registered`; on none (a roster refusal) the bodies are released and the
   outcome is `Refused`.
5. **Frame**: `tryRegister` logs `tryRegister: registered … isAuthority=1` and answers `Ready` (the adapter then runs
   `afterRegistered` and the render target), or logs the refusal (`[Error]`) and answers `Refused`.

Unregistration: **frame, `requestUnregister(simId, entityKey)`** (`RequestUnregister`) answers whether the id was registered (the
adapter's host work needs it), forgets the record, the bodies' key and the recorded slot, and posts an `Unregister` request when
the runner holds bodies for it and still runs; **runner**: through og-sim-net's queue once a registration was posted (`⛔G-07`),
else a direct release; og-sim-net's unregistering hook (`onUnregistering`, `DrainInbox`, `⛔G-08`) tells the executor before the
facade; the `applied` callback releases the bodies. The `applied` hooks check `DrainInbox` too.

**No `SessionOp` row was added.** Every door's thread is in task 22's table: the frame doors use the frame rows, the runner doors
the authority's Step row (the runner is its owner thread). Two hidden cases (`[SessionLifecycleDeath]`) show the checks fire:
`applyRequests` and `onUnregistering` on the frame thread.

### §10.3 Requests are data

The queue's element is `AuthorityRequest<BodyRequest, Simulatable>`, a `std::variant` of three structs:
`CreateBodies{simId, bodies, simulatable}`, `Register{simId, key, physicsSlot, ownerConnection, localIndex, debugName,
simulatable}` and `Unregister{simId, key, registerPosted}`. Before task 30 the queue held `TUniqueFunction<void()>` closures that
captured the session (`this`), a shared simulatable and a pointer to the component's static data; the runner ran them. Now the
class itself runs each request (`run(…)`), so nothing it needs is captured:

- the session's state is the class's own, and its engine calls go through the port and the bodies;
- the static-data pointer is a field of the bodies' own request (the Unreal model's `Request::staticData`), made on the frame
  thread by `makeRequest` with the component held strongly until the bodies answer, as before;
- the simulatable is a `std::shared_ptr` field. It is shared on purpose, as before: the frame thread and the runner hand it back
  and forth across three steps (the runner binds its bodies, the frame runs `afterBodiesCreated` on it, the runner moves it into
  og-sim-net), and an unregistration can drop the frame's record while a request still holds it.

A `static_assert` in the class checks that the element holds no callable field. Exactly what it checks:

- **The walk.** Every alternative of `AuthorityRequest` must be an aggregate, and so must the bodies' request. No field list
  exists: a request's fields are counted by brace-initializing it from `AnyField` values (the largest count that compiles, up
  to `kMaxFields` = 8; a ninth field fails a `static_assert`), then bound with that many structured bindings. Each field is
  checked, and a field that is itself an aggregate class is walked the same way: the bodies' request inside `CreateBodies`,
  and an aggregate inside it. A field added to a request is found by the count, with nothing to update.
- **A miscount does not pass.** Too many names or too few break the binding; a count of zero on a request that is not empty
  fails a `static_assert`. An array field is counted element by element and breaks the binding (clang-cl and MSVC); a
  reference to a function cannot be counted on clang-cl (the `static_assert`) and is found as callable on MSVC.
- **Callable** (`authorityRequest::kIsCallable`, after removing references and cv-qualifiers): a function, a pointer to a
  function, a member pointer (to a function or to data); a class that names an `operator()` of any arity, a template (a generic
  lambda), private or inherited, found because a probe deriving from the class and from a base that declares `operator()`
  makes the name ambiguous; a class callable with no argument (`std::is_invocable`); and a union or a final class, refused
  because the probe cannot derive from it. So a `std::function` of any signature, a lambda of any arity and a member such as
  `std::function<void(Channel*)> applied` are each refused.
- **Not checked:** what a pointer or a smart pointer points to, and the members of a field whose class is not an aggregate
  (`std::string`, `std::shared_ptr`, `std::optional` are checked as types only). A
  `std::optional<std::function<…>>` or `std::shared_ptr<std::function<…>>` field passes. Nor is **a class callable only
  through a conversion to a function pointer that takes arguments** (a surrogate call: `operator void(*)(int)()`, so
  `request.field(1)` compiles): it names no `operator()`, and `std::is_invocable` is asked with no argument. A conversion to
  a pointer to a function of no argument is caught (the `std::is_invocable` arm). Found by og-session-runtime task 30's re-review
  probes on clang-cl and MSVC 14.38.
- **A base class: the compilers differ.** A request (or the bodies' request) that derives from a base is still an aggregate.
  A callable in the base is caught on both compilers, and an empty base with a callable field breaks the binding on both.
  A base that holds only data does not: MSVC binds the fields and passes the request, clang-cl breaks the structured binding
  ("both it and its base class have non-static data members"), so such a request compiles on MSVC and fails on clang. No
  request has a base today. Whether to refuse a base outright is task 33b's decision; this paragraph is the record until then
  (og-session-runtime task 30's re-review probes).

The namespace `authorityRequestSelfCheck` holds the check's own cases (data passes; a unary, generic, private and inherited
`operator()`, pointers to functions and members, a final class, a union, a callable field, one inside an aggregate field, one
in the bodies' request and a request that is not an aggregate are each refused).

### §10.4 The bodies: `SessionBodies`

The runtime calls the bodies through the concept `SessionBodies` (`makeRequest`, `create`, `slotOf`, `onRegistered`, `release`,
a `Request` type that carries its bodies' `key`, and the `Simulatable`), with a reference shape and one negative per requirement
in `sessionBodiesSelfCheck`. The tick backend's wrapper, `TickDriverBackendUImpl`, models it: its `makeBodyRequest_GameThread`,
`createBodies_Runner`, `onRegistered_Runner` and `releaseBodies_Runner` became `makeRequest`, `create` (with `slotOf` beside it),
`onRegistered` and `release`. `create` answers whether the bodies were bound and `slotOf` the slot; the class composes them as the
old `createBodies_Runner` did (`create ? slotOf(key) : nullopt`).

**The entity key** is the simulatable component's address on the authority too, as on the client (§8): `makeRequest` needs the
component (its static data, its character's capsule), and the key is opaque to this class. The bodies' key is the pawn's address
(the Unreal model's `slotKeyOf`), carried in the bodies' request; the port's `bodyKeyOf(entityKey)` gives it for an
unregistration with no record.

### §10.5 The authority's slots

`authoritativeSlotFor(bodyKey)` and `recordAuthoritativeSlot(bodyKey, slot)` (frame, `TryRegister`) are the old session's
`physicsSlotFor` (its authority arm) and `recordAuthoritativeSlot`, which the tick backend calls through the game's
`callWithSlotRequest`. The map is keyed by the bodies' key, the pawn's address the callers already pass, where the old map was
keyed by the pawn itself (`const AActor*`): the same value, with no engine type. `requestUnregister` forgets the slot, and
`forgetRegistered()` (the adapter's `endPlay`) clears the slots and the registered ids.

### §10.6 The outcomes

The authority's three body outcomes are `LifecycleOutcome` alternatives (`BodiesBound{simId, slot}`, `BodiesPending{simId}`,
`BodiesUnresolvable{simId}`, `SessionTypes-rationale.md` §6), with `Registered` and `Refused`. `pollOutcomes` hands every outcome
to the adapter first and then updates its record, as the old switch erased the held component before it updated the record.
An outcome for an id with no record is dropped, as before: the old switch dropped it unless the record was on the runner, and
every authority record is.

### §10.7 What the adapter keeps, and the port

The Unreal adapter keeps the component maps and strong references (`m_registeredComponentsById`, `m_componentsHeldForRunner`),
the game's `afterBodiesCreated` / `afterRegistered` / `onUnregistered`, the render target (`onRegisteredHost_GameThread`,
`onUnregisteredHost_GameThread`), the latency forget and the `NewFramework: unregistered` line (the host work, as on the client),
and the engine facts it reads into each `RegisterRequest`. `SessionAuthorityLifecyclePort` (one negative per requirement in
`sessionAuthorityLifecycleSelfCheck`): `server()` (a pointer, null with no server), `bodies()`, `bodiesResolvable(simulatable)`,
`bodyKeyOf(entityKey)`, `runnerStopped()` (the adapter's: set when the runner joined), the executor's two notifies,
`inStorage(id)`, `logRouted(line)`. The server models `SessionAuthorityServer` (og-sim-net's `registerCharacter` /
`unregisterCharacter`), the hooks `SessionAuthorityRegisterHooks` (`bodiesRequested`, `bodiesCreated`).

### §10.8 Behaviour, against the old path

The same calls in the same order on every path the old code took. The differences:
- the registration's log line now precedes the adapter's map insert (both on the frame thread, no reader between; as task 29's
  B9);
- the `checkf`s of the old `recordAuthoritativeSlot` are `OG_CHECK`s without the name and the slots in their messages;
- the old `tryRegisterOnRunner`'s character `checkf` before the register post is gone (the same component was checked at its body
  request and at `afterBodiesCreated`);
- `forgetRegistered()` at `endPlay` also forgets the bodies' keys, which the old `endPlay` left: a later unregistration of such an id
  found its key there, but posts nothing once the runner has stopped, so the key was never used.

### §10.9 Tests

`tests/SessionLifecycleAuthorityTest.cpp` drives the class on a real runner thread: a `std::thread` that configures the
authority's thread context as its owner (free-running, as `SimulationTickDriver::runAuthorityLoop` does on the Unreal runner) and
runs each wake through og-sim-session's real `SessionNetHooks::beforePump`. A fake bodies provider (a slot table keyed by the
bodies' key, as `TickDriverBackendUImpl` over `JoltTickWorldHost`) and a fake og-sim-net server (its lifecycle queue and drain,
over og-sim-net's real `RosterAuthority`) record what they are asked. Every case runs the same script against a test-local
transcription of the Unreal code at task 30 (a) (each function with its file:line at `221b701`) and compares the event lists:
the full register sequence; a roster refusal releases the bodies (og-input-latency P5-23's capacity and duplicate-owner-slot
cases: a later registration with the same bodies' key finds no slot taken); an unregistration before the register was posted;
an unregistration after the runner stopped; three registrations in one wake keep their order (the requests, then the drain);
and 500 seeded scripts.
