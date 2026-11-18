<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESimNetLanes.h` — rationale

`UESimNetLanes<TraitsT>` is the og-net driver's per-driver sim lanes object: the `IOGNetSimLanes` that `UOGNetDriver` creates at
`InitBase` through the factory a game registers, and calls at its five frame points (`OGNetSimLanes-rationale.md`). It owns the
sim transport over the driver's erased endpoint and, on a client, the client session. It is a header-only template over the
game's session traits. This file is the docs pointer of `UESimNetLanes.h`; its one prohibition is in `UESimNetLanes-guards.md`.
The hosts it uses are in `UESimNetHost.h` (`UESimNetHost-rationale.md`).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 What it holds, per role

| member | server | client |
|---|---|---|
| `Transport` = `ogsimnet::SimNetTransport<SimLaneEndpoint, kUESimNetInboxSlots>` | yes | yes |
| `ClientHost` and `Client` = `ogsimnet::SimNetClient<Composition, ClientHost, Transport>` | no | yes |
| `ogsimnet::SimNetBindings` | unused | yes |
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
  - a clock before the world binds is dropped by the host, as today's timing relay drops an OnRep that finds no listener;
  - a correction for a sim id with no channel yet counts `unknownSim` and is dropped; a relay ring for a character with no
    channel yet is held per sim id and stored at the character's Add.
  
  A session created at bind would leave all of that in the inbox for the whole load (`UESimNetLanes-guards.md` G-01).
  Only map load (the engine's LoadMap) itself is a gap in the draining: the unreliable lanes' overflow there is counted as `drops`, in windows
  before steady state.
- **Server.** The server's world and its listen happen in one map load (the engine's LoadMap), so no frame passes between the driver's start and the
  world's BeginPlay in which a client could have sent anything (the P5 design's inference, K4; not measured here). The world's session creates `SimNetServer` (which owns the
  reception coordinator) at BeginPlay and binds it here with `bindServerSession(dispatchFn, flushFn)`. The lanes keep the two
  functions, not the server: its type depends on the session's correction policy and its lifetime on the world.

## §3 The five call points

| driver point | server | client |
|---|---|---|
| `beforeStart(endpoint)` | the transport, its handlers, the forward target | the same, then the client host and the client session |
| `afterStart()` | `markEndpointStarted` | `markEndpointStarted` |
| `afterDispatch()` | the bound dispatch function, if any | `dispatchFrame`, then a disconnect request (below) |
| `onTickFlush()` | the bound flush function, if any | `flushFrame` |
| `beforeStop()` | the session's stop handler, then unbind the server session | the session's stop handler, then unbind the client session |

`afterDispatch` runs after `Super::PostTickDispatch` (every packet dispatched, before the first tick group), and `onTickFlush`
after `Super::TickFlush` (after the engine's own send). `OGNetDriver-rationale.md` §13 has the verified engine order.

**The disconnect request.** A client that refuses a roster message (P7's rule: a refused roster decode disconnects) latches a
request. `afterDispatch` takes it after each drain, logs `[SimNet.Host] role=client disconnect conn= reason=` at Error to
`LogOGNet`, and disconnects that og-net connection through the endpoint.

## §4 The transport is built inside `beforeStart`

The transport's sizes come from its endpoint's config, `ogsimnet::simNetConfigFor(endpoint.config(), false)`: a server's for
its `maxConnections`, a client's for its one server, and the Control quota from the lane table. That config exists only once
the driver has built its endpoint, and the driver hands the endpoint over in `beforeStart` and nowhere earlier. So the
transport is an `std::optional` emplaced there (it can be neither copied nor moved), and so is the client session that holds
a reference to it. `beforeStart` also `checkf`s that the endpoint's role is the role the lanes were made for.

`immediate` is `false`: Ph3's traffic is frame-locked, held by the transport until `onTickFlush`.

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
  `markRegistered`, `markUnregistered`).
- `setStopHandler(onStop)` (og-input-latency task P5-7): the world session's handler for the driver's stop, which `beforeStop`
  calls once, first, and clears. Unreal shuts a world's net driver down before it destroys the world's pawns and ends its
  play (its map load), and the lanes, with a client's channels, die with the driver. So the session unregisters every
  registered character there, while the channels and the endpoint still live, and forgets the lanes
  (`UESimulationSession-rationale.md` §16.5). A session that ends first clears the handler (its `detachFromSimNetLanes`).

## §6 The server session's binding

`bindServerSession(dispatchFn, flushFn)` `checkf`s that both are set; `unbindServerSession()` clears both. Unbound,
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
connection handlers, which `beforeStart` installs before the endpoint starts.

## §9 Lifetime

The driver destroys the lanes after its endpoint, so after og-net's thread has joined (`OGNetDriver-rationale.md` §13): no
handler can run into a destroyed transport. Within the lanes, the client session is created after the transport and the host
it references, and destroyed before them; none of their destructors calls another. A world session never keeps a pointer to
lanes past `beforeStop`: the stop handler (§5) runs first in it and the session forgets the lanes there.

## §10 The switch, and the namespace aliases

Since og-input-latency task P5-7 the brawler registers its factory (`makeBrawlerSimNetLanes`) at module start-up, so every
og-net driver makes a lanes object at `InitBase`, and the world session binds to it (`UESimulationSession-rationale.md` §16).
`UESimNetTransport` and `UESimNetChannel<SimT>` name the transport and the per-character channel at namespace scope, without
the session traits, so a game's `SimulatableOwnerTraits` can name the channel where the traits are not complete yet; the class's
`Transport` is `UESimNetTransport`, and the session asserts its channel type equals `UESimNetChannel<Simulatable>`. Before the
switch nothing in the game included this header except its concept test, `UESimNetHostsConceptTest.cpp`, which still builds a
detached lanes object for each role (the automation test `OGBrawler.SimNet.Hosts`).
