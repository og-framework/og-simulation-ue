<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimLaneEndpoint.h` — rationale

`ognetunreal::SimLaneEndpoint` is a type-erased view of an og-net endpoint's sim lanes: one context pointer and nine function
pointers (`SimLaneEndpointOps`; P5-3's seven, and task P5-18's `serviceUntil` and `hasHighResolutionWait`). The engine-lane driver's core builds one over its own endpoint and hands it to the sim side
before the endpoint starts (`EngineLaneDriverCore-rationale.md` §12); a world without the og-net driver uses a detached one. This
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
  game one concrete type, `SimNetTransport<SimLaneEndpoint, N>`, for every driver and for the detached case.
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
| `hasHighResolutionWait()` | whether the core's backend waits with a high-resolution timer (`ognet::BsdDatagramBackend::hasHighResolutionWait`), so that a host can tell a precise idle from a millisecond one (P11-8 F1). It is false for a backend that does not report it, and before the core has started. |
| `detachedDrops()` | the detached endpoint's count (§3); 0 for the core's. |

The core's operations do nothing once the endpoint is gone (a failed start). After a stop, og-net's command queue no longer accepts
a send (`EndpointCore.cpp`, `EndpointCore::enqueue`), so a late send is dropped there.

## §3 The detached endpoint

`SimLaneEndpoint::detached(config)` is for a world with no og-net driver (a standalone world, or a networked world on another
driver, design P5 §3.2): the sim session can then run unchanged with no traffic.

- `send`, `broadcastExcept` and `disconnect` do nothing and each adds one to `detachedDrops()` (relaxed atomic: a Ph4 client sends
  from its step thread).
- `setLaneHandler`, `setConnectionHandler` and `noteDelivered` do nothing; no handler is ever called.
- `config()` returns the config it was given.
- `serviceUntil` returns at once with `stopped` set (no datagrams, deadline not reached): there is no endpoint to service, and a
  host's idle over it reads "unavailable" rather than waiting. `hasHighResolutionWait()` is false.
  `EngineLaneDriver.TheDetachedEndpointServicesNothingAndHasNoHighResolutionWait`.
- Its context is the endpoint itself, which is why the type is neither copied nor moved (guard G-01). `detached` returns a prvalue,
  so a holder initialises it in place (C++17 guaranteed copy elision): a member initialiser or a local.
- `EngineLaneDriver.TheDetachedEndpointCountsAndDropsItsSendsAndDisconnects`: two direct sends and a disconnect count 3; a
  `SimNetTransport` over it holds two sends until `flushFrame`, which counts them (5); its inbox stays empty.

## §4 Tests

`SimLaneSeamTest.cpp` holds this file's cases and the core's attach cases (`EngineLaneDriverCore-rationale.md` §12), including
the host-serviced ones of task P5-18 (`EngineLaneDriverCore-rationale.md` §13).
