<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: UWorld::Listen -- Unreal Engine method (World.cpp), outside every scan root; UESimNetLanes §12 cites its order (SetWorld before InitListen) -->
# `UESimNetLanes.h` — rationale

`UESimNetLanes<TraitsT>` is the og-net driver's per-driver sim lanes object: the `IOGNetSimLanes` that `UOGNetDriver` creates at
`InitBase` through the factory a game registers, and calls at six points: `ioMode` and the five frame points
(`OGNetSimLanes-rationale.md`). Since og-session-runtime task 28 it is an adapter over og-sim-net's engine-free `SimNetLink`
(`SimNetLink-rationale.md`), which holds the sim transport, the SimClock server or client, the roster bindings and, on a client,
the client session and its host. The lanes keep what is Unreal's: the driver, the I/O mode, the refused knob, the two RTT
publishes, the authority runner's functions and the stop handler. It is a header-only template over the game's session traits.
This file is the docs pointer of `UESimNetLanes.h`; its prohibitions are in `UESimNetLanes-guards.md`. The engine services the
link is handed, and the session hooks it attaches, are in `UESimNetHost.h` (`UESimNetHost-rationale.md`).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 What it holds, per role

| member | server | client |
|---|---|---|
| `m_link`: `ogsimnet::SimNetLink<Composition, SimLaneEndpoint, FUEPlatformClock, UESimNetLinkServices, UEFMutexLockableUImpl>` (`SimNetLink-rationale.md` §2: the transport, the clock server or client, the bindings, the client session) | yes | yes |
| `m_services`: `UESimNetLinkServices` (`UESimNetHost.h`), the link's engine services, over the driver | yes | yes |
| `m_sessionHooks`: `UESimNetLinkSession<TraitsT>`, the world session's hooks the link attaches | unused | yes |
| a client's refused `SimTrafficImmediate=0` (§11) | no | yes |
| whether the driver asked for host-serviced I/O (`hostServiced()`, §12) | true on a dedicated server | false |
| the published og-net ack RTT (§13) | no | yes |
| the authority runner's RTT publish and join (§12), the session's stop handler (§5) | while bound | the stop handler |

The role is the link's (`m_link.isServer()`), the driver the lanes' (`driver()`): one owner each. The inbox has 2,048 slots
(`kUESimNetInboxSlots` is og-sim-net's `kSimNetLinkInboxSlots`). With og-net's default 16 connections, og-sim-net's own
`static_assert`s size each server connection's quota from it: 124 undrained unreliable messages, and a Control ring of 7,296
slots (`SimNetTransport.h`). A client's inbox has one connection and no quota on the unreliable lanes.

## §2 Ownership, and the join window

The transport and the client session live with the **driver** (in the link); the server session lives with the **world**.

- **Client.** The link creates the client session at the driver's start, before the world exists, so the roster, the floor,
  the tier and a SimClock sample drained in the join window are held for the world (`SimNetLink-rationale.md` §3,
  `SimNetLink-guards.md` G-01). Only map load (the engine's LoadMap) itself is a gap in the draining: the unreliable lanes'
  overflow there is counted as `drops`, in windows before steady state.
- **Server.** The server's world and its listen happen in one map load (the engine's LoadMap), so no frame passes between the driver's start and the
  world's BeginPlay in which a client could have sent anything (the P5 design's inference, K4; not measured here). The world's session creates `SimNetServer` (which owns the
  reception coordinator) on its authority runner and binds the runner's two functions here with `bindAuthorityRunner` (§12). The
  lanes keep the functions, not the server: its type depends on the session's correction policy and its lifetime on the world.

## §3 The six call points

`ioMode` is the driver's sixth call, the first of them (og-input-latency task P5-18 added it with a default, so the five frame
points below were the lanes' only calls until task P5-19 answered it). Each frame point forwards to one of the link's doors
(`SimNetLink-rationale.md` §4), adding what is Unreal's.

| driver point | server | client |
|---|---|---|
| `ioMode(bIsServer)` | `HostServiced` on a dedicated server, `OwnThread` on a listen server (§12) | `OwnThread` |
| `beforeStart(endpoint)` | `m_link.start(endpoint)` | the refused knob's read (§11), then `m_link.start(endpoint)` |
| `afterStart()` | `m_link.markStarted()` | the same |
| `afterDispatch()` | the authority runner's RTT publish, if bound (§12) | `m_link.serviceFrame(...)`: the SimClock ping, then the ack-RTT and the server-RTT publishes (§13, §11), then the drain and a disconnect request only while no session is attached |
| `onTickFlush()` | `m_link.flushFrame()`, which does nothing on a server | `m_link.flushFrame()` |
| `beforeStop()` | the authority runner's join, if bound (G-03); the session's stop handler; unbind the server session; `m_link.stop()` | the session's stop handler, then `m_link.stop()` (it detaches the client session) |

