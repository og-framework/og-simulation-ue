<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetTransport.h` — guards

Every prohibition that governs a line of `SimNetTransport.h`. Each entry has an **opaque, stable id**. In the header a single
line `// ⛔G-nn  docs/SimNetTransport-guards.md` sits exactly where the forbidden edit would be typed.

**If this file and `SimNetTransport.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to §R and its number is
spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn` **tag**.

⛔ **Nothing in this file is a rationale.** The narrative lives in `SimNetTransport-rationale.md`. A guard is a prohibition plus
the consequence of ignoring it.

---

## G-01 — the lane handler only copies into the inbox

- **Site:** the `receive` call in `onLaneMessage`.
- **Prohibition:** do not decode, dispatch or answer a message in the lane handler or in `receive`, and do not call a session
  from either. They find the connection's slot, refuse a message from a connection with no slot, one that is closing or one
  left unannounced, push the message into the inbox with its slot, ask the endpoint to disconnect a connection over its
  Control quota, and return.
- **Consequence:** the handler runs on og-net's servicing thread. Every reader of sim state (the roster, the channels, the
  reception coordinator) runs on the session's thread, which drains the inbox. Work done here races them.
- **What breaks if it moves:** nothing may move into it. A Ph4 handler that must answer on the servicing thread (the clock
  pong) gets its own handler for its lane, not code in this one.

## G-02 — `installHandlers` never sets the endpoint's connection handler

- **Site:** the line after the lane-handler loop in `installHandlers` (an absence tag).
- **Prohibition:** do not add a `setConnectionHandler` call to `installHandlers`.
- **Consequence:** an endpoint has one connection handler. In the game, the engine-lane driver core sets it when it starts the
  endpoint, and needs it: it reads the client's failed-connect `Disconnected` from it. Setting the transport's thunk instead
  replaces the driver's handler, so the driver never learns that a connect failed. The host chains `onConnectionEvent` from
  its own handler, or installs `connectionHandler()` when it has none.

## G-04 — the return after the endpoint check stays

- **Site:** the `if (m_endpoint == nullptr)` return in `dispatch`, after its `OG_CHECK`.
- **Prohibition:** do not delete the return.
- **Consequence:** it runs only where `OG_CHECK` is compiled out: a Shipping build, and the standalone build with `NDEBUG`,
  where `OG_CHECK` is `assert`. There a send before `installHandlers` would dereference a null endpoint. Where the check is
  live, it stops the program, or ends the case in a test runner that reports a failed check as a failed case, so no test can
  reach the return and its deletion passes every test.

---

## G-06 — the fallback after the slot check of `Connected` stays

- **Site:** the `if (!slotted)` after the `OG_CHECK` in `onConnected`.
- **Prohibition:** do not delete the `if` and its disconnect as dead code behind the check.
- **Consequence:** it runs only where `OG_CHECK` is compiled out (a Shipping build, and the standalone build with `NDEBUG`).
  There a connection that found every slot taken (a transport whose config was not built from its endpoint's) would stay
  connected while `receive` refuses all its messages, as `unslottedRefused`: a peer that is connected and never heard. The
  disconnect closes it instead, counted in `noSlotConnects`. Where the check is live it stops the program, or ends the case in
  a test runner that reports a failed check as a failed case, so no test reaches the fallback and its deletion passes every
  test.

## G-07 — the fallback after the input-set owner check stays

- **Site:** the `if (!owned)` after the `OG_CHECK` in `withInputSet`, the one door of `appendInputBundle` and `endInputSends`.
- **Prohibition:** do not delete the `if` and its return as dead code behind the check, and do not route an append or a set end
  around `withInputSet`.
- **Consequence:** it runs only where `OG_CHECK` is compiled out (a Shipping build, and the standalone build with `NDEBUG`).
  There the transport's input set is written by the thread that opened it, the client's step, while a bundle or a set end
  from another thread, or with no set open, would write the same buffer and length at the same time: a torn set on the wire,
  or a bundle appended into the middle of the step's set. The return drops
  that call instead, counted in `unwindowedRefused`. Where the check is live it stops the program, or ends the case in a test
  runner that reports a failed check as a failed case, so no test reaches the fallback and its deletion passes every test.

## §R Retired ids

- **G-03** (2026-11-24) forbade deleting the frame hold's release in `submit`'s overflow branch, or moving it after the
  overflowing send: that send would have left ahead of every send held earlier in the same frame, reordering the roster on
  Control. Retired with its site: og-session-runtime task 15 deleted the hold arena, its release and its flush, so every send
  leaves at once, in call order (rationale §7). Successor: none: removed by og-session-runtime task 15.
- **G-05** (2026-11-15) forbade giving a freed slot to a new connection before its tag held no undrained message. The rule was
  replaced by its opposite: a slot is taken again at once, and the residue counts against the new connection (rationale
  §10). The wait it guarded let a connection that found no free slot push untagged, outside every quota, which is how
  Control could bypass the Control ring's bound. No test or site needs it; a test pins the new rule
  (`SimNetTransport.AClosedConnectionsUndrainedMessagesCountAgainstItsSlotWhichANewConnectionTakesAtOnce`).
