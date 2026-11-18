<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGNetSimLanes.h` — rationale

`IOGNetSimLanes` is the interface through which `UOGNetDriver` (`OGNetDriver-rationale.md`) carries a game's simulation traffic on
the sim lanes of its own og-net endpoint, beside the engine's packets on the engine lane. The driver owns one lanes object per
driver, made by a factory the game registers, and calls it at five points of its own life. This file is the docs pointer of
`OGNetSimLanes.h`; the header states no prohibition, so it has no guards file. Design P5 K1 to K4, task P5-3.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 Why a factory and an interface

- **OGNetUnreal depends on no OGSimulation module** (P5's layering rule): the driver cannot name og-sim-net's transport or the
  game's sessions. The game implements `IOGNetSimLanes` with them (P5-4's `UESimNetLanes<TraitsT>`) and registers a factory, so
  the driver creates the game's object without knowing its type.
- **One object per driver, created in `InitBase`.** The sim lanes' handlers must be on the endpoint before og-net starts it
  (`EngineLaneDriverCore-rationale.md` §12), and the driver starts it in `InitBase`. A client's lanes then exist from the first
  packet, seconds before its world (design P5 K4).
- **The header names no og-net type.** `ognetunreal::SimLaneEndpoint` is only forward-declared, so `OGNetDriver.h`, a reflected
  public header, can include this one without og-net's include paths (`OGNetDriver-guards.md` G-01). An implementer includes
  `OGNetUnreal/Core/SimLaneEndpoint.h` itself.

## §2 The five calls, in the order the driver makes them

| call | when (game thread) | what it is for |
|---|---|---|
| `beforeStart(SimLaneEndpoint&)` | inside `InitBase`, in the core's `start`, after the core's own connection handler and before og-net starts the endpoint | install the sim transport's lane handlers and its connection-event target (`setConnectionHandler`). The endpoint reference stays valid until the driver's endpoint is destroyed. |
| `afterStart()` | inside `InitBase`, after the core's `start` returned true and the `[OGNet.Driver] start` line | everything that needs a started endpoint (og-sim-net's `markEndpointStarted`) |
| `afterDispatch()` | every frame, at the end of the driver's `PostTickDispatch` | the frame's receive point: every packet of the frame has been dispatched (design P5 K3) |
| `onTickFlush()` | every frame, at the end of the driver's `TickFlush` | the frame's send point: the engine has sent its own traffic |
| `beforeStop()` | in `LowLevelDestroy`, before the core stops og-net | unbind from the sessions while og-net still runs |

- **A failed start** destroys the lanes object without calling `afterStart` or `beforeStop`.
- **Destruction** comes after the driver's endpoint, so after og-net's thread has joined: until then that thread may call the
  handlers the lanes installed.
- The connection events and lane messages themselves reach the lanes' handlers on og-net's own thread, not through these calls.

## §3 The factory

`FOGNetSimLanesFactory` is a plain function pointer, `TUniquePtr<IOGNetSimLanes> (*)(UOGNetDriver&, bool bIsServer)`, set once
with `UOGNetDriver::SetSimLanesFactory` (`OGNetDriver-rationale.md` §13). A factory that returns null leaves that driver without
sim lanes.