`afterDispatch` runs after `Super::PostTickDispatch` (every packet dispatched, before the first tick group), and `onTickFlush`
after `Super::TickFlush` (after the engine's own send). `OGNetDriver-rationale.md` §13 has the verified engine order. The
disconnect request is the link's (`SimNetLink-rationale.md` §6).

## §4 The transport is built inside `beforeStart`

The transport's sizes come from its endpoint's config, which exists only once the driver has built its endpoint and hands it
over in `beforeStart`. So `beforeStart` calls the link's `start`, which builds the transport, the clock server or client and the
client session there and checks that the endpoint's role is the role the lanes were made for (`SimNetLink-rationale.md` §4).

## §5 The client session's binding

- `bindClientSession(worldSession)`: points the lanes' `UESimNetLinkSession` at the world session and attaches it to the link
  (`SimNetLink-rationale.md` §5: the listener bind that replays the latched floor and tier, the roster seed, the attached flag).
- `unbindClientSession()`: the link's `detachSession`. The client session itself stays, with its roster, latches and channels,
  for the next world on the same driver.
- `setOnRegisteredRemoved(fn)`: forwards the world session's unregister of a registered sim id, which the link's client host calls
  on a roster Remove (`SimNetLink-rationale.md` §8). A session sets it before it registers a character.
- `bindings()` and `editBindings()` give the world session the link's bindings, the gate it registers through
  (`SimNetBindings::verdict`, `markRegistered`, `markUnregistered`, and `markRegisteredIfReady`), behind
  `UEFMutexLockableUImpl` (`SimNetLink-rationale.md` §8).
- `clientSessionBound()` is the link's attached flag, which decides where a client drains (`SimNetLink-rationale.md` §6).
- `setStopHandler(onStop)` (og-input-latency task P5-7): the world session's handler for the driver's stop, which `beforeStop`
  calls once, first, and clears. Unreal shuts a world's net driver down before it destroys the world's pawns and ends its
  play (its map load), and the lanes, with a client's channels, die with the driver. So the session unregisters every
  registered character there, while the channels and the endpoint still live, and forgets the lanes
  (`UESimulationSession-rationale.md` §16.5). A session that ends first clears the handler (its `detachFromSimNetLanes`).

## §6 The server session's binding

A server session binds no frame points: it drains and sends on the authority runner, and og-sim-net's server has no
frame-point calls (og-session-runtime task 15). It binds the runner's two functions (`bindAuthorityRunner`, §12), and `unbindServerSession()`
clears them. On a server `afterDispatch` publishes the runner's RTT table while they are bound, and `onTickFlush` does nothing.
The server session sets the stop handler too (§5); its handler asks the runner to unregister every character, sending the
roster Removes over the still-running endpoint, then releases og-sim-net's server.

Until og-session-runtime task 14 an in-frame server (a listen server, a detached session, or a dedicated server with
og.Sim.AuthorityLoop=0) bound its frame-point dispatch and flush here; those worlds start no session now
(`UESessionRefusal-rationale.md`).

## §7 Detached lanes (retired by og-session-runtime task 14)

Until og-session-runtime task 14 a world with no og-net driver built its own lanes over a detached endpoint and its session ran
alone; such a world starts no session now (`UESessionRefusal-rationale.md`), so a lanes object has one owner, the og-net driver,
and the concept test builds its lanes over a test endpoint of its own (§10).

## §8 Threads

Every member function of the lanes runs on the game thread: the driver's six points do (`InitBase`, `PostTickDispatch`,
`TickFlush`, `LowLevelDestroy`), and so do the world session's binds. The link records the thread that starts it, the game thread
here, and checks every frame door against it (`SimNetLink-rationale.md` §4). The exceptions: a bound client's `drainClient_Step`,
which the world session calls on the step's thread and which forwards to the link's `drainForStep` (its check: not the game
thread); og-net's servicing thread, which runs only the transport's lane and connection handlers; and on a dedicated server the
runner, which services the endpoint (so it runs the handlers and the SimClock server) and calls the server host (§12). The
session's thread table is `UESimulationSession-rationale.md` §24.

## §9 Lifetime

