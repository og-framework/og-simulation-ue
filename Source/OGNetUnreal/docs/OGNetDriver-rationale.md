<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGNetDriver.h` and `.cpp` — rationale

`UOGNetDriver` is an Unreal net driver whose every packet travels on og-net's engine lane: encrypted, authenticated and
replay-protected, over og-net's own UDP socket. It is thin engine glue over the engine-free `EngineLaneDriverCore`
(`EngineLaneDriverCore-rationale.md`), which owns every policy (batching, the connection table, the client's pre-connect
buffer, the drained close, connect retries, deferred acceptance). This file is the docs pointer of `OGNetDriver.h`,
`OGNetDriver.cpp` and the automation test `OGNetDriverTests.cpp`; the prohibitions are in `OGNetDriver-guards.md`. Its
connection class is `UOGNetConnection` (`OGNetConnection-rationale.md`) and its address `FInternetAddrOGNet`
(`OGNetInternetAddr-rationale.md`).

Engine source citations are to Unreal Engine 5.6 and are written without backticks, because the engine is outside every
tree this document's lint can resolve against.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 How the driver is selected

- **It is the project's default (task P4-8).** The project's DefaultEngine.ini clears the engine's NetDriverDefinitions in
  its `[/Script/Engine.Engine]` section (`!NetDriverDefinitions=ClearArray`) and defines all three again: GameNetDriver names
  `/Script/OGNetUnreal.OGNetDriver` as its class and as its fallback, and BeaconNetDriver and DemoNetDriver are BaseEngine.ini's
  (lines 321-322). Every build configuration, Shipping included, then carries the game's traffic over og-net. The server logs
  `Name:GameNetDriver Def:GameNetDriver OGNetDriver_0 OGNetDriver listening on port 7883` and every process its
  `[OGNet.Driver] start … handler=off` line (P4-8's default headless smoke, no override). The engine's IpNetDriver section
  stays: the beacon definition uses it, and it holds the switch back (§12 keeps it equal to this driver's).
- **Why the fallback names the OG class.** The engine loads the definition's class and, when the load fails or the class
  reports itself unavailable, loads DriverClassNameFallback instead, without asking the fallback whether it is available
  (UnrealEngine.cpp, lines 14531-14547). BaseEngine.ini's fallback is IpNetDriver, so a definition that kept it would select
  a plaintext driver silently. Measured (P4-8, headless split smokes of one binary whose `IsAvailable` returned false): with
  the fallback set to IpNetDriver the server logged `Name:GameNetDriver Def:GameNetDriver IpNetDriver_0 IpNetDriver listening
  on port 7887`, every client connected over IpNetDriver, all three joins succeeded, and no process logged an `[OGNet.` line or
  any warning about the fallback; with the fallback naming the OG class, every process logged its `[OGNet.Driver] start` line
  and ran this driver.
- **The switch back** is one line: GameNetDriver's definition with BaseEngine.ini's classes,
  `+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")`.
  The IpNetDriver section holds the same values as this driver's (§12), so the engine's own traffic is unchanged by it.
  ⚠ **Since og-input-latency task P5-7 the switch back also switches the simulation's traffic off** (the user's ruling Q2).
  Inputs, corrections, relay rings, the clock, the tier, the floor and the roster ride og-net's sim lanes, which only this
  driver creates (its `IOGNetSimLanes`, from the factory the game registers); on IpNetDriver a networked world's session logs
  `[SimNet.Host] refused role=… driver=IpNetDriver: simulation traffic needs the og-net driver`, shows a toast and starts no
  session; a dedicated server then exits with code 3 (`UESimulationSession-rationale.md` §23; until og-session-runtime task
  14 the session ran detached). The IpNetDriver section stays, for the beacon and config parity. For one run of a
  non-Shipping build the engine's three-part override does the same:
  `-NetDriverOverrides="GameNetDriver,/Script/OnlineSubsystemUtils.IpNetDriver,/Script/OnlineSubsystemUtils.IpNetDriver"`.
  The override is compiled out of Shipping (UnrealEngine.cpp, line 14431). A one-part or two-part override keeps the
  definition's fallback, which is now this driver (lines 14487-14498); the three-part override that names the OG class twice,
  which the Ph2 captures pass, now selects what the definitions already select.
