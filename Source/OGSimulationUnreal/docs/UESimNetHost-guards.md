<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESimNetHost.h` — guards

Every prohibition that governs a line of `UESimNetHost.h`. Each entry has an **opaque, stable id**. In the source a single
line `// ⛔G-nn  docs/UESimNetHost-guards.md` sits directly above the statement where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not soften
the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `UESimNetHost-rationale.md`.

---

## G-01 — the server's "no connection" RTT is the negative sentinel, never 0

**Tag site:** `UESimNetHost.h`, `UESimNetServerHost::rttMs`, the line above `return kNoRttReadingMs;`.

**The prohibition.** Do not return `0.0`, or any other non-negative value, when the og-net id has no Unreal connection.

**The consequence.** og-sim-net passes `rttMs` straight into the reception coordinator's `noteRttSample`, which skips a
negative reading and publishes no tier. A `0.0` is a perfect 0 ms ping to it: the connection is put on the lowest tier and
its input delay derived from a reading that does not exist. That is the silent-zero failure `readRoundTripMs` was changed to
end (rationale §2).

**What breaks if it moves.** The automation test `OGBrawler.SimNet.Hosts` fails its sentinel case.

---

## G-02 — the client's "no reading" RTT is the negative sentinel, never 0

**Tag site:** `UESimNetHost.h`, `UESimNetClientHost::serverRttSeconds`, the line above `return kNoRttReadingSeconds;`.

**The prohibition.** Do not return `0.0`, or any other non-negative value, when the server connection's ping has no reading.

**The consequence.** The value is the round trip `onTimingInfoReceived` hands to the network time estimator, which rejects
and counts a negative sample. A `0.0` is latched as a real 0 ms round trip, and the prediction offset then runs on its floor
while every log says it is tracking the network.

**What breaks if it moves.** Nothing at compile time and no test: the client host is only driven by a live server.

---

## G-03 — a registered character is unregistered inside `onRosterRemoved`

**Tag site:** `UESimNetHost.h`, `UESimNetClientHost::onRosterRemoved`, the line above `m_onRegisteredRemoved(simId)`.

**The prohibition.** Do not defer this call: not to a queue, not to a later frame, not to the pawn's EndPlay.

**The consequence.** og-sim-net's client calls `onRosterRemoved` while the character's channel still exists and destroys the
channel as soon as the call returns. The registered simulatable holds that channel as its net-sync owner, and
unregistration clears the callbacks bound on it, so an unregister that runs later runs against a destroyed channel. A host
that does defer it must keep the channel alive itself first (og-sim-net's `SimNetClient-rationale.md` §5).

**What breaks if it moves.** Nothing at compile time; at run time a use of a destroyed object on the first leave.

---

## G-04 — the wire-format mismatch is reported with no session bound

**Tag site:** `UESimNetHost.h`, `UESimNetClientHost::onWireMismatch`, the line above its `UE_LOG`.

**The prohibition.** Do not make this function return early, or do nothing, when no session is bound.

**The consequence.** og-sim-net's client calls `onWireMismatch` once, on the first correction whose version differs, latches
the mismatch and from then on ignores every correction without calling the host again. A mismatch that arrives before the
world binds (a correction can arrive in the join window) would then never be reported: no Error line and no toast, only a
client whose corrections are all silently ignored.

**What breaks if it moves.** Nothing at compile time and no test.

---

## R. Retired ids

None.
