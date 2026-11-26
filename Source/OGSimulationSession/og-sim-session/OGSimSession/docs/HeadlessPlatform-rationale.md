<!-- SPDX-License-Identifier: MPL-2.0 -->
# `HeadlessPlatform.h` — rationale

`HeadlessPlatform.h` (in og-sim-session's `tests/`) is the headless host's engine: what an engine already has before it hosts a session (a clock, a
log, threads, a runner, an executor and an og-net peer), written over the C++ standard library and og-net's test network. It is
test code (og-session-runtime task 32). The session adapter that uses it is `HeadlessSessionHost.h`; the link adapter is
`HeadlessSessionLink.h`. On Unreal the same parts are `UESessionEngine.h`, `UEAuthorityRunner.h` and OGNetUnreal's
`SimLaneEndpoint.h`.

## §1 One peer, one context

`headlessSession::PeerContext` is what Unreal has once per process: the log (`UE_LOG` and the log routes) and the frame counter
(`GFrameCounter`). A test runs several peers in one process, so each peer has its own context, and every thread a peer owns finds
it through the thread-local `t_peer`:

- the peer's frame thread sets it (`PeerScope`) before it builds the peer;
- every thread the peer starts inherits it from the thread that starts it: `PeerThreads::startThread` (og-net's I/O thread and the
  executor's worker) and `Runner` (the authority runner) copy the starting thread's `t_peer` into the new thread.

So a line logged anywhere in a peer, by the runtime, og-sim-net, og-net or the Jolt world host, lands in that peer's sink.

The frame counter is shared for a reason found by running it: until og-session-runtime task 33b the runtime built a second
engine inside its worlds (`SessionRuntime::Worlds`, default-constructed), and the Jolt world host checks the frame number its
shadow world was restored on against its own engine's (`JoltTickWorldHost::overwriteShadowPoses_GameThread`). With one counter
per engine instance that check failed on the first frame. On Unreal every engine instance reads `GFrameCounter`; here every
engine of a peer reads its context's `frames`. Since task 33b `Worlds` builds its world host on the adapter's engine, so a
peer has one engine (and one executor thread, not two); the counter stays the context's, which needs nothing more.

## §2 `Engine`: the SessionEngine model

`headlessSession::Engine` models og-sim-session's `SessionEngine` (`SessionEngine.h`; the `static_assert` below the class says so):

- **Time:** `std::chrono::steady_clock` in seconds, the same clock og-net's `StdHostServices::nowSeconds` reads, so the receive
  stamps og-net hands the link and the times the session reads agree (on Unreal both are `FPlatformTime::Seconds`).
- **The executor:** og-tickdriver's `ThreadChainExecutor`, one worker thread per engine, in order. Its `deferUntil` always
  returns false, so the driver's end of frame waits for the step instead of deferring (`ThreadChainExecutor-guards.md` G-01).
- **The runner factory:** `startRunner` builds a `Runner` (§3).
- **`kHostDiagnostics`** is true, as on a non-Shipping Unreal build.

## §3 `Runner`

A `std::thread` that runs the runtime's body once, the start latch (`waitForStart`, `releaseStart`) the runtime's authority start
waits on, and the stop flag the authority loop polls. `join` sets the flag and joins; a second `join` does nothing (the link's stop
and the runtime's stop both join). `Host::sleepUntil` sleeps on the steady clock.

## §4 `Endpoint`: the og-net peer

`ognet::NetEndpoint` over og-net's `VirtualBackend` (`VirtualNetwork.h`, og-net's test network) with the members og-sim-net's link,
transport, server host and idle need (`SimNetLinkEndpoint`, `SimNetServerHostEndpoint`, `SimNetIdleEndpoint`). Two additions:

- **Connection events for the engine.** The transport installs its own connection handler; the endpoint keeps it and installs one
  of its own in front of it, which records each event for the frame thread (`takeConnectionEvents`) and then forwards it. On a
  server this is how the engine learns that a client connected or left, as Unreal learns it from its net driver. On a client it
  keeps the server's connection id for the round trip (§5).
- **No high-resolution wait.** `hasHighResolutionWait` is false; the virtual backend has none.

`noteDelivered` does nothing: on Unreal it feeds the `[OGNet.Window]` row of the driver's diagnostics, which this host does not
have.

## §5 Round trips

The link's services ask for the server's round trip, and the server host's port for each connection's. Both read og-net's own
connection statistics (`ConnectionStats::smoothedRttSeconds`), which og-net publishes under a lock, so any thread may read
them. A connection without a sample reads `ogsimnet::kSimNetLinkNoRttSeconds`. This is og-net's ping, not the engine ping Unreal
uses (`rttSource=EnginePing` on the `[SimNet.Host]` line is the runtime's text and does not change).

## §6 `NullRenderSink`

The presenter's render sink with nothing to render: no root pose is applied and no anchor is read. The adapter still presents each
frame, as on Unreal; on an authority that present also drains the presenter's registration ring (`SimulationTickBackend::present`).
