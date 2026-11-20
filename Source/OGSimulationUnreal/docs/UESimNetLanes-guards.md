<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESimNetLanes.h` — guards

Every prohibition that governs a line of `UESimNetLanes.h`. Each entry has an **opaque, stable id**. In the source a single
line `// ⛔G-nn  docs/UESimNetLanes-guards.md` sits directly above the statement where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not soften
the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `UESimNetLanes-rationale.md`.

---

## G-01 — a client's session is created at driver start, not when the world binds

**Tag site:** `UESimNetLanes.h`, `beforeStart`, the line above `m_client.emplace(*m_clientHost, *m_transport)`.

**The prohibition.** Do not move the client session's creation out of `beforeStart`: not into `bindClientSession`, not into
the world's BeginPlay, not behind any later event.

**The consequence.** The server attaches a client on og-net's `Connected`, before Unreal's login, and from then on sends it
the roster, the floor, the tier, the clock and the relay rings. A client whose session does not exist yet drains nothing,
so all of it waits in the transport's inbox until the world binds. Past 1,984 undrained messages the inbox refuses the
newest one (counted as `drops`), so the oldest relays and clocks are kept and the fresh ones lost; and past 456 undrained
Control messages the transport disconnects the server itself. The latched floor and tier that `bindClientSession` replays
are only latched by a session that drained them.

**What breaks if it moves.** Nothing at compile time, and no test before the switch (no factory is registered). After it,
a slow world load shows `drops=` in the first `[SimNet.Window]` lines and, at the worst, a client that disconnects during
its own load.

---

## G-02 — an immediate client's game thread does not drain once a world session is bound

**Tag site:** `UESimNetLanes.h`, `afterDispatch`, the line above `if (m_clientSessionBound.load(std::memory_order_relaxed))`,
the early return before the client's game-thread `drain()`.

**The prohibition.** Do not drop this early return, do not make it depend on anything but the session's bind, and do not move
the game-thread `drain()` above it. Once a world session is bound, the session's steps drain an immediate client from the step's
thread (`drainClient_Step`, rationale §11).

**The consequence.** A game-thread drain then runs every consumer the drain reaches (the correction inject, the relay ingest, the
estimator feed, the tier receipt, the roster with its registrations) on the game thread while a step runs the same objects on a
worker, and the lifecycle queue's registrations land in the middle of a step. og-sim-net's in-drain flag catches two drains that
overlap in time (an `OG_CHECK`); one that does not overlap is caught by nothing and is a data race on every structure the step
reads.

**What breaks if it moves.** Nothing at compile time; a smoke with `SimTrafficImmediate=1` stays healthy on most runs.

---

## G-03 — `beforeStop` joins the authority runner before anything else

**Tag site:** `UESimNetLanes.h`, `beforeStop`, the line above `if (joinRunner)`.

**The prohibition.** Do not drop the join, do not move it below the stop handler or the unbind, and do not move it into the
lanes' destructor. On an authority-loop server the runner is the endpoint's servicing thread; it must have stopped before the
driver stops og-net, and before the session's stop handler tears down what the runner reads.

**The consequence.** og-net's stop runs on the game thread right after `beforeStop` returns, and checks that no other thread
services the endpoint (`EndpointCore.cpp`'s stop check, an `OGNET_CHECK`): with the runner still idling in `serviceUntil`, the
server aborts at every map change and shutdown. Below the stop handler, the handler unregisters characters and releases
og-sim-net's server while the runner drains and steps them. The driver's destructor never calls `beforeStop`, so a join in the
lanes' destructor would come after og-net's stop (`DestroySimLanesAfterEndpoint`).

**What breaks if it moves.** Nothing at compile time. With `og.Sim.AuthorityLoop=1` a dedicated server aborts in og-net's stop
check (or races its runner) when its driver is destroyed; the loop-off servers bind no join and are unaffected.

---

## G-04 — the loop's RTT publish asks the base host only from a connection's Join

**Tag site:** `UESimNetLanes.h`, `UESimNetLanesServerHost::publishedRttMs_GameThread`, the line above
`if (connection.ClientLoginState == EClientLoginState::ReceivedJoin)`.

**The prohibition.** Do not call `Base::rttMs` (the base host's warning path) for a connection whose `ClientLoginState` is not
yet `ReceivedJoin`, and do not drop the gate to publish `Base::rttMs` for every connection. Before the Join read the round
trip without a warning.

**The consequence.** `publishRunnerRtt_GameThread` asks every frame from the frame og-net accepts a connection. Before the
Join UE may have sampled no ack, so the base's read answers `AccumulatorEmpty`, and its warning is one-shot per cause: the
server logs a false `[RttSample] SERVER tier RTT source unavailable (... accumulator is empty (no ack sampled yet))` line
before the first login, and a later real `AccumulatorEmpty` outage is silent for the rest of the session.

**What breaks if it moves.** Nothing at compile time, and the published values do not change. With
`og.Sim.AuthorityLoop=1` a dedicated server's log carries the false warning shortly before its first `Login request`;
loop-off servers never call the publish.

---

## R. Retired ids

None.
