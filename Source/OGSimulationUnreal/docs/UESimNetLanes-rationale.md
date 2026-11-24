<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: UWorld::Listen -- Unreal Engine method (World.cpp), outside every scan root; UESimNetLanes §12 cites its order (SetWorld before InitListen) -->
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
| a client's refused `SimTrafficImmediate=0` (§11) and whether a client session is bound | no | the refused knob and the bind |
| whether the driver asked for host-serviced I/O (`hostServiced()`, §12) | true on a dedicated server | false |
| the SimClock's end of the transport: `UESimNetClockServer` (§12) or `UESimNetClockClient` and the published og-net ack RTT (§13) | the server | the client |
| the authority runner's RTT publish and join (§12) | while bound | no |

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
  reception coordinator) on its authority runner and binds the runner's two functions here with `bindAuthorityRunner` (§12). The
  lanes keep the functions, not the server: its type depends on the session's correction policy and its lifetime on the world.

## §3 The six call points

`ioMode` is the driver's sixth call, the first of them (og-input-latency task P5-18 added it with a default, so the five frame
points below were the lanes' only calls until task P5-19 answered it).

| driver point | server | client |
|---|---|---|
| `ioMode(bIsServer)` | `HostServiced` on a dedicated server, `OwnThread` on a listen server (§12) | `OwnThread` |
| `beforeStart(endpoint)` | the transport, the SimClock server, the handlers, the forward target | the transport, the SimClock client (§13), the handlers, the forward target, then the client host and the client session |
| `afterStart()` | `markEndpointStarted` | `markEndpointStarted` |
| `afterDispatch()` | the authority runner's RTT publish, if bound (§12) | first the SimClock ping and the ack-RTT publish (§13), then the server-RTT publish (§11); then `drain` and a disconnect request (below) only while no session is bound (§11) |
| `onTickFlush()` | nothing | `flushFrame` while no session is bound; with a bound session, nothing (§11) |
| `beforeStop()` | the authority runner's join, if bound (G-03); the session's stop handler; then unbind the server session | the session's stop handler, then unbind the client session |

`afterDispatch` runs after `Super::PostTickDispatch` (every packet dispatched, before the first tick group), and `onTickFlush`
after `Super::TickFlush` (after the engine's own send). `OGNetDriver-rationale.md` §13 has the verified engine order.

**The disconnect request.** A client that refuses a roster message (P7's rule: a refused roster decode disconnects) latches a
request. `afterDispatch` takes it after each drain, logs `[SimNet.Host] role=client disconnect conn= reason=` at Error to
`LogOGNet`, and disconnects that og-net connection through the endpoint.

## §4 The transport is built inside `beforeStart`

The transport's sizes come from its endpoint's config, `ogsimnet::simNetConfigFor(endpoint.config())`: a server's for
its `maxConnections`, a client's for its one server, and the Control quota from the lane table. That config exists only once
the driver has built its endpoint, and the driver hands the endpoint over in `beforeStart` and nowhere earlier. So the
transport is an `std::optional` emplaced there (it can be neither copied nor moved), and so is the client session that holds
a reference to it. `beforeStart` also `checkf`s that the endpoint's role is the role the lanes were made for. Until
og-session-runtime task 15 the config also carried a flag that chose between sending at once and holding the frame's sends until
the flush; every transport sends at once now, and the flag and its argument are gone (`SimNetTransport-rationale.md` §7).

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
  under one lock). Every call takes the bindings' mutex (`UESimNetHost-rationale.md` §12), because a client's drain
  feeds them on the step's thread while the world session reads them on the game thread.
- The bind and the unbind set and clear `clientSessionBound()`, which decides where a client drains (§11).
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

Every member function runs on the game thread: the driver's six points do (`InitBase`, `PostTickDispatch`, `TickFlush`,
`LowLevelDestroy`), and so do the world session's binds. og-net's servicing thread runs only the transport's lane and
connection handlers, which `beforeStart` installs before the endpoint starts. The exceptions: a bound client's
`drainClient_Step`, which the world session calls on the step's thread (§11), with `clientSessionBound()` an atomic for that
reader; and on a dedicated server the runner, which services the endpoint (so it runs the handlers and the SimClock
server) and calls the server host (§12). The session's thread table is `UESimulationSession-rationale.md` §24.

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

**Where a client drains.**
- **Before a world session binds**, at `afterDispatch` on the game thread, through `drain()`, the client's only drain
  (`SimNetClient-rationale.md` §16). Nothing
  steps yet, so the game thread is the drain's only caller. The disconnect request is acted on after it.