- **The quotes must reach the process.** The engine parses the value with FParse::Value, which stops at the first comma
  unless the value is quoted, and on Windows it rebuilds its command line from CommandLineToArgvW, re-quoting only
  arguments that contain a space (LaunchWindows.cpp, lines 152-187). Plain quotes are consumed by the Windows argument
  parser, so the process sees `GameNetDriver` as the driver class, fails to load it and falls back to the definition's
  fallback. Measured before P4-8, when that fallback was IpNetDriver: launched through PowerShell's Start-Process with plain
  quotes, every process logged
  `DriverClassName: GameNetDriver, DriverClassNameFallback: /Script/OnlineSubsystemUtils.IpNetDriver` and ran IpNetDriver.
  Escaped quotes (`\"` on the Windows command line) arrive as quotes, and the processes logged both class names as
  `/Script/OGNetUnreal.OGNetDriver`. With the flipped definitions the same mistake on a switch back runs this driver.
- `IsAvailable` is always true: the driver has no external dependency that could be missing, and an unavailable driver is
  exactly what triggers the fallback.
- `GetSocketSubsystem` is the platform's socket subsystem. The driver opens no engine socket; it uses the subsystem only to
  resolve host names (§5) and to build `LocalAddr`.
- `IsNetResourceValid` is true while the endpoint is started and not stopped. The engine sends only when it is true
  (NetConnection.cpp, FlushNet, lines 2281-2390), and the client must be able to send before og-net's connection is up so the core can
  buffer its first packets.

## §2 The pimpl

`OGNetDriver.h` is a reflected public header and names no og-net type (guard G-01). Everything og-net lives in
`FOGNetEndpointHost`, defined in the `.cpp` and held by a `TUniquePtr`: the host services (`FOGNetUnrealHost`), the BSD
backend, the `SwitchableBackend` over it, the core, the map from og-net connection id to the engine connection, the close
reasons og-net reported, a 1,232 B receive buffer and the log counters.

- The members are declared so that destruction runs core, backends, host: the core's destructor stops og-net's endpoint,
  which waits for og-net's thread to leave its loop (`EndpointCore.cpp`, the destructor and `stop`), and the host's
  destructor then joins that thread (`OGNetUnrealHost-rationale.md` §3).
- Nothing is created for the class default object; the endpoint exists only between a successful `InitBase` and
  `LowLevelDestroy` (or the destructor).
- The sim lanes object (§13) is a second pointer member, `SimLanes`, outside the pimpl: the game implements it, and it must outlive
  the endpoint (§13).
- The class declares its default constructor, the FVTableHelper constructor and its destructor itself, all defined in the
  `.cpp` where `FOGNetEndpointHost` is complete. Unreal's header tool otherwise generates the FVTableHelper constructor and
  the destructor in the generated `.cpp`, which instantiates the pointer's deleter on an incomplete type: measured, warning
  C4150 as an error. A user-declared FVTableHelper constructor and destructor suppress both.

## §3 `InitBase`: the endpoint

