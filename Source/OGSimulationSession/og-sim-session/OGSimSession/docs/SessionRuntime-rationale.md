<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionRuntime.h` — rationale

`ogsession::SessionRuntime<TraitsT>` is one world's simulation session, engine-free: the composition, its start and its stop.
It is og-session-runtime task 31's composition of the components tasks 23–30 moved out of Unreal's `UESimulationSession`
(`SessionKnobs`, `SessionClock`, `SessionDisplay`, `SessionNetHooks`, `SessionTickGame`, `SessionLifecycle`) with the tick
backend that `TickDriverBackendUImpl` wrapped until then. The prohibitions are in `SessionRuntime-guards.md`.

## 1. What it owns, and in what order

The members are the session's, in the order they had in `UESimulationSession` (task 31 step (a) moved the declarations
verbatim, step (b) wrapped them in this class): the backend worlds (`Worlds`: the world host's engine, then the world host),
the three adapters, the storage, the static data, the three peers (reconciliation, input resolution, net sync), the systems,
the integration layer, the manager, then the tick backend's members (its tick game, `SimulationTickBackend`, the runner's world
config and statics, the started driver, the runner), the thread checks, the clock's engine and the clock, the display, the net
hooks, the two lifecycles (one per role; the authority's bodies provider beside it), the link pointer, the server host, the
server, the idle, the clock server, the authority's stopped flag and wake count, the frame probe, and the client's pending
registrations.

`compositionContractsHold` holds the order: every row of the Unreal session's (each message names its history and "moved by
og-session-runtime task 31"), plus the tick backend's rows (the manager, the worlds and the tick game before it, the runner
after it; were `TickDriverBackendUImpl`'s and the session's `m_physicsBackend` rows), the worlds' engine-before-host row, and
one new row, the authority lifecycle after its bodies and the tick backend. The latency diagnostics and the frame host's
engine are the engine adapter's: the runtime holds both by reference, and the Unreal session declares both before its runtime
member (its `adapterContractsHold`).

## 2. `start`: `beginPlay`'s order, both roles

`start(SessionStart&&, const ConfigSource&, Link*)` runs what `UESimulationSession::beginPlay` ran after its refusals and its
diagnostics' begin: read the knobs (`readSessionKnobs`, no line), build the thread checks, then the configuration lambda, in
the role's order:

- **Prediction:** build the world (`buildWorld`: the statics split, the world host's config from the traits, the tick backend's
  build) → configure: the host's `onConfigure` (Unreal: the instance registry), the integration layer, the systems
  (`Game::emplaceSystems`), the manager, the clock, the display, the net hooks, the lifecycle, the three loggers, the neutral
  input (G-02), the tier consumer (G-03), attach the link (`attachClientLink`: the registered-removed callback, the stop handler,
  the client's lifecycle bind, `bindClientSession`, the `[SimNet.Host]` line; G-06), the global logger, the game's loggers, the
  diagnostic gates (host), the `[ResimProbe.Session]` line (host), the resim policy (G-04), the latency knobs, the game's
  `afterSessionConfigured` (host `onSessionConfigured`), the `[SimHost.Backend]` line → start stepping (`startStepping`, then the
  host's `beginFrames`) → `SimulationManager: … initialized`.
- **Authority:** keep the world's configuration and statics (`buildWorld(true)`) → start the runner (the engine's `startRunner`) → on
  the runner: build the world, run the configuration (as above, with the server host, the server, its lifecycle bind, the clock
  server, the idle, the runner's RTT publish and join callbacks and the stop handler, the `[SimNet.Host]` line, then the session
  knobs and the floor, and no `[ResimProbe.Session]` line) → start the driver, release the start latch → on the frame thread,
  after `waitForStart`, the host's `beginFrames` → `SimulationManager: … initialized`.

`SessionRuntimeStartStopTest.cpp` transcribes both sequences from the step (a) commit (OGSimulation `5224ad5`, file:line per step) and
compares them with what the runtime does over fakes (a Jolt world host, a recording host, link, server and idle, the fake engine
and runner). Seen failing: poison P1 (steps started before `afterSessionConfigured`) fails the prediction case at the first moved
step.

## 3. `stop`: `endPlay`'s order

`stop()` stops the steps first (G-07: the host's `endFrames`, the runner's join, the tick backend's stop, which waits for an
outstanding step, G-05), then, if the session was configured: the authority's stopped flag, the host's component teardown, the
link's detach, the server's members (G-08), the host's component maps, the authority lifecycle's registered set, the tier
consumer, the display's held snapshot. It returns whether the session was configured; the Unreal session then unregisters its
instance and ends the diagnostics, as `endPlay` did. The authority's stop joins the runner before the server goes: the test's
server sentinel records a server destroyed before the join, and poison P2 (the server reset above `stopStepping`) trips it.

## 4. The engine adapter's port (`SessionRuntimeHost`)

The runtime calls back into the engine adapter for what is the engine's: the instance registry (`onConfigure`), the engine's
configuration lines and gates (`bindDiagnosticGates`, `afterPredictionConfigured`), the game's instance hook
(`onSessionConfigured`), the frame entry (`beginFrames`, `endFrames`), the input sample and the post-physics pass
(`sampleInput`, `onPostPhysics`), the routed message sink for lines that keep their level token (`logHost`: Unreal's
`RouteOGMessage`), the bodies' engine facts (`makeBodyRequest`, `bodyKeyOf`), the input provider's wrap, and the component
bookkeeping (`teardownComponents`, `forgetComponents`, `onLinkStopping`, `forEachRegisteredId`). The concept is checked in
`start`, where the host is complete (the host holds the runtime as a member). Lines that carry their own `[Level][Tag]` go to the
engine's `logRouted` (Unreal: `RouteOGLevelLine`): `[SimHost.Backend]` (the route table's `[SimHost.` row, task 31) and
`SimulationManager: … initialized` keep the category and level their `UE_LOG`s had.

## 5. The traits

`TraitsT` names the game's engine-free facts (`Game`'s statics, `Composition`, `StaticData`, `Simulatable`, `PlayerInput`,
`NetConfig`), the engine's models (`EngineTraits`, `Engine`, `ClockEngine`, `Diagnostics`), the link and the server types (`Link`,
`Server`, `Idle`) and `worldConfig(isAuthority, staticData)`. The Unreal session's are `UESessionRuntimeTraits`
(`TickDriverBackendUImpl.h`). The runtime spells the core systems itself and asserts that the composition names the same types
(the user ruling of 2026-10-24); `Server` and `Idle` come from the traits so the start/stop test can replace them.

## 6. The client's registration

`tryRegisterPrediction` is the client's pending record (bodies created, queued) moved from the Unreal adapter's `tryRegister`;
the adapter's hooks object creates the bodies, runs the game's `afterBodiesCreated` and polls the outcomes. The adapter keeps the
component map and maps the status onto the component's retry loop.

## 7. Threads: the og-net I/O thread and the stopped drain

`SessionStart::netIoThread`, when an engine knows og-net's I/O thread, makes the prediction role's `NetIo` row exact
(`SessionThreadChecks::setNetIoThread`): only that thread passes. Unreal's og-net does not expose its servicing thread, so the
Unreal session leaves it empty and the row keeps its older rule (not the frame, not a step). One scope guard,
`StoppedDrainScope` (`SessionThreads.h`), now sets and clears the stopped-drain flag of both the clock and the lifecycle.

## 8. Held components (task 29's L1)

`SessionHeldForUnregister` (`SessionLifecycle.h`) holds each component an `AfterApplied` unregistration keeps, in order per id;
each `Unregistered` outcome releases the oldest. A second hold for one id no longer replaces the first.

## 9. Codegen: the moved code is optimized now

`UESimulationSession.h` keeps its `OGSIM_OPTIMIZE_OFF` … `OGSIM_OPTIMIZE_ON` pair around its out-of-class members, and the code
step (a) moved stood inside it: the composition's rows, `beginPlay`'s configuration and `endPlay`'s order. This header has no
such pair (og-sim-session's headers have none), so since step (b) that code compiles with the build's optimization: in a
Development build, optimized where it was unoptimized before. The order is the source's in both (the start/stop test and the
smokes' start-up lines check it), and the `static_assert` rows are compile-time, so no row changes meaning. A debugger stepping
through `start` or `stop` sees optimized code; the Unreal adapter's own members are still in its pair.