- **Once a world session is bound**, at the start of each of its steps, on the step's thread, through `drainClient_Step()`,
  which the session calls from the step's receive hook, before the step's resimulation check and collect (the step's send
  comes later, after its capture; `UESimulationSession-rationale.md` §18.2). It `checkf`s that it is off the game thread (the AC's check at the drain) and
  that the session is bound, drains, acts on the disconnect request, and calls `flushFrame()`:
  that holds nothing (every send already went to the endpoint), but it emits the `[SimNet.Window]` line, whose counters the drain
  writes, so it belongs to the drain's thread. `afterDispatch` then drains nothing (G-02) and `onTickFlush` does nothing.
- **After the session unbinds**, back at `afterDispatch`. The session stops its steps before it unbinds
  (`UESimulationSession-rationale.md` §18.6), so the two never overlap.
- **At the session's teardown**, once, through `drainClientWhileStopped_GameThread()`: the steps have stopped, and the drain
  applies the unregistrations the session queued last (`UESimulationSession-rationale.md` §18.6).

**The RTT.** Each client `afterDispatch` calls the host's `publishServerRtt_GameThread()` after the SimClock ping and the
ack-RTT publish (§13) and before the unbound drain, so the step's clock delivery reads the server connection's round trip as of
this frame (`UESimNetHost-rationale.md` §5). Until og-session-runtime task 12 a frame-locked client published nothing: its drain
read the connection itself, on the game thread.

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
  built on every server's transport, before `installHandlers`, because the transport hands the SimClock lane to
  its clock handler only if one is set by then. It answers each client ping at receipt on og-net's servicing thread (the runner on a
  dedicated server; og-net's own thread on a listen server's lanes, which no session binds). Its clock is `FPlatformTime::Seconds()`, the clock og-net's host stamps
  receive times with (`OGNetUnrealHost.h`), so t1 and t2 are on one clock (og-input-latency P2-20 F2). The world session
  publishes the server tick to it after every authority step. Every client pings it (§13).
- **The hosts.** `ServerHost` and `ClientHost` are `UESimNetLanesServerHost` and `UESimNetLanesClientHost`, thin derivations of
  `UESimNetHost.h`'s hosts:
  - both answer og-sim-net's optional `onLifecycleUnregistering(simId)` (`SimNetHost-rationale.md` §4a) by forwarding it to the
    world session, which notifies its systems while the character is still in storage;
  - the client host also answers the optional `onClockSample(sample)` (`SimNetHost-rationale.md` §4b) by forwarding it to the
    world session (`onSimClockSample`, §13); with no session bound the newest sample is held, and fed ahead of the first
    authority tick after the bind (`FClockSampleLatch`, §13; a clock tick with no session is dropped);
  - the server host answers `rttMs` from `FRunnerRttTable` and posts its two probe stamps through the latency mailbox:
    `stampServerReceived` through `stampServerReceived_Internal` and `stampHanded` through `stampHandedAndLeft_Internal` (the
    hand-over is the send at once, on the runner; `UELatencyBudgetDiagnostics-rationale.md` §14). og-sim-net calls the
    host from the runner. Until og-session-runtime task 14 the three forwarded to the base host's game-thread members (its
    `rttMs` and its two stamps through the diagnostics' game-thread doors) until the session switched the host to the runner,
    for the in-frame server; task 14 deleted the switch and the base's two stamps (`UESimNetHost-rationale.md`), and kept the
    base `rttMs`, which the RTT publish below calls.
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
  reads `kNoRttReadingMs`, as in the base. From the Join it asks the base (G-04), so every warning there means what it says;
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

- **Built with the transport.** `beforeStart` emplaces `UESimNetClockClient` (og-sim-net's `SimNetClockClient` over the transport
  and `FUEPlatformClock`) on a client's transport before `installHandlers`, as the server's clock server is (§12): the transport
  gives the SimClock lane to its clock handler only if one is set by then. Its clock is `FPlatformTime::Seconds()`, the clock
  og-net's host stamps receive times with, so the ping's t0 and the pong's receive time t3 are on one clock.
- **Pinged from the driver's first frame.** `afterDispatch` calls `servicePing()` first on every client frame, before the drain: og-sim-net's clock pings at 20 Hz from the previous ping (`kClockPingIntervalSeconds`), and a frame point at
  the game thread's rate is at or above that. It sends nothing until the transport knows its server connection (the drain's
  `Connected`), so the first ping leaves one frame after the connection is drained, before the client's world exists. That is
  what lets the clock switch to SimClock before the join's re-centre decision (og-input-latency P3-10 part A F1): a source
  switched after the decision does not move the decided lead.
- **The pong is a sample, not a message.** The clock's lane handler turns each pong into a clockSample record on og-net's
  servicing thread and pushes it into the inbox; the client session reads it ahead of its envelope decode
  (`SimNetClient-rationale.md` §4) and calls the client host's `onClockSample`, which forwards it to the world session on the
  drain's thread, the step's thread once a session is bound: the estimator's writer.
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
  - **Threads.** The latch is written by unbound drains, on the game thread (a client drains on the game thread until its
    session binds, `UESimNetLanes-guards.md` G-02), and read by the first bound drain, on the step's thread. That hand-over is
    the one every other piece of the client session's state takes when a client's steps start.
- **The ack RTT.** `afterDispatch` also publishes og-net's smoothed ack RTT to the server connection
  (`UOGNetDriver::GetSmoothedRttSeconds`, game-thread only) into one relaxed atomic (`ackSrttSeconds()`), which the session's
  `[ClockSource.Window]` line reads on the drain's thread as a cross-check (`UESimulationSession-rationale.md` §20). It reads
  `kNoRttReadingSeconds` before og-net has a sample or without a server connection.
- **Lifetime.** The clock client holds the transport and the clock by reference and is the transport's SimClock handler, so
  it is declared after both and destructs first; a `static_assert` in `compositionContractsHold` checks the member order, as
  for the clock server. The driver destroys the lanes after og-net's endpoint, so the handler outlives every call og-net can make.
