<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetLink.h` — guards

Every prohibition that governs a line of `SimNetLink.h`. Each entry has an **opaque, stable id**. In the source a single
line `// ⛔G-nn  docs/SimNetLink-guards.md` sits directly above the statement where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not soften
the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `SimNetLink-rationale.md`.

---

## G-01 — a client's session is created at `start`, not when a world session attaches

**Tag site:** `SimNetLink.h`, `SimNetLink::start`, the line above `m_client.emplace(*m_clientHost, *m_transport)`.

**The prohibition.** Do not move the client session's creation out of `start`: not into `attachSession`, not behind any later
event.

**The consequence.** The server attaches a client on og-net's `Connected`, before the engine's login, and from then on sends it
the roster, the floor, the tier, the clock and the relay rings. A client whose session does not exist yet drains nothing, so all
of it waits in the transport's inbox until a world attaches. Past 1,984 undrained messages the inbox refuses the newest one
(counted as `drops`), so the oldest relays and clocks are kept and the fresh ones lost; and past 456 undrained Control messages
the transport disconnects the server itself. The latched floor and tier that the attach replays, and the clock sample it feeds,
are only latched by a session that drained them.

**What breaks if it moves.** `SimNetLinkTest.cpp`'s "a client connected before any session holds the roster, the floor, the tier and
a clock sample" fails at once (the session does not exist to hold them). In the engine, a slow world load shows `drops=` in the
first `[SimNet.Window]` lines and, at the worst, a client that disconnects during its own load.

Was `UESimNetLanes-guards.md` G-01 (og-session-runtime task 28 moved the line into the link).

---

## G-02 — a client's frame does not drain once a world session is attached

**Tag site:** `SimNetLink.h`, `SimNetLink::serviceFrame`, the line above `if (m_clientSessionBound.load(std::memory_order_relaxed))`,
the early return before the frame's `drain()`.

**The prohibition.** Do not drop this early return, do not make it depend on anything but the attach, and do not move the
frame's `drain()` above it. Once a world session is attached, the session's steps drain the client from the step's thread
(`drainForStep`, `SimNetLink-rationale.md` §6).

**The consequence.** A frame drain then runs every consumer the drain reaches (the correction inject, the relay ingest, the
estimator feed, the tier receipt, the roster with its registrations) on the frame thread while a step runs the same objects on a
worker, and the lifecycle queue's registrations land in the middle of a step. og-sim-net's in-drain flag catches two drains that
overlap in time (an `OG_CHECK`); one that does not overlap is caught by nothing and is a data race on every structure the step
reads.

**What breaks if it moves.** Nothing at compile time; a smoke stays healthy on most runs.

Was `UESimNetLanes-guards.md` G-02 (og-session-runtime task 28 moved the line into the link).

---

## G-03 — a registered character is unregistered inside `onRosterRemoved`

**Tag site:** `SimNetLink.h`, `SimNetLinkClientHost::onRosterRemoved`, the line above `m_onRegisteredRemoved(simId)`.

**The prohibition.** Do not defer this call: not to a queue, not to a later frame, not to the engine object's end of play.

**The consequence.** og-sim-net's client calls `onRosterRemoved` while the character's channel still exists and destroys the
channel as soon as the call returns. The registered simulatable holds that channel as its net-sync owner, and unregistration
clears the callbacks bound on it, so an unregister that runs later runs against a destroyed channel. A host that does defer it
must keep the channel alive itself first (`SimNetClient-rationale.md` §5).

**An attached client** makes the call here, at once, inside the drain on the step's thread. og-sim-net has already unregistered a
queue-registered character before it (`SimNetClient-guards.md` G-12), so the Unreal session's handler runs no og-sim
unregistration; it posts the engine's own work to its frame thread. Since og-session-runtime task 12 every Unreal client registers
through that queue, so the destroyed-channel consequence above has no path left there. The call still stays here, at once: the
handler erases the id from the session's step-thread set, which the step's thread owns while the steps run.

**What breaks if it moves.** Nothing at compile time; at run time a use of a destroyed object on the first leave.

Was `UESimNetHost-guards.md` G-03 (og-session-runtime task 28 moved the call into the link's client host).

---

## G-04 — a latched floor is applied as received: it pays for an increase

**Tag site:** `SimNetLink.h`, `SimNetLinkClientHost::onRelayFloor`, the line above `m_session.onRelayFloor(floorTicks);`.

**The prohibition.** Do not route a replayed floor (`replayed == true`) anywhere else, and do not make the call depend on
`replayed`. The session hook takes the floor alone (`SimNetLinkSession` has no `replayed` parameter; a self-check refuses a
session whose `onRelayFloor` takes one).

**The consequence.** og-net delivers the floor on the server's attach, before the client's world exists, so every floor a client
session hands over at its attach is a replay. Applied as a replay it would never request a prediction stall, so the client's join
skips the stall a client pays when its floor arrives after the attach: the client's prediction lead at join changes, and Ph3's
identical row `[RelayDelayFloor] floor = 6 ticks, UPWARD` is absent from every client log.

**What breaks if it moves.** Nothing at compile time and no test; the gate's start-up comparison shows it.

Was `UESimNetHost-guards.md` G-05 (og-session-runtime task 28 moved the call into the link's client host).

---

## G-05 — the wire-format mismatch is reported with no session attached

**Tag site:** `SimNetLink.h`, `SimNetLinkClientHost::onWireMismatch`, the line above `m_services.onWireMismatch(received, expected);`.

**The prohibition.** Do not make this function return early, or do nothing, when no session is attached.

**The consequence.** og-sim-net's client calls `onWireMismatch` once, on the first correction whose version differs, latches the
mismatch and from then on ignores every correction without calling the host again. A mismatch that arrives before the world
attaches (a correction can arrive in the join window) would then never be reported: no Error line and no toast, only a client
whose corrections are all silently ignored.

**What breaks if it moves.** Nothing at compile time and no test.

Was `UESimNetHost-guards.md` G-04 (og-session-runtime task 28: the report's engine half, the line and the toast, stays in
`UESimNetLinkServices::onWireMismatch`; the unconditional call to it is the link's).

---

## R. Retired ids

None.
