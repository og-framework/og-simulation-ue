<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: SimNetLinkServerHost -- the class name of og-session-runtime task 31a's step (a), renamed SimNetServerHostCore in step (b); named in this file's history -->
<!-- lint-external-ref: UESimNetServerHost -- Unreal's base server host (the game-thread engine ping and its warnings), deleted by og-session-runtime task 39 with the authority's game-thread RTT copy; named in this file's history -->
<!-- lint-external-ref: FRunnerRttTable -- Unreal's runner copy of the driver's round trips, deleted by og-session-runtime task 39 (the core reads og-net's stats, §6); named in this file's history -->
<!-- lint-external-ref: publishRunnerRtt_GameThread -- Unreal's per-frame game-thread publish into the runner's copy, deleted by og-session-runtime task 39; named in this file's history -->
# `SimNetServerHost.h` — rationale

`ogsimnet::SimNetServerHostCore<ServicesT, SessionT, EndpointT>` is the engine-free half of the authority's og-sim-net host: the
`HostT` a `SimNetServer` runs on (`SimNetHost.h`'s `SimNetServerHost`, with both optional hooks, the delivery note and the
unregistering hook). It routes the server's lines, makes the delivery note, forwards the latency stamps and the unregistering
hook to the world session (and drops them while there is none), reads the round trip from og-net's stats through its endpoint
(§6, og-session-runtime task 39) and asks the session for the reception tick, and checks every call against the thread that built
it. What it needs from an engine is two small ports and an endpoint. This file is the docs pointer of `SimNetServerHost.h`; its prohibitions are in `SimNetServerHost-guards.md`.

It came out of Unreal's `UESimNetHost.h` (og-session-runtime task 31a, the server-side counterpart of task 28's client host): step
(a) moved `UESimNetServerHost`'s `log` and `noteDelivered` and `UESimNetLanesServerHost`'s two stamps and `onLifecycleUnregistering`
unchanged into a class deriving from Unreal's host; step (b) cut that derivation, gave the class its three parameters and its
thread check, and left Unreal the engine half (`UESimNetHost-rationale.md` §13).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 The name, and the file

- **Not `SimNetServerHost`:** that is og-sim-net's concept for `SimNetServer`'s `HostT` (`SimNetHost.h`), which this class models;
  a class and a concept cannot share a name in one namespace.
- **Not `SimNetLinkServerHost`** (its name at task 31a's step (a)): the link (`SimNetLink.h`) holds a host only on a client. The
  server's host is its world session's, built where the session builds its `SimNetServer` and destroyed after it (Unreal: the
  session's `m_simNetServerHost`; og-sim-session's runtime from og-session-runtime task 31). A `Link` prefix would say the link owns
  it.
- **Not a `Session…` name:** og-sim-session's classes are `Session…`; a server host named after the session would read as one of
  them.
- **`Core`:** the part of the host that is the same on every engine. An engine wraps it with its ports (Unreal:
  `UESimNetLanesServerHost`, `UESimNetHost.h`).
- **Its own header, not `SimNetLink.h`:** it uses none of the link's members, and the runtime that owns the server's host needs it
  without the link's client half. It includes `SimNetLink.h` for two free functions the client host shares, `routeSimNetHostLine` and
  `noteSimLaneDelivered`; the link does not include this file.

## §2 The parameters (the ports)

The design's question (og-session-runtime task 28's review, item 4): its own concept, or a widened `SimNetLinkServices`. Its own,
split in two the way task 28 split the client's (`SimNetLink-rationale.md` §9), because the two halves have different owners
and lifetimes:

**`SimNetServerHostServices`**, what outlives a world session (each requirement has a negative in `simNetServerHostSelfCheck`):
- `nowSeconds()` — the host's clock, and the delivery note's now (G-02); Unreal: `FPlatformTime::Seconds`;
- `logHost(line)` — og-sim-net's server lines, printed whole with any leading token (G-01); Unreal: `RouteOGMessage`, the sink the
  link's services also call `logHost`.

Until og-session-runtime task 39 the services also had `rttMs(conn)`, the round trip in milliseconds; Unreal answered it from
`FRunnerRttTable`, a copy the game thread filled each frame (`publishRunnerRtt_GameThread`). The core reads it from the endpoint
since (§6).