After the base InitBase (which has read the URL's timeout and packet-simulation options, NetDriver.cpp, lines 1588-1652):

- It refuses a NetConnectionClass that is not a `UOGNetConnection`, so the driver never creates an engine connection that
  cannot reach og-net.
- **The endpoint configuration** (all from `EngineLaneSettings.h`; `EngineLaneSettings-rationale.md` has the derivations):
  - the role from `bInitAsClient`; the server binds the listen URL's port, a client port 0; dual stack;
  - the build identity `kEngineLaneBuildIdentity`;
  - at most 16 connections (`kEngineLaneMaxConnections`, asserted to hold the session's 8 players and to be within og-net's
    limit);
  - the budget from `budgetFor` over MaxClientRate, MaxInternetClientRate and the player's ConfiguredInternetSpeed and
    ConfiguredLanSpeed, so og-net's budget sits above every engine rate and the engine stays the only shaper;
  - the timings from `timingsFor` over InitialConnectTimeout, ConnectionTimeout and the multiplier the engine itself
    applies: TimeoutMultiplierForUnoptimizedBuilds when it is above 0 and the build is an editor or debug build or the
    command line has `-longtimeouts`, else 1 (NetConnection.cpp, GetTimeoutValue, lines 4477-4511). og-net's idle timeout
    is then always longer than the engine's, so the engine times a connection out first.
  - default admission, and no server static key (key pinning is optional through Ph3).
- **The emulator intake** (§4) runs before the endpoint opens its socket, because the switchable backend can enable og-net's
  emulator only before open.
- The core starts in own-thread mode unless the sim lanes ask otherwise (§13: `ioMode`, task P5-18): og-net runs its I/O on its
  own thread (`OGNet`, above normal priority), so its timers keep running through game-thread hitches and map loads. In
  host-serviced mode og-net starts no thread, and the game thread still never services the endpoint: `TickDispatch` polls the
  engine lane only (`core.tick`, §6), and the lanes' own thread calls `serviceUntil`. If the core cannot start (an occupied port, an invalid
  configuration) InitBase logs an Error and fails. There is no port walk: IpNetDriver tries up to 512 further ports
  (BaseEngine.ini, MaxPortCountToTry), og-net binds the one port it is given.
- `LocalAddr` is a platform address `0.0.0.0` with the port og-net bound, so the engine and the session log read the real
  port.

## §4 The emulator intake

Outside Shipping (DO_ENABLE_NET_TEST), the engine's packet simulation settings are loaded before InitBase: from the ini, the
command line and the editor's emulation profile when the driver is created (NetDriver.cpp, lines 633-660 and 683), and from
the URL in the base InitBase. The driver then:

- copies them into an `EnginePacketSimulation` field by field. A `static_assert` checks that the engine struct holds
  exactly those 15 int32 fields, so an engine upgrade that adds a field fails the build here;
- maps them with `mapPacketSimulation` and the seed from `-OGNetEmulatorSeed=` (default 1). When anything is mapped it
  enables og-net's emulator on the switchable backend, with this driver's `Now` as the emulator's clock;
- logs one Warning per setting og-net's emulator has no equivalent for, then the `emulator` line and the profile line;
- zeroes the engine's settings, sets bNeverApplyNetworkEmulationSettings and pushes the zeroed settings to every connection.
  From then on the engine ignores every later change: SetPacketSimulationSettings and the runtime Pkt console commands
  both return early on that flag (NetDriver.cpp, lines 3922-3926 and 4695-4706), and each new connection copies the zeroed
  settings in its InitBase (NetConnection.cpp, line 554). The packets are shaped once, by og-net.

With `bUseEnginePacketSimulation=True` (an ini override for one calibration arm) the driver leaves the engine's settings in
place, so the engine's own emulator shapes the OG driver's packets, and og-net's stays off. The start line then says
`emulator=engine` when any setting is non-zero. In Shipping neither emulator exists and the start line says `emulator=off`.

## §5 Listening and connecting

- **`InitListen`** is the base InitBase as a server, then the listen URL's port is set to the bound port and the engine's
  listen line is logged as IpNetDriver logs it, with `OGNetDriver` in place of `IpNetDriver` (IpNetDriver.cpp, lines
  1017-1030). It never initialises the engine's connectionless handler (the packet handler is off, §3 of
  `OGNetConnection-rationale.md`); a check at the end of InitListen fails if one exists. Measured: adding
  InitConnectionlessHandler before it stops the server at listen with `Assertion failed: !ConnectionlessHandler.IsValid()`.
- **`InitConnect`** is InitBase as a client, the server connection (`USOCK_Pending`) and the engine's "Game client on port"
  line (as IpNetDriver.cpp, line 1011). When the notify is the pending net game that owns this driver, the initial client
  channels are not created here: the join is held, and UE's hello leaves at the first TickDispatch after og-net is up and
  after the frame InitConnect ran in (§14). For any other notify the channels are created here, as IpNetDriver does
  (IpNetDriver.cpp, line 1012). The connect deadline is now plus
  InitialConnectTimeout times the multiplier: while the engine's pending connection lives, the core retries when og-net's
  handshake gives up.
  - A numeric host (`numericJoinAddress`) connects at once.
  - A name is resolved with the socket subsystem's GetAddressInfoAsync, which runs on a background task
    (SocketSubsystem.cpp, lines 320-324). The result is handed to the game thread with a weak pointer to the driver; the
    first IPv4 result is preferred, else the first result. A failed resolution logs an Error and closes the server
    connection, which the engine reports as a network failure.

