<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimLaneEndpoint.h` — rationale

`ognetunreal::SimLaneEndpoint` is a type-erased view of an og-net endpoint's sim lanes: one context pointer and ten function
pointers (`SimLaneEndpointOps`; P5-3's seven, task P5-18's `serviceUntil` and `hasHighResolutionWait`, and og-session-runtime task
39's `stats`). The engine-lane driver's core builds one over its own endpoint and hands it to the sim side
before the endpoint starts (`EngineLaneDriverCore-rationale.md` §12). A world without the og-net driver starts no sim session
(og-session-runtime task 14), so it needs no endpoint; the detached one it used to get is retired (§3). This
file is the docs pointer of `SimLaneEndpoint.h`; its one prohibition is in `SimLaneEndpoint-guards.md`. Its tests are in
`SimLaneSeamTest.cpp` (`[OGNet][EngineLaneDriver]`, run by `OGNetTests`).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 Why an erased endpoint

- **OGNetUnreal depends on no OGSimulation module, and og-sim-net on no engine module.** The sim transport
  (`ogsimnet::SimNetTransport<EndpointT, InboxN>`) is a template over its endpoint, which only has to satisfy og-sim-net's
  `SimNetEndpoint` concept: `send`, `broadcastExcept`, `setLaneHandler`, `setConnectionHandler` and `disconnect`, each returning
  `void`. The driver's endpoint is `ognet::NetEndpoint<FOGNetUnrealHost, SwitchableBackend<BsdDatagramBackend>>`, a type private to
  `OGNetDriver.cpp`. Naming it in the game's transport would pull the driver's host and backend into the game; erasing it gives the
  game one concrete type, `SimNetTransport<SimLaneEndpoint, N>`, for every driver.
- **The concept is checked where both are visible:** the test asserts `ogsimnet::SimNetEndpoint<SimLaneEndpoint>` and installs a
  `SimNetTransport<SimLaneEndpoint, 128>` through the core's attach (`EngineLaneDriver.AnAttachedSimNetTransportReceivesEachSimLaneIntoItsInboxWithConnectedFirst`).
  This module cannot assert it, because it does not include og-sim-net.
- **The cost** is one indirect call per send, per handler set and per drained message's `noteDelivered`; og-net's own send already
  takes a mutex and copies into its command queue.

## §2 The members

| member | what it does |
|---|---|
| `send(id, lane, bytes)`, `broadcastExcept(skip, lane, bytes)` | og-net's own calls on the endpoint. They are safe from any thread (og-net's command queue). |
| `setLaneHandler(lane, handler)` | og-net's, before the endpoint starts. The Engine lane is refused (`OGNET_CHECK`): the core polls it for the engine's packets. |
| `setConnectionHandler(handler)` | stores the target the core forwards every connection event to, before start only. og-net has one connection handler, and the core holds it. |
| `disconnect(id)` | og-net's. |
| `noteDelivered(lane, recvSeconds, nowSeconds)` | notes one delivered sim message's residence in the core's `[OGNet.Window]` (`EngineLaneDriverCore-rationale.md` §11). og-sim-net calls it once per drained message through an optional host hook (task P5-1). |
| `config()` | the endpoint configuration the core started with. It is set before the attach runs, so the attach can size its transport from it (`ogsimnet::simNetConfigFor`). |
| `serviceUntil(deadlineSeconds)` | the core's `serviceUntil`, so og-net's mode B: the caller services the endpoint until the deadline and gets og-net's `ognet::ServiceResult`. Only a core started in `IoMode::HostServiced` takes it (the core's `OGNET_CHECK`), and og-net fixes the servicing thread at the first call: a call from any other thread afterwards aborts (`EndpointCore.cpp`, `EndpointCore::fixServicingThread`). It is the Ph4 server's runner that calls it (design P5 K14, task P5-19). |
| `stats(id)` | og-net's per-connection stats, the core's `connectionStats(id)` (og-net's `NetEndpoint::stats`; an empty `ConnectionStats` before the core has an endpoint). og-net publishes them every 50 ms and reads them "from any thread" under its stats mutex (`EndpointCore-rationale.md` §10), so the Ph4 server's runner reads them on its own thread: og-sim-net's server host core takes each connection's round trip from them (`SimNetServerHost-rationale.md` §6, og-session-runtime task 39). The core's endpoint exists from before `beforeStart` until the core is destroyed (or its start fails), and the runner is joined in the sim lanes' `beforeStop`, so the runner never reads a destroyed endpoint. |
| `hasHighResolutionWait()` | whether the core's backend waits with a high-resolution timer (`ognet::BsdDatagramBackend::hasHighResolutionWait`), so that a host can tell a precise idle from a millisecond one (P11-8 F1). It is false for a backend that does not report it, and before the core has started. |

The core's operations do nothing once the endpoint is gone (a failed start). After a stop, og-net's command queue no longer accepts
a send (`EndpointCore.cpp`, `EndpointCore::enqueue`), so a late send is dropped there.

## §3 The detached endpoint (retired by og-session-runtime task 15)

Until task 15 a static factory built a detached endpoint over a given config for a world with no og-net driver (a standalone
world, or a networked world on another driver, design P5 §3.2), so the sim session could run there with no traffic: its sends
and disconnects did nothing but count drops, its handler setters did nothing, `config()` returned the given config, and
`serviceUntil` returned at once with `stopped` set. Its context was the endpoint itself. Task 14 made those worlds start no
session at all and deleted the session's detached lanes, which left one test as the factory's only user, so the factory, its
operations, its config copy and its drop counter went in task 15 with its two `SimLaneSeamTest.cpp` cases (one counted the
drops, one checked that it serviced nothing). A test that wants an endpoint with nothing behind it builds one through the public constructor with its own ten
operations, as `UESimNetHostsConceptTest.cpp`'s `FDroppingEndpoint` does.

## §4 Tests

`SimLaneSeamTest.cpp` holds this file's cases and the core's attach cases (`EngineLaneDriverCore-rationale.md` §12), including
the host-serviced ones of task P5-18 (`EngineLaneDriverCore-rationale.md` §13).
