<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: FUEConnectionHandle -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: UEConnectionHandle.h -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: OnRep -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: UNetConnection::InitBase -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: Net/Core/Connection/ConnectionHandle.h -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: UChildConnection::AssignConnectionHandle -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: bExcludeFrameTime -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: ServerFrameTime -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: ReadPacketInfo -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: UNetConnection::ReceivedRawPacket -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: CurrentTime -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: SimulatableNetSyncComponent.cpp -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
# `UESimNetHost.h` — rationale

`UESimNetServerHost<TraitsT>` and `UESimNetClientHost<TraitsT>` are the Unreal hosts of og-sim-net's two sessions: the services
`SimNetServer` and `SimNetClient` take as their `HostT` (og-sim-net's `SimNetServerHost` and `SimNetClientHost` concepts, plus
the optional delivery hook). Each one answers from today's Unreal sources: the engine's ping, the latency probe, the log routes,
and the world session's tier, floor and timing consumers. This file is the docs pointer of `UESimNetHost.h`; its prohibitions
are in `UESimNetHost-guards.md`. The lanes object that holds the client host is in `UESimNetLanes.h`. The header also holds
the engine primitives the hosts and the world session read: `LogOGRtt`, the RTT sentinels, `GetRootNetConnection`,
`GetPlayerSlotForActor` and `readRoundTripMs` (§11), and its `.cpp` defines `LogOGRtt`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 The concept members and their sources

| member | server host | client host |
|---|---|---|
| `nowSeconds` | `FPlatformTime::Seconds` | the same |
| `rttMs(conn)` | the og-net connection's engine ping, plus the probe's wire sample (§2) | — |
| `serverRttSeconds` | — | the server connection's engine ping (§5) |
| `receptionTick` | the session's `getServerReceptionTick` (§3) | — |
| `stampServerReceived`, `stampHanded` | the latency probe (§4) | — |
| `stampClientReceived` | — | the latency probe (§4) |
| `onTimingInfo`, `onRelayFloor`, `onConnectionTier` | — | the world session's consumers (§6) |
| `onRosterAdded`, `onRosterRemoved` | — | the bindings, and the session's unregister (§7) |
| `onWireMismatch` | — | today's Error line and toast (§8) |
| `log` | `RouteOGMessage` (§9) | the same |
| `noteDelivered` | the driver's `[OGNet.Window]` sim-lane rows (§9) | the same |

The world session is `UESimulationSession<TraitsT>`, named here only by a forward declaration: every call into it is in a
template body, instantiated where the session is complete. The calls use its public members.

**Bound and unbound.** The client host is made by the lanes at driver start and bound to a world session later
(`bindSession`), so each member that needs the session forwards to it when bound and drops otherwise. The exceptions act
unbound too: the roster's bindings (§7), the mismatch report (§8), the log and the delivery note. The server host is made by the
world session with a pointer to it. A null pointer is allowed, for a test or a detached world: the stamps then drop, and
`receptionTick` `checkf`s, because og-sim-net only asks for it while draining a received input, which a server with no session
never does.

**Neither host is `final`.** og-sim-net detects a `noteDelivered` member by deriving a probe class from the host
(`SimNetHost.h`), and its sessions `static_assert` the result, so a `final` host would not compile.

## §2 The server's round trip

`rttMs(conn)` looks the og-net id up in the driver (`UOGNetDriver::FindConnection`) and reads that connection's engine ping
with `readRoundTripMs` (§11.5). The value is the engine's raw
`RoundTrip`, in milliseconds, with no smoothing on top: the tier table's own smoothing is the only one.

- **The wire sample.** When a session is bound, each reading is also added to the latency probe's wire hop
  (`noteWireSample(rtt / 1000)`), as today's input bundle receipt does once per bundle (`UESimulatableOwnerBinding.h`). og-sim-net
  calls `rttMs` once per accepted input bundle (`SimNetServer::receiveBundle`), so the rate is the same.
- **No connection.** An og-net id with no Unreal connection (it closed between the receipt and the drain, or no driver) is a
  reading that does not exist. The host logs a one-shot `[RttSample] SERVER tier RTT source unavailable (…)` warning to
  `LogOGRtt` and returns `kNoRttReadingMs` (−1), which the reception coordinator skips: no tier is derived or published
  (`UESimNetHost-guards.md` G-01). `readRoundTripMs` does not warn on a null connection (§11.5), so `rttMs` never passes it one; here
  the og-net id came off the wire, so its absence is worth one line. The
  one-shot flag is the host's own, so each server session warns at most once.