## §6 `TickDispatch` and the core's callbacks

The base TickDispatch runs first (on a server it cleans up closed client connections, NetDriver.cpp, lines 2548-2565), then
`core.tick(now, the server connection is pending)`, which also feeds and closes og-net's `[OGNet.Window]` (§10). The callbacks
run on the game thread, after og-net's poll has returned:

- **accept** asks the engine's NotifyAcceptingConnection. Accept creates a `UOGNetConnection` (open, with the og-net id as its
  address), then NotifyAcceptedConnection and AddClientConnection, as Steam's sockets driver does
  (SteamSocketsNetDriver.cpp, lines 451-541), and logs the `up` line. Ignore keeps og-net's connection pending (the engine
  answers Ignore while the server switches levels); Reject has the core disconnect it. With no notify yet, the verdict is
  Ignore.
- **deliver** copies the packet into the receive buffer and calls the connection's ReceivedRawPacket; a packet for an id
  with no live engine connection is counted and dropped.
- **closed** (og-net closed the connection) remembers the reason for the `closed` line and marks the engine connection
  `USOCK_Closed`; the engine cleans it up and reports the failure on its own.
- **serverConnectionUp** (client) gives the server connection its og-net id and address and logs the `up` line. **It leaves
  the connection `USOCK_Pending`.** Steam's driver opens the connection at this point (SteamSocketsNetDriver.cpp, line 552),
  but over IpNetDriver the client's server connection stays pending until the engine itself opens it when the player
  controller arrives (NetConnection.cpp, HandleClientPlayer, line 4986), and the client gates on that state: its actors do
  not tick before it (LevelTick.cpp, lines 1410-1417), a split-screen join is refused before it (LocalPlayer.cpp, line 335)
  and streaming levels wait on it for visibility acks (LevelStreaming.cpp, line 773). Opening early would change all
  three. The task text said "then USOCK_Open"; it is not followed, for this reason.
- **connectEnded** (the handshake ended for good: rejected, wrong server key, or out of time) logs a Warning and closes the
  server connection.
- After the tick, a client logs `connect retry n=` whenever the core's retry count grew, then, while its join is held,
  releases it or keeps holding it (§14). Either role logs the foreign
  traffic Warning when og-net's malformed-datagram count grew, at most once per 60 s. A peer still on IpNetDriver sends
  datagrams og-net cannot parse, so this names the likely cause of a connection that never comes up.

## §7 Sending

- `sendEngine(id, bytes)` is the core's `send` (the connection's LowLevelSend calls it). Before the client is up the core
  buffers; a refused send is counted by the core; nothing blocks.
- The driver's connectionless `LowLevelSend` never sends. The engine uses it only for the stateless handshake of a packet
  handler, which this driver does not have; each call is counted and logged at Verbose.

## §8 Closing and shutting down

- **`engineClosed(connection)`** runs after the connection's base CleanUp (`OGNetConnection-rationale.md` §6) and checks
  that the engine has closed the connection. It logs the `closed` line and then tells the core, which drains the
  connection's queue (at most 0.2 s) before disconnecting it in og-net. In the line, `reason` is og-net's reason when og-net
  closed the connection first and `engine` otherwise; `stats=live` when og-net still had the connection's statistics and
  `gone` when it had already dropped them (og-net reports an unknown id as disconnected, `EndpointCore.cpp`, `stats`), in
  which case the per-connection counts are 0. `refused` and `preConnectDropped` are the driver's totals.
- **Shutdown** is the base's: a server sends every client the close reason and cleans each connection up (each reaches
  `engineClosed`), a client closes and flushes its server connection (NetDriver.cpp, lines 2222-2293). The driver adds
  nothing to it.
