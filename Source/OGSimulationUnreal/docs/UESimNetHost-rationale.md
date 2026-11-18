<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESimNetHost.h` — rationale

`UESimNetServerHost<TraitsT>` and `UESimNetClientHost<TraitsT>` are the Unreal hosts of og-sim-net's two sessions: the services
`SimNetServer` and `SimNetClient` take as their `HostT` (og-sim-net's `SimNetServerHost` and `SimNetClientHost` concepts, plus
the optional delivery hook). Each one answers from today's Unreal sources: the engine's ping, the latency probe, the log routes,
and the world session's tier, floor and timing consumers. This file is the docs pointer of `UESimNetHost.h`; its prohibitions
are in `UESimNetHost-guards.md`. The lanes object that holds the client host is in `UESimNetLanes.h`.

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
with `readRoundTripMs` (`UEConnectionHandle.h`, included until that header is retired). The value is the engine's raw
`RoundTrip`, in milliseconds, with no smoothing on top: the tier table's own smoothing is the only one.

- **The wire sample.** When a session is bound, each reading is also added to the latency probe's wire hop
  (`noteWireSample(rtt / 1000)`), as today's input bundle receipt does once per bundle (`UESimulatableOwnerBinding.h`). og-sim-net
  calls `rttMs` once per accepted input bundle (`SimNetServer::receiveBundle`), so the rate is the same.
- **No connection.** An og-net id with no Unreal connection (it closed between the receipt and the drain, or no driver) is a
  reading that does not exist. The host logs a one-shot `[RttSample] SERVER tier RTT source unavailable (…)` warning to
  `LogOGRtt` and returns `kNoRttReadingMs` (−1), which the reception coordinator skips: no tier is derived or published
  (`UESimNetHost-guards.md` G-01). `readRoundTripMs` does not warn on a null connection, because for today's callers a null
  connection is routine (a pawn with no wire); here the og-net id came off the wire, so its absence is worth one line. The
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

`serverRttSeconds` reads the driver's `ServerConnection` engine ping, `RoundTrip`, in seconds. Its body is the one in
`SimulationTimingRelay.cpp`, which is retired with the relay, with the same one-shot `[RttSample] CLIENT …` warning text and the
same causes; "no UWorld" is gone because the host reads its driver directly.

- **The sentinel.** Every failure returns `kNoRttReadingSeconds` (−1), which the network time estimator rejects and counts
  (`UESimNetHost-guards.md` G-02).
- **Its own one-shot flag,** a member: each client host (one per driver) warns at most once.
- **Unbound, it reads nothing.** It is only called right before `onTimingInfo`, which drops when no session is bound. Today's
  relay returns before its read when no listener exists, so a warm-up "accumulator is empty" read in the join window would
  spend the one-shot on a reading nobody uses. Unbound, it returns the sentinel without reading or warning.

## §6 The session's consumers

- `onTimingInfo(tick, rtt)` → `onTimingInfoReceived(tick, rtt)`: the wire sample and the estimator's RTT and authority tick.
- `onRelayFloor(v, replayed)` → `onRelayDelayFloorReplayed(v)` for a replay, else `onRelayDelayFloorReceived(v)`: a replayed
  floor is applied without a prediction stall, a received one pays for an increase.
- `onConnectionTier(old, new, replayed)` → `onConnectionTierReplayed(new)` for a replay, else
  `onConnectionTierReceived(old, new)`: the same split.

All three drop when unbound. The floor and tier are not lost: og-sim-net's client latches them and replays them at bind.

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
(`kSimNetWireMismatchToastKey`, `0x4F47574DF0000001`), the texts copied from `SimulatableNetSyncComponent.cpp`, which is retired
later. The line still says "on OnRep_CorrectionState", so that a reader matching it finds the same text; under og-net there is
no OnRep. It acts unbound (`UESimNetHost-guards.md` G-04).

## §9 Log and delivery

- `log(line)` is `RouteOGMessage`: the line's tag picks its category, and a leading `[Warning]` or `[Verbose]` its level.
  og-simulation's route table has the `[SimNet.` and `[Roster.` rows, which send og-sim-net's lines to `LogOGNet` at plain level
  Log (`SimulationLogRoutes-rationale.md` §5).
  - ⚠ **A token-less line is not shown by default in the brawler.** Its `Config/DefaultEngine.ini` sets `LogOGNet=Warning` in
    `[Core.Log]`, so a Log-level line on `LogOGNet` is filtered at run time. The `[SimNet.Window]`, `[SimNet.Disconnect]` and
    server `[Roster.Refused]` lines carry `[Warning]` and are shown; the client's `[Roster.Refused]`, `[Roster.InputRefused]` and
    `[Roster.BindMismatch]` carry no token and are not, unless `LogOGNet` is raised to Log.
- `noteDelivered(lane, recv)` is the endpoint's `noteDelivered(lane, recv, now)`, which writes the driver's `[OGNet.Window]`
  sim-lane rows. og-sim-net calls it once per drained message, before decoding it. A detached endpoint ignores it.

## §10 Tests

Before the switch nothing drives the hosts but the brawler's `UESimNetHostsConceptTest.cpp`:
- `static_assert`s that `UESimNetServerHost<BrawlerSessionTraitsUImpl>` is a `SimNetServerHost`, the client host a
  `SimNetClientHost`, both have the delivery hook, the probe lane is the sim id, and the route table sends each og-sim-net tag to
  the Net route at Log;
- the automation test `OGBrawler.SimNet.Hosts`: `rttMs` of an unknown og-net id returns the sentinel twice and warns once; four
  routed lines (three `[Warning]` lines and a token-less `[Roster.` line) reach `LogOGNet`, the token-less one at Log, with
  `LogOGNet` raised to Log for the probe and restored after; a detached server flushes a staged clock to nothing, counted. The
  test does not check the `[Warning]` lines' level: they are expected messages, which the automation framework logs at
  Verbose.