**`SimNetServerHostSession`**, the world session's hooks:
- `receptionTick()` — the tick the coordinator files an RTT sample under, `int32_t`; Unreal: the session's
  `getServerReceptionTick`;
- `stampServerReceived(simId, captureTick, recvSeconds)`, `stampHanded(simId, captureTick)` — the latency probe's two server
  stamps (`UESimNetHost-rationale.md` §4); Unreal: the diagnostics' mailbox doors;
- `onLifecycleUnregistering(simId)` — the session's notify before og-sim-net's facade unregisters a queue-registered character
  (og-sim-session's `SessionLifecycle-guards.md` G-08); Unreal: the session's `onSimNetLifecycleUnregistering`;
- `noteWireSample(roundTripSeconds)` — the latency probe's server `WIRE` sample, once per round trip read with a reading (§6;
  og-session-runtime task 39); Unreal: the diagnostics' `noteServerWireSample_Internal`, a mailbox door on the runner.

**`SimNetServerHostEndpoint`**: `noteDelivered(lane, recvSeconds, nowSeconds)`, og-net's endpoint call that writes the
`[OGNet.Window]` sim-lane rows, and `stats(id)`, og-net's per-connection stats (`ognet::ConnectionStats`), whose smoothed round
trip is the core's `rttMs` (§6). Only those two: the host does not send (the transport does). og-net's own `NetEndpoint` has
both spellings; Unreal's `ognetunreal::SimLaneEndpoint` forwards `stats` to the engine-lane driver core
(`SimLaneEndpoint-rationale.md` §2).

The core holds the services and the endpoint by reference and the session by pointer. One object may model both ports: Unreal's
`UESimNetServerHostPort` does, and its host passes the same object as both.

**The self-check** (`simNetServerHostSelfCheck`) holds a reference shape and one negative per requirement for each concept
(services 2, session 5, endpoint 2), three type negatives (`nowSeconds` returning `float`, `receptionTick` returning `uint32_t`,
the endpoint's `stats` returning a bare `double`), and `static_assert`s that the core over the reference shapes is a
`SimNetServerHost` with both optional hooks. Each session negative declares every other session requirement, so it fails for its
own reason only; the endpoint's two-argument `noteDelivered` negative declares `stats` for the same reason (og-session-runtime
task 39). Each
negative names what its requirement is for. Task 31a's mutants (each one edit in an overlay copy, compiled alone by the clang
header probe): every requirement deleted (8), the three non-`void` returns weakened, and the core's two optional doors deleted
are killed; the five `void` returns weakened to "any return" survived and are equivalent in behaviour, because the core calls
those members only as statements and discards what they return. og-session-runtime task 33b pinned them anyway, one negative
each returning `bool`, as it did `SimNetLink`'s: a port written to report through a return the core never reads is refused
at compile time; its mutants are all killed, 19 of 19; the two stamp negatives replace Unreal's old `static_assert(!SimNetServerHost<UESimNetServerHost>)`
("the base host keeps no stamps", og-session-runtime task 14), which since task 31a's step (a) held for more than one reason
and isolated none (review_SR5_31a, L3). og-session-runtime task 39 removed the services' `rttMs` requirement with its two
negatives (missing; returning `float`), and added `noteWireSample` (missing; returning `bool`) and the endpoint's `stats` (missing;
a bare `double`); deleting either new requirement from its concept is killed at compile time by its negative's message.

## §3 The doors, the session, and the thread

| door | to | with no session |
|---|---|---|
| `nowSeconds()` | services | — |
| `rttMs(conn)` | endpoint's `stats(conn)` (§6), then the session's `noteWireSample` with a reading | the reading; no wire sample |
| `receptionTick()` | session | `OG_CHECK`s, then 0 |
| `log(line)` | `routeSimNetHostLine` → services' `logHost` (G-01) | — |
| `noteDelivered(lane, recv)` | endpoint, with the services' now (G-02) | — |
| `stampServerReceived`, `stampHanded` | session | dropped |
| `onLifecycleUnregistering(simId)` | session | dropped |

- **No session** is a test's case: Unreal's hosts concept test builds a host with none. og-sim-net asks for the reception tick
  only while it drains a received input, which a server with no session never does, so the check states the contract; the
  stamps and the hook have nothing to reach. This was Unreal's host's behaviour, a `checkf` and two `if`s, before task 31a.
