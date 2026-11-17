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

- Nothing selects it by default: the project's NetDriverDefinitions name IpNetDriver. One run selects it with the engine's
  three-part override, which names the OG class as its own fallback:
  `-NetDriverOverrides="GameNetDriver,/Script/OGNetUnreal.OGNetDriver,/Script/OGNetUnreal.OGNetDriver"`. A one-part override
  keeps the definition's IpNetDriver fallback, which the engine would select silently if the OG class failed to load or
  reported itself unavailable: a plaintext fallback (UnrealEngine.cpp, lines 14437-14547).
- **The quotes must reach the process.** The engine parses the value with FParse::Value, which stops at the first comma
  unless the value is quoted, and on Windows it rebuilds its command line from CommandLineToArgvW, re-quoting only
  arguments that contain a space (LaunchWindows.cpp, lines 152-187). Plain quotes are consumed by the Windows argument
  parser, so the process sees `GameNetDriver` as the driver class, fails to load it and falls back to IpNetDriver.
  Measured: launched through PowerShell's Start-Process with plain quotes, every process logged
  `DriverClassName: GameNetDriver, DriverClassNameFallback: /Script/OnlineSubsystemUtils.IpNetDriver` and ran IpNetDriver.
  Escaped quotes (`\"` on the Windows command line) arrive as quotes, and the processes logged both class names as
  `/Script/OGNetUnreal.OGNetDriver`.
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
- The core starts in own-thread mode: og-net runs its I/O on its own thread (`OGNet`, above normal priority), so its timers
  keep running through game-thread hitches and map loads. If the core cannot start (an occupied port, an invalid
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
- **`InitConnect`** is InitBase as a client, the server connection (`USOCK_Pending`), the engine's "Game client on port"
  line (as IpNetDriver.cpp, line 1011) and the initial client channels. The connect deadline is now plus
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
- After the tick, a client logs `connect retry n=` whenever the core's retry count grew, and either role logs the foreign
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
- **`LowLevelDestroy`** runs the base, then (not for the class default object) stops the core with a 0.2 s drain on the
  host's clock, destroys the endpoint, which joins og-net's thread, and logs the `stop` line with the core's counters. The
  engine destroys a driver with SetWorld, Shutdown, then LowLevelDestroy, and defers it while the driver ticks
  (UnrealEngine.cpp, lines 14620-14630). An engine connection cleaned up after that finds no endpoint and does nothing.
- **The destructor** stops an endpoint that is still running, with no drain.

## §9 One clock

Every time the core sees (the tick, the deadlines, `engineClosed`, `stop`) and the emulator's clock come from
`FOGNetEndpointHost::Now`, which is the host's FPlatformTime::Seconds, never the engine's frame or world time (guard G-02).
The core's `stop` compares the time it is given with its host's clock.

## §10 Log lines (`LogOGNetTransport`)

| line | when |
|---|---|
| `[OGNet.Driver] start role= mode=ownThread handler=off port= maxPacket=1024 overhead=28 budget= idle= emulator=ognet\|engine\|off` | the endpoint started |
| `[OGNet.Driver] emulator out(loss= lag=min..max dup=) in(loss= lag=min..max) seed= ignored=` | the intake mapped or ignored anything |
| `[OGNet.Emulator] profile=<canonical text>` | og-net's emulator is on |
| `[OGNet.Driver] ignored <setting>=<value>: …` (Warning) | per ignored setting |
| `[OGNet.Driver] up id= handler=off\|on` | an engine connection is bound to an og-net connection |
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
