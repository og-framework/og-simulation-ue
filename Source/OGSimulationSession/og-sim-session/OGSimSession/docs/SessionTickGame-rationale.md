<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: PhysicsBackendHostUImpl.h -- deleted by og-session-runtime task 31 (the brawler's backend selector: the PhysicsBackendHost concept, BackendFor, BackendWorlds, the traits' PhysicsBackend and PhysicsBackendWorlds, and the host's extern instantiation; the tick backend is og-sim-session's SessionRuntime member, the Unreal bodies TickDriverBackendUImpl, named by UESessionRuntimeTraits); kept in the history -->
# `SessionTickGame.h` — rationale

Companion to og-sim-session's `OGSimSession/SessionTickGame.h` (og-session-runtime task 26). Prohibitions:
`SessionTickGame-guards.md`.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What it is, and where it came from

`ogsession::SessionTickGame<HostT, DiagnosticsT>` is og-simulation's `TickDriverGame` for a session: every hook
`SimulationTickDriver` and its `TickDriverStepHooks` call, plus the four optional ones (`sampleInput`, `receiveForStep`,
`publishForDisplay`, `idleUntil`). Each hook calls the session's components in a fixed order (§3): the clock
(`SessionClock`), the display (`SessionDisplay`), the net hooks (`SessionNetHooks`), the latency stamps (the diagnostics
parameter) and, for what is still the engine's, its host (§2).

Until og-session-runtime task 26 the game was the brawler's `BrawlerTickDriverGameUImpl<SessionT>`
(`Source/OGBrawlerUnreal/`, now `BrawlerSessionHooksUImpl.h`): each hook forwarded to a member function of the Unreal session
(`beforePump_Owner`, `onFrameStepsDue_Step`, `afterAuthorityBatch`, `noteAuthorityStepTime_Step` and
`releaseDelayedInputsForStep`, `receiveForClientStep_Step`, `afterClientCapture_Step`, `afterStep_Step`,
`publishDisplaySnapshot_Step`, `onPostPhysicsStep`, `idleUntil_Runner`), and the session befriended it. Task 26 moved the
session's net-hook bodies into `SessionNetHooks.h` (its step (a), verbatim) and then (step (b)) replaced the forwarders with
this class; the brawler keeps only its engine-side hooks (`sampleInput`, `gravity`, `callWithSlotRequest`), and the session's
friend line for the binding is gone.

## §2 The host, in two concepts

The class holds its host by value and asks it for everything, on every call:

| concept | what | checked |
|---|---|---|
| `SessionTickHost` | `Manager`, `Storage`, `RenderSnapshot`; `manager()`, `storage()` by reference | where `SessionTickGame<…>` is named (its template-parameter constraint) |
| `SessionTickComponents<H, D>` | `threads()` (const), `clock()`, `display()`, `netHooks()`, `clockServer()` (each a pointer), `diagnostics()` (a `D&`, with `D` a `SessionStepStamps`), `sampleInput()`, `onFrameStepsDue(tick, steps)`, `onPostPhysics()` | a `static_assert` in the private `threads()`, which every door calls first, so it is evaluated where the doors are instantiated |

**Why two.** The tick driver needs the game's `Manager`, `Storage` and `RenderSnapshot` where the driver's types are formed, and
in the Unreal adapter (until og-session-runtime task 31, which moved this class into the runtime) that is early: the backend that held this class (`TickDriverBackendUImpl`) was the session's member
`m_physicsBackend`, so it is instantiated while the session class is still being defined, and the brawler's
`PhysicsBackendHostUImpl.h` names it (`extern template class TickDriverBackendUImpl<BrawlerSessionHooks, …>`) before the game's
traits struct is complete. The host's component types are the session's own (its `SessionClock`, `SessionDisplay`,
`SessionNetHooks` specialisations), which cannot be named there, so the Unreal host (`UESessionTickHost`, OGSimulationUnreal)
takes its three early types from the game binding, declares the component accessors with deduced return types and defines them
after the session. Constraining the class on the components would have checked them at that early point; checking them in the
doors checks them where the session is complete. A host that lacks one fails to compile at its first door, with
`SessionTickComponents` named. `SessionTickComponents` does not repeat `SessionTickHost`: the class's constraint holds it
wherever the doors are instantiated.

**The diagnostics parameter** is the type of the latency stamps (`stampAfterStep_Internal(stepStartSeconds)`,
`stampStepEnd_Internal()`); the Unreal adapter passes `UELatencyBudgetDiagnostics<TraitsT>`. The stamps were the brawler
binding's direct calls on `editLatencyBudgetDiagnostics()` until task 26.

**A null component.** `clock()` null skips the clock's calls, as the session's `m_clock.has_value()` tests did; `netHooks()`
and `display()` null fail an `OG_CHECK`: both are built with the session's configuration, before the driver's first frame or
wake.

## §3 The call order (design §3.4), per door

