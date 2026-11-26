<!-- SPDX-License-Identifier: MPL-2.0 -->
# `HeadlessSessionHost.h` — rationale

`HeadlessSessionHost.h` (in og-sim-session's `tests/`) is the first engine adapter of `SessionRuntime` that is not Unreal (og-session-runtime task 32).
It is a test harness: `HeadlessSessionIntegrationTest.cpp` runs one authority and two clients through it, engine-free, in one
process. Its size is the backlog's measure of what an engine adapter writes against the runtime; `UESimulationSession.h` is its
Unreal counterpart. The engine services it stands on are `HeadlessPlatform.h`, and the link adapter is `HeadlessSessionLink.h`.

## §1 What it is made of

- **`Diagnostics`:** the runtime's latency-diagnostics port with every stamp a no-op, and the input provider type og-sim-net's
  client takes (`SimNetClient::InputProvider`'s shape).
- **`RuntimeTraits`:** the runtime's traits: the game's traits (the test's `HsGameTraits`: game, composition, engine traits, static
  data, simulatable, input, net config and `worldConfig`) plus this host's engine, clock engine, link, diagnostics, server and idle.
- **`Peer`:** one process. It owns the og-net peer and the link, the engine, the diagnostics and the runtime, models the runtime's
  host (`SessionRuntimeHost`, checked by the runtime's `start`), and does the engine's part of registration.

## §2 A peer's lifetime, in Unreal's order

All on the peer's frame thread, the thread that built it inside its `PeerScope` (`HeadlessPlatform-rationale.md` §1):

1. **`open`** is the net driver's start: the lanes' `beforeStart`, og-net's start, og-net's own I/O thread on a client (Unreal's
   `IoMode::OwnThread`; the server stays host-serviced, so its runner services og-net), `markStarted`, and a client's connect.
2. **`startSession`** is `BeginPlay`: it fills `SessionStart` (the step length from the net config, 60 catch-up steps and the 0.5 ms
   idle slack the Unreal smoke prints, the static world) and starts the runtime with the shipped `[OGNetcode]` values. A client's
   `SessionStart::netIoThread` is og-net's I/O thread (§4).
3. **`frame`** is one engine frame: the frame counter, the lanes' dispatch service (Unreal's `PostTickDispatch`), the component
   registrations, the driver's `startFrame` and `endFrame`, and the link's flush (`TickFlush`).
4. **`close`** then **`stopSession`**: on a map change Unreal destroys the net driver before the world's `EndPlay` (in LoadMap), so
   the link's stop (and, through the runtime's stop handler, the client's component teardown) runs first and the runtime's `stop`
   second. The runtime supports both orders; this host takes that one.

## §3 The link is attached before any step

`beginFrames` checks that a client's link session is bound (`SimNetLink::clientSessionBound`) when the runtime hands over the
driver. The runtime calls it after it attached the link and started stepping, and before any frame; a step is dispatched only
from a frame, so the link's roster seed and bound flag reach the steps by that order (`SimNetLink-rationale.md` §5). The runtime
asserts the same order on its side (`SessionRuntime-guards.md` G-06).

## §4 og-net's I/O thread in the session's thread table

The runtime makes a prediction role's `NetIo` row exact when `SessionStart::netIoThread` is set (task 31). Unreal leaves it unset
(its og-net servicing thread is not exposed); this host sets it from the thread og-net started (`PeerThreads::started`), and
`startSession` refuses to start a client before that thread runs. The forbidden-thread case,
`HeadlessSession.AServiceNetOpOffOgNetsIoThreadTripsTheExactRow`, runs a prediction `ServiceNet` op on a fresh thread and must trip
the check.

## §5 Registration: the engine's part

`addEntity` queues a character the way an Unreal component exists before it is registered; `frame` retries each queued one until
the runtime says `Ready` or `Refused`, as the component's tick does:

- **On the authority:** the runner path (`SessionAuthorityLifecycle::tryRegister`), with the owner connection the test took from
  og-net's connection event.
- **On a client:** the roster's bind step first (`SessionRuntime::bindStep`: wait until the roster carries the character, with the
  engine's word on whether it is local), then `SessionRuntime::tryRegisterPrediction`; a local character carries its input
  provider, a remote one none. Bodies are created on the first call through the runtime (`SessionRuntime::acquireAndBind`) with the
  slot the roster gives (`SessionRuntime::physicsSlotFor`).

A character's entity key is a number made from its sim id; it is also its body key. Leaving:

- the authority removes a character when its connection closes (`removeEntity`, the runner's unregister request);
- a client loses a character when the roster removes it (the lifecycle's `RosterRemoved` outcome) or when its session tears down.
  A client never removes a character itself, so the held-for-unregister path of Unreal's component removal is not written here.

## §6 Left out, on purpose

The game's instance hooks (`beforeWorldBuild`, `afterSessionConfigured` and the rest), the latency diagnostics, the visualization
doors, the render table and the present controls (the presenter gets `PresentControls` defaults and a null sink), the diagnostic
predicate gates and the `[ResimProbe.Session]` line. None of them is something the runtime needs to run.
