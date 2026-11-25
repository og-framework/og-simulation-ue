<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionNetHooks.h` — rationale

Companion to og-sim-session's `OGSimSession/SessionNetHooks.h` (og-session-runtime task 26). It has no guards file: every rule
it keeps is a check (each door's thread, the first batch's tick) or a concept.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What it is, and where it came from

`ogsession::SessionNetHooks<PortT, ManagerT, NetSyncT, StorageT, DiagnosticsT>` is the session's side of og-sim-net at each
step: the prediction role's receive and send, the authority's drain, release, post-batch send and idle. `SessionTickGame` calls
it from the tick driver's hooks (`SessionTickGame-rationale.md` §3).

Its bodies are the Unreal session's (`Source/OGSimulationUnreal/UESimulationSession.h`), moved by og-session-runtime task 26: step
(a) cut `idleUntil_Runner`, `beforePump_Owner`, `releaseDelayedInputsForStep`, `afterAuthorityBatch`, `correctionTickOf`,
`stampCorrectionsHanded_Runner`, `boundImmediateClient_Step`, `afterClientCapture_Step` and `receiveForClientStep_Step` into this
header unchanged; step (b) made them this engine-free class. What changed in (b):
- `beforePump_Owner`'s clock half (the two rate doors) is `SessionTickGame::beforePump`'s; this class keeps the authority's
  runner requests and the drain.
- `releaseDelayedInputsForStep`'s first statement, the kept step time (`SessionClock::takeReleaseStepSeconds`), is the tick
  game's too: `releaseForTick` receives the time as an argument.
- `[ReleaseBatch] first=%d numSteps=%d` was `OG_LOG_ROUTED(LogOGNet, …)`; it is formatted with `snprintf` and sent through the
  port's `logRouted`, which in Unreal reaches LogOGNet at Verbose through the route table's `[ReleaseBatch]` row. The format is
  `sessionNetHooksLines::kReleaseBatch`; the Unreal session asserts its route and level, and a parity test compares the two
  formattings byte for byte.
- `afterAuthorityBatch`'s `checkf` is an `OG_CHECK` at the same place, its message naming the check it replaced.
- `FPlatformTime::Seconds()` (the corrections-handed time) is the port's `nowSeconds()`; the Unreal port returns the same call.
- `m_latencyBudgetDiagnostics.stampCorrectionHanded_Internal` is the diagnostics parameter's.

## §2 The port and the parameters

| | what | Unreal (`UESimulationSession::NetPort`) |
|---|---|---|
| `SessionNetHooksPort` | `Lanes` (a `SessionNetLanes`: `client()`, a const `clientSessionBound()`, `drainClient_Step()`), `Server`, `Idle`, `Simulatable`; `lanes()` (const), `server()`, `idle()` (each a pointer, null when absent), `runRunnerRequests()`, `applyUnregisterRequests(client)`, `publishPresentation()`, `nowSeconds()` (const), `logRouted(line)` | the session's `m_simNetLanes`, `m_simNetServer`, `m_simNetIdle`; `runRunnerRequests_Owner`, `applyUnregisterRequests_Step`, `publishAuthorityPresentation_Runner`; `FPlatformTime::Seconds()`; `RouteOGLevelLine` |
| `ManagerT` | `runsPrediction()`, `currentIntegratedTick()`, `getTimeConfig()`, `onPostSimulationGameThread()`, `getServerClock()` | the `SimulationManager` |
| `NetSyncT`, `StorageT` | the capture's sender; `forEachSimulatable<Simulatable>(fn)` | the session's `m_netSync`, `m_storage` |
| `DiagnosticsT` | a `SessionCorrectionStamps` (`stampCorrectionHanded_Internal(lane, tick, seconds)`) | `UELatencyBudgetDiagnostics<TraitsT>` |

Each port requirement has a negative in `sessionNetHooksSelfCheck` (the `Simulatable` type's is `PortWithoutSimulatable`, since
nothing else in the concept names it), except three type names that another requirement names in its return type: the lanes'
`Client` (`client()`), the port's `Server` (`server()`) and `Idle` (`idle()`). A port without one of them fails there, so
deleting the type-name line alone changes nothing. The server and the idle service are used by their calls
(`drain`, `releaseForStep`, `attachedConnections`, `publishClock`, `afterInlineBatch`, `channel<Simulatable>(id)`;
`idleUntil(deadline)`), as og-sim-net's `SimNetServer` and `SimNetIdle` declare them. The port is held by value: the Unreal one
is a session pointer, so the server, the idle service and the lanes are read through it on every call, as the session's
members were (they are built and reset over the session's life).

The lifecycle halves are the runtime's lifecycle component's: the client's unregister requests are
`SessionLifecycle::applyRequests` since og-session-runtime task 29 (`applyUnregisterRequests_Step` before), which the port's
`applyUnregisterRequests` calls before the drain (`SessionLifecycle-rationale.md` §2); the runner requests
(`runRunnerRequests_Owner`, the bridge's swap-and-run that `beforePump_Owner` held) stay with the Unreal session until task 30.

## §3 The doors

| door | `SessionOp` | what |
|---|---|---|
| `receiveForStep()` | `DrainInbox` | a bound client only (§4): the unregister requests, then the lanes' drain (in Unreal `UESimNetLanes::drainClient_Step`: og-sim-net's `SimNetClient::drain`, then the lanes' disconnect request, then the client's `flushFrame`) |
| `afterCapture()` | `Send` | a bound client only: `afterCaptureTick_Step(netSync, integratedTick, timeConfig)`, the bundle's send at capture |
| `beforePump()` | `DrainInbox` | the runner requests, then the server's drain if there is a server |
| `idleUntil(deadline)` | `ServiceNet` | the idle service's `idleUntil`, or `IdleWake::Unavailable` without one |
| `releaseForTick(first, steps, time)` | `Step` | with a server: `[ReleaseBatch]`, then `releaseForStep(first, steps, time)` |
| `afterBatch(tick)` | `Send` | with a server: the first-batch check (§5), the manager's `onPostSimulationGameThread`, `publishClock(serverTick)`, `afterInlineBatch(tick)`, the corrections-handed stamps, the presentation publish |

Each door's first statement is its `SessionThreadChecks::checkOp`; the rows are `SessionThreads-rationale.md`'s.
`correctionTickOf(channel)` is public and static: it reads the tick from the correction buffer's header, or 0 while the header
is shorter than `correctionStateBuffer::kHeaderBytes` (a never-written buffer).

## §4 A bound client

A client sends and receives only while its world session is bound: the manager predicts, the port has lanes, the lanes have a
client and `clientSessionBound()` is true. Before the bind and after the teardown both doors return at once. This was
`boundImmediateClient_Step`.

## §5 The first batch at tick 0

The authority loop's first batch reports tick 0 (the driver publishes its tick offset after a tick ran), and a client capture
reaped against it is refused by the coordinator's domain gate. The runner services og-net only in its idle, after its first
batch, so no connection can be attached at that batch; `afterBatch` checks it (`OG_CHECK`), as the Unreal session's `checkf`
did since og-input-latency P2-19 F2 (`SessionThreads-rationale.md` §5 records why the receipt gate depends on it).

## §6 Tests

`tests/SessionTickGameOrderTest.cpp` runs the real class under `SessionTickGame` with a recording port, manager, storage, lanes,
client, server and idle (`SessionTickGame-rationale.md` §6); its authority case covers a correction tick that is stamped, one that
is 0 and a simulatable without a channel, and a hidden death case trips §5's check.