The driver destroys the lanes after its endpoint, so after og-net's thread has joined (`OGNetDriver-rationale.md` §13): no
handler can run into a destroyed transport. The link's own member order is `SimNetLink-rationale.md` §2. The link holds the
lanes' services and session hooks by reference, so both are declared before it; a `static_assert` in the lanes' constructor
keeps `m_services` and `m_sessionHooks` before `m_link`. A world session never keeps a pointer to lanes past `beforeStop`: the
stop handler (§5) runs first in it and the session forgets the lanes there.

## §10 The switch, and the namespace aliases

Since og-input-latency task P5-7 the brawler registers its factory (`makeBrawlerSimNetLanes`) at module start-up, so every
og-net driver makes a lanes object at `InitBase`, and the world session binds to it (`UESimulationSession-rationale.md` §16).
`UESimNetTransport` and `UESimNetChannel<SimT>` name the transport and the per-character channel at namespace scope, without
the session traits, so a game's `SimulatableOwnerTraits` can name the channel where the traits are not complete yet; the class's
`Transport` is `UESimNetTransport`, and the session asserts its channel type equals `UESimNetChannel<Simulatable>`. Before the
switch nothing in the game included this header except its concept test, `UESimNetHostsConceptTest.cpp`, which builds a lanes
object for each role over an endpoint of its own that counts and drops what is sent (`FDroppingEndpoint`, through
`SimLaneEndpoint`'s public constructor; the automation test `OGBrawler.SimNet.Hosts`), checks that the frame points send nothing
and that the clock a server publishes leaves in its batch hook, at once, and pins the I/O-mode rule of §12 in a `static_assert`.
Until og-session-runtime task 15 it used `SimLaneEndpoint`'s detached endpoint and also checked each role's transport flag (the
client's set since task 12, the server's since task 14); the flag is gone.

## §11 Immediate mode on a client (og-input-latency task P5-17)

**A client is always immediate** (og-session-runtime task 12). Every transport sends at once, a client's as a server's
(og-session-runtime task 15 removed the transport's flag).
`[OGNetcode] SimTrafficImmediate` (game or engine ini) is still read there, by `readSimTrafficImmediateKnob`: on a client a value
of 0 is remembered (`simTrafficImmediateKnobRefused`, read back as `simTrafficImmediateRefused()`), and the world session refuses
it at `beginPlay`, `[SimHost.Refused] knob=SimTrafficImmediate value=0 reason=retired`, and starts no session
(`UESessionRefusal-rationale.md`). Any other value, or none, runs the client, silently. The shipped
`Config/DefaultEngine.ini` no longer sets the key; its one comment line names the proof line and the refusal. A `static_assert` in
`UESimNetHostsConceptTest.cpp` pins which values are refused.

A server does not read the knob (og-session-runtime task 14). Every session prints `immediate=1` on its `[SimNet.Host]` line
(`UESimulationSession-rationale.md` §18), which keeps the key byte for byte (design Q7). The knob is refused rather than
ignored because the Ph4 and Ph5 runbooks still pass it for their attribution arms (design §2.2). Until og-session-runtime
task 12 it chose a client's mode, and its 0 ran the client frame-locked: received at the driver's `PostTickDispatch` and
sent at its `TickFlush`.

A client whose knob is refused still has its lanes: they stay unbound and drain before a bind, below, for the world's life.

**Where a client drains** is the link's rule (`SimNetLink-rationale.md` §6): before a world session binds, in `afterDispatch`
on the game thread; once one is bound, at the start of each of its steps, on the step's thread, through `drainClient_Step()`,
which the session calls from the step's receive hook and which forwards to the link's `drainForStep()`; at the session's
teardown, once, through `drainClientWhileStopped_GameThread()` (the link's `drainWhileStopped()`).

**The RTT.** Each client `afterDispatch` hands the link a callable that calls the services' `publishServerRtt_GameThread()`
after the ack-RTT publish (§13); the link runs it after the SimClock ping and before the unbound drain, so the step's clock
delivery reads the server connection's round trip as of this frame (`UESimNetHost-rationale.md` §5). Until og-session-runtime
task 12 a frame-locked client published nothing: its drain read the connection itself, on the game thread.

## §12 The authority loop's server lanes (og-input-latency task P5-19)

- **The I/O mode is the lanes', at `ioMode`.** og-net fixes its servicing thread at the first `serviceUntil`, and the endpoint
  starts in `InitBase`, before the world's session, so the mode cannot wait for the session. `ioMode(bIsServer)` returns
  `simLanesIoMode(isServer, driver->GetNetMode() == NM_DedicatedServer)` and remembers it (`hostServiced()`). The driver's
  net mode is the world's: `UWorld::Listen` gives the driver its world before `InitListen`, and in an editor the driver
  reads the world context's "run as dedicated" flag, so an in-editor Play-In-Editor server world is a dedicated server here
  too (`UESimulationSession-rationale.md` §23.1). Until og-session-runtime task 16b the rule read
  `IsRunningDedicatedServer()`, the process's `-server` switch, which is false for that world:
  - `HostServiced` on a dedicated server: og-net starts no thread and nothing services the endpoint until the session's runner
    idles on it (`OGNetSimLanes-rationale.md` §2). Every dedicated server's session runs the authority loop;
  - `OwnThread` on a client, and on a listen server, so og-net's own thread keeps servicing a listen server's engine lane while
    its world starts no session (`UESimulationSession-rationale.md` §23).

  A `static_assert` in `UESimNetHostsConceptTest.cpp` pins the rule. Until og-session-runtime task 14 `ioMode` read
  og.Sim.AuthorityLoop and latched one of three decisions: off on a client or with the knob 0, refused on a listen server (one
  Warning, `[SimHost.Runner] og.Sim.AuthorityLoop=<n> refused: only a dedicated server runs the authority loop ...`, then the
  server stepped inline), on otherwise. The knob is a tombstone since (`UEAuthorityRunner-rationale.md` §1): its 0 is refused at
  the session's `beginPlay`, and a listen server is refused there too.
- **The transport sends at once** (og-sim-net has one mode since og-session-runtime task 15): the session drains in
  `beforePump` and sends after each batch, on the runner.
- **The SimClock server** (`UESimNetClockServer`, og-sim-net's `SimNetClockServer` over the transport and `FUEPlatformClock`) is
  the link's, built in its `start` before `installHandlers` (`SimNetLink-rationale.md` §4). It answers each client ping at
  receipt on og-net's servicing thread (the runner on a dedicated server; og-net's own thread on a listen server's lanes, which
  no session binds). Its clock is `FPlatformTime::Seconds()`, the clock og-net's host stamps receive times with
  (`OGNetUnrealHost.h`), so t1 and t2 are on one clock (og-input-latency P2-20 F2). The world session publishes the server tick
  to it after every authority step. Every client pings it (§13).
- **The hosts.** The client's host is og-sim-net's `SimNetLinkClientHost`, inside the link (`SimNetLink-rationale.md` §7).
  The server's, `ServerHost`, is `UESimNetLanesServerHost`, which og-session-runtime task 28 moved unchanged into
  `UESimNetHost.h` (`UESimNetHost-rationale.md` §13): it answers og-sim-net's optional `onLifecycleUnregistering(simId)` by
  forwarding it to the world session, answers `rttMs` from `FRunnerRttTable`, and posts its two probe stamps through the
  latency mailbox: `stampServerReceived` through `stampServerReceived_Internal` and `stampHanded` through
  `stampHandedAndLeft_Internal` (the hand-over is the send at once, on the runner;
  `UELatencyBudgetDiagnostics-rationale.md` §14). og-sim-net calls the host from the runner.
- **The RTT table.** `FRunnerRttTable` is 64 lock-free 64-bit words, each an og-net id and a `float` round trip in
  milliseconds, so the runner never reads one connection's id with another's reading. The game thread fills it in `afterDispatch`
  (`publishRunnerRtt_GameThread`): one word per og-net client connection, its value the base host's `rttMs` (which also feeds
  the probe's WIRE sample and keeps the base's one-shot warnings) from the connection's Join and a quiet read before it (next
  bullet), and clears the rest. A connection not in the table reads
  `kNoRttReadingMs`, which the tier table skips. The reading is up to one frame old. This is the final shape of the per-connection
  RTT on the loop (og-input-latency task P5-20: "per-connection RTT reaches the runner through atomics the game thread
  publishes each frame").
- **No RTT warning before the Join** (og-input-latency task P5-20, rework 1: the lead's 17:55 line, from P5-21 F6). The publish
  runs every frame for every connection og-net has accepted, while the retired in-frame server asked for a round trip only when a
  received input bundle names the connection, so after its Join. UE fills a connection's round-trip accumulator from the acks
  it reads (the engine's packet-info pass, ReadPacketInfo in NetConnection.cpp), so a connection that is still logging in can have no reading, and the base's
  read answers `AccumulatorEmpty`. Its warning is one-shot per cause (`TRttCauseWarnings`, `UESimNetHost.h`): asked then, the
  base printed a false `[RttSample] SERVER ... accumulator is empty` line about 0.17 s before the first `Login request` and spent
  the warning a later outage needs. So until UE sets the connection's `ClientLoginState` to `ReceivedJoin` (`UWorld`'s
  `NMT_Join` handler, once the player controller exists) the publish reads `readRoundTripMs` itself and publishes what it
  reads, with no warning and no WIRE sample (the in-frame server took none before the Join either), and a closing connection
  reads `kNoRttReadingMs`, as in the base. From the Join it asks the base (`UESimNetHost-guards.md` G-13), so every warning there means what it says;
  a configuration fault (`NoNetPing`, `RoundTripNotEnabled`) is warned at the Join instead of at the accept. The base `rttMs` is
  untouched.
- **A word of 0 is an empty slot.** The high half is the og-net id, and og-net never assigns 0 (`ognet::kInvalidConnection`, a
  `static_assert` in the table), so no connection's word is 0, even with a round trip of 0. A `UOGNetConnection` whose og-net id
  is still 0 is not published, and `read(0)` answers `kNoRttReadingMs` without a scan (review_P5-19 N2).
- **`bindAuthorityRunner(publishRunnerRtt, joinRunner)`** is the session's, and `checkf`s that the lanes are host-serviced:
  `afterDispatch` calls the first, and `beforeStop` calls the second first of all (G-03), because og-net's stop must run after
  the servicing thread has stopped (`EndpointCore.cpp`'s stop check; `OGNetSimLanes-rationale.md` §2). `unbindServerSession`
  clears both.
- **The driver's destructor does not call `beforeStop`** (`OGNetDriver-rationale.md` §8). A host-serviced driver destroyed
  without `LowLevelDestroy` while its runner still runs would fail og-net's stop check; the session joins its runner in its own
  `endPlay` too, and a map load destroys the driver through `LowLevelDestroy`, so no path in the tree reaches it.
- **The engine lane at stop.** On a dedicated server, `LowLevelDestroy`'s stop does not drain the engine lane (nothing services it
  after the join; `EngineLaneDriverCore-rationale.md` §13). The runner does not service a drain window before it exits: the gate
  decides whether abrupt closes need one (og-input-latency task P5-18's F3).

## §13 The client's SimClock (og-input-latency task P3-10)

- **Built with the transport**, in the link's `start` (`SimNetLink-rationale.md` §4): `UESimNetClockClient` (og-sim-net's
  `SimNetClockClient` over the transport and `FUEPlatformClock`). Its clock is `FPlatformTime::Seconds()`, the clock og-net's host
  stamps receive times with, so the ping's t0 and the pong's receive time t3 are on one clock.
- **Pinged from the driver's first frame.** The link's `serviceFrame` pings first on every client frame, before the drain:
  og-sim-net's clock pings at 20 Hz from the previous ping (`kClockPingIntervalSeconds`), and a frame point at the game thread's
  rate is at or above that. It sends nothing until the transport knows its server connection (the drain's `Connected`), so the
  first ping leaves one frame after the connection is drained, before the client's world exists. That is what lets the clock
  switch to SimClock before the join's re-centre decision (og-input-latency P3-10 part A F1): a source switched after the
  decision does not move the decided lead.
- **The pong is a sample, not a message**, held by the client session while no world session is bound and fed ahead of the first
  authority tick after the bind (`SimNetClient-rationale.md` §19; `SimNetLink-rationale.md` §3). The link's client host forwards
  each sample to the world session through `UESimNetLinkSession::onClockSample` (`onSimClockSample`), on the drain's thread. The
  held line, `[ClockSource.Held] ...`, keeps `LogOGNet` at `Warning` through the link's log routing
  (`SimNetLink-rationale.md` §9) and the route table's `[ClockSource.Held]` row; the automation test
  `OGBrawler.SimNet.ClockHeldLine` (`UESimNetClockHeldLineTest.cpp`) compares it with the old `UE_LOG` (text, category, level).
- **The ack RTT.** `afterDispatch` also publishes og-net's smoothed ack RTT to the server connection
  (`UOGNetDriver::GetSmoothedRttSeconds`, game-thread only) into one relaxed atomic (`ackSrttSeconds()`), which the session's
  `[ClockSource.Window]` line reads on the drain's thread as a cross-check (`UESimulationSession-rationale.md` §20). It reads
  `kNoRttReadingSeconds` before og-net has a sample or without a server connection.
- **Lifetime.** The clock client is the link's, and its member order is `SimNetLink-rationale.md` §2.
