<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: isNetSyncBound -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: onCorrectionStateAccepted -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: forgetReceptionOwner -- removed by og-input-latency task P5-7 (the switch to og-net); kept in the history -->
<!-- ================= DECLARED LINT ESCAPES ==================================
<!-- lint-external-ref: ChaosTickBackendUImpl.cpp -- deleted by og-session-runtime task 3 (the brawler's Chaos backend arm: the Chaos host, the Chaos binding and their four docs); kept in the history -->
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: UWorld::OnPostTickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FPlatformTime::Seconds -- Unreal Engine function, outside every scan root -->
<!-- lint-external-ref: SendTo -- Unreal Engine socket method, outside every scan root -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root -->
<!-- lint-external-ref: PublicDefinitions -- Unreal Build Tool module-rules list, outside every scan root -->
<!-- lint-external-ref: PrivateDefinitions -- Unreal Build Tool module-rules list, outside every scan root -->
<!-- lint-external-ref: net.IpConnectionUseSendTasks=1 -- Unreal Engine console variable and value, outside every scan root -->
<!-- lint-external-ref: MaxNetTickRate -- Unreal Engine net-driver setting, outside every scan root -->
# `UELatencyBudgetDiagnostics` — rationale

Companion to `UELatencyBudgetDiagnostics.h` and `UELatencyBudgetDiagnostics.cpp`. Both source files carry no prose: a
two-line docs pointer, code, assertion messages, and one-line tags `⛔G-nn` → an entry in
`UELatencyBudgetDiagnostics-guards.md`.

> **Licence.** This tier is **MPL-2.0**, matching the code it describes.

* **§1** says what the template is and why it is diagnostics.
* **§2** is what it needs from its traits and from the game, including the two-phase input-sample read.
* **§3** is the gate, `OG_LATENCY_BUDGET_DIAGNOSTICS`, per module, and what a build with it at 0 contains.
* **§4** is how the template reads production state, and the compile-time check on it.
* **§5** is the fence test, the machine check that the diagnostics feed no production output.
* **§6** is the template's own stamp points, their threads and their measurement error.
* **§7** is the log lines and the category.
* **§8** is how it is built: a header-only template instantiated by the game, and a `.cpp` with one definition.
* **§9** is the provenance.
* **§10** is og-ue-simulationcomponentandmanagersplit task 24a: the owner is the session, and the weak-lambda owner stays the actor.
* **§11** is og-input-latency task 6: the late-heal hop `H6L`, where its three stamps come from, and why a resimulation's
  use is read from the observation ring.
* **§12** is og-input-latency task P5-5: the server's correction-tick walk moved into the session, and the stampAt door (both
  deleted by og-session-runtime task 14).
* **§13** is og-input-latency task P5-17: an immediate client's step-thread doors.
* **§14** is og-input-latency task P5-20: the authority runner's doors, on every dedicated server since og-session-runtime task 14.

---

## §1 What the template is