- **The serving thread.** The constructor records `std::this_thread::get_id()`, and every door's first statement checks the caller
  against it (`OG_CHECK`). The thread that builds the host is the thread that drives its `SimNetServer`: Unreal builds both in the
  session's `beginServerSimNet`, inside the configure step the authority runner runs before its loop starts, and the server drains,
  batches and applies its lifecycle on that runner. og-sim-net has no session thread table (og-sim-session's, tier 3, which og-sim-
  net may not include), so the host checks its one expectation against the id it recorded, as the link does
  (`SimNetLink-rationale.md` §4). Until og-session-runtime task 39 the game thread also read the round trip, in Unreal's port (its
  per-frame publish and its join-time read, §6); every read is the core's since, on the serving thread.
- **The order inside one drained message** is og-sim-net's (`SimNetServer.h`): the delivery note, then one ServerReceived stamp per
  slot, then the reception tick and the round trip (the two are arguments of one call, so their order is the compiler's; the
  round trip's wire sample goes to the session inside `rttMs`), then the relay's Handed stamps.

## §4 Unreal's side

`UESimNetHost.h` keeps the engine half (`UESimNetHost-rationale.md` §1, §13): `UESimNetServerHostPort` (both ports: the clock,
the log sink and the session hooks, each one line to Unreal) and `UESimNetLanesServerHost`, the core over that port and the lanes'
`SimLaneEndpoint`, held in a private base so the port is built before the core takes its address. Since task 31 the runtime names
it as its link's (`UESimNetLanes::ServerHost` on Unreal); since og-session-runtime task 39 it builds it from the endpoint and itself
(`SessionRuntime.h`, `attachServerLink`), and until then from the driver too. Until task 39 the port also derived from
`UESimNetServerHost` (the game-thread engine ping and its warnings) and held the runner's copy of the round trips (§6).

## §5 Tests (`og-sim-net/tests/SimNetServerHostTest.cpp`, `[NetTransport][SimNetServerHost]`)

A rig over og-net's `VirtualNetwork`: an og-net server endpoint with og-sim-net's `SimNetServer` over the core, and a raw og-net
client endpoint that sends input bundle sets on the SimInput lane. The core's services, session and endpoint append to one event
list, so the order across the three is observable.
- **Delivery notes and stamps reach the host in order.** Six messages of three slots: the events, but the Handed stamps, are
  exactly `delivered`, three `received` in capture order, and the two queries, per message; every Handed stamp follows the
  ServerReceived stamp of its capture tick and its message's delivery note. Non-vacuity, measured: 6 notes, 18 ServerReceived
  stamps, 18 Handed stamps. The services' clock runs 0.25 s ahead of the network's, so a note made with the receive time as now
  fails (G-02).
- **The lifecycle-unregistering hook runs once per removal.** Two queue-registered characters (one unregistered through the queue,
  one removed directly) are reported once each; a character added without the queue is not reported.
- **With no session the hooks are dropped and the lines still route**: the stamps and the hook reach nothing, the delivery note
  still goes to the endpoint, a token-less `[Roster.` line gets `[Warning]` and another line passes as is (G-01).
- **The round trip is the endpoint's smoothed RTT, and each reading is a wire sample** (og-session-runtime task 39): once og-net's
  server endpoint has a sample for the client's connection, `rttMs` equals its `smoothedRttSeconds` × 1000 and the session gets that
  one wire sample in seconds; an id the endpoint has no stats for reads the sentinel and gives no wire sample; a host built with
  no session reads the same value and drops the sample.
- **A door off the serving thread trips the check** (hidden, `[.][SimNetServerHostDeath]`, run by name): the host is built on a
  `std::thread` and `stampHanded` is called on the test's thread. In Unreal's test runner the `OG_CHECK` is reported as a failed
  assertion; under `OG_STANDALONE_BUILD` it is an `assert`. A run that reaches the case's `FAIL` line means the check did not fire.

**The rig keeps its `TimeConfig` as a member.** `SimNetServer` and its reception coordinator hold the configuration by reference;
a rig that passes a local or a temporary leaves them reading a dead object (this rig's first version refused every capture tick
but the server's own as out of domain, because both window bounds read 0).

## §6 The round trip (og-session-runtime task 39)

`rttMs(conn)` asks the endpoint for og-net's stats of the connection, `stats(conn)`. With a sample (`hasRttSample`) it returns
og-net's smoothed round trip, `smoothedRttSeconds` × 1000, in milliseconds and as a `double` (G-04: nothing on top); without one
it returns `kSimNetServerNoRttMs` (−1), which the reception coordinator skips, so no tier is derived or published (G-03). With a
session, each reading also goes to the session's `noteWireSample` in seconds, the probe's server `WIRE` hop (half of it). A read
without a sample gives no wire sample: the probe would count it as `outOfOrder`, as it counted the −1 ms samples Unreal's engine
read took on a closing connection until og-input-latency task P5-26.

- **When it is −1.** Before og-net's first RTT sample for the connection, and in the first stats publish period (50 ms) after it;
  and for an id the endpoint has not published (one that has closed and gone, or one it never had): og-net returns a value with
  `state = Disconnected`, id 0 and no sample. There is no warning: og-net has no cause to report beyond "no sample yet". The first
  reading publishes its tier, tier 0 included
  (og-input-latency P6-2, `SimNetServer-rationale.md` §8).
- **Readable on the serving thread: og-net's rule.** `EndpointCore-rationale.md` §10: "Every 50 ms (`kStatsPublishSeconds`) the
  servicing thread copies each connection's `ConnectionStats` (P1-8 §10) and one `EndpointStats` into a staging array, then swaps
  it with the published one under a small mutex. `stats(id)` and `endpointStats()` read the published copy from any thread; they
  are at most 50 ms old (plus however long the servicing thread is busy)." On a dedicated server the thread that drives the server
  (the authority runner) is also og-net's servicing thread (mode B, `SimLaneEndpoint-rationale.md` §2), so its reading is at most
  one publish old.
- **On Unreal** the lanes' `SimLaneEndpoint` forwards `stats` to the engine-lane driver core's `connectionStats`, og-net's
  `NetEndpoint::stats`. The core's endpoint exists from before the sim lanes' `beforeStart` until the core's destruction (or a
  failed start), and the runner is joined in the lanes' `beforeStop`, so the runner never reads a destroyed endpoint
  (`SimLaneEndpoint-rationale.md` §2). The driver's own `UOGNetDriver::GetSmoothedRttSeconds` is game-thread-only, because the
  driver's pimpl is created and destroyed on the game thread; the runner does not use it. The client keeps its own sources (its
  engine-ping shadow figure, `UESimNetHost-rationale.md` §5, and its og-net smoothed RTT, read on the game thread).
