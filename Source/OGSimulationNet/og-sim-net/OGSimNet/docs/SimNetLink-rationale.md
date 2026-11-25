<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: UESimNetLanesClientHost -- Unreal's lanes client host, which og-session-runtime task 28 replaced with og-sim-net's SimNetLinkClientHost; named in this file's history -->
<!-- lint-external-ref: FLockedSimNetBindings -- Unreal's locked bindings, which og-session-runtime task 28 replaced with og-sim-net's LockedSimNetBindings; named in this file's history -->
# `SimNetLink.h` — rationale

`ogsimnet::SimNetLink<GameT, EndpointT, ClockT, ServicesT, LockT>` is the connection-lifetime half of a session: everything that
lives as long as one og-net endpoint, engine-free. It holds the sim transport, the SimClock server or client, the roster
bindings behind a lock, and on a client the client session (`SimNetClient`) and its host (`SimNetLinkClientHost`). An engine
creates one when its og-net peer starts, services it each frame and stops it with the peer; a world session attaches to it and
detaches from it. This file is the docs pointer of `SimNetLink.h`; its prohibitions are in `SimNetLink-guards.md`.

It came out of Unreal's `UESimNetLanes` and `UESimNetHost` (og-session-runtime task 28; the session-runtime design's §3.2):
step (a) moved the lanes' link members, the two destruction-order `static_assert`s and the client host's engine-free half
unchanged; step (b) gave the link its five parameters, its doors and its own thread checks. Unreal's `UESimNetLanes` is now the
driver's `IOGNetSimLanes` adapter over a link (`UESimNetLanes-rationale.md`).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 The parameters

