<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGNetConnection.h` and `.cpp` — guards

Every prohibition that governs a line of `OGNetConnection.h` or `OGNetConnection.cpp`. Each entry has an **opaque, stable
id**. In the source a single line `// ⛔G-nn` sits exactly where the forbidden edit would be typed.

**If this file and the sources disagree, the sources are authoritative and this file is stale.** Fix this file; do not soften the
sources to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to a "Retired" section and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `OGNetConnection-rationale.md`.

---

## G-01 — `InitHandler` creates no packet handler and never calls the base

**Tag site:** `OGNetConnection.cpp`, inside the empty body of `UOGNetConnection::InitHandler`: an absence tag, with the blank
line under it where the call would be typed.

**The prohibition.** Do not call the base class's InitHandler here, and do not create a `PacketHandler` or add a handler component in
this override. The connection's `Handler` stays null in every configuration, on both roles.

**The consequence.** The base creates a packet handler with the stateless connect component on every connection (outside
Shipping unless `-NoPacketHandler` is given). The connection's `LowLevelSend` hands packets to og-net without passing them
through the handler, while the engine's ReceivedRawPacket passes every received packet through the handler's Incoming once
a handler exists (NetConnection.cpp, lines 2030-2036), and the driver never runs the component's connectionless challenge.
The two directions then disagree about the packet format, and the second admission layer the design removed (og-net's
cookie already admits the connection) comes back.

**What breaks if it moves.** No test in this task fails. The driver's `[OGNet.Driver] up` line reports
`handler=on` instead of `handler=off` (it reads the connection's `Handler`), which the on-game smokes check on every
connection.