- **`LowLevelDestroy`** runs the base, then (not for the class default object) calls the sim lanes' `beforeStop` (§13), stops
  the core with a 0.2 s drain on the host's clock, destroys the endpoint, which joins og-net's thread, destroys the sim lanes, and
  logs the `stop` line with the core's counters. In host-serviced mode there is no og-net thread and no drain
  (`EngineLaneDriverCore-rationale.md` §8): `beforeStop` has joined the lanes' servicing thread, and the core's stop closes the
  connections on the game thread (`OGNetSimLanes-rationale.md` §2). The
  engine destroys a driver with SetWorld, Shutdown, then LowLevelDestroy, and defers it while the driver ticks
  (UnrealEngine.cpp, lines 14620-14630). An engine connection cleaned up after that finds no endpoint and does nothing.
- **The destructor** stops an endpoint that is still running, with no drain, then destroys the endpoint and the sim lanes, in
  that order. It does not call `beforeStop`: a driver destroyed without `LowLevelDestroy` is being collected, and what the lanes
  would unbind from may already be gone.

## §9 One clock

Every time the core sees (the tick, the deadlines, `engineClosed`, `stop`) and the emulator's clock come from
`FOGNetEndpointHost::Now`, which is the host's FPlatformTime::Seconds, never the engine's frame or world time (guard G-02).
The core's `stop` compares the time it is given with its host's clock.

## §10 Log lines (`LogOGNetTransport`)

| line | when |
|---|---|
| `[OGNet.Driver] start role= mode=ownThread\|hostServiced handler=off port= maxPacket=1024 overhead=28 budget= idle= emulator=ognet\|engine\|off` | the endpoint started; `mode` is the core's (§13) |
| `[OGNet.Driver] emulator out(loss= lag=min..max dup=) in(loss= lag=min..max) seed= ignored=` | the intake mapped or ignored anything |
| `[OGNet.Emulator] profile=<canonical text>` | og-net's emulator is on |
| `[OGNet.Driver] ignored <setting>=<value>: …` (Warning) | per ignored setting |
| `[OGNet.Driver] up id= handler=off\|on` | an engine connection is bound to an og-net connection |
| `[OGNet.Driver] hello held frame=` | a client's InitConnect, when its notify is the pending net game (§14) |
| `[OGNet.Driver] hello released frame= heldFrame= id=` | the TickDispatch that sends UE's hello for a held join (§14) |
| `[OGNet.Driver] closed id= reason= stats= engineSent= engineRecv= refused= preConnectDropped= queueDropsAge= queueDelayP99Us=` | the engine cleaned a connection up |
| `[OGNet.Driver] connect retry n=` | the client reconnected after og-net's handshake gave up |
| `[OGNet.Driver] connect ended reason=` (Warning) | the client's handshake ended for good |
| `[OGNet.Driver] foreign traffic malformed= (a peer on IpNetDriver?)` (Warning) | og-net's malformed count grew, at most once per 60 s |
| `[OGNet.Driver] stop role= accepted= … deliveriesWithoutConnection=` | LowLevelDestroy |
| `[OGNet.Window]`, `[OGNet.Window.Lane]`, `[OGNet.Window.Rx]`, `[OGNet.Window.Endpoint]` | every 10 s, from TickDispatch: og-net's window lines, written by the core (`EngineLaneDriverCore-rationale.md` §11) |

The engine's own LogNet lines are kept: "Game client on port …" and "… OGNetDriver listening on port …". og-net's own
lines (`[OGNet.Conn] up`, `down` and the rest) are on the same category. Ids are printed in decimal, as og-net prints
them, so the two sets of lines join; the address text uses hex (`OGNet:` and 8 digits).

## §11 Differences from IpNetDriver the driver does not remove

