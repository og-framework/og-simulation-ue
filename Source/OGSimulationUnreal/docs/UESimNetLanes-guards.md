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

## R. Retired ids

None.
