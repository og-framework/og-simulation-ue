<!-- SPDX-License-Identifier: MPL-2.0 -->
# `EngineLaneDriverCore.h` — guards

Every prohibition that governs a line of `EngineLaneDriverCore.h`. Each entry has an **opaque, stable id**. In the source a single
line `// ⛔G-nn` sits exactly where the forbidden edit would be typed.

**If this file and the sources disagree, the sources are authoritative and this file is stale.** Fix this file; do not soften the
sources to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to a "Retired" section and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `EngineLaneDriverCore-rationale.md`.

---

## G-01 — the poll callback only copies; nothing is dispatched from inside `poll`

**Tag site:** `EngineLaneDriverCore.h`, the line above `self.copyIntoBatch(event);` in `onPolled`.

**The prohibition.** Do not call `dispatch`, a callback, or anything that reaches engine code from the poll callback. Copy the
event into the batch there, and dispatch after `poll` has returned.

**The consequence.** Engine code then runs inside og-net's `poll`. A `deliver` that stops the core does not stop the poll: the rest
of the batch and the stop's own `Disconnected` are still delivered, after the stop. Engine code that polls again aborts on og-net's
check (`EngineLane::poll called from its own callback`), and the event's bytes are wiped as soon as the callback returns.

**What breaks if it moves.** `EngineLaneDriver.ADeliverThatStopsTheCoreRunsNoFurtherCallback` fails: 5 callbacks after the stop,
not 0, and 5 `deliver`s, not 1 (P4-3's arm D).

## G-02 — `engineClosed` schedules the disconnect; it never disconnects at once

**Tag site:** `EngineLaneDriverCore.h`, the line above `record->state = RecordState::Draining;` in `engineClosed`.

**The prohibition.** Do not replace the draining mark with og-net's `disconnect(id)`, and do not add a `disconnect` call to
`engineClosed`. The disconnect belongs to `serviceDrainedCloses`, once `queuedBytes(id)` is 0 or 0.2 s have passed.

**The consequence.** The engine's last packet on the connection (its close reason, sent just before it cleans the connection up)
is still queued when og-net starts closing; the closing connection seals its Close frame first, and the peer never delivers the
packet. The peer's engine sees the connection drop without the reason.

**What breaks if it moves.** `EngineLaneDriver.ALastPacketSentWithEngineClosedReachesThePeerBeforeItsClose` fails: the client
delivers 0 packets before its `closed`, not 1 (P4-3's arm B).

## G-03 — the pre-connect buffer is sent oldest first

**Tag site:** `EngineLaneDriverCore.h`, the line above the `for` loop in `flushPreConnect`.

**The prohibition.** Do not reverse, reorder or skip the buffered packets when the server connection comes up: send slot 0 first,
then each later slot in turn, and only then call `serverConnectionUp`.

**The consequence.** The client's first engine packets, its join packet among them, reach the server's engine in a different order
than the client's engine sent them, at the very start of the connection, where the engine lane otherwise keeps a connection's
packets in send order on an in-order path.

**What breaks if it moves.** `EngineLaneDriver.PacketsSentBeforeTheServerConnectionIsUpArriveFirstInOrderAndByteExact` fails: the
server's first delivered packet is the third one sent (P4-3's arm A).

## G-04 — after every dispatched event, the batch loop stops if the core has stopped

**Tag site:** `EngineLaneDriverCore.h`, the line above `if (!isRunning())` inside the batch loop of `pollRing`.

**The prohibition.** Do not delete this check, and do not move it after the loop: it must run after each event.

**The consequence.** A callback that stops the core (the engine shutting the driver down from inside a packet) is followed by more
callbacks from the same batch, into an engine that has already torn the driver down.

**What breaks if it moves.** `EngineLaneDriver.ADeliverThatStopsTheCoreRunsNoFurtherCallback` fails: 4 callbacks after the stop,
not 0 (P4-3's arm E).

## G-05 — the connection handler forwards every event, unfiltered

**Tag site:** `EngineLaneDriverCore.h`, the line above `if (self.m_forward.fn != nullptr)` in `onConnectionEvent`.

**The prohibition.** Do not add a condition on the event's kind or id to this `if`: every connection event goes to the forward
target, and the core's own id-0 filter comes after it.

**The consequence.** The sim transport behind the forward never sees the events the condition drops, and neither does the session
that drains its inbox. Without `Connected`, og-sim-net's server never attaches the connection, so it never sends it the roster or
the relay floor (`SimNetServer.h`, `attach`). Without `Disconnected`, the server keeps the gone connection attached, and the
transport never frees its connection slot (`SimNetTransport.h`, `onDisconnected`), so a later connection can find every slot taken.

**What breaks if it moves.** `EngineLaneDriver.EveryConnectionEventIsForwardedAndConnectedPrecedesItsConnectionsFirstMessage` fails
(P5-3's arm (b): forwarding only `Disconnected`).