- **A killed peer is noticed later.** On loopback, IpNetDriver receives the ICMP port-unreachable a killed client's closed
  port produces and cleans the connection up at once (IpNetDriver.cpp, lines 1198-1221). og-net ignores ICMP, which anyone
  on the path can forge, so the engine notices only at its own ConnectionTimeout. Measured in the split harness (the
  client killed at 60 s, the engine's ConnectionTimeout 10 s on the server): IpNetDriver released the client 0.02 s after
  the kill, this driver 10.07 s after it.
- No port walk (§3); og-net's handshake replaces the engine's stateless challenge; og-net's emulator replaces the engine's
  (§4).

## §12 The test `OGNet.Unreal.Driver.ConfigMatchesIpNetDriver`

The ini section `[/Script/OGNetUnreal.OGNetDriver]` in the project's DefaultEngine.ini mirrors the effective values
IpNetDriver gets from BaseEngine.ini and the project's own IpNetDriver section, because the engine reads config per class
and IpNetDriver's section does not reach a sibling class. The automation test (EditorContext, ProductFilter) loads
IpNetDriver's class and compares every config property of `UNetDriver` and its supers, except NetConnectionClassName,
between the two class default objects with Identical; a failure names the property and both values. Measured: 23 properties
compared; deleting `MaxClientRate=250000` from the OG section fails it with
`UNetDriver config property MaxClientRate differs: IpNetDriver=250000 OGNetDriver=15000`.

## §13 The sim lanes (task P5-3, design P5 K1 to K3)

The driver can carry a game's simulation traffic on its endpoint's sim lanes through one `IOGNetSimLanes` object
(`OGNetSimLanes-rationale.md`). **With no factory registered, nothing below runs**: no lanes object exists, `start` gets no attach
(`EngineLaneDriverCore-rationale.md` §12), and the two tick overrides only call the base, so the driver behaves as P4 built it,
with the same log lines.

- **`SetSimLanesFactory(factory)`** (static) stores one factory for every later driver. Three checks: it is called on the game
  thread (InitBase reads it there); it is called once; and no `UOGNetDriver::InitBase` has run before it, because a driver that
  started without lanes would carry no sim traffic for its whole life. The game registers it at start-up (task P5-7).
- **`InitBase`** creates the lanes object with `factory(driver, bIsServer)` just before the core's `start`, after the emulator
  intake, asks it for the I/O mode (`ioMode(bIsServer)`, task P5-18; own-thread without a lanes object), starts the core in that
  mode, and passes the attach whose `beforeStart` calls the object's `beforeStart` with the core's sim endpoint. The `start` line
  prints the mode the core started in (§10). A `static_assert` checks that the interface's default, `IoMode{}`, is `OwnThread`
  (`OGNetSimLanes-rationale.md` §1). After a
  successful start and the `start` line it calls `afterStart`. When the start fails, the endpoint is destroyed and then the lanes
  object, without `afterStart`.
- **The frame points are the driver's own overrides**, not world delegates:
  - `PostTickDispatch` is the base's, then `afterDispatch()`. The world broadcasts the dispatch and then the post-dispatch
    delegates (LevelTick.cpp, lines 1335-1336), both before the tick groups (`TG_PrePhysics`, line 1511). The driver's dispatch
    delegate is InternalTickDispatch: Iris's PreReceiveUpdate, TickDispatch (every packet of the frame), then Iris's
    PostReceiveUpdate (NetDriver.cpp, lines 2187-2205). So `afterDispatch` sees every packet of the frame dispatched, and runs
    before the frame's simulation.
  - `TickFlush` is the base's, then `onTickFlush()`. The world broadcasts the flush after the tick groups (LevelTick.cpp, line
    1687), and the base TickFlush is where Iris sends (NetUpdate, NetDriver.cpp, lines 1064 and 1094). So `onTickFlush` runs
    after the engine has sent the frame's traffic.
  - Both are virtual in UNetDriver (NetDriver.h, lines 1665 and 1668), and the world's delegates call them through the driver's
    registration (NetDriver.cpp, RegisterTickEvents, line 2165).
  - **Why not world delegates.** A multicast delegate calls its bound functions in reverse order (MulticastDelegateBase.h, line
    251), so a handler a session binds after the driver registered runs before the driver's own: before the frame's packets are
    dispatched, or before Iris sends.
- **`LowLevelDestroy`** calls `beforeStop()` before the core stops, while og-net still delivers and sends; a `checkf` fails if the
  core has already stopped. The lanes object is then destroyed after the endpoint, whose destruction joins og-net's thread: until
  then that thread may call the handlers the lanes installed. A `checkf` in `DestroySimLanesAfterEndpoint` fails if the endpoint
  still exists; the destructor and a failed `InitBase` go through the same function.