- **Game thread only.** `FindConnection` `checkf`s the game thread, and so does the probe. Ph3's server drains on the game
  thread. Ph4's authority loop drains on its runner thread and needs another source there.

## §3 The reception tick

`receptionTick` is the session's `getServerReceptionTick`: the server clock's current tick, the tick the coordinator files an
RTT sample under.

## §4 Latency probe stamps

The lane of every stamp is the sim id. In the brawler the probe's lane is the character's storage key, and the storage key is
the sim id (`toStorageKey` is the identity cast; the brawler's concept test asserts it).

| call | stream | stamps |
|---|---|---|
| `stampServerReceived(simId, tick, recv)` | Input | `ServerReceived` at now; `NetReceived` at `recv` |
| `stampHanded(simId, tick)` | Input | `Handed` at now (each relayed entry) |
| `stampClientReceived(Input, simId, tick, recv)` | Input | `ClientReceived` at now; `NetReceived` at `recv` |
| `stampClientReceived(State, simId, tick, recv)` | State | `stampCorrectionReceived` (`ClientReceived` at now, and the heal stamp); `NetReceived` at `recv` |

"Now" is the drain, which is where today's receipt stamps are taken (the replication callback). `recv` is og-net's receive time
of the datagram, on the same clock (`FPlatformTime::Seconds`), taken on og-net's thread; `stampAt` is the diagnostics' door for
a stamp at a time the caller measured. The hops that read `NetReceived` are masked off until the switch ORs them in.

## §5 The client's round trip

`serverRttSeconds` reads the driver's `ServerConnection` engine ping, `RoundTrip`, in seconds. Its body is the one the timing
relay's `.cpp` held until og-input-latency task P5-9 deleted the relay, with the same one-shot `[RttSample] CLIENT …` warning text and the
same causes; "no UWorld" is gone because the host reads its driver directly.

- **The sentinel.** Every failure returns `kNoRttReadingSeconds` (−1), which the network time estimator rejects and counts
  (`UESimNetHost-guards.md` G-02).
- **Its own one-shot flag,** a member: each client host (one per driver) warns at most once.
- **Unbound, it reads nothing.** It is only called right before `onTimingInfo`, which drops when no session is bound. The
  relay returned before its read when no listener existed, so a warm-up "accumulator is empty" read in the join window would
  spend the one-shot on a reading nobody uses. Unbound, it returns the sentinel without reading or warning.

## §6 The session's consumers

- `onTimingInfo(tick, rtt)` → `onTimingInfoReceived(tick, rtt)`: the wire sample and the estimator's RTT and authority tick.
- `onRelayFloor(v, replayed)` → `onRelayDelayFloorReceived(v)`, replayed or not (`UESimNetHost-guards.md` G-05): the floor
  pays for an increase.
- `onConnectionTier(old, new, replayed)` → `onConnectionTierReceived(old, new)`, replayed or not.

All three drop when unbound. The floor and tier are not lost: og-sim-net's client latches them and replays them at bind.

**Why a replay is applied as received (og-input-latency task P5-7).** Under Unreal's replication a value replayed only when its
property's `OnRep` beat the session's bind, a race; in the common case the timing relay arrived after the client's world had
bound, and the floor was received, paying for the increase with a prediction stall (a Ph2 client log:
`[RelayDelayFloor] floor = 6 ticks, UPWARD, requesting 2-tick prediction stall`, then the relay's replay with delta 0). og-net
delivers the floor on the server's attach, before the client's world exists, so it is always latched and always replayed. Applied
as a replay it would never stall, which changes the join's timing and drops Ph3's identical row for that line; applied as
received it is today's common case. The tier follows the same rule: its first application never stalls either way
(`hadAnyTier` is false), and as received it keeps today's `[ConnectionTier] tier a -> b hadAnyTier=0, no stall` line. The
session's two replay members had no caller since, and og-input-latency task P5-9 deleted them.

## §7 The roster and the bindings

- `onRosterAdded(simId)` looks the entry up in the roster mirror (`RosterMirror::entryFor`; the mirror reports an Add after it
  applied it) and hands it to `SimNetBindings::onRosterAdded`. It acts unbound: the bindings belong to the lanes, not to a
  world.
- `onRosterRemoved(simId)` tells the bindings first. They return true exactly once if the sim id was registered, and then the
  host calls the world session's unregister handler (`setOnRegisteredRemoved`), at once (`UESimNetHost-guards.md` G-03). A
  registered id with no handler set is a `checkf`; with checks off the unregister is skipped rather than called through an
  empty function.

## §8 The wire-format mismatch

`onWireMismatch(received, expected)` logs today's Error line to `LogOGNet` and shows today's 15 s red toast with the same key
(`kSimNetWireMismatchToastKey`, `0x4F47574DF0000001`), the texts copied from `SimulatableNetSyncComponent.cpp`, which
og-input-latency task P5-8 deleted. The line still says "on OnRep_CorrectionState", so that a reader matching it finds the same text; under og-net there is
no OnRep. It acts unbound (`UESimNetHost-guards.md` G-04).

## §9 Log and delivery

- `log(line)` is `ueSimNetHost::routeLine`, which calls `RouteOGMessage`: the line's tag picks its category, and a leading
  `[Warning]` or `[Verbose]` its level. og-simulation's route table has the `[SimNet.` and `[Roster.` rows, which send
  og-sim-net's lines to `LogOGNet` at plain level Log (`SimulationLogRoutes-rationale.md` §5).
  - **A token-less `[Roster.` line is routed at `Warning`** (og-input-latency task P5-7, from P5-4's finding F1). The brawler's
    `Config/DefaultEngine.ini` sets `LogOGNet=Warning` in `[Core.Log]`, so a Log-level line on `LogOGNet` is filtered at run
    time. The `[SimNet.Window]`, `[SimNet.Disconnect]` and server `[Roster.Refused]` lines carry `[Warning]` themselves; the
    client's `[Roster.Refused]`, the server's `[Roster.InputRefused]` and the session's `[Roster.BindMismatch]` carry none, and a
    refusal or a mismatch is exactly what an operator must see. So `routeLine` prepends `[Warning]` to a line that starts with
    `[Roster.`, into a buffer the size of og-sim-net's roster line plus the token. The printed text carries the token, as every
    other `[Warning]` line does; the tools strip it. Other token-less lines (`[ReceiveCorrectionState]`, `[RelayFlush]`) keep
    their row's level.
- `noteDelivered(lane, recv)` is the endpoint's `noteDelivered(lane, recv, now)`, which writes the driver's `[OGNet.Window]`
  sim-lane rows. og-sim-net calls it once per drained message, before decoding it. A detached endpoint ignores it.

## §10 Tests

The game drives the hosts since the switch (og-input-latency task P5-7), through its lanes factory and its session. Before it
nothing drove them but the brawler's `UESimNetHostsConceptTest.cpp`, which still runs:
- `static_assert`s that `UESimNetServerHost<BrawlerSessionTraitsUImpl>` is a `SimNetServerHost`, the client host a
  `SimNetClientHost`, both have the delivery hook, the probe lane is the sim id, and the route table sends each og-sim-net tag to
  the Net route at Log;
- the automation test `OGBrawler.SimNet.Hosts`: `rttMs` of an unknown og-net id returns the sentinel twice and warns once; four
  routed lines (three `[Warning]` lines and a token-less `[Roster.` line) reach `LogOGNet`, the token-less one with the
  `[Warning]` token `routeLine` adds (it was checked at Log until P5-7), with `LogOGNet` raised to Log for the probe and restored
  after; a detached server flushes a staged clock to nothing, counted. The
  test does not check the `[Warning]` lines' level: they are expected messages, which the automation framework logs at
  Verbose.

## §11 The engine primitives (og-input-latency task P5-9)

`LogOGRtt`, the two no-reading sentinels, `GetRootNetConnection`, `GetPlayerSlotForActor` and `readRoundTripMs` are free
functions and constants at file scope in `UESimNetHost.h`. Until og-input-latency task P5-9 they were in
`UEConnectionHandle.h`, with the UE connection handle `FUEConnectionHandle`, which nothing used after P5-7 and which was deleted
with that header. They moved with their texts; each block below is the text as it stood, then what R0 found. The
prohibitions among them are `UESimNetHost-guards.md` G-06 to G-10.

### §11.1 `LogOGRtt`

As it stood:

> LogOGRtt — the RTT-source diagnostic channel, shared by the two read sites (readRoundTripMs below, feeding the TIER;
> ASimulationTimingRelay::OnRep_Buffer, feeding the client prediction OFFSET). Declared here because this header is where the
> RTT engine primitive lives, and because a category owned by OGSimulationUnreal keeps the log off the OGBrawlerUnreal LogOG*
> family — this module must not depend on the game module.
>
> Default verbosity Log, so the one-shot Warnings below are visible with NO DefaultEngine.ini entry. That is deliberate: the
> failure these report is exactly the kind that must not need a config opt-in to be seen.

*R0:* the read sites are now three, all in this header: `readRoundTripMs` (§11.5), the server host's no-connection warning
(§2) and the client host's `warnRttSourceUnavailableOnce` (§5). The timing relay is deleted. `Config/DefaultEngine.ini` has no
`LogOGRtt` entry. The category is defined in `UESimNetHost.cpp`, which exists for that; the old `.cpp` said the same, and that
it was the header-only header's compile anchor, which `UESimNetHost.cpp` now also is.

### §11.2 The no-reading sentinels

As it stood:

> The "no reading" sentinel, in each site's own unit. NEGATIVE by design and shared by both RTT read sites, because both
> consumers already reject negatives: ServerReceptionCoordinator::noteRttSample skips `rttMs < 0.0` (publishing no tier), and
> NetworkTimeEstimator::updateRTT rejects and counts it (latching nothing). Zero must never be used — it is indistinguishable
> from a genuine 0 ms LAN ping, which is what made the pre-T21 silent-zero defect invisible.

*R0:* both consumer claims verified (`ServerReceptionCoordinator.h`'s `noteRttSample`, `NetworkTimeEstimator.cpp`'s
`updateRTT`). "Negative, never zero" is now a `static_assert` next to the constants, whose message names this text. The two
returns that hand a sentinel on are guards G-01 and G-02.

### §11.3 `GetRootNetConnection`

As it stood:

> Walks from `actor` to the root UNetConnection driving it, resolving any UChildConnection to its parent. Returns nullptr when
> the actor has no wire identity (standalone, local PIE host, or a null actor). The loop is bounded by the connection chain,
> which UE builds at most one level deep; the `parent == nullptr` guard additionally makes a malformed chain terminate rather
> than spin.
>
> WHY THE *ROOT* CONNECTION: under split-screen / multi-ULocalPlayer clients, each ULocalPlayer gets its own
> APlayerController backed by a UChildConnection of the one real client connection. Tier, input delay, and substitution state
> are properties of the WIRE, not of a viewport, so all siblings must resolve to a single table entry. GetRootNetConnection
> walks UChildConnection::Parent to guarantee that.

*R0:* the one caller is the world session's `addSimNetCharacter`, which casts the root to `UOGNetConnection` and takes its
og-net id as the roster entry's owner. A child connection is not an og-net connection, so without the walk a second local
player's pawn would have no owner. The "single table entry" is now og-sim-net's: the reception coordinator keys a connection
by that og-net id and the local index (§11.4).

### §11.4 `GetPlayerSlotForActor`

As it stood:

> GetPlayerSlotForActor — which LOCAL PLAYER on the wire owns `actor`. Returns 0 for the primary player (the parent
> UNetConnection's), and 1..N for each additional local player on that same machine. Returns 0 for an actor with no wire
> identity, matching GetRootNetConnection's sentinel — a standalone or listen-server-local pawn is the only player on its
> (absent) wire.
>
> DERIVATION: `UNetConnection::GetConnectionHandle().GetChildConnectionId()`. This is the ENGINE'S OWN child-connection
> identity, and it already has exactly the numbering this function needs: FConnectionHandle documents "a ChildConnectionId of
> zero indicates it's the parent connection itself", parent connections are constructed as
> `FConnectionHandle(AllocateConnectionId())` (child id 0), and UChildConnection::AssignConnectionHandle assigns children the
> lowest unused id starting at 1 (0 is reserved as InvalidChildId).
>
> STABILITY CONTRACT (why this matters here): the input delay queue keys parked input on (root connection, slot). If a slot
> changed under a live character its parked input would be orphaned in the old bucket and silently dropped. The assign-once
> child id is what makes that impossible.

*R0:* the derivation is verified in UE 5.6 (`Net/Core/Connection/ConnectionHandle.h`; `UNetConnection::InitBase`;
`UChildConnection::AssignConnectionHandle`). The value is now the roster entry's `localIndex`; og-sim-net's server hands each
received bundle to the reception coordinator with the entry's index (`SimNetServer.h`, `receiveBundle`), so the coordinator's
key is (the connection, this index), as the stability contract says. The prohibitions are guards G-06 (the own connection),
G-07 (the child id) and G-08 (the saturation).

### §11.5 `readRoundTripMs`

As it stood:

> readRoundTripMs — a connection's RAW round-trip RTT in MILLISECONDS. Returns kNoRttReadingMs when there is no reading — null
> connection, no FNetPing, the ping type not enabled, or its accumulator still empty. That sentinel is passed straight into
> ServerReceptionCoordinator::noteRttSample, which skips the sample so the adapter publishes no tier.
>
> FPingValues are in SECONDS (NetPing.h); the tier table and TimeConfig::rttTierBoundariesMs are in MILLISECONDS, hence the
> *1000.
>
> UNetConnection::ReadPacketInfo (Engine/Private/NetConnection.cpp) does:
>
>     const double RTT          = (CurrentTime - OutLagTime[Index]);
>     const double RTTExclFrame = RTT - (bExcludeFrameTime ? ServerFrameTime : 0.0);
>     const double NewLag       = FMath::Max(RTTExclFrame, 0.0);
>     NetPing->UpdatePing(EPingType::RoundTrip,          CurrentTime, NewLag);
>     NetPing->UpdatePing(EPingType::RoundTripExclFrame, CurrentTime, FMath::Max(RTT, 0.0));
>
> NewLag IS the frame-time-subtracted value, and it goes to `RoundTrip`. The raw, frame-time-INCLUSIVE RTT goes to
> `RoundTripExclFrame`. So the type whose name promises exclusion is the one that never excludes anything.
>
> AND TODAY THEY ARE THE SAME NUMBER ANYWAY. `bExcludeFrameTime` reads net.PingExcludeFrameTime, which defaults to 0 and is
> set nowhere in this project's config, so the subtraction is a no-op and both types receive max(RTT, 0.0) — identical
> samples. Switching the ping type here would change NOTHING about frame time; it would only swap the averaging configured in
> DefaultEngine.ini (RoundTrip=PlayerStateAvg vs RoundTripExclFrame=MovingAverage), i.e. a pure smoothing change with no
> evidence behind it.
>
> AND ON THIS SITE SPECIFICALLY IT COULD NOT MATTER EVEN IF ENABLED. This read runs SERVER-side. ServerFrameTime is parsed
> under `if (!Driver->IsServer())` and is therefore structurally 0.0 on the server, so the subtracted term is zero here by
> construction regardless of any cvar.
>
> WHAT ACTUALLY INFLATES RTT DURING A LOCAL FRAME HITCH — the effect T21 set out to kill — is neither ping type. CurrentTime is
> FApp::GetCurrentTime() (frame-start), so a local hitch delays ack processing and inflates the measurement, and the only frame
> time either type can ever subtract is the REMOTE's. The lever that excludes local frame time is net.PingUsePacketRecvTime;
> it is NOT enabled here, carries threading prerequisites, and is scoped as its own backlog task rather than folded in.
>
> ENGINE PRIMITIVE: a second engine (Godot) must supply its own RTT source here.

and, on the one-shot warning in its body:

> ONE-SHOT diagnostic. Shares the rationale of the client-side twin in SimulationTimingRelay.cpp: the silent-zero class of
> failure is defined by being unobservable, so it has to announce itself — but this runs on the per-bundle input-RPC receive
> path, once per connection per datagram, so an unthrottled warning would be a firehose. Function-local static in an inline
> function is one object across the module (game-thread only, per the ServerReceptionCoordinator threading contract).

and on the null connection:

> Routine and self-resolving (an actor with no wire identity, or a connection not yet established) — the documented sentinel
> path, not a fault. Deliberately NOT warned, so the one-shot budget above is spent on the failures that never resolve on
> their own.

and on the empty accumulator: `GetPingValues` documents `Current = -1.0` for "not set"; before T21 that fell through and
returned −1000, negative and so skipped downstream by accident, and the check makes it explicit.

*R0, og-input-latency task P5-9:*
- The `ReadPacketInfo` lines, the `net.PingExcludeFrameTime` default (0), the server-side `ServerFrameTime` parse and the
  `Config/DefaultEngine.ini` averaging (`net.NetPingTypes`) are verified in UE 5.6 and the tree. Under og-net the same code runs:
  `UOGNetDriver` hands each datagram to `UNetConnection::ReceivedRawPacket`.
- **Corrected:** `CurrentTime` is the packet's OS receive time when `net.PingUsePacketRecvTime` is set and the socket gave one,
  and `FApp::GetCurrentTime()` otherwise; the project does not set the cvar, so the moved claim holds here.
- **Corrected:** the caller is the server host's `rttMs`, which og-sim-net calls once per accepted input bundle (§2), not an
  input RPC. The warning's flag is a function-local static of an inline function, so there is one per binary that calls it
  (in the brawler, the game module, which instantiates the host), not one per module that declares it.
- **Corrected:** the null branch is no longer reached: `rttMs` returns its own sentinel, with its own one-shot warning, before
  it calls this function (§2). The branch is kept for a caller that passes a null connection.
- The prohibitions are guards G-09 (the ping type) and G-10 (nothing on top of the reading). The client host's read
  (`readServerRoundTrip`, §5) reads the same type for the same reason, untagged.
