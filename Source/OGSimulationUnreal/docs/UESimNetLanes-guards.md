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

## G-03 — `beforeStop` joins the authority runner before anything else

**Tag site:** `UESimNetLanes.h`, `beforeStop`, the line above `if (joinRunner)`.

**The prohibition.** Do not drop the join, do not move it below the stop handler or the unbind, and do not move it into the
lanes' destructor. On a dedicated server the runner is the endpoint's servicing thread; it must have stopped before the
driver stops og-net, and before the session's stop handler tears down what the runner reads.

**The consequence.** og-net's stop runs on the game thread right after `beforeStop` returns, and checks that no other thread
services the endpoint (`EndpointCore.cpp`'s stop check, an `OGNET_CHECK`): with the runner still idling in `serviceUntil`, the
server aborts at every map change and shutdown. Below the stop handler, the handler unregisters characters and releases
og-sim-net's server while the runner drains and steps them. The driver's destructor never calls `beforeStop`, so a join in the
lanes' destructor would come after og-net's stop (`DestroySimLanesAfterEndpoint`).

**What breaks if it moves.** Nothing at compile time. A dedicated server aborts in og-net's stop check (or races its runner)
when its driver is destroyed. Every dedicated server runs the loop since og-session-runtime task 14; until then the in-frame
servers bound no join and were unaffected.

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-01 — RETIRED by og-session-runtime task 28 (2026-11-25): moved to `SimNetLink-guards.md` G-01 with the client session's
creation (og-sim-net's `SimNetLink::start`)

**Was:** *"a client's session is created at driver start, not when the world binds"*. Its tag sat in `UESimNetLanes.h`'s
`beforeStart`, above the client session's `emplace`.

### G-02 — RETIRED by og-session-runtime task 28 (2026-11-25): moved to `SimNetLink-guards.md` G-02 with the frame drain's early
return (og-sim-net's `SimNetLink::serviceFrame`)

**Was:** *"a client's game thread does not drain once a world session is bound"*. Its tag sat in `UESimNetLanes.h`'s
`afterDispatch`, above the bound check before the game thread's drain.

### G-04 — RETIRED by og-session-runtime task 28 (2026-11-25): moved to `UESimNetHost-guards.md` G-13 with its function, which
step (a) moved unchanged into `UESimNetHost.h`

**Was:** *"the loop's RTT publish asks the base host only from a connection's Join"*. Its tag sat in `UESimNetLanes.h`'s
`UESimNetLanesServerHost::publishedRttMs_GameThread`, above the `ReceivedJoin` gate.
