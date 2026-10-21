<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- ================= DECLARED LINT ESCAPES ==================================
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: UWorld::OnPostTickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FPlatformTime::Seconds -- Unreal Engine function, outside every scan root -->
<!-- lint-external-ref: SendTo -- Unreal Engine socket method, outside every scan root -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root -->
<!-- lint-external-ref: PublicDefinitions -- Unreal Build Tool module-rules list, outside every scan root -->
<!-- lint-external-ref: PrivateDefinitions -- Unreal Build Tool module-rules list, outside every scan root -->
<!-- lint-external-ref: net.IpConnectionUseSendTasks=1 -- Unreal Engine console variable and value, outside every scan root -->
<!-- lint-external-ref: TWeakObjectPtr::Get -- Unreal Engine method, outside every scan root -->
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

---

## §1 What the template is

`UELatencyBudgetDiagnostics<TraitsT>` runs og-simulation's latency-budget probe (`latencyBudget::LatencyBudgetProbe`,
`OGSimulation/LatencyBudgetProbe.h`, whose own rationale explains keys, joins, counters, windows and percentiles) for one
session owner: it creates the probe at the owner's begin of play, wraps every client input provider, walks every
simulatable on every step to post mailbox events, drains the mailbox every frame and logs at `Warning` every 10 s. The
probe's only output is its log lines, `[LatencyBudget]` and `[LatencyBudget.Window]`, plus whatever per-window line the
backend host's diagnostics view adds (§6), and nothing reads them back. Deleting it and every caller changes no
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

**The verbs.** `stamp`, `post_Internal`, `stampAfterStep_Internal` and `stampStepEnd_Internal` write probe stamps;
`noteWireSample` writes a probe sample (`note*`, `DiagnosticsConventions.md` §3); `wrapInputProvider` adds the capture stamp;
`onPostTickFlush` is the `H3` and `H3c` end; `tick_GameThread` drains, stamps `H3c` and `H7`, and closes the window. `begin`
creates the probe and its two mailboxes and binds `onPostTickFlush`; `end` unbinds it; `forgetCharacter` drops a departed
character's lane. `withInstance` runs a function on the role's instance, if there is one. `clockCountsInWindow` and
`restartClockWindow` are a backend host's reads and writes of the clock counts.

**Threads.** `stamp`, `noteWireSample`, `onPostTickFlush`, `tick_GameThread`, `begin`, `end`, `forgetCharacter`,
`withInstance`, `clockCountsInWindow` and `restartClockWindow` are game-thread; `post_Internal`, `stampAfterStep_Internal`,
`stampStepEnd_Internal` and the wrapped provider run on the step's thread.

---

## §2 What it needs from the traits and from the game

The template names no game type. Everything typed comes from `TraitsT`:

| member of `TraitsT` | what the template does with it |
|---|---|
| `PlayerInput` | the provider's result type: `InputProvider` is `std::function<PlayerInput(const SimulationTimeStep&, const LocalInputCache<PlayerInput>&)>`, which the owner asserts is its own provider type, so the wrapper never converts (a second heap-allocated callable per registration) |
| `Simulatable` | the type argument of the storage walks and of the two input-resolution reads, `getLastUsedCaptureTick` and `relayedReadObservations` |
| `Component` | the registering component `wrapInputProvider` and `wrap` take, and the element type of the owner's delayed-input map, whose correction-buffer tick `tick_GameThread` reads through a const pointer (§4) |
| `SessionOwner` | the owner: a const reference `m_owner` (§4), its static `instanceFor(bool)` and `editLatencyBudgetDiagnostics()`, which `instanceFor` calls, and the members the template reads (`m_manager`, `m_storage`, `m_inputResolution`, `m_delayedInputComponentsById`) |
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
* **`tick_GameThread` does nothing,** so no backend host's diagnostics view is called.

**The category is gated on the build, not on the macro.** `LogOGLatencyBudget` is declared (exported) in this header and
defined in this module's `.cpp`, both under `!UE_BUILD_SHIPPING`. Each module may force the macro to a different value,
and a category declared and defined under the macro would leave one pairing unlinkable: this module at 0 and the game at
1 would import a category nobody defines. Under `!UE_BUILD_SHIPPING` every pairing links, and Shipping is unchanged. ⚠
The price: in a non-Shipping build forced to 0 the category is still declared, so code that logs to it outside the gate
compiles there; only Shipping, where it is not declared, makes such a line a compile error.

---

## §4 Reading production state

The template reads its owner's `m_manager`, `m_storage`, `m_inputResolution` and `m_delayedInputComponentsById`, and the
client clock through `m_manager`. It reaches them through **one const reference**, `m_owner`: the owner declares the
template a friend, and every read goes through that const reference, so it can call only const members
(`currentIntegratedTick`, `runsPrediction`, `getClientClock`, `forEachSimulatable`'s const overload,
`getLastUsedCaptureTick`, `getDiagnostics`).

