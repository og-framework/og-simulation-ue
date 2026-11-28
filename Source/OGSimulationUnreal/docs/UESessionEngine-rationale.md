<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESessionEngine.h` — rationale

The Unreal models of og-sim-session's engine port, and the engine's parts the runtime does not own (og-session-runtime task 31).
The prohibitions are in `UESessionEngine-guards.md`.

## 1. `SimulationEngineUImpl`, the `SessionEngine` model

The class moved here from `UETickDriverHost.h` (task 31 step (a)), its out-of-line members and the step task's priority object
from `UETickDriverHost.cpp` to `UESessionEngine.cpp`. It is still the tick driver's engine (`TickDriverEngine`: the clock, the
frame number, the log sink, the task-graph executor; `UETickDriverHost-rationale.md` describes it). Step (b) made it
og-sim-session's `SessionEngine`: `logRouted` is the log routes' `RouteOGLevelLine`, and `startRunner` builds a `UESessionRunner`
over `FUEAuthorityRunner` and starts its thread. The runtime uses the frame host's instance (`UETickDriverHost::engine()`, owned by
the Unreal session) for the tick backend, the routed lines and the runner, as `TickDriverBackendUImpl` did.

## 2. `UESessionRunner`

`SessionRunner` asks for the runner's host, the stop flag, the join and, since task 31, the start latch (`waitForStart`,
`releaseStart`): the authority's frame host begins only after the runner started its driver. `FUEAuthorityRunner` keeps its
behaviour (the `[SimHost.Runner]` lines, the `OGSimAuthority` thread, the latch's wait line).

## 3. The static import, the presentation and the gates

- `ueSessionEngine::importStatics` is the import and the report line of the backend's world build before task 31, filling
  `SessionStart`'s static world and physics-only flags; the runtime splits them.
- `UESessionPresentation` owns the render table and presents the runtime's frame through it with the console's present controls
  (the backend's game-thread present before task 31).
- `ueSessionEngine::bindDiagnosticGates` (inline, G-03) sets og-simulation's two diagnostic predicates; `logResimProbeSession`
  prints the `[ResimProbe.Session]` line, which reads the category's verbosity.