| parameter | concept | what it is | Unreal's model |
|---|---|---|---|
| `GameT` | `SimNetGame` | the game's simulatables, for `SimNetClient` | the session traits' `Composition` |
| `EndpointT` | `SimNetLinkEndpoint` | the transport's endpoint (`SimNetEndpoint`) plus `config()` (const, the og-net config the transport is sized from) and `noteDelivered(lane, recv, now)` (the endpoint's `[OGNet.Window]` row) | `ognetunreal::SimLaneEndpoint` |
| `ClockT` | `SimNetClockSource` | the clock the SimClock server or client reads; default-constructed in the link | `FUEPlatformClock` (`FPlatformTime::Seconds`) |
| `ServicesT` | `SimNetLinkServices` | the engine's services that outlive a session (§9) | `UESimNetLinkServices` (`UESimNetHost.h`) |
| `LockT` | `SimNetLinkLock` | a `BasicLockable` (`lock`, `unlock`) the bindings are held behind (§8) | `UEFMutexLockableUImpl` (og-tickdriver K8's lock over `UE::FMutex`) |

og-sim-net may not include og-tickdriver (the repository's module tier table: OGSimNet ↛ OGTickDriver), so `SimNetLinkLock` is the
link's own concept, the same two members as og-tickdriver's `BasicLockable`. The link holds the services by reference (the engine
owns them, declared before the link) and the clock by value. The inbox has `kSimNetLinkInboxSlots` (2,048) slots, the size
Unreal's lanes always used (`kUESimNetInboxSlots` is that constant now).

## §2 What it holds, and the member order

| member | server | client |
|---|---|---|
| `Transport` = `SimNetTransport<EndpointT, kSimNetLinkInboxSlots>` | yes | yes |
| `ClockServer` / `ClockClient` (og-sim-net's `SimNetClockServer` / `SimNetClockClient` over the transport and `ClockT`) | the server | the client |
| `Bindings` = `LockedSimNetBindings<LockT>` | unused | yes |
| `ClientHost` = `SimNetLinkClientHost<ServicesT, LockT, EndpointT>` and `Client` = `SimNetClient<GameT, ClientHost, Transport>` | no | yes |
| the frame thread's id, and whether a session is attached | yes | yes |

Members destruct in reverse declaration order, and three `static_assert`s in `compositionContractsHold` (checked in the
constructor) keep the order that matters:
- the clock and the transport before the clock server and the clock client: each holds both by reference and is the transport's
  SimClock lane handler (og-input-latency P5-19 and P3-10, P2-20 F2; they were `UESimNetLanes`' until task 28);
- the bindings before the client host before the client session: the session holds its host by reference and the host the
  bindings (task 28; it was prose in `UESimNetLanes-rationale.md` §9).

An engine destroys the link only after og-net's endpoint, so no handler runs into a destroyed transport (Unreal's driver:
`DestroySimLanesAfterEndpoint`). `SimNetLinkTest.cpp`'s rig orders its members the same way: with the link destroyed first, og-net's
endpoint stop calls the transport's connection handler on freed memory (the test's first run crashed in teardown that way).

## §3 The join window: why a client's session exists from `start`

og-sim-net's server attaches a client on og-net's `Connected`, which comes before an engine's login, and from then on sends it
the roster, the floor, the tier, the clock and the relay rings. A world exists seconds later. So `start` creates the client
session with the transport (`SimNetLink-guards.md` G-01), and the frame service drains it from the first frame:
- the roster mirror follows every Add and Remove from the first drain;
- a floor or a tier is latched by the session, and the attach replays it (`SimNetClient::bindListener`);
- a clock tick before a world attaches is dropped by the host; the newest SimClock sample before it is held by the session and fed
  ahead of the first authority tick after the attach (`SimNetClient-rationale.md` §19, its G-14 guards the order);
- a correction for a sim id with no channel yet counts `unknownSim` and is dropped; a relay ring for a character with no channel
  yet is held per sim id and stored at the character's Add.

## §4 The doors and their threads

The link's **frame thread** is the thread that calls `start`. `start`'s first statement checks that the link was never started
and records `std::this_thread::get_id()`; from then on every frame door's first statement is `checkFrameThread()`, an `OG_CHECK`
that the caller is that thread, and `drainForStep`'s is an `OG_CHECK` that it is not. og-sim-net has no session thread table
(that is og-sim-session's `SessionThreadChecks`, tier 3, which og-sim-net may not include), so the link checks its own two
expectations against the id it recorded.

| door | thread | what it does |
|---|---|---|
| `start(endpoint)` | defines the frame thread | the transport, the clock server or client before `installHandlers` (the transport gives the SimClock lane to a handler set by then), the handlers, and on a client the host and the session (G-01) |
| `markStarted()` | frame | `markEndpointStarted` |
| `serviceFrame(afterPing)` | frame | the client's ping (`servicePing`), then `afterPing()`, then, while no session is attached, the drain and the disconnect request (G-02) |
| `flushFrame()` | frame | a client's `flushFrame` while no session is attached; nothing on a server |
| `drainForStep()` | not the frame | an attached client's drain, the disconnect request and `flushFrame` (§6) |
| `drainWhileStopped()` | frame | one drain at an attached session's teardown, with the steps stopped |
| `attachSession(session)` / `detachSession()` | frame | the bind protocol (§5) |
| `setOnRegisteredRemoved(fn)` | frame | the session's unregister handler for a registered id's roster Remove (§8) |
| `stop()` | frame | detaches; an engine calls it when its peer stops |

- **Why the frame thread is the start's.** Unreal's driver calls `beforeStart` in `InitBase` on the game thread, and every other
  driver point and every session bind runs there too; the step drains run on a worker. Until task 28 the two checks were
  `checkf(!IsInGameThread())` in `drainClient_Step` and `checkf(IsInGameThread())` in the teardown drain, and the other doors
  checked no thread. The link's checks are the same rule for every door, on any engine.
- **`serviceFrame`'s `afterPing`.** Unreal publishes two RTT readings on the game thread between the ping and the drain (the
  ack RTT and the server connection's round trip; `UESimNetLanes-rationale.md` §13 and `UESimNetHost-rationale.md` §5), so the
  drain's clock delivery reads this frame's round trip. The callable keeps that order without the link knowing the engine.
- **Seen failing.** The hidden case `SimNetLink.DrainForStepOnTheFrameThreadTripsTheCheck` (`[.][SimNetLinkDeath]`) attaches a
  session and calls `drainForStep()` on the test's own thread, the one that started the link: the `OG_CHECK` fires (in the
  Unreal test runner it is a `checkf`, reported as a failed assertion; under `OG_STANDALONE_BUILD` an `assert`). A run that
  reaches the case's `FAIL` line means the check did not fire.

## §5 The attach protocol

- `attachSession(session)` checks the client was started, points the client host at the session (`SimNetLinkSessionRef`, §9),
  requests the listener bind (`SimNetClient::bindListener`: the next drain replays the latched floor and tier and arms the held
  sample), seeds the bindings from the roster mirror (`seedFrom`), and sets the attached flag. The order is Unreal's
  `bindClientSession`'s.
- `detachSession()` requests the listener unbind, points the host at no session and clears the flag. The client session stays,
  with its roster, latches and channels, for the next world on the same endpoint; a second attach replays the floor and the tier
  once more, and holds and feeds a sample again.
- **The bindings follow the roster only while a session is attached** (task 28, step (b)). Until then Unreal's client host fed
  them from the first drain, attached or not, so the attach's seed changed nothing: it replaced the entries with the same
  entries. Now the host feeds them only while attached, and the seed at the attach brings them up to the mirror. Nothing reads
  the bindings while no session is attached (every reader is the attached session's registration path, and a session's detach
  unregisters what it registered first), so every verdict is the same; and the seed is no longer dead code, so a link that
  skips it is caught: `SimNetLink.ACharacterAlreadyInTheRosterBindsAtOnce` fails (task 28's seen failing P1).
- The attach and the detach run on the frame thread, before the steps start and after they stop; the host's session pointer is
  written there and read on the step thread after the listener bind's release store (as Unreal's host's session pointer was).

## §6 Where a client drains

- **Before a session attaches**, in `serviceFrame` on the frame thread, through `drain()`, the client's only drain
  (`SimNetClient-rationale.md` §16). Nothing steps yet, so the frame is the drain's only caller. The disconnect request is acted on
  after it.
- **Once a session is attached**, at the start of each of its steps, on the step's thread, through `drainForStep()`, which the
  session calls from the step's receive hook. It checks the thread and the attach, drains, acts on the disconnect request, and
  calls `flushFrame()`: that holds nothing (every send already went to the endpoint), but it emits the `[SimNet.Window]` line,
  whose counters the drain writes, so it belongs to the drain's thread. `serviceFrame` then drains nothing (G-02) and
  `flushFrame` on the frame does nothing.
- **After the session detaches**, back in `serviceFrame`. A session stops its steps before it detaches, so the two never overlap.
- **At the session's teardown**, once, through `drainWhileStopped()`: the steps have stopped, and the drain applies the
  unregistrations the session queued last.

**The disconnect request.** A client that refuses a roster message (a refused roster decode disconnects) latches a request.
The link takes it after each drain, logs `[Error][SimNet.Host] role=client disconnect conn=<c> reason=<r>: the server's roster was
refused, so this client leaves the server` through `logRouted` (§9), and disconnects that og-net connection through the
endpoint.

## §7 The client host

`SimNetLinkClientHost` is the `HostT` of the link's `SimNetClient` (og-sim-net's `SimNetClientHost` concept, plus the optional
delivery, unregistering and clock-sample hooks). It was the engine-free half of Unreal's `UESimNetClientHost` and its derived
`UESimNetLanesClientHost` until task 28.

| member | source | attached only |
|---|---|---|
| `nowSeconds` | the services' `nowSeconds` | no |
| `serverRttSeconds` | the services' `serverRttSeconds`; `kSimNetLinkNoRttSeconds` (−1) with no session | yes |
| `onTimingInfo`, `onRelayFloor` (G-04), `onConnectionTier`, `onClockSample`, `onLifecycleUnregistering` | the attached session | yes |
| `onRosterAdded`, `onRosterRemoved` | the bindings (§8), and the session's unregister handler (G-03) | yes (the mirror check: no) |
| `onWireMismatch` | the services' `onWireMismatch` (G-05) | no |
| `log` | `routeSimNetClientLine` (§9) | no |
| `stampClientReceived` | the attached session | yes |
| `noteDelivered` | the endpoint's `noteDelivered(lane, recv, now)`, now from the services' `nowSeconds` | no |

- **Unattached, `serverRttSeconds` reads nothing.** It is only called right before `onTimingInfo`, which drops with no session; a
  warm-up read in the join window would spend the services' one-shot warning on a reading nobody uses.
- **The floor and the tier are applied as received**, replayed or not (G-04): og-net delivers the floor on the server's attach,
  before any world, so every attach-time floor is a replay, and applied as a replay it would never pay its stall.
- **The stamps are the session's**, not the services': the sink is the attached world session's latency diagnostics, which do not
  exist without one (Unreal's `UESimNetLinkSession::stampClientReceived` maps the stream and posts through the diagnostics'
  mailbox, `UELatencyBudgetDiagnostics-rationale.md` §13).
- **`onRosterAdded` checks the mirror always** (an `OG_CHECK`: the mirror reports an Add after it applied it, and the link sets the
  mirror before the first drain) and feeds the bindings only while attached (§5).

## §8 The roster bindings and the lock

`LockedSimNetBindings<LockT>` is og-sim-net's `SimNetBindings` behind a `LockT`, with the same members plus
`markRegisteredIfReady` (it was Unreal's `FLockedSimNetBindings` over a `std::mutex`, og-input-latency task P5-17). On an attached
client the drain writes the bindings on the step's thread (`onRosterAdded`, `onRosterRemoved`) while the session reads and marks
them on the frame thread (its verdicts, its registrations, its detach).
- **`markRegisteredIfReady(simId, engineSaysLocal)`** asks the verdict and, when it is `Ready`, marks the id registered, in one
  hold. A client's registration decides on the verdict and then queues the register for the step's drain; a verdict and a
  separate mark would let a drain between them apply a Remove whose `onRosterRemoved` found the id unregistered (so the session
  never heard of it), after which the mark would `OG_CHECK` on an entry that is gone.
- **Every call is short** and takes no other lock; nothing calls out while holding it.
- **`onRosterRemoved`** tells the bindings first; they return true exactly once if the sim id was registered, and then the host
  calls the session's handler at once (G-03). A registered id with no handler set is an `OG_CHECK`; with checks off the call is
  skipped rather than made through an empty function.
- `SimNetBindings` itself stays single-threaded by contract (`SimNetBindings-rationale.md`); the lock is the link's because the
  threads are.

## §9 The ports: services, session hooks, log routing

**`SimNetLinkServices`** (each requirement has a negative in `simNetLinkSelfCheck`):
- `nowSeconds()` — the client host's clock and the delivery note's time;
- `logRouted(line)` — a line whose leading `[Verbose]`, `[Warning]` or `[Error]` token is its level and is not printed (og-sim-
  session's `SessionEngine` has the same sink; Unreal: `RouteOGLevelLine`);
- `logHost(line)` — og-sim-net's host lines, printed whole, a leading token included (Unreal: `RouteOGMessage`, as the hosts'
  `log` always was);
- `serverRttSeconds()` — the round trip the clock tick is delivered with (Unreal: the published `FNetPing` copy and its
  once-per-cause warnings, `UESimNetHost-rationale.md` §5);
- `onWireMismatch(received, expected)` — the engine's report of a build mismatch (Unreal: the Error line and the toast).

The design's list (the session-runtime design's §3.3 and og-session-runtime task 28) also names `rttMs(conn)` and the diagnostics stamps.
The link holds no server host (the authority's host is its world session's, owned by og-sim-session's runtime from task 31), so
no door reads a connection's round trip; and the client's stamps go to the attached session (§7). `logHost` and
`onWireMismatch` are added because the client host needs them. The concepts are checked by `simNetLinkSelfCheck` and by
task 28's requirement-deletion mutants (each requirement deleted in turn, and two return types weakened: every mutant fires a
self-check).

**`SimNetLinkSession`**, the attached session's hooks: `onTimingInfo(tick, rtt)`, `onRelayFloor(floor)`,
`onConnectionTier(old, new)`, `onClockSample(sample)`, `onLifecycleUnregistering(simId)`,
`stampClientReceived(stream, simId, tick, recv)`. `attachSession` takes any type that models it and stores a
`SimNetLinkSessionRef`: the object's address and one function pointer per hook, so the link's five parameters do not grow a sixth
and the host calls through plain pointers. The session object must outlive its attach (Unreal's lanes own their
`UESimNetLinkSession` and declare it before the link).

**Log routing** (`routeSimNetClientLine`, the client host's `log`):
- a line that begins with `kClockSourceHeldTag` (`[ClockSource.Held]`, formatted by the client session) gets a `[Warning]` token
  and goes to `logRouted`, so it is printed without the token at Warning; Unreal's route table has a `[ClockSource.Held]` row on the
  Net route, so the line keeps `LogOGNet` at `Warning`, the category and level of the `UE_LOG` that wrote it until og-session-runtime
  task 20;
- every other line goes through `routeSimNetHostLine` to `logHost`: a line that begins with `[Roster.` gets a `[Warning]` token
  first (og-input-latency task P5-7, from P5-4's F1: a token-less roster refusal or mismatch must reach an operator under
  `LogOGNet=Warning`), into a buffer the size of og-sim-net's roster line plus the token, and the printed text keeps the token as
  every other host line does. Unreal's server host routes its lines through the same function (`ueSimNetHost::routeLine`);
- the link's own disconnect line goes to `logRouted` with an `[Error]` token, so it stays at `LogOGNet` `Error` (Unreal's level
  lines gained the `[Error]` token for it, `UESimulationLogRoutes-rationale.md`).

Unreal's parity tests (`UESimNetClockHeldLineTest.cpp`): `OGBrawler.SimNet.ClockHeldLine` routes nine held lines through
`routeSimNetClientLine` over `UESimNetLinkServices` beside the old `UE_LOG` and compares text, category and level;
`OGBrawler.SimNet.DisconnectLine` formats five disconnect lines with `kSimNetDisconnectFormat`, compares their bytes with the
old `TEXT` format, then logs each the old way and through `logRouted` and compares text, category (`LogOGNet`) and level
(`Error`).

## §10 Tests (`og-sim-net/tests/SimNetLinkTest.cpp`, `[NetTransport][SimNetLink]`)

A rig over og-net's `VirtualNetwork`: an og-net server endpoint with og-sim-net's `SimNetServer` and `SimNetClockServer`, and a
client link over a test endpoint that adds `config()` and a counted `noteDelivered` to og-net's `NetEndpoint`. Frames run the
link's frame doors on the test thread; an attached session's steps drain on a new `std::thread` each.
- **A client connected before any session holds the roster, the floor, the tier and a clock sample**: two roster entries (one
  the client's own), floor 6, tier 2, a held sample, ticks delivered and dropped; no RTT read, no held line, the bindings empty.
- **Attach replays the floor and tier and feeds the held sample before the first tick**: the session's first events are
  `floor 6`, `tier 0->2`, `sample` (the held one: its `t3` and round trip equal the held sample's), then a tick; one
  `[Warning][ClockSource.Held]` line. The attach waits until no pong is in flight, so the first step drain holds the tick and no
  newer sample.
- **A character already in the roster binds at once**: right after the attach, with no drain, both entries are `Ready` with
  their roles, the wrong role is `RoleMismatch`, and `markRegisteredIfReady` registers.
- **A character added after the attach binds after the step drain**, and its Remove unbinds it.
- **Detach then attach again replays once**: the second session sees one floor and one tier, first; the first session hears
  nothing after its detach.
- **The disconnect request disconnects**: a roster Add with a revision gap pushed into the client's inbox; the frame drain refuses
  it, the link logs the exact `[Error][SimNet.Host] ... reason=RevisionGap ...` line and disconnects the server's connection, and
  the server detaches it.
- The hidden death case of §4.