- **`GetSimLanes()`** returns the object (null without a factory, before `InitBase` and after `LowLevelDestroy`).
- **`FindConnection(OGNetId)`** (game thread, a `checkf`) returns the engine connection bound to an og-net id, from the pimpl's
  map, or null: the sim side's way from a sim connection id (which is og-net's id) to the engine's connection, for the RTT read and
  the server's owner lookup (design P5 §3.3).
- **`GetSmoothedRttSeconds(OGNetId)`** (game thread, a `checkf`) returns og-net's smoothed ack round trip for that connection,
  in seconds, from the core's published connection statistics, or −1.0 when the endpoint is gone, the id is not a published
  connection, or og-net has no sample yet. A plain `double`, so this reflected header names no og-net type (G-01). Its one
  caller is the client lanes' publish for the session's `[ClockSource.Window]` cross-check (og-input-latency task P3-10;
  `UESimNetLanes-rationale.md` §13).

## §14 The held join: UE's hello leaves after the frame that opened the connection (task P4-13)

**What it fixes.** UPendingNetGame::InitNetDriver calls BeginHandshake as soon as InitConnect returns (PendingNetGame.cpp,
lines 64-80; the default ULocalPlayer::PreBeginHandshake runs its delegate at once, LocalPlayer.h, line 450). With no packet
handler, BeginHandshake calls SendInitialJoin at once (PendingNetGame.cpp, lines 112-124), which sends NMT_Hello and flushes
the connection (lines 166-172). So while InitConnect created the initial client channels, UE's hello was sent inside the frame
that opened the connection; at start-up that is the engine-init frame. og-net buffered it until its own handshake was up, and
the client sees that during its first engine tick, whose frame counter is still 0. The engine stamps a packet's send time when
it flushes it (NetConnection.cpp, line 2326, kept at lines 2400-2401) and its ack time as the start of the frame that
processes the ack (lines 2912-2913). The hello's round trip therefore held the rest of that long frame. Measured by P4-12: on
rendered loopback og-net's first RTT window read 83-104 ms against IpNetDriver's 44-51 ms, which raised the clock's target
offset; headless it read 36-43 ms against 10-16 ms.

**IpNetDriver's order.** Its client hello is sent by the packet handler's handshake-complete delegate (PendingNetGame.cpp,
lines 116-118; PacketHandler.cpp, line 1084), which fires when the stateless handshake's last packet is received
(StatelessConnectHandlerComponent.cpp, line 1036): on the receive path of a TickDispatch, never inside InitConnect, and in a
frame after the one that opened the connection. Its first handshake request goes unanswered and is resent after
net.HandshakeResendInterval, 1 s (StatelessConnectHandlerComponent.cpp, line 243), so its hello leaves about 1.04 s after
Browse.

**What the driver does.**
- **InitConnect** looks for the UPendingNetGame whose NetDriver is this driver and which is the notify it was given (an object
  iteration over UPendingNetGame; the engine sets NetDriver before it calls InitConnect, PendingNetGame.cpp, lines 44-54). When
  it finds one it holds the join: it remembers the pending game and the engine's frame counter, logs
  `[OGNet.Driver] hello held frame=`, and does not create the initial client channels. The synchronous SendInitialJoin that
  follows then sends nothing: NMT_Hello is only written when the connection has a control channel (DataChannel.h, line 123),
  and FlushNet has no bits, no acks and no keepalive due (NetConnection.cpp, line 2293). The engine still logs its
  "UPendingNetGame::SendInitialJoin: Sending hello" line at that point.