- **Smoothing.** og-net's `srtt` is `7/8 srtt + 1/8 sample`, seeded by the first sample (`Connection.cpp`); the tier table smooths
  its input again (`rttSmoothingAlpha`). The source before task 39, Unreal's `FNetPing` `RoundTrip` (averaged `PlayerStateAvg`,
  `Config/DefaultEngine.ini`), was an average too.
- **What it replaced (Unreal, until task 39).** The game thread read each og-net connection's engine ping (through a lookup that
  remembered closed ids, with one warning per cause), and each frame published it into `FRunnerRttTable` (one lock-free word per
  connection, `publishRunnerRtt_GameThread`, driven by the lanes' `afterDispatch`); the runner read that copy. The game-thread read
  also took the wire sample, once per frame per joined connection; the core takes it once per accepted input bundle
  (`SimNetServer::receiveBundle` asks `rttMs` once per bundle). Unreal's side of the history is `UESimNetHost-rationale.md` §2.
- **The deviation, measured** (task 39's shaped b0 sessions on the Editor binary, at `RelayDelayFloorTicks` 0, 3 and 6, before and
  after): og-net's own smoothed RTT on the server (`[OGNet.Window]`, the same on both sources) was 66.6–69.2 ms; the server's `WIRE`
  hop (half the source's round trip) was 44.5–45.1 ms on the engine ping and 33.8–33.9 ms on og-net's, so the engine ping read
  about 89–90 ms. The clients went to tier 2 (`0 → 1 → 2`) on the engine ping and stay on tier 1 (`0 → 1`) on og-net's; at floor 0
  the input delay is 2 ticks instead of 3, at floor 3 and 6 it is the floor either way. Gate 40 (G-SR6d) measures it against the
  SR6 package.