| door | thread op (§4) | what it calls, in order | was |
|---|---|---|---|
| `sampleInput()` | `SampleInput` | the host's `sampleInput()` (the game's engine hook) | the binding's walk over the registered characters |
| `beforePump(host, rate)` | `RateDoor` | prediction (a clock and a predicting manager): `clock->beforePredictionPump(host, rate)`, return; otherwise `clock->noteAuthorityRateDoor(host, rate)` if there is a clock, then `netHooks().beforePump()` (the runner requests, then og-sim-net's drain) | `beforePump_Owner` |
| `onFrameStepsDue(tick, steps)` | `FrameProbe` | the host's frame probe (PROBE A, the Unreal session's `onFrameStepsDue_Step`) | the same |
| `receiveForStep()` | `DrainInbox` | `netHooks().receiveForStep()` (the unregister requests, then the lanes' drain) | `receiveForClientStep_Step` |
| `onStepStarted(seconds)` | `Send` | the stamp `stampAfterStep_Internal(seconds)`, then `netHooks().afterCapture()` (the send at capture) | the binding's stamp, then `afterClientCapture_Step` |
| `onStepEnded()` | `Step` | the stamp `stampStepEnd_Internal()`, then `clock->publishServerTick(clockServer())` if there is a clock | the binding's stamp, then `afterStep_Step` |
| `publishForDisplay()` | `Publish` | `display->publishAfterBatch()` | `publishDisplaySnapshot_Step` |
| `releaseInputsForAuthorityTick(tick)` | `Step` | `clock->noteAuthorityStepStart()`, then the kept step time `clock->takeReleaseStepSeconds()` (each if there is a clock), then `netHooks().releaseForTick(tick, 1, time)` (⛔G-01) | `noteAuthorityStepTime_Step`, then `releaseDelayedInputsForStep(tick, 1)` |
| `afterInlineBatch(tick)` | `Send` | `netHooks().afterBatch(tick)`: the manager's post-simulation send, `publishClock`, og-sim-net's `afterInlineBatch`, the corrections-handed stamps, the presentation publish | `afterAuthorityBatch` |
| `idleUntil(deadline)` | `ServiceNet` | `netHooks().idleUntil(deadline)` | `idleUntil_Runner` |
| `onPostPhysics()` | `Present` | the host's post-physics pass (the Unreal session's `onPostPhysicsStep`: the test hitch first, then present, collect the lifecycle outcomes, the display view or the presentation, the game's `afterVisualization`, the diagnostics tick) | the same |

The step's `takeReleaseStepSeconds` is read before the net hooks test for a server, as `releaseDelayedInputsForStep` did, so
the kept time is cleared on every authority step. On the authority `onStepStarted`'s `afterCapture()` finds no bound client
and returns; the stamp is still taken first.

## §4 The thread checks

Each door's first statement is `threads().checkOp(op)` with the row of §3 (`SessionThreads.h`; the table and its reasons are
`SessionThreads-rationale.md`). The components check their own rows again where they have one (the clock, the display, the net
hooks). One row is new with this class: **`FrameProbe`** (client: frame; authority: step, the runner), the frame probe that runs
after the pump, where the pump runs. No other existing row named it (`RateDoor` is the rate door's; the probe is not), so it
got its own row rather than a wider one. `manager()` and `storage()` are accessors, not doors: the driver reads them on the
frame, the step and the runner, and they check nothing.

## §5 The four refining concepts

`TickDriverSampleGame`, `TickDriverReceiveGame`, `TickDriverDisplayGame` and `TickDriverIdleGame` are asserted at the end of the
header over a reference host, with the reasons the brawler binding's four `static_assert`s gave (each optional hook is called
only under `if constexpr`, so a missing one compiles and silently does nothing; `idleUntil` is the authority loop's
requirement). The runtime asserts the four again on the type its tick backend holds (`SessionRuntime.h`,
`compositionContractsHold`, since og-session-runtime task 31; the Unreal session's before).

## §6 Tests

`tests/SessionTickGameOrderTest.cpp` (`[Session][SessionTickGame]`):
- **The order test.** A recording double (a host, a clock, a display, the stamps, a port with lanes, a client, a server with
  correction channels, an idle) drives one prediction frame with two steps and one authority wake with two steps through this
  class and the real `SessionNetHooks`, in og-tickdriver's call order. The recorded events equal a list transcribed from the
  Unreal code at the task's step (a), each entry with its file and line (17 events for the frame, 25 for the wake).
  Seen failing: the capture's send before the stamp fails entries 5, 6, 11 and 12; `publishClock` after `afterInlineBatch`
  fails entries 19 and 20.
- Without a server the authority releases nothing and idles `Unavailable`; an unbound client steps without receiving or
  sending; the correction tick is read from the buffer's header only once the header is written.
- Hidden death cases: a step door on a client's frame thread (the thread check), the authority's first batch at tick 0 with a
  connection attached (`SessionNetHooks::afterBatch`'s check).