- **Each client TickDispatch**, after the core's tick, while the join is held:
  - the hold ends without a hello when the pending game is gone, is no longer this driver's, or the server connection is no
    longer pending (the engine reports that failure on its own);
  - when og-net is up (the core has a server connection id) and the engine's frame counter (GFrameCounter, incremented once
    per engine tick, at its end, LaunchEngineLoop.cpp, line 5872) is past the held frame, the driver checks that the control
    channel is still absent and that the connection has sent no packet (a `checkf`, which fires if the channels are ever
    created in InitConnect again), creates the initial client channels, logs `[OGNet.Driver] hello released frame=
    heldFrame= id=` and calls the pending game's SendInitialJoin. The engine logs its "Sending hello" line a second time, and
    this time the hello is sent;
  - otherwise it sets the server connection's LastSendTime to the driver's elapsed time.
- **A notify that is not a pending net game** keeps the order from before this task: the channels are created in InitConnect.
  No engine path uses that today; the beacon definition stays on IpNetDriver.
- **The hold assumes the engine's immediate PreBeginHandshake.** InitNetDriver reaches BeginHandshake through the first
  local player's PreBeginHandshake (PendingNetGame.cpp, lines 69-76), whose default runs it at once (LocalPlayer.h, line
  450), and the project's local player class (OGBrawlerLocalPlayer) does not override it. A local player that deferred it,
  for an online pre-authentication say, would get the driver's release first; its own late BeginHandshake would then call
  SendInitialJoin with the control channel open, sending a second NMT_Hello, and the first hello would have left before the
  game's pre-handshake work finished.

**Why the frame, and not only "og-net is up".** At start-up og-net is up within the first engine tick, which is still frame 0.
In P4-12's `Ph2_c_B1` client1 the hello was logged at 31.184 s, "Engine is initialized" at 31.315 s and `up` at 31.329 s, all
in frame 0, and frame 1 began at 31.397 s. A hello sent at that first TickDispatch would still be acked at frame 1's start
(68 ms in that log), so its sample would still hold the end of the engine-init frame. The first TickDispatch of a later frame
is the earliest point outside the frame that opened the connection, and it is the order IpNetDriver has: a hello sent from a
TickDispatch, after that frame. It still leaves sooner after Browse than IpNetDriver's, because og-net has no 1 s resend
(P4-12's review estimated 0.2-0.5 s); the round trip it measures no longer includes the engine-init frame.

**Why LastSendTime.** With IpNetDriver no keepalive is sent before the packet handler's handshake completes
(NetConnection.cpp, lines 4784-4792). With the handler off nothing else stops UNetConnection::Tick's keepalive in TickFlush
once the driver's elapsed time (advanced in TickDispatch, NetDriver.cpp, line 2539) passes KeepAliveTime, 0.2 s. A keepalive
sent while the join is held would be a client packet from inside the frame that opened the connection, with the same stretched
stamp. Holding LastSendTime at the elapsed time keeps the hello the client's first engine packet.

**The engine's log at a join, on og-net.** Before: "Game client on port", "Sending hello", then `[OGNet.Driver] up`. After:
"Game client on port", `hello held`, "Sending hello" (nothing sent), `up`, `hello released`, "Sending hello" (sent). A
script that marks the hello by the engine's line takes the last "Sending hello" before "Welcomed by server", or
`hello released`.

**The test `OGNet.Unreal.Driver.HelloAfterTheConnectingFrame`** (EditorContext, ProductFilter) runs the engine's own join
call: a server `UOGNetDriver` listens on an ephemeral port with a recording notify, and a `UPendingNetGame` (no world context,
so it calls BeginHandshake at once) is given a client `UOGNetDriver` through InitNetDriver. In the same frame it checks that the
connection has sent no packet and has no control channel. It then pumps both drivers (TickDispatch, and TickFlush on the
client) until og-net is up and the client's elapsed time is past KeepAliveTime, and checks again: no packet, no hello at the
server, still the same frame. A latent command waits for the next engine frame, pumps the client once, and checks that the
hello left and that the server received exactly one NMT_Hello.
Measured: with the channels created in InitConnect again (the order before this task) it fails with `Expected 'UE packets
sent inside InitConnect and BeginHandshake' to be 0, but it was 1`; with the frame condition dropped (G-03) with `Expected 'UE
packets sent in the frame that opened the connection, og-net up' to be 0, but it was 3`; with the LastSendTime hold removed
the release's `checkf` stops the editor with `the held join found the control channel open or 2 packets sent before UE's
hello`.