`UELatencyBudgetDiagnostics<TraitsT>` runs og-simulation's latency-budget probe (`latencyBudget::LatencyBudgetProbe`,
`OGSimulation/LatencyBudgetProbe.h`, whose own rationale explains keys, joins, counters, windows and percentiles) for one
session (`UESimulationSession`, since og-ue-simulationcomponentandmanagersplit task 24a; before, the session owner itself): it creates the probe at the
owner's begin of play, wraps every client input provider, walks every
simulatable on every step to post mailbox events, drains the mailbox every frame and logs at `Warning` every 10 s. The
probe's only output is its log lines, `[LatencyBudget]` and `[LatencyBudget.Window]`, and nothing reads them back
(until og-session-runtime task 5 a backend host's diagnostics view could add a per-window line of its own, §7). Deleting it and every caller changes no
production behaviour and no shipped telemetry, so it is diagnostics by og-simulation's own test
(`DiagnosticsConventions.md` §2: "who calls it, and what breaks if it is deleted").

That test puts it under the codebase's three structural markers:

1. **A grouped view.** One class holds the whole feature, and its type is the marker. So, as
   `DiagnosticsConventions.md` §1 requires of a view, **no member carries a `Diagnostic` prefix or infix**: <!-- lint-anchor-ignore: `Diagnostic` here is a naming INFIX under discussion, not a type this file claims exists -->
   `m_probe`, not a prefixed name. The owner holds one member of this type and exposes it through a `get`/`edit`
   pair, `getLatencyBudgetDiagnostics()` (const) and `editLatencyBudgetDiagnostics()`; `instanceFor` finds the second
   one. The owner's own call sites read `editLatencyBudgetDiagnostics().x(...)`, and a game's per-component sites reach
   the role's instance through `withInstance`, so the channel is named at the call, not the fact
   (`DiagnosticsConventions.md` §4).
2. **A fenced block.** Every data member sits in one block, guarded as `G-01`, which exists only while the gate is 1
   (§3).
3. **A fence test** (§5).

**The verbs.** `post_Internal`, `stampAfterStep_Internal`, `postRelayedReads_Internal`, `stampStepEnd_Internal` and the
step's and the runner's doors (§13, §14) write probe stamps;
`noteWireSample` writes a probe sample (`note*`, `DiagnosticsConventions.md` §3); `wrapInputProvider` adds the capture stamp;
`onPostTickFlush` is the `H3` and `H3c` end; `tick_GameThread` drains, stamps `H7`, and closes the window. The game-thread stamp
doors (stamp, stampAt and the correction's) and `tick_GameThread`'s `H3c` stamp went with the in-frame server
(og-session-runtime task 14): no stamp site runs on the game thread since. `begin`
creates the probe and its two mailboxes and binds `onPostTickFlush`; `end` unbinds it; `forgetCharacter` drops a departed
character's lane. `withInstance(world, isAuthority, f)` runs a function on that world's instance of the role, if there is
one; nothing calls it today. `clockCountsInWindow` and
`restartClockWindow` are a backend host's reads and writes of the clock counts.

**Threads.** `noteWireSample`, `onPostTickFlush`, `tick_GameThread`, `begin`, `end`, `forgetCharacter`, `withInstance`,
`clockCountsInWindow` and `restartClockWindow` are game-thread; `post_Internal`, `stampAfterStep_Internal`,
`postRelayedReads_Internal`, `stampStepEnd_Internal`, the `_Internal` doors and the wrapped provider run on the step's thread
(a client's worker, or the authority runner).

---

## §2 What it needs from the traits and from the game

The template names no game type. Everything typed comes from `TraitsT`:

| member of `TraitsT` | what the template does with it |
|---|---|
| `PlayerInput` | the provider's result type: `InputProvider` is `std::function<PlayerInput(const SimulationTimeStep&, const LocalInputCache<PlayerInput>&)>`, which the owner asserts is its own provider type, so the wrapper never converts (a second heap-allocated callable per registration) |
| `Simulatable` | the type argument of the storage walks and of the three input-resolution reads, `getLastUsedCaptureTick`, `relayedReadObservations` and `findRemoteInputCache` |
| `Component` | the registering component `wrapInputProvider` and `wrap` take (until og-input-latency task P5-7 also the element type of the session's delayed-input map, whose correction-buffer tick `tick_GameThread` read; the walk reads og-sim-net channels since, §4) |
| `SessionOwner` | the session's owner: its static `instanceFor(const UWorld*, bool)` (by world and role since og-session-runtime task 16b) and `editLatencyBudgetDiagnostics()`, which `instanceFor` calls (the owner's forwards to its session's). The template's const reference `m_owner` (§4) is to the session, `UESimulationSession<TraitsT>`, since og-ue-simulationcomponentandmanagersplit task 24a (to the owner before), and it reads the session's `m_manager`, `m_storage` and `m_inputResolution` (until og-session-runtime task 14 also, through the session's correction-tick walk, the in-frame server's registered channels, §12) |
| `Game` | two static hooks, below |
| `InputSampleSource` | the value `inputSampleSource` returns and the wrapper's lambda captures |

**The input sample is two-phase.** The `H1` start, the time the game last sampled the player's input, is read in two
steps that run on different threads:

* `wrap` calls `Game::inputSampleSource(owner)` **once, at registration, on the game thread**, outside the lambda, and the
  lambda captures the `InputSampleSource` it returns.
* The lambda calls `Game::inputSampledSeconds(source)` **once per provider call, on the step's thread**. The game returns
  `0.0` when the source is empty, and a time of `0.0` posts no `InputSampled` stamp.

So the step thread reads only the value the game published for it (a relaxed atomic, in the first game), and never
resolves the source through the component on that thread. A single per-call hook taking the component would move that
resolution onto every step; `G-02` forbids it.

`inputSampledSeconds` exists only while the gate is 1, as the value it reads does; `inputSampleSource` is a production
read and compiles in both states.

---

## §3 The gate, `OG_LATENCY_BUDGET_DIAGNOSTICS`

**One macro,** defined in this header as `#ifndef OG_LATENCY_BUDGET_DIAGNOSTICS` / `#define
OG_LATENCY_BUDGET_DIAGNOSTICS (!UE_BUILD_SHIPPING)`, named once so that a Development build can force it to 0 per module
for the fence test (§5). Its value is a property of the **including** module: the template is header-only and is
instantiated by the game module (§8), so the game module's value decides what the instantiation contains. This module
compiles the header only in its own `.cpp`.

**At 1** (every non-Shipping build) the template is the feature.

**At 0** (Shipping, and the fence build):

* **The class is empty.** Its members are inline no-ops with the same signatures, so **no call site carries an
  `#if`**.
* **`wrapInputProvider` returns the provider unchanged.** Provider presence is the core's local-versus-remote identity
  test. The public `wrapInputProvider` is written once, above the gate, and `checkf`s in both states that a present
  provider stays present and an empty one empty; only the private `wrap` differs per state.
* **`withInstance` does nothing,** so a game's per-site owner lookups that exist only to stamp are not made. The
  function it would run is still compiled at 1 and 0 alike, which keeps every call site's code checked in both states,
  and is discarded at 0.
* **`begin` binds nothing,** so `UWorld::OnPostTickFlush` has no latency subscriber.
* **`tick_GameThread` does nothing.** Until og-session-runtime task 5 it took the backend host's diagnostics view, and
  this state called neither of the view's hooks.

**The category is gated on the build, not on the macro.** `LogOGLatencyBudget` is declared (exported) in this header and
defined in this module's `.cpp`, both under `!UE_BUILD_SHIPPING`. Each module may force the macro to a different value,
and a category declared and defined under the macro would leave one pairing unlinkable: this module at 0 and the game at
1 would import a category nobody defines. Under `!UE_BUILD_SHIPPING` every pairing links, and Shipping is unchanged. ⚠
The price: in a non-Shipping build forced to 0 the category is still declared, so code that logs to it outside the gate
compiles there; only Shipping, where it is not declared, makes such a line a compile error.

---

## §4 Reading production state

The template reads its session's `m_manager`, `m_storage` and `m_inputResolution`, and the client clock through `m_manager`
(until og-session-runtime task 14 also the in-frame server's correction ticks, through the session's const correction-tick walk,
§12). It reaches them
through **one const reference**, `m_owner`, to the session (§10): the session and its owner each declare the template a
friend, and every read goes through that const reference, so it can call only const members
(`currentIntegratedTick`, `runsPrediction`, `getClientClock`, `forEachSimulatable`'s const overload,
`getLastUsedCaptureTick`, `getDiagnostics`, `findRemoteInputCache`'s const overload).

⭐ **Machine-checked.** A `static_assert` beside `m_owner` requires its referenced type to be `const`. A diagnostics class
with a mutable reference to its owner, which befriends it, would be a write door into every private member of the
owner; the assertion makes that a compile error, checked when the owner instantiates the template.

**Why an owner's member initializer should be `{ std::as_const(*this) }`.** With a plain `{ *this }` (an implicit
conversion of `*this` to the const reference the constructor takes), MSVC (the toolchain of 2026-10) compiled every
**later** default member initializer of the owner that names `*this` or a sibling as though `this` were const, failing
with C2665, "Conversion loses qualifiers". The explicit `std::as_const` compiles, and a revert to `{ *this }` is
therefore a compile error, not a silent change.

**The correction reads were const too.** Until og-session-runtime task 14 the in-frame server's walk (`tick_GameThread`'s own
loop until og-input-latency task P5-5, the session's const correction-tick walk since, §12) read each channel through
`std::as_const`, so only the channel's `const` `getSyncedCorrectionStateBuffer` was called; the non-`const` one marks the channel
dirty for the next flush, which a diagnostic must never do. Until P5-7 the walk read the game component's `const` getter, which
reached the net-sync sibling through the binding's sibling pointer. The authority runner reads each correction tick itself
(§14). Every path from the template into production state is const.

**Until og-input-latency task P5-7 the loop skipped a component that was not bound** (`UESimulationSession-guards.md` G-29,
retired with it; this file's G-03 until P5-5), with the binding's `isNetSyncBound()`, a `const` query. A channel has no
binding, so the skip went; the paragraph below is its record. The
getter dereferences the sibling pointer, which the component's teardown clears. No shape that runs today reaches the
skip: the unregistration (`unregisterFromNewFramework`, the session's own since og-ue-simulationcomponentandmanagersplit
task 28, the owner's before) calls the session's `forgetReceptionOwner`, which erases the component's map entry (the owner
erased it itself until task 26a), the
component's `EndPlay` runs that unregistration before it unbinds, and a component is added to the map only after its
registration, which the owning actor's bind precedes. So the skip changes no output; it keeps the read from becoming a
null dereference if a later change reorders the teardown or the map's erase (a lead ruling of that task, 2026-10-21). It is
a skip in the walk, which is instantiated only by this class's call, compiled only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1,
rather than an erase of the session's map entry, which is the session's own production state and which this class must not
write. Since og-input-latency task P5-5 the walk is the session's `const` member; the skip moved with it (§12).

---

## §5 The fence test

The fence must be machine-checked. For this template the check is a build and two harness sessions:

1. Force `OG_LATENCY_BUDGET_DIAGNOSTICS` to 0 in the **game** module (a `PublicDefinitions` or `PrivateDefinitions` entry
   in its Build.cs, copied first and restored byte-for-byte afterwards), leaving this module at its default.
2. Build the Development Editor in that state, in each physics configuration the game has. That build compiling is the
   first half: no ungated code names a member that the gate removes.
3. Run the game's headless harness session per configuration and compare each with the baseline: every non-latency
   invariant and every `--identical` prefix must PASS, and the latency tags must be absent.
4. The reverse pairing, this module at 0 (a `PrivateDefinitions` entry in its Build.cs) and the game at 1, must link.
   That is the category rule of §3.

So the diagnostics feed no production output: removing them leaves every other line of the session within the
same-binary noise.

---

## §6 The template's stamp points, threads and errors

Times are `FPlatformTime::Seconds` (one monotonic clock per process). "GT" is the game thread and "PT" the step's
thread (a worker on a client, the authority runner on a dedicated server). The game's own stamp sites (its input sample, its
send and receive sites, its step hooks) are the game's documentation; these are the template's:

| point | where (thread) | known error |
|---|---|---|
| `InputSampled`, `Captured` (the `H1` end, the `H2` start) | the wrapper `wrapInputProvider` returns, after the inner provider returns (PT) | The `H1` start is the game's sample time; a sample time read as a relaxed atomic is unordered with the caches the capture reads, so a capture racing a refresh can pair a new time with the previous frame's values: at most one frame, rare. |
| `Handed` → `LeftProcess` (the `H3` and `H3c` ends) | `onPostTickFlush`, bound to `UWorld::OnPostTickFlush` (GT) | See *the `H3` hook* below. |
| `Handed` (the server's `H3c` start) | `stampCorrectionHanded_Internal`, after each authority batch, when a character's correction tick changed (the runner; §14) | The correction leaves at the runner's hand-over to og-net, at this time (§14). Until og-session-runtime task 14 the in-frame server stamped it in `tick_GameThread` from the session's correction-tick walk, a lower bound late by the owner's frame. |
| `Consumed` (the server's `H4` end) | `stampAfterStep_Internal`, for the capture tick `getLastUsedCaptureTick` reports after the step, stamped with that step's start time (PT) | An underrun step reports the sentinel and stamps nothing. |
| `Consumed` (the client's `H6` end; `H6c` through a pending stamp) | `stampAfterStep_Internal`, through `postRelayedReads_Internal`: the relayed-read observation for the step's tick names the applied capture tick (PT); the pending `ClientReceived` → `Consumed` stamp at the start of the first step after receipt | Only reads that applied a relayed capture stamp an end; a miss stamps nothing and the capture finalizes as `noEnd`. `H6c` measures when a correction is available to the next step, not when a resimulation applies it. |
| `Healed` (the client's `H6L` end), by a use | `postRelayedReads_Internal` (PT): a prediction `Hit` for the step's tick, beside its `Consumed`; and a resimulation's `Hit` for an earlier tick, found as a changed observation (§11) | A resimulation's use is stamped with the start time of the prediction step after the replay: late by at most the replay's length. |
| `Healed` (the client's `H6L` end), by a correction | `stampClientReceived_Internal`'s State branch, which the og-net client host calls at the correction's drain, with the same time as its `ClientReceived` (the step's thread, §13; the game-thread correction door until og-session-runtime task 12, deleted by task 14) | `dA` is the newest relayed capture's delay stamp for that character, one value for the lane (§11). |
| `StepEnd` (the `H5` end, the `H7` start) | `stampStepEnd_Internal`, at each step's end, per simulatable, for `currentIntegratedTick` (PT) | `noEnd` counts the ticks a simulatable's state was not written; a correction rotation skips some on purpose. |
| `Rendered` (the `H7` end) | `tick_GameThread`: the newest pending tick per simulatable only (GT) | Not the photon: the render poses interpolate behind the simulation, and the render thread and GPU add one or two frames more. |
| `WIRE` | `noteWireSample`, half the round trip it is given (GT): the server host's base `rttMs`, from the runner's RTT publish; a client's `noteWireSample_Internal` (§13) | Half of a round trip assumes symmetric paths. |

**Threads — the probe adds no unsynchronized crossing.** The probe is **game-thread-owned** and has no internal lock.
Everything reaches it one of three ways:

| path | used by | synchronization |
|---|---|---|
| `noteWireSample` | the server host's base `rttMs`, from the RTT publish (until og-session-runtime task 14 also the game-thread stamp doors, every game-thread stamp site's) | same thread as the owner; it `checkf`s that it runs on the game thread |
| `post_Internal` → `m_mailbox` → drained in `tick_GameThread` | every step-thread site | the mailbox's release/acquire pair (`LatencyBudgetProbe-guards.md` G-01) |
| `m_clockMailbox` | the client clock's skip/stall/hard-resync counts | the same mailbox type |

Every step-thread read the template makes is same-thread with its writer: the storage walk, `getLastUsedCaptureTick`
(written while the core collects input on that thread), the relayed-read observation ring (noted there too, by
prediction and resimulation reads alike), `currentIntegratedTick` and the client clock's diagnostics, and the relay store's
newest delay stamp, which the same drain writes (§13). The template makes no game-thread read of production state since
og-session-runtime task 14: its two were the in-frame server's correction-tick walk and the game-thread correction door's read of
the relay store. `m_seenReads_Physics` is the template's own, and only the step's thread touches it (G-05). On a dedicated
server the step thread is the authority runner, the mailbox's one producer (§14).

**The hop masks.** `begin` creates the probe with the role's hops and, since og-input-latency task P5-7, the role's og-net
hops ORed in: `kServerHops | kServerNetHops` (adds `N4`) on the authority, `kClientHops | kClientNetHops` (adds `N6`, `N6c`,
and since og-input-latency task P5-37 `N6u`) on a client. `N6u` ends at the `Consumed` stamp `postRelayedReads_Internal`
already makes for a relayed capture at the start of the step that read it (`stampAfterStep_Internal`'s `stepStartSeconds`,
taken after that step's drain), so it needed no new stamp here. The og-net hosts stamp `NetReceived` at the datagram's receive time (through the mailbox doors'
`recv` argument, §13, §14), which those hops start from
(`LatencyBudgetProbe-rationale.md`).

**Lifetime.** `begin` creates the probe and the two mailboxes; nothing resets them before the template is destroyed, so
an owner that calls `begin` before its backend starts stepping can never have a step reach a null or freed mailbox.
The wrapper captures `this`, the diagnostics object, which the owner holds as a member, so it lives as long as the core
container the owner keeps the callable in.

### The `H3` hook

`UWorld::OnPostTickFlush` is broadcast by the world's tick immediately after the net drivers' `TickFlush`, and it is the
earliest engine hook after it. With IpNetDriver the actual `SendTo` runs inside `TickFlush` (the connection's flush of
its send buffer), so the stamp is late by whatever `TickFlush` did after this connection's send: sub-millisecond with one connection,
longer on a busy server. It is early, or simply wrong, whenever the message did not leave at this flush:

* a client rendering faster than its `MaxNetTickRate` (120) skips a connection tick, so the packet leaves at the next
  frame;
* a saturated connection or the bandwidth limit defers the bunch;
* `net.IpConnectionUseSendTasks=1` (default 0) moves IpNetDriver's `SendTo` onto a task after the flush. The cvar is
  read only by UIpConnection (IpConnection.cpp, lines 179-510), so it does not apply to the og-net driver, the project's default: its connection hands
  each packet to og-net's engine lane, which queues it as a command, and og-net's own thread sends it after the flush
  (the `commandDelay` of og-net's `[OGNet.Window.Endpoint]` line measures that queue);
* a replicated property is sent only when the replication system polls it, so a write can wait a poll period.

In each case `H3` reads as though the message left this frame. Read `H3` as a lower bound.

---

## §7 Log lines and the category

```
[LatencyBudget] role=Client hop=H2 p50=0.131 p95=0.273 p99=0.308 max=0.397 n=601 unit=ms noStart=0 noEnd=0 outOfOrder=0 overflow=0
[LatencyBudget] role=Client hop=H6L p50=10.112 p95=110.017 p99=120.062 max=150.052 n=599 unit=ms noStart=0 noEnd=0 outOfOrder=0 overflow=0 healedByUse=421 healedByCorrection=178
[LatencyBudget.Window] role=Client seconds=10.01 stale=0 duplicate=2402 laneOverflow=0 mailboxDropped=0
```

The `H6L` line carries the two heal counters **appended** after the fields every hop line has, so a pattern that
reads the common fields matches it unchanged (og-input-latency task 6; the counters are
`LatencyBudgetProbe-rationale.md` §4). It comes after `H7` and before `WIRE` in a window.

One `[LatencyBudget]` line per enabled hop and one `[LatencyBudget.Window]` line per window, both under
**`LogOGLatencyBudget`** at `Warning` (one category per probe family, `UESimulationLogRoutes-rationale.md` §3). They are
logged with `UE_LOG` directly, never as core SIMLOG strings, so `RouteOGMessage` has no arm for them. Until
og-session-runtime task 5 `tick_GameThread` then called the backend view's latency-window log hook (and, before the window
closed, its sample hook), which only the former Chaos host filled with a window line of its own; the tick-driver host's
hooks were empty, and task 5 removed the parameter and both calls. `duplicate` counts the copies first-stamp-wins discarded (an input re-sent in several redundant
bundles, a capture carried by more than one relay round), so a large number there is the expected state, not a fault.

---

## §8 How it is built

* **Header-only.** Every member is defined in the header: the in-class ones where they were, the out-of-line ones after
  the class, inside one `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair under `#if OG_LATENCY_BUDGET_DIAGNOSTICS`. The game
  instantiates the template with its traits; `tick_GameThread` is instantiated by the session's call (in
  `onPostPhysicsStep` since og-ue-simulationcomponentandmanagersplit task 27a, the owner's before; a member template over
  its backend's view until og-session-runtime task 5), so no explicit instantiation is needed.
* ⚠ **The pair does not decide how the members compile.** MSVC compiles a template's instantiations, explicit ones
  included, with the `#pragma optimize` state in force at the **end** of the translation unit that instantiates them, and
  every file of the game ends with `OGSIM_OPTIMIZE_ON`. So in a Development build the members and their lambdas compile
  optimized, where the game module's non-template class compiled them unoptimized in its own `.cpp` (measured by task
  19a, with a scratch compile of the rule). The output is the same; a debugger sees optimized frames there.
* **The `.cpp`** holds only the `LogOGLatencyBudget` definition. This module builds per file, so it is its own object file.
* **Includes.** The always-included part is the probe, the time step and the input cache; the gated part adds the
  headers the out-of-line bodies name (input resolution, the prediction clock, the world). The correction-buffer codec was
  one of them until og-input-latency task P5-5 moved its one reader, the server's walk, into the session, which includes it
  (§12).

---

## §9 Provenance

Written by og-tickdriver task 18 as a game-module class over one game's manager; made this template and moved into
OGSimulationUnreal, under MPL-2.0, by og-ue-simulationcomponentandmanagersplit task 19a, with the statements unchanged
(the type names come from the traits, the input sample from the two game hooks of §2). The probe's host side was written
by og-simulationscheduler-withjolt task 1.

---

## §10 The session as the owner (og-ue-simulationcomponentandmanagersplit task 24a)

Since task 24a the template is a member of OGSimulationUnreal's `UESimulationSession<TraitsT>` (`UESimulationSession-rationale.md`),
declared first after the session's two references, and its constructor takes `const UESimulationSession<TraitsT>&`; the
header forward-declares the session. Its member initializer is still `{ std::as_const(*this) }`, now the session's.

* **The reads.** `m_manager`, `m_storage` and `m_inputResolution` are the session's, read with the same spellings; the
  session befriends the template. The delayed-input map stayed the owner's, read under the owner's friend line for the
  template, until task 26a moved it into the session; it was read through the session's correction-tick walk from
  og-input-latency task P5-5 (§12), the walk read og-sim-net channels from P5-7, and og-session-runtime task 14 deleted it. The `const` assertion beside `m_owner` holds for
  the session reference; its message names the session as the one class that befriends this template (*R0, P5-5:* it said
  "the session's and the owner's friend", but the owner's friend line went with task 27b).
* **The weak-lambda owner stays the actor.** `begin` binds the world's post-tick flush with `AddWeakLambda(&m_owner.owner(), …)`:
  a delegate's weak object must be a `UObject`, the session is not one, and the owner is the object the binding tested
  before.
* **`instanceFor`** still finds the instance through `TraitsT::SessionOwner::instanceFor(world, isAuthority)` (the world
  since og-session-runtime task 16b) and the owner's
  `editLatencyBudgetDiagnostics()`, which forwards to the session's. (Since og-ue-simulationcomponentandmanagersplit task
  25 the owner's `instanceFor` itself forwards to the session's registry, `UESimulationSession-rationale.md` §9.)
* **F1.** Nothing about how the members compile changed: they were already template members compiled optimized (§8).

---

## §11 The late-heal hop `H6L` (og-input-latency task 6)

`H6L` is `ClientReceived → Healed` on the input stream, a client hop (`kClientHops`). The probe's side, and why `H6`
was not redefined, is `LatencyBudgetProbe-rationale.md` §10. This section is where this template stamps `Healed`.

**The question the task asked first: do resimulation steps reach `stampAfterStep_Internal` with an observation for
their tick?** No, on either backend, verified 2026-11-11:

* **Jolt.** `SimulationStepDriver::runClientTick` (`SimulationStepDriver.h:120-167`) calls `resimulateIfRequested`
  (`:129`) before `beforeSimulate` (`:133`); the replay loop there (`:221-236`) calls `onGameSimulation`, the world step and
  `onPostGameSimulation` per replayed tick and never calls a step hook. `beforePhysics` (`:150`), which reaches
  `onStepStarted` (`TickDriverStepHooks.h:151-155`) and so `stampAfterStep_Internal`
  (og-sim-session's `SessionTickGame::onStepStarted` since og-session-runtime task 26; the brawler binding's at the time),
  runs once per physics step, for the prediction tick only. (Line numbers re-checked 2026-11-19, og-input-latency task
  P5-33.)
* **Chaos.** `OnPreSimulate_Internal` calls `stampLatencyAfterStep_Internal` only `if (!isResimulating)`
  (`ChaosTickBackendUImpl.cpp:266-267`). This bullet is the 2026-11-11 record of the former Chaos configuration, removed
  by og-session-runtime tasks 3–6; Jolt is the only backend since.

A resimulation's reads are still recorded: `collectResimInputForCharacter` notes a `RelayedReadObservation` for the
replayed tick from its reference row (`SimulationInputResolution.h:1557-1558`) and from its scheduled read
(`:1576`), into the same per-character ring the prediction read writes (last write wins per tick).

**So a resimulation's use is read back from that ring, on the step's thread, at the next prediction step.**
`postRelayedReads_Internal` walks the ring once per step per remote character. It keeps a shadow of every slot it has
seen (`m_seenReads_Physics`):

* the observation **for the step's own tick** is the prediction read: `Consumed` when it applied a capture (`H6`,
  unchanged), and `Healed` as well when it was a `Hit`;
* an observation **for an earlier tick that differs from the shadow** was rewritten since the last step, which only a
  resimulation does: a `Hit` there posts `stampHealedIfPending` (G-04). An unchanged slot posts nothing, so a replay
  that re-serves what the prediction served costs no mailbox traffic;
* the first walk of a character only primes the shadow, so a character the template meets mid-session does not post
  one event per filled slot.

This is the "stamp at the resimulation read site through the physics-thread mailbox" the task asked for, realised
from the record that read site already writes, rather than with a new hook in the engine-free core: the core files
are outside this task, and the ring is the record `H6` already reads.

**Its timing.** The prediction step's start time is taken in `beforeSimulate`, after the replay (`TickDriverStepHooks`
G-03), so a resimulation's `Healed` is late by at most the replay's length. It is never early.

**The shadow's size** is bounded on the step's thread: when a character not in it appears and it already holds
`ProbeConfig{}.maxLanes` characters (the most lanes the probe tracks), it is cleared and every character re-primes,
which only skips the resimulation heals of that one step. It is never erased from the game thread (G-05).

**A correction's heal.** The binding's `onCorrectionStateAccepted` called the diagnostics' game-thread correction door instead
of the plain stamp (the og-net client host's drain since og-input-latency task P5-7, through §13's mailbox door since P5-17;
the game-thread door was deleted by og-session-runtime task 14). It takes one time and stamps the state stream's `ClientReceived` (the `H6c` start, as before) and then
`stampHealedByCorrection(id, tick, dA, now)`, where `dA` is `findLatest().dA` of the character's relay store, read on
the game thread that writes it. A character with no relay store (this client's own) or no arrival yet heals nothing.
One `dA` serves the whole lane: exact while the delay is steady, which it is in every shipped configuration (the
floor pins it); after a delay change, the captures still pending were applied with the older delay and are judged
with the newer one.

**What the counters say.** `healedByUse` is the late (and on-time) population served at its scheduled tick by a
step; `healedByCorrection` is the population whose first heal was a correction. Their sum is `n`. `noEnd` is the
captures neither reached before the probe's 64-tick ring reused the slot.

---

## §12 The session seams (og-input-latency task P5-5)

og-input-latency's switch to og-net (its task P5-7) changes what the server's `Handed` stamps read and adds stamps at a time
og-net measured. Task P5-5 prepared both, refactor-only.

**The server's walk moved into the session.** `tick_GameThread`'s server branch walked the session's delayed-input map: the
null check on the weak pointer, the skip of an unbound component (its G-03, retired), the correction-buffer tick read and the
skip of tick 0, then the per-id `Handed` stamp when the tick changed. The walk, up to and including the skip of tick 0, became
the session's const correction-tick walk; this class kept the rest, in the lambda it passed: the last-handed map, the
comparison and the stamp. The statements, their order and the time (`nowSeconds`, taken once before the walk) were unchanged,
so the `[LatencyBudget]` lines were too. The switch made the walk iterate the server's channels. og-session-runtime task 14
deleted the walk, the map and the stamp with the in-frame server: the authority runner stamps its corrections itself (§14).

**stampAt** was the game-thread stamp with the time passed in, for og-net's `NetReceived` point at the receive time og-net
reports (P5 design §3.3); the hosts called it from og-input-latency task P5-7 (`UESimNetHost-rationale.md` §4), and the hop
masks include the og-net hops (§6). og-session-runtime task 14 deleted it with the plain stamp: their last callers were the
base server host's game-thread stamps, the in-frame server's. The mailbox doors take the time as an argument instead (§13, §14).

## §13 An immediate client's step-thread doors (og-input-latency task P5-17)

A client drains og-sim-net and sends its inputs on the step's thread (`UESimulationSession-rationale.md` §18, §24), so three
stamp sites are step-thread doors. Each one posts, through the mailbox (§6's second path), what a game-thread door stamped
for the frame-locked client until og-session-runtime task 12, in the same order and with the same times:

| door | posts | was (the frame-locked client's, until og-session-runtime task 12) |
|---|---|---|
| `stampClientReceived_Internal(stream, lane, tick, recv)` | `ClientReceived` at now; for the State stream, the heal-by-correction (below); then `NetReceived` at `recv` | the host's game-thread stamp and stampAt (Input), the correction door and stampAt (State); all three deleted by og-session-runtime task 14 |
| `noteWireSample_Internal(rtt)` | a `Sample` of `Hop::Wire`, half the round trip | `noteWireSample` |
| `stampHandedAndLeft_Internal(lane, captureTick)` | `Handed` and `LeftProcess`, both at now | the channel's handed callback's `stamp(Handed)`, then `onPostTickFlush`'s `LeftProcess` |

- **The heal-by-correction rides the mailbox as its own item.** The probe's `stampHealedByCorrection` walks the probe's slots,
  which only the game thread may touch, and `latencyBudget::Event` (og-sim) has no kind for it. So the mailbox carries a
  `PostedEvent`, an `Event` plus the heal's delay ticks and a flag, and `tick_GameThread` calls `stampHealedByCorrection` for a
  flagged item and `apply` for the rest. The delay (`dA`, the newest relayed capture's) is read on the step, from the relay
  store, which the same drain writes. One queue keeps the order: the heal is applied after the
  `ClientReceived` stamps that came before it and before the ones after it, as it was on the game thread.
- **`LeftProcess` is the send.** The transport hands each input set to the og-net endpoint at once
  (`SimNetTransport-rationale.md` §12), so the input leaves the process where it is handed, on the step, not at the frame's
  `TickFlush`. `H3` then reads about 0 on a client; `onPostTickFlush` still runs and finds those slots already
  stamped.
- **The single producer.** On a client every producer of the mailbox is the step's thread: these three, the
  capture wrapper and the step hooks. The world session's teardown drain runs on the game thread, but only after the steps
  have stopped (`UESimulationSession-rationale.md` §18.6), so producers never overlap.
- **The gate.** With `OG_LATENCY_BUDGET_DIAGNOSTICS` at 0 the three doors are empty, like the rest.

## §14 The authority runner's doors (og-input-latency task P5-20)

A dedicated server steps, drains og-sim-net and sends on the runner (`UESimulationSession-rationale.md` §19, §24), so the
server's stamp sites are runner doors. Each door posts through the mailbox (§6's second path):

| door | posts | was (the loop off, until og-session-runtime task 14) |
|---|---|---|
| `stampServerReceived_Internal(lane, captureTick, recv)` | `ServerReceived` at now, then `NetReceived` at `recv` (Input) | the base server host's game-thread stamp and stampAt (`UESimNetHost.h`) |
| `stampHandedAndLeft_Internal(lane, captureTick)` (§13's door) | `Handed` and `LeftProcess`, both at now (Input) | the base server host's game-thread `Handed` stamp, then `onPostTickFlush`'s `LeftProcess` |
| `stampCorrectionHanded_Internal(lane, correctionTick, handed)` | `Handed` and `LeftProcess`, both at `handed` (State), once per new correction tick of the lane | `tick_GameThread`'s walk of the session's correction ticks at its own now, then `onPostTickFlush`'s `LeftProcess` |

- **Who calls them.** The first two are the server host's port (`UESimNetServerHostPort`, `UESimNetLanes-rationale.md` §12),
  which og-sim-net's server host core calls on the runner. The third is og-sim-session's `SessionNetHooks::afterBatch` (the session's `stampCorrectionsHanded_Runner` until
  og-session-runtime task 26), after og-sim-net's `afterInlineBatch` has flushed the batch's corrections; it reads each
  character's correction tick from og-sim-net's own channel, the runner's object, with `SessionNetHooks::correctionTickOf`.
- **`LeftProcess` is the hand-over.** The transport hands each relay and each correction to the og-net endpoint at once
  (`SimNetTransport-rationale.md` §7), as a client hands its inputs (§13), so the server's `H3` (Input) and `H3c` (State) read about
  0, and `H5` (State, `StepEnd` → `Handed`) ends at the runner's hand-over instead of the game thread's next latency
  tick. `onPostTickFlush` still runs and finds those slots stamped.
- **`tick_GameThread` has no correction walk.** The runner's walk reads storage ids and og-sim-net's channels, both the
  runner's. Until og-session-runtime task 14 the in-frame server walked the channels on the game thread.
- **The dedup map is the runner's.** `m_lastHandedCorrectionTick_Runner` is written only on the runner; `forgetCharacter` (the game thread) never touches it (G-05). Without the forget a new character that
  reuses a sim id starts from the old one's last tick, which costs at most one `Handed` stamp, the one whose tick equals it.
- **The single producer.** On a dedicated server every producer of the mailbox is the runner: these three doors, the capture
  wrapper (a dedicated server captures no input) and the step hooks. The game thread only drains.
- **The gate.** With `OG_LATENCY_BUDGET_DIAGNOSTICS` at 0 the doors are empty, like the rest.
