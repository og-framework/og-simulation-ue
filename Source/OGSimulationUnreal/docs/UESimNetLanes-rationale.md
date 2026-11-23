<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESimNetLanes.h` — rationale

`UESimNetLanes<TraitsT>` is the og-net driver's per-driver sim lanes object: the `IOGNetSimLanes` that `UOGNetDriver` creates at
`InitBase` through the factory a game registers, and calls at six points: `ioMode` and the five frame points
(`OGNetSimLanes-rationale.md`). It owns the sim transport over the driver's erased endpoint, on a server the SimClock server, and,
on a client, the client session. It is a header-only template over the game's session traits. This file is the docs pointer of
`UESimNetLanes.h`; its prohibitions are in `UESimNetLanes-guards.md`.
The hosts it uses are in `UESimNetHost.h` (`UESimNetHost-rationale.md`).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 What it holds, per role

| member | server | client |
|---|---|---|
| `Transport` = `ogsimnet::SimNetTransport<SimLaneEndpoint, kUESimNetInboxSlots>` | yes | yes |
| `ClientHost` and `Client` = `ogsimnet::SimNetClient<Composition, ClientHost, Transport>` | no | yes |
| `FLockedSimNetBindings` (`ogsimnet::SimNetBindings` behind a mutex, `UESimNetHost.h`) | unused | yes |
| the immediate decision (`ESimTrafficImmediate`, §11) and whether a client session is bound | `RefusedServer` or `Off` | yes |
| the SimClock's end of the transport: `UESimNetClockServer` (§12) or `UESimNetClockClient` and the published og-net ack RTT (§13) | the server | the client |
| the bound server session's two frame functions | while bound | no |
| a detached endpoint | when made by `makeDetached` | when made by `makeDetached` |

The inbox has 2,048 slots (`kUESimNetInboxSlots`). With og-net's default 16 connections, og-sim-net's own `static_assert`s size
each server connection's quota from it: 124 undrained unreliable messages, and a Control ring of 7,296 slots
(`SimNetTransport.h`). A client's inbox has one connection and no quota on the unreliable lanes.

## §2 Ownership, and the join window

The transport and the client session live with the **driver**; the server session lives with the **world**.

- **Client.** og-sim-net's server attaches a client on og-net's `Connected`, which comes before Unreal's login, and from then
  on sends it the roster, the floor, the tier, the clock and the relay rings. The client's world exists seconds later. So the
  client session is created in `beforeStart`, with the transport, and drains from the driver's first `PostTickDispatch`:
  - the roster mirror and the bindings follow every Add and Remove from the first drain;
  - a floor or a tier is latched by the session, and `bindClientSession` replays it into the world's session
    (`SimNetClient::bindListener`), as today's latch-and-replay does;
  - a clock tick before the world binds is dropped by the host; the newest SimClock sample before it is held and fed to the
    session just ahead of the first authority tick that reaches a bound session (§13, og-input-latency P3-11);
  - a correction for a sim id with no channel yet counts `unknownSim` and is dropped; a relay ring for a character with no
    channel yet is held per sim id and stored at the character's Add.
  
  A session created at bind would leave all of that in the inbox for the whole load (`UESimNetLanes-guards.md` G-01).
  Only map load (the engine's LoadMap) itself is a gap in the draining: the unreliable lanes' overflow there is counted as `drops`, in windows
  before steady state.
- **Server.** The server's world and its listen happen in one map load (the engine's LoadMap), so no frame passes between the driver's start and the
  world's BeginPlay in which a client could have sent anything (the P5 design's inference, K4; not measured here). The world's session creates `SimNetServer` (which owns the
  reception coordinator) at BeginPlay and binds it here with `bindServerSession(dispatchFn, flushFn)`. The lanes keep the two
  functions, not the server: its type depends on the session's correction policy and its lifetime on the world.

## §3 The six call points

`ioMode` is the driver's sixth call, the first of them (og-input-latency task P5-18 added it with a default, so the five frame
points below were the lanes' only calls until task P5-19 answered it).

| driver point | server | client |
|---|---|---|
| `ioMode(bIsServer)` | latches `og.Sim.AuthorityLoop`; `HostServiced` when the loop is chosen, else `OwnThread` (§12) | `OwnThread` |
| `beforeStart(endpoint)` | the transport (immediate when the loop is chosen), the SimClock server, the handlers, the forward target | the transport, the SimClock client (§13), the handlers, the forward target, then the client host and the client session |
| `afterStart()` | `markEndpointStarted` | `markEndpointStarted` |
| `afterDispatch()` | the authority runner's RTT publish, if bound (§12); then the bound dispatch function, if any | first the SimClock ping and the ack-RTT publish (§13); then frame-locked: `dispatchFrame`, then a disconnect request (below). Immediate: the RTT publish, then `drain` and the request only while no session is bound (§11) |
| `onTickFlush()` | the bound flush function, if any | `flushFrame`; immediate with a bound session: nothing (§11) |
| `beforeStop()` | the authority runner's join, if bound (G-03); the session's stop handler; then unbind the server session | the session's stop handler, then unbind the client session |

`afterDispatch` runs after `Super::PostTickDispatch` (every packet dispatched, before the first tick group), and `onTickFlush`
after `Super::TickFlush` (after the engine's own send). `OGNetDriver-rationale.md` §13 has the verified engine order.

**The disconnect request.** A client that refuses a roster message (P7's rule: a refused roster decode disconnects) latches a
request. `afterDispatch` takes it after each drain, logs `[SimNet.Host] role=client disconnect conn= reason=` at Error to
`LogOGNet`, and disconnects that og-net connection through the endpoint.

## §4 The transport is built inside `beforeStart`

The transport's sizes come from its endpoint's config, `ogsimnet::simNetConfigFor(endpoint.config(), immediate)`: a server's for
its `maxConnections`, a client's for its one server, and the Control quota from the lane table. That config exists only once
the driver has built its endpoint, and the driver hands the endpoint over in `beforeStart` and nowhere earlier. So the
transport is an `std::optional` emplaced there (it can be neither copied nor moved), and so is the client session that holds
a reference to it. `beforeStart` also `checkf`s that the endpoint's role is the role the lanes were made for.

`immediate` is decided in `beforeStart` too, before the transport exists, because og-sim-net fixes it when the transport is
built and the client session reads it from the transport once (`SimNetClient-rationale.md` §16). It is `false` unless the ini
asks for it and the client can honour it (§11). Frame-locked, the transport holds the frame's traffic until `onTickFlush`. A
`checkf` after the client session's construction confirms that the session took the lanes' mode.

## §5 The client session's binding

- `bindClientSession(worldSession)`: binds the host to the session, replays the latched floor and tier (`bindListener`), and seeds
  the bindings from the roster mirror (`SimNetBindings::seedFrom`). The bindings are also fed incrementally by the host from
  the first drain, so the seed changes nothing then; it is there for a session that binds after entries exist, and it
  `OG_CHECK`s that no sim id is registered yet. `SimNetBindings-rationale.md` §6 has the order: seed before the first
  registration.
- `unbindClientSession()`: stops the replays and unbinds the host. The client session itself stays, with its roster, latches
  and channels, for the next world on the same driver.
- `setOnRegisteredRemoved(onRegisteredRemoved)`: the world session's unregister of a registered sim id, which the host calls on a roster Remove
  (`UESimNetHost-rationale.md` §7). A session sets it before it registers a character.
- `bindings()` and `editBindings()` give the world session the gate it registers through (`SimNetBindings::verdict`,
  `markRegistered`, `markUnregistered`, and since og-input-latency task P5-17 `markRegisteredIfReady`, the verdict and the mark
  under one lock). Every call takes the bindings' mutex (`UESimNetHost-rationale.md` §12), because an immediate client's drain
  feeds them on the step's thread while the world session reads them on the game thread.
- The bind and the unbind set and clear `clientSessionBound()`, which decides where an immediate client drains (§11).
- `setStopHandler(onStop)` (og-input-latency task P5-7): the world session's handler for the driver's stop, which `beforeStop`
  calls once, first, and clears. Unreal shuts a world's net driver down before it destroys the world's pawns and ends its
  play (its map load), and the lanes, with a client's channels, die with the driver. So the session unregisters every
  registered character there, while the channels and the endpoint still live, and forgets the lanes
  (`UESimulationSession-rationale.md` §16.5). A session that ends first clears the handler (its `detachFromSimNetLanes`).

## §6 The server session's binding

`bindServerSession(dispatchFn, flushFn)` `checkf`s that both are set; `unbindServerSession()` clears both, and the authority
runner's two functions (§12). A server on the authority loop binds no frame points: its session drains and sends on the runner,
and og-sim-net refuses the frame-locked calls of an immediate session. Unbound,
`afterDispatch` and `onTickFlush` do nothing: there is no server session to drain or flush. `serverSessionBound()` says which.
The server session sets the stop handler too (§5); its handler unregisters every character, sending the roster Removes over the
still-running endpoint, then releases og-sim-net's server.

## §7 Detached lanes

`makeDetached(role, config)` builds a lanes object over `SimLaneEndpoint::detached(config)` and runs `beforeStart` and
`afterStart` on it at once. A world with no og-net driver uses one (a standalone world, or a networked world on another driver,
which logs an Error and runs detached), so the session has one code path either way: its sends and disconnects are dropped and
counted in the endpoint's `detachedDrops()`, and nothing arrives.

- **The role argument decides the role.** The config's own `role` is overwritten with it, because the transport's sizes follow
  the endpoint's role and a default `ognet::EndpointConfig` is a client's.
- **The detached endpoint is heap-held** (`std::unique_ptr`): it can be neither copied nor moved, and its operations use its own
  address as their context (`SimLaneEndpoint-guards.md` G-01). It is declared before the transport, so the transport, which
  keeps a pointer to it, is destroyed first.

## §8 Threads

Every member function runs on the game thread: the driver's five points do (`InitBase`, `PostTickDispatch`, `TickFlush`,
`LowLevelDestroy`), and so do the world session's binds. og-net's servicing thread runs only the transport's lane and
connection handlers, which `beforeStart` installs before the endpoint starts. The exceptions: an immediate client's
`drainClient_Step`, which the world session calls on the step's thread (§11), with `clientSessionBound()` an atomic for that
reader; and on an authority-loop server the runner, which services the endpoint (so it runs the handlers and the SimClock
server) and calls the server host (§12).

## §9 Lifetime

The driver destroys the lanes after its endpoint, so after og-net's thread has joined (`OGNetDriver-rationale.md` §13): no
handler can run into a destroyed transport. Within the lanes, the client session is created after the transport and the host
it references, and destroyed before them; none of their destructors calls another. A world session never keeps a pointer to
lanes past `beforeStop`: the stop handler (§5) runs first in it and the session forgets the lanes there.

The SimClock server holds the transport and the clock, and is the transport's SimClock lane handler, so it is declared after
both and goes first; a member-order `static_assert` (`compositionContractsHold`, checked in the constructor) keeps it so. og-net
calls it only until the endpoint is destroyed, which is before the lanes.

## §10 The switch, and the namespace aliases

Since og-input-latency task P5-7 the brawler registers its factory (`makeBrawlerSimNetLanes`) at module start-up, so every
og-net driver makes a lanes object at `InitBase`, and the world session binds to it (`UESimulationSession-rationale.md` §16).
`UESimNetTransport` and `UESimNetChannel<SimT>` name the transport and the per-character channel at namespace scope, without
the session traits, so a game's `SimulatableOwnerTraits` can name the channel where the traits are not complete yet; the class's
`Transport` is `UESimNetTransport`, and the session asserts its channel type equals `UESimNetChannel<Simulatable>`. Before the
switch nothing in the game included this header except its concept test, `UESimNetHostsConceptTest.cpp`, which still builds a
detached lanes object for each role (the automation test `OGBrawler.SimNet.Hosts`).

## §11 Immediate mode on a client (og-input-latency task P5-17)

**The switch.** `[OGNetcode] SimTrafficImmediate` (game or engine ini; absent or 0 = frame-locked) is read by
`readSimTrafficImmediateKnob` in `beforeStart`, and `decideSimTrafficImmediate` turns it into the lanes' `ESimTrafficImmediate`.
The game's shipped `Config/DefaultEngine.ini` sets it to 1 since og-input-latency task P5-22 (after Ph4's gate), so `On` is the default on
a Jolt client and a refusal below is what a shipped build prints where immediate cannot run. Frame-locked stays the
attribution arm (design Q11; it was also the former Chaos backend's only mode); `-ini:Engine:[OGNetcode]:SimTrafficImmediate=0`
selects it. The decisions:

| decision | when | the lanes' transport |
|---|---|---|
| `Off` | the knob is absent or 0 | frame-locked |
| `RefusedServer` | a server's lanes: the server drains frame-locked until its authority loop drains it | frame-locked |
| `RefusedRunInline` | `og.Sim.RunInline=1`: the client steps on the game thread, where the step drain is refused | frame-locked |
| `RefusedDetached` | detached lanes (§7): nothing arrives | frame-locked |
| `On` | a client's og-net lanes on the tick driver, stepping on workers | immediate |

The order of the refusals is the order of the table. The world session prints the refusal as a Warning on `LogOGNet` and the
mode on its `[SimNet.Host] … immediate=` line (`UESimulationSession-rationale.md` §18).

Until og-session-runtime task 5 a fourth refusal sat between the server's and `og.Sim.RunInline`'s: a client whose engine
traits had no world host (the former Chaos backend, EQ12) did not step through og-simulation's tick driver, so nothing
called the step hook that drains, and it stayed frame-locked. Every engine left has a world host (Jolt), so task 5 removed
the refusal, its decision parameter and its Warning; the decision is the same on every remaining build.

**Where an immediate client drains.**
- **Before a world session binds**, at `afterDispatch` on the game thread, as frame-locked does, but through `drain()`:
  og-sim-net refuses `dispatchFrame()` on an immediate session (`SimNetClient-rationale.md` §16). Nothing steps yet, so the
  game thread is the drain's only caller. The disconnect request is acted on after it.
- **Once a world session is bound**, at the start of each of its steps, on the step's thread, through `drainClient_Step()`,
  which the session calls from the step's receive hook, before the step's resimulation check and collect (the step's send
  comes later, after its capture; `UESimulationSession-rationale.md` §18.2). It `checkf`s that it is off the game thread (the AC's check at the drain) and
  that the session is bound and immediate, drains, acts on the disconnect request, and calls `flushFrame()`: in immediate mode
  that holds nothing (every send already went to the endpoint), but it emits the `[SimNet.Window]` line, whose counters the drain
  writes, so it belongs to the drain's thread. `afterDispatch` then drains nothing (G-02) and `onTickFlush` does nothing.
- **After the session unbinds**, back at `afterDispatch`. The session stops its steps before it unbinds
  (`UESimulationSession-rationale.md` §18.6), so the two never overlap.
- **At the session's teardown**, once, through `drainClientWhileStopped_GameThread()`: the steps have stopped, and the drain
  applies the unregistrations the session queued last (`UESimulationSession-rationale.md` §18.6).

**The RTT.** Each `afterDispatch` of an immediate client first calls the host's `publishServerRtt_GameThread()`, so the step's
clock delivery reads the server connection's round trip as of this frame (`UESimNetHost-rationale.md` §5). A frame-locked
client publishes nothing: its drain reads the connection itself, on the game thread, as before.

## §12 The authority loop's server lanes (og-input-latency task P5-19)

- **The decision is the lanes', at `ioMode`.** og-net fixes its servicing thread at the first `serviceUntil`, and the endpoint
  starts in `InitBase`, before any world, so the mode cannot wait for the session. `ioMode(bIsServer)` reads
  `og.Sim.AuthorityLoop` once (`readAuthorityLoopKnob`, `UEAuthorityRunner.h`) and latches `EAuthorityLoop`:
  - `Off` on a client, or with the knob 0 (the default until og-input-latency task P5-22 set it to 1,
    `UEAuthorityRunner-rationale.md` §1);
  - `RefusedNotDedicated` on a listen server, whose game thread presents the authority. The refusal is one Warning on
    `LogOGSimHost`, `[SimHost.Runner] og.Sim.AuthorityLoop=<n> refused: ...`; the server then steps inline, as with the knob off.
    Until og-session-runtime task 5 a refusal for an engine that did not step through the tick driver (the former Chaos
    backend) came before it, with a Warning of the same form;
  - `On` otherwise: `ioMode` returns `HostServiced`, so og-net starts no thread and nothing services the endpoint until the
    session's runner idles on it (`OGNetSimLanes-rationale.md` §2). A detached world never asks (`makeDetached` skips `ioMode`), and
    its session refuses the loop with its own Warning (`UESimulationSession-rationale.md` §19).
- **The transport is immediate exactly when the loop is on** (og-sim-net reads the mode from the transport's config): the
  session drains in `beforePump` and sends after each batch, on the runner. `[OGNetcode] SimTrafficImmediate` on a server is
  `On` with the loop and `RefusedServer` without it, so the old refusal line is printed only when nothing drains immediately.
- **The SimClock server** (`UESimNetClockServer`, og-sim-net's `SimNetClockServer` over the transport and `FUEPlatformClock`) is
  built on every server's transport, loop or not, before `installHandlers`, because the transport hands the SimClock lane to
  its clock handler only if one is set by then. It answers each client ping at receipt on og-net's servicing thread (og-net's own
  thread with the loop off, the runner with it on). Its clock is `FPlatformTime::Seconds()`, the clock og-net's host stamps
  receive times with (`OGNetUnrealHost.h`), so t1 and t2 are on one clock (og-input-latency P2-20 F2). The world session
  publishes the server tick to it after every authority step. Every client pings it (§13).
- **The hosts.** `ServerHost` and `ClientHost` are `UESimNetLanesServerHost` and `UESimNetLanesClientHost`, thin derivations of
  `UESimNetHost.h`'s hosts:
  - both answer og-sim-net's optional `onLifecycleUnregistering(simId)` (`SimNetHost-rationale.md` §4a) by forwarding it to the
    world session, which notifies its systems while the character is still in storage;
  - the client host also answers the optional `onClockSample(sample)` (`SimNetHost-rationale.md` §4b) by forwarding it to the
    world session (`onSimClockSample`, §13); with no session bound the newest sample is held, and fed ahead of the first
    authority tick after the bind (`FClockSampleLatch`, §13; a clock tick with no session is dropped);
  - the server host, once the session calls `serveFromRunner()`, answers `rttMs` from `FRunnerRttTable` and posts its two probe
    stamps through the latency mailbox instead of the game-thread `stamp`/`stampAt`: `stampServerReceived` through
    `stampServerReceived_Internal` and `stampHanded` through `stampHandedAndLeft_Internal` (the hand-over is the send on the
    loop's immediate transport; `UELatencyBudgetDiagnostics-rationale.md` §14). Both base members are game-thread only
    (`UOGNetDriver::FindConnection` and the probe `checkf` it), and on the loop og-sim-net calls the host from the runner. With
    the loop off nothing changes: the derived members forward to the base ones.
- **The RTT table.** `FRunnerRttTable` is 64 lock-free 64-bit words, each an og-net id and a `float` round trip in
  milliseconds, so the runner never reads one connection's id with another's reading. The game thread fills it in `afterDispatch`
  (`publishRunnerRtt_GameThread`): one word per og-net client connection, its value the base host's `rttMs` (which also feeds
  the probe's WIRE sample and keeps the base's one-shot warnings) from the connection's Join and a quiet read before it (next
  bullet), and clears the rest. A connection not in the table reads
  `kNoRttReadingMs`, which the tier table skips. The reading is up to one frame old. This is the final shape of the per-connection
  RTT on the loop (og-input-latency task P5-20: "per-connection RTT reaches the runner through atomics the game thread
  publishes each frame").
- **No RTT warning before the Join** (og-input-latency task P5-20, rework 1: the lead's 17:55 line, from P5-21 F6). The publish
  runs every frame for every connection og-net has accepted, while a loop-off server asks for a round trip only when a
  received input bundle names the connection, so after its Join. UE fills a connection's round-trip accumulator from the acks
  it reads (the engine's packet-info pass, ReadPacketInfo in NetConnection.cpp), so a connection that is still logging in can have no reading, and the base's
  read answers `AccumulatorEmpty`. Its warning is one-shot per cause (`TRttCauseWarnings`, `UESimNetHost.h`): asked then, the
  base printed a false `[RttSample] SERVER ... accumulator is empty` line about 0.17 s before the first `Login request` and spent
  the warning a later outage needs. So until UE sets the connection's `ClientLoginState` to `ReceivedJoin` (`UWorld`'s
  `NMT_Join` handler, once the player controller exists) the publish reads `readRoundTripMs` itself and publishes what it
  reads, with no warning and no WIRE sample (a loop-off server takes none before the Join either), and a closing connection
  reads `kNoRttReadingMs`, as in the base. From the Join it asks the base (G-04), so every warning there means what it says;
  a configuration fault (`NoNetPing`, `RoundTripNotEnabled`) is warned at the Join instead of at the accept. With the loop off
  nothing changes: the base `rttMs` is untouched and the publish runs only on the loop.
- **A word of 0 is an empty slot.** The high half is the og-net id, and og-net never assigns 0 (`ognet::kInvalidConnection`, a
  `static_assert` in the table), so no connection's word is 0, even with a round trip of 0. A `UOGNetConnection` whose og-net id
  is still 0 is not published, and `read(0)` answers `kNoRttReadingMs` without a scan (review_P5-19 N2).
- **`bindAuthorityRunner(publishRunnerRtt, joinRunner)`** is the session's: `afterDispatch` calls the first before the server's
  dispatch, and `beforeStop` calls the second first of all (G-03), because og-net's stop must run after the servicing thread
  has stopped (`EndpointCore.cpp`'s stop check; `OGNetSimLanes-rationale.md` §2). `unbindServerSession` clears both.
- **The driver's destructor does not call `beforeStop`** (`OGNetDriver-rationale.md` §8). A host-serviced driver destroyed
  without `LowLevelDestroy` while its runner still runs would fail og-net's stop check; the session joins its runner in its own
  `endPlay` too, and a map load destroys the driver through `LowLevelDestroy`, so no path in the tree reaches it.
- **The engine lane at stop.** With the loop on, `LowLevelDestroy`'s stop does not drain the engine lane (nothing services it
  after the join; `EngineLaneDriverCore-rationale.md` §13). The runner does not service a drain window before it exits: the gate
  decides whether abrupt closes need one (og-input-latency task P5-18's F3).

## §13 The client's SimClock (og-input-latency task P3-10)

- **Built with the transport.** `beforeStart` emplaces `UESimNetClockClient` (og-sim-net's `SimNetClockClient` over the transport
  and `FUEPlatformClock`) on a client's transport before `installHandlers`, as the server's clock server is (§12): the transport
  gives the SimClock lane to its clock handler only if one is set by then. Its clock is `FPlatformTime::Seconds()`, the clock
  og-net's host stamps receive times with, so the ping's t0 and the pong's receive time t3 are on one clock. A detached client's
  lanes build one too; nothing pings from it (no driver calls `afterDispatch`).
- **Pinged from the driver's first frame.** `afterDispatch` calls `servicePing()` first on every client frame, before the drain,
  in both modes: og-sim-net's clock pings at 20 Hz from the previous ping (`kClockPingIntervalSeconds`), and a frame point at
  the game thread's rate is at or above that. It sends nothing until the transport knows its server connection (the drain's
  `Connected`), so the first ping leaves one frame after the connection is drained, before the client's world exists. That is
  what lets the clock switch to SimClock before the join's re-centre decision (og-input-latency P3-10 part A F1): a source
  switched after the decision does not move the decided lead.
- **The pong is a sample, not a message.** The clock's lane handler turns each pong into a clockSample record on og-net's
  servicing thread and pushes it into the inbox; the client session reads it ahead of its envelope decode
  (`SimNetClient-rationale.md` §4) and calls the client host's `onClockSample`, which forwards it to the world session on the
  drain's thread: the game thread frame-locked, the step's thread immediate, the estimator's writer in both modes.
- **Held while no session is bound, fed ahead of the first tick** (og-input-latency P3-11, from P3-10 part B F1). Pongs come
  back from the driver's first frames, long before the world, and a host with no session used to drop them, as it drops a clock
  tick. The estimator's first sample then came 4 to 43 ms after the first authority tick, and the join's hard resync landed on
  the floor target and took a second one when the reading arrived (` cause=firstRtt`, `TimeConfig-rationale.md` §16). The
  client host keeps the newest unbound sample in a `FClockSampleLatch`, and its own `onTimingInfo` (which hides the base
  host's) feeds it to the session just before the first authority tick that reaches a bound session. A sample that reaches a
  bound session first supersedes it, and it is fed once. It is the floor and tier latch's pattern (`SimNetClient-rationale.md` §9):
  held while unbound, delivered on the first drain after the bind. A `static_assert` in the lanes' constructor checks that the
  client host declares its own `onTimingInfo`. When it feeds a held sample the host logs one line, at `Warning` under
  `LogOGNet`: `[ClockSource.Held] fed the SimClock sample held while no world session was bound, ahead of authority tick <t>:
  rttMs=<r> heldMs=<h>`, `heldMs` the time since the pong arrived (now minus its receive stamp t3, one clock as above). It is the
  smoke's evidence that a join's first sample came before its world. A join with none held logs nothing, and so does one whose
  bind drain carries a fresh pong, which supersedes the held one (og-input-latency P3-11's diagnostic run on shape (a), three
  joins: each held 4 or 5 samples before its world; two fed the newest, held 16 and 32 ms, and one was superseded).
  - **Why on the first tick and not in `bindClientSession`.** The session binds the lanes before it applies its knobs
    (`UESimulationSession-rationale.md`), and the first SimClock sample switches the estimator's source once, by the knob it
    reads then: fed at the bind, an ini `ClockRttSource=Engine` arm would already have switched. The first tick is on the
    drain's thread, after the session is configured, and ahead of the advance that resyncs.
  - **Threads.** The latch is written by unbound drains, on the game thread in both modes (an immediate client drains on the game
    thread until its session binds, `UESimNetLanes-guards.md` G-02), and read by the first bound drain: the game thread
    frame-locked, the step's thread immediate. That hand-over is the one every other piece of the client session's state takes
    when an immediate client's steps start.
- **The ack RTT.** `afterDispatch` also publishes og-net's smoothed ack RTT to the server connection
  (`UOGNetDriver::GetSmoothedRttSeconds`, game-thread only) into one relaxed atomic (`ackSrttSeconds()`), which the session's
  `[ClockSource.Window]` line reads on the drain's thread as a cross-check (`UESimulationSession-rationale.md` §20). It reads
  `kNoRttReadingSeconds` before og-net has a sample or without a server connection.
- **Lifetime.** The clock client holds the transport and the clock by reference and is the transport's SimClock handler, so
  it is declared after both and destructs first; a `static_assert` in `compositionContractsHold` checks the member order, as
  for the clock server. The driver destroys the lanes after og-net's endpoint, so the handler outlives every call og-net can make.
