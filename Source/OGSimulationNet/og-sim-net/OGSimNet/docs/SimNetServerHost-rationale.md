<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: SimNetLinkServerHost -- the class name of og-session-runtime task 31a's step (a), renamed SimNetServerHostCore in step (b); named in this file's history -->
# `SimNetServerHost.h` — rationale

`ogsimnet::SimNetServerHostCore<ServicesT, SessionT, EndpointT>` is the engine-free half of the authority's og-sim-net host: the
`HostT` a `SimNetServer` runs on (`SimNetHost.h`'s `SimNetServerHost`, with both optional hooks, the delivery note and the
unregistering hook). It routes the server's lines, makes the delivery note, forwards the latency stamps and the unregistering
hook to the world session (and drops them while there is none), asks the engine for the round trip and the session for the
reception tick, and checks every call against the thread that built it. What it needs from an engine is two small ports and an
endpoint. This file is the docs pointer of `SimNetServerHost.h`; its prohibitions are in `SimNetServerHost-guards.md`.

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
  link's services also call `logHost`;
- `rttMs(conn)` — a connection's round trip in milliseconds, `double` (the tier table smooths a `double`; a negative value is
  "no reading"); Unreal: the runner's copy in `FRunnerRttTable`, which the game thread publishes (`UESimNetHost-rationale.md` §13).

**`SimNetServerHostSession`**, the world session's hooks:
- `receptionTick()` — the tick the coordinator files an RTT sample under, `int32_t`; Unreal: the session's
  `getServerReceptionTick`;
- `stampServerReceived(simId, captureTick, recvSeconds)`, `stampHanded(simId, captureTick)` — the latency probe's two server
  stamps (`UESimNetHost-rationale.md` §4); Unreal: the diagnostics' mailbox doors;
- `onLifecycleUnregistering(simId)` — the session's notify before og-sim-net's facade unregisters a queue-registered character
  (og-sim-session's `SessionLifecycle-guards.md` G-08); Unreal: the session's `onSimNetLifecycleUnregistering`.

**`SimNetServerHostEndpoint`**: `noteDelivered(lane, recvSeconds, nowSeconds)`, og-net's endpoint call that writes the
`[OGNet.Window]` sim-lane rows. Only that member: the host does not send (the transport does).

The core holds the services and the endpoint by reference and the session by pointer. One object may model both ports: Unreal's
`UESimNetServerHostPort` does, and its host passes the same object as both.

**The self-check** (`simNetServerHostSelfCheck`) holds a reference shape and one negative per requirement for each concept
(services 3, session 4, endpoint 1), three type negatives (`nowSeconds` or `rttMs` returning `float`, `receptionTick` returning
`uint32_t`), and `static_assert`s that the core over the reference shapes is a `SimNetServerHost` with both optional hooks. Each
negative names what its requirement is for. Task 31a's mutants (each one edit in an overlay copy, compiled alone by the clang
header probe): every requirement deleted (8), the three non-`void` returns weakened, and the core's two optional doors deleted
are killed; the five `void` returns weakened to "any return" survive and are equivalent, because the core calls those members
only as statements and discards what they return; the two stamp negatives replace Unreal's old `static_assert(!SimNetServerHost<UESimNetServerHost>)`
("the base host keeps no stamps", og-session-runtime task 14), which since task 31a's step (a) held for more than one reason
and isolated none (review_SR5_31a, L3).

## §3 The doors, the session, and the thread

| door | to | with no session |
|---|---|---|
| `nowSeconds()` | services | — |
| `rttMs(conn)` | services | — |
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
  (`SimNetLink-rationale.md` §4). The game thread's two RTT calls (Unreal's publish and its join-time read) go to Unreal's port,
  never through the core.
- **The order inside one drained message** is og-sim-net's (`SimNetServer.h`): the delivery note, then one ServerReceived stamp per
  slot, then the reception tick and the round trip (the two are arguments of one call, so their order is the compiler's), then
  the relay's Handed stamps.

## §4 Unreal's side

`UESimNetHost.h` keeps the engine half (`UESimNetHost-rationale.md` §1, §13): `UESimNetServerHost` (the game-thread round trip
and its warnings), `UESimNetServerHostPort` (both ports, plus the game-thread publish into `FRunnerRttTable`), and
`UESimNetLanesServerHost`, the core over that port, held in a private base so the port is built before the core takes its
address. The session still named its host as its lanes' `ServerHost` and built it from the driver, the endpoint and itself, as
before; it did not change in task 31a. Since task 31 the runtime names it as its link's (`UESimNetLanes::ServerHost` on
Unreal) and builds it the same way (`SessionRuntime.h`, `attachServerLink`).

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
- **A door off the serving thread trips the check** (hidden, `[.][SimNetServerHostDeath]`, run by name): the host is built on a
  `std::thread` and `stampHanded` is called on the test's thread. In Unreal's test runner the `OG_CHECK` is reported as a failed
  assertion; under `OG_STANDALONE_BUILD` it is an `assert`. A run that reaches the case's `FAIL` line means the check did not fire.

**The rig keeps its `TimeConfig` as a member.** `SimNetServer` and its reception coordinator hold the configuration by reference;
a rig that passes a local or a temporary leaves them reading a dead object (this rig's first version refused every capture tick
but the server's own as out of domain, because both window bounds read 0).