⭐ **Machine-checked.** A `static_assert` beside `m_owner` requires its referenced type to be `const`. A diagnostics class
with a mutable reference to its owner, which befriends it, would be a write door into every private member of the
owner; the assertion makes that a compile error, checked when the owner instantiates the template.

**Why an owner's member initializer should be `{ std::as_const(*this) }`.** With a plain `{ *this }` (an implicit
conversion of `*this` to the const reference the constructor takes), MSVC (the toolchain of 2026-10) compiled every
**later** default member initializer of the owner that names `*this` or a sibling as though `this` were const, failing
with C2665, "Conversion loses qualifiers". The explicit `std::as_const` compiles, and a revert to `{ *this }` is
therefore a compile error, not a silent change.

**The component reads are const too.** `tick_GameThread` binds the pointer `TWeakObjectPtr::Get` returns from the const
map to a `const` pointer to the component, so only a `const` `getSyncedCorrectionStateBuffer` is callable, and a
component without one is a compile error here. Every path from the template into production state is therefore const.
Since og-ue-simulationcomponentandmanagersplit task 21 that getter is `UESimulatableOwnerBinding`'s, which reaches the
`const` overload of the `USimulatableNetSyncComponent` sibling through the binding's sibling pointer.

**The loop skips a component that is not bound** (G-03), with the binding's `isNetSyncBound()`, a `const` query. The
getter dereferences the sibling pointer, which the component's teardown clears. No shape that runs today reaches the
skip: the owner erases a component's map entry in its unregistration (the brawler: `unregisterFromNewFramework`), the
component's `EndPlay` runs that unregistration before it unbinds, and a component is added to the map only after its
registration, which the owning actor's bind precedes. So the skip changes no output; it keeps the read from becoming a
null dereference if a later change reorders the teardown or the map's erase (a lead ruling of that task, 2026-10-21). It is
a skip in this diagnostics loop, compiled only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1, rather than an erase of the
owner's map entry, which is production state this class must not write.

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
thread (a worker on a client, the game thread on a dedicated server). The game's own stamp sites (its input sample, its
send and receive sites, its step hooks) are the game's documentation; these are the template's:

| point | where (thread) | known error |
|---|---|---|
| `InputSampled`, `Captured` (the `H1` end, the `H2` start) | the wrapper `wrapInputProvider` returns, after the inner provider returns (PT) | The `H1` start is the game's sample time; a sample time read as a relaxed atomic is unordered with the caches the capture reads, so a capture racing a refresh can pair a new time with the previous frame's values: at most one frame, rare. |
| `Handed` → `LeftProcess` (the `H3` and `H3c` ends) | `onPostTickFlush`, bound to `UWorld::OnPostTickFlush` (GT) | See *the `H3` hook* below. |
| `Handed` (the server's `H3c` start) | `tick_GameThread`, when a component's correction-buffer tick changed since the last frame (GT) | A **lower** bound: a property leaves only when the replication system polls and sends it. The start is late by whatever the owner does on the frame before it calls `tick_GameThread`. |
| `Consumed` (the server's `H4` end) | `stampAfterStep_Internal`, for the capture tick `getLastUsedCaptureTick` reports after the step, stamped with that step's start time (PT) | An underrun step reports the sentinel and stamps nothing. |
| `Consumed` (the client's `H6` end; `H6c` through a pending stamp) | `stampAfterStep_Internal`: the relayed-read observation for the step's tick names the applied capture tick (PT); the pending `ClientReceived` → `Consumed` stamp at the start of the first step after receipt | Only reads that applied a relayed capture stamp an end; a miss stamps nothing and the capture finalizes as `noEnd`. `H6c` measures when a correction is available to the next step, not when a resimulation applies it. |
| `StepEnd` (the `H5` end, the `H7` start) | `stampStepEnd_Internal`, at each step's end, per simulatable, for `currentIntegratedTick` (PT) | `noEnd` counts the ticks a simulatable's state was not written; a correction rotation skips some on purpose. |
| `Rendered` (the `H7` end) | `tick_GameThread`: the newest pending tick per simulatable only (GT) | Not the photon: the render poses interpolate behind the simulation, and the render thread and GPU add one or two frames more. |
| `WIRE` | `noteWireSample`, half the round trip it is given (GT) | Half of a round trip assumes symmetric paths. |

**Threads — the probe adds no unsynchronized crossing.** The probe is **game-thread-owned** and has no internal lock.
Everything reaches it one of three ways:

| path | used by | synchronization |
|---|---|---|
| `stamp` / `noteWireSample` | every game-thread site | same thread as the owner; both `checkf` that they run on the game thread |
| `post_Internal` → `m_mailbox` → drained in `tick_GameThread` | every step-thread site | the mailbox's release/acquire pair (`LatencyBudgetProbe-guards.md` G-01) |
| `m_clockMailbox` | the client clock's skip/stall/hard-resync counts | the same mailbox type |

Every step-thread read the template makes is same-thread with its writer: the storage walk, `getLastUsedCaptureTick`
(written while the core collects input on that thread), the relayed-read observation ring (noted there too),
`currentIntegratedTick` and the client clock's diagnostics. Its game-thread reads (the correction-buffer tick, the
delayed-input map) are written on the game thread. On a dedicated server the step thread is the game thread, so the
mailbox is pushed and drained by one thread: a legal degenerate case of single-producer, single-consumer.

**Lifetime.** `begin` creates the probe and the two mailboxes; nothing resets them before the template is destroyed, so
an owner that calls `begin` before its backend starts stepping can never have a step reach a null or freed mailbox.
The wrapper captures `this`, the diagnostics object, which the owner holds as a member, so it lives as long as the core
container the owner keeps the callable in.

### The `H3` hook

`UWorld::OnPostTickFlush` is broadcast by the world's tick immediately after the net drivers' `TickFlush`, and it is the
earliest engine hook after it. The actual `SendTo` runs inside `TickFlush` (the connection's flush of its send buffer),
so the stamp is late by whatever `TickFlush` did after this connection's send: sub-millisecond with one connection,
longer on a busy server. It is early, or simply wrong, whenever the message did not leave at this flush:

* a client rendering faster than its `MaxNetTickRate` (120) skips a connection tick, so the packet leaves at the next
  frame;
* a saturated connection or the bandwidth limit defers the bunch;
* `net.IpConnectionUseSendTasks=1` (default 0) moves the `SendTo` onto a task after the flush;
* a replicated property is sent only when the replication system polls it, so a write can wait a poll period.

In each case `H3` reads as though the message left this frame. Read `H3` as a lower bound.

---

## §7 Log lines and the category

```
[LatencyBudget] role=Client hop=H2 p50=0.131 p95=0.273 p99=0.308 max=0.397 n=601 unit=ms noStart=0 noEnd=0 outOfOrder=0 overflow=0
[LatencyBudget.Window] role=Client seconds=10.01 stale=0 duplicate=2402 laneOverflow=0 mailboxDropped=0
```

One `[LatencyBudget]` line per enabled hop and one `[LatencyBudget.Window]` line per window, both under
**`LogOGLatencyBudget`** at `Warning` (one category per probe family, `UESimulationLogRoutes-rationale.md` §3). They are
logged with `UE_LOG` directly, never as core SIMLOG strings, so `RouteOGMessage` has no arm for them. After them,
`tick_GameThread` calls the backend view's `logLatencyWindow_GameThread`, which may log a backend's own window line under
its own category. `duplicate` counts the copies first-stamp-wins discarded (an input re-sent in several redundant
bundles, a capture carried by more than one relay round), so a large number there is the expected state, not a fault.

---

## §8 How it is built

* **Header-only.** Every member is defined in the header: the in-class ones where they were, the out-of-line ones after
  the class, inside one `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair under `#if OG_LATENCY_BUDGET_DIAGNOSTICS`. The game
  instantiates the template with its traits; `tick_GameThread` is instantiated by the owner's call with its backend's view,
  so no explicit instantiation is needed.
* ⚠ **The pair does not decide how the members compile.** MSVC compiles a template's instantiations, explicit ones
  included, with the `#pragma optimize` state in force at the **end** of the translation unit that instantiates them, and
  every file of the game ends with `OGSIM_OPTIMIZE_ON`. So in a Development build the members and their lambdas compile
  optimized, where the game module's non-template class compiled them unoptimized in its own `.cpp` (measured by task
  19a, with a scratch compile of the rule). The output is the same; a debugger sees optimized frames there.
* **The `.cpp`** holds only the `LogOGLatencyBudget` definition. This module builds per file, so it is its own object file.
* **Includes.** The always-included part is the probe, the time step and the input cache; the gated part adds the
  headers the out-of-line bodies name (the correction-buffer codec, input resolution, the prediction clock, the world).

---

## §9 Provenance

Written by og-tickdriver task 18 as a game-module class over one game's manager; made this template and moved into
OGSimulationUnreal, under MPL-2.0, by og-ue-simulationcomponentandmanagersplit task 19a, with the statements unchanged
(the type names come from the traits, the input sample from the two game hooks of §2). The probe's host side was written
by og-simulationscheduler-withjolt task 1.
