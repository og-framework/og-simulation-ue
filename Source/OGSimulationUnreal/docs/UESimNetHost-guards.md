<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: UEConnectionHandle.h -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: IndexOfByKey -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: UNetConnection::Close -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: AssignConnectionHandle -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: UChildConnection::InitChildConnection -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: Engine/Private/NetConnection.cpp -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: EPingType::RoundTripExclFrame -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: UNetConnection::ReadPacketInfo -- Unreal Engine symbol or file, outside every scan root -->
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

**Tag site:** `UESimNetHost.h`, `UESimNetLinkServices::serverRttSeconds`, the line above `return kNoRttReadingSeconds;`.

**The prohibition.** Do not return `0.0`, or any other non-negative value, when the server connection's ping has no reading.

**The consequence.** The value is the round trip `onTimingInfoReceived` hands to the network time estimator, which rejects
and counts a negative sample. A `0.0` is taken as a real 0 ms round trip: once the estimator has a reading above 0 (a 0 does
not seed it, og-input-latency P5-28), every `0.0` pulls the smoothed RTT toward 0, and the prediction offset falls to its floor
while every log says it is tracking the network.

**What breaks if it moves.** Nothing at compile time and no test: the client host is only driven by a live server.

---

## G-06 — `GetPlayerSlotForActor` reads the actor's own connection, never the root

**Tag site:** `UESimNetHost.h`, `GetPlayerSlotForActor`, the line above `const UNetConnection* conn = actor->GetNetConnection();`.

**The prohibition, as it stood in the source** (`UEConnectionHandle.h`, moved here by og-input-latency task P5-9):

> NOTE: deliberately the actor's OWN connection, NOT the root. The root walk is what erases the distinction between local
> players; this function exists to recover it, so it must read the un-walked connection.

**The prohibition.** Do not read the root connection here: not `GetRootNetConnection(actor)`, and not a walk to the parent.

**The consequence.** A root connection is a parent connection, whose child connection id is 0. Every local player of a
split-screen client would get local index 0. The world session hands that index to og-sim-net's `registerCharacter`
(`UESimulationSession::tryRegisterOnRunner`, through the authority runner), and the roster refuses a second live character with
the same owner and local
index (`RosterAuthority::add`, `RosterAddRefusal::DuplicateOwnerSlot`): the second local player's character is refused
(`[Roster.Refused]`) and its registration abandons.

**What breaks if it moves.** Nothing at compile time and no test; only a split-screen client shows it.

---

## G-07 — the local index is the engine's child connection id, never an index into `Children`

**Tag site:** `UESimNetHost.h`, `GetPlayerSlotForActor`, the line above
`const uint32 childId = conn->GetConnectionHandle().GetChildConnectionId();`.

**The prohibition, as it stood in the source** (`UEConnectionHandle.h`, moved here by og-input-latency task P5-9):

> WHY NOT `Parent->Children.IndexOfByKey(child) + 1`, the obvious candidate: Epic explicitly warns against it in
> AssignConnectionHandle's own comment — "Don't assume the children remain on a particular index or that the array isn't
> ever shrinking." The child id is assigned ONCE at InitChildConnection and never reassigned, so it is stable for the
> connection's lifetime and therefore for the lifetime of any character on it; an array index is not contractually either.

**The prohibition.** Do not derive the index from the parent's `Children` array (`IndexOfByKey`, a position, a count).

**The consequence.** The array can shrink when another local player leaves, so a later player's position moves. The
character's roster entry keeps the index it was added with (`RosterAuthority::add` takes it once), so the index of a new
character could collide with a live one and be refused, or name the wrong local player.

**What breaks if it moves.** Nothing at compile time and no test.

*R0, og-input-latency task P5-9:* verified in UE 5.6: `UChildConnection::InitChildConnection` calls `AssignConnectionHandle`
once, and that function's comment is the one quoted (`Engine/Private/NetConnection.cpp`). The moved text's next sentence, that
the id "is also what the engine itself round-trips over the wire for child connections", was not verified (the engine reads it
locally, in `AssignConnectionHandle` and Iris's filtering), so it was not moved.

---

## G-08 — a child id above 255 saturates to 255, never wraps

**Tag site:** `UESimNetHost.h`, `GetPlayerSlotForActor`, the line above its last `return`.

**The prohibition, as it stood in the source** (`UEConnectionHandle.h`, moved here by og-input-latency task P5-9):

> Saturate rather than wrap. The caller range-checks against the uint8 substitution-mask bound
> (ConnectionSlotKey::kMaxPlayerSlot) and takes a documented fallback; a truncating cast could alias a large id onto a valid
> slot and reintroduce exactly the conflation T17 removes.

**The prohibition.** Do not replace the saturation with a plain `static_cast<uint8>(childId)` or a mask.

**The consequence.** A truncated id 256 + n is the valid index n of another local player on the same connection.

*R0, og-input-latency task P5-9:* the caller no longer range-checks. The world session passes the value to og-sim-net's
`addCharacter` as the roster entry's `localIndex`, and `RosterAuthority::add` `OG_CHECK`s it: an owned character needs an index
of 0..7 (`kRosterMaxLocalIndex`, asserted equal to `ConnectionSlotKey::kMaxPlayerSlot`). So 255 (`kRosterNotLocal`) for an owned
character fails that check instead of aliasing a valid index; a wrapped value would pass it.

**What breaks if it moves.** Nothing at compile time and no test.

---

## G-09 — the RTT is read from `EPingType::RoundTrip`; the "ExclFrame" type is not the fix it looks like

**Tag site:** `UESimNetHost.h`, `readRoundTripMs`, the line above `const UE::Net::FPingValues pingVals = netPing->GetPingValues(EPingType::RoundTrip);`.
The type check two statements above it (`EnumHasAnyFlags(netPing->GetPingTypes(), EPingType::RoundTrip)`) is its partner and
carries no tag: the two must name the same type.

**The prohibition, as it stood in the source** (`UEConnectionHandle.h`, moved here by og-input-latency task P5-9):

> READ THIS BEFORE "FIXING" THE PING TYPE. The enum names are INVERTED relative to what the engine feeds them. Verified against
> UE 5.6.1 source (unmodified in this tree) by og-netcode-v2-input-relay T21, whose entire stated purpose was to switch these
> reads to EPingType::RoundTripExclFrame and which was reversed by this finding.

**The prohibition.** Do not switch this read (or its type check) to `EPingType::RoundTripExclFrame` to exclude frame time.

**The consequence.** In `UNetConnection::ReadPacketInfo` the frame-time-subtracted value goes to `RoundTrip` and the raw,
frame-time-inclusive one to `RoundTripExclFrame`, and with `net.PingExcludeFrameTime` at its default 0 both receive the same
sample. On this server-side read the subtracted term is structurally zero anyway. So the switch excludes nothing and changes
only the averaging configured in `Config/DefaultEngine.ini` (`RoundTrip=PlayerStateAvg`, `RoundTripExclFrame=MovingAverage`):
a smoothing change with no evidence behind it. The derivation is in the rationale §11.5.

**What breaks if it moves.** Nothing at compile time and no test.

---

## G-10 — the server's RTT is the engine's raw reading, with nothing smoothed on top

**Tag site:** `UESimNetHost.h`, `readRoundTripMs`, the line above `outMs = pingVals.Current * 1000.0;` (a `return` of the
same expression until og-input-latency task P5-26 made the reading an out parameter; "the return" below is that write).

**The prohibition, as it stood in the source** (`UEConnectionHandle.h`, moved here by og-input-latency task P5-9):

> SINGLE-SMOOTHING (Option A, load-bearing — UNCHANGED by the T21 edit): this returns the engine's RAW RoundTrip value with NO
> NetworkTimeEstimator EMA on top; the tier table's own rttSmoothingAlpha EMA is the only smoothing in this path.
> Double-smoothing would reintroduce exactly the client/server asymmetry Option A removes. The T21 change added only a validity
> gate in front of the value; nothing was inserted between the read and the return.

**The prohibition.** Insert nothing between the read and the return: no estimator, EMA, clamp or offset. The only change the
value takes is seconds to milliseconds.

**The consequence.** The tier table smooths its input once (`rttSmoothingAlpha`). A second smoothing makes the server's tier
lag the client's view of the same round trip.

**What breaks if it moves.** Nothing at compile time and no test.

---

## G-11 — a closing connection is skipped before the read, with no warning and no wire sample

**Tag site:** `UESimNetHost.h`, `UESimNetServerHost::rttMs`, the line above `if (found.connection->IsClosingOrClosed())`.

**The prohibition.** Do not delete this check, and do not move it below `readRoundTripMs` or below the wire sample.

**The consequence.** In the frame of a graceful close `UNetConnection::Close` has set `USOCK_Closed` and reset the
connection's `FNetPing`, while `FindConnection` still finds it and og-sim-net can still drain the leaving client's input. Without
the check the read reports "no FNetPing on the connection - check net.NetPingEnabled": a false cause, which spends that cause's
one warning (rationale §2), so a later genuine loss of the ping on any connection is silent, and a −1 ms wire sample reaches the
latency probe on every graceful leave. Moved below the read, the warning fires; moved below the wire sample, the sample is taken.

**What breaks if it moves.** Nothing at compile time and no test: a graceful-leave smoke shows the warning return.

## G-12 — the server finds a connection through the host's lookup, never the driver's `FindConnection` directly

**Tag site:** `UESimNetHost.h`, `UESimNetServerHost::rttMs`, the line above
`const FServerConnectionFound found = m_connections.find(m_driver, conn, nowSeconds());`.

**The prohibition.** Do not replace `m_connections.find(m_driver, conn, nowSeconds())` with `m_driver->FindConnection(conn)`.

**The consequence.** In the frame of a graceful leave the driver can drop the leaving client's og-net id before og-sim-net
drains that client's last input bundle and asks `rttMs` for it: `UOGNetConnection::CleanUp` calls `UOGNetDriver::engineClosed`,
which removes the id, and og-net's own close removes it too. `FindConnection` then misses, and a bare miss reads as "no UE
connection for og-net id": a routine leave that spends the `NoConnection` cause's one warning (rationale §2), so a later
genuine no-connection outage on that server is silent. The lookup remembers each og-net id it found and reports a miss of a
remembered one with no cause (`missingCause` is `None`): no warning, and the sentinel is still returned (G-01). Calling the
driver directly skips the remembering as well as the check.

**What breaks if it moves.** Nothing at compile time. The automation test `OGBrawler.SimNet.Hosts` checks the lookup itself
(a graceful leave, then an unknown id, through one `FServerRttWarnings`), not `rttMs`'s use of it, so a `rttMs` that bypasses
the lookup still passes; a graceful-leave smoke shows the `no UE connection` warning return.

---

## G-13 — the loop's RTT publish asks the base host only from a connection's Join

**Tag site:** `UESimNetHost.h`, `UESimNetLanesServerHost::publishedRttMs_GameThread`, the line above
`if (connection.ClientLoginState == EClientLoginState::ReceivedJoin)`.

**The prohibition.** Do not call `Base::rttMs` (the base host's warning path) for a connection whose `ClientLoginState` is not
yet `ReceivedJoin`, and do not drop the gate to publish `Base::rttMs` for every connection. Before the Join read the round
trip without a warning.

**The consequence.** `publishRunnerRtt_GameThread` asks every frame from the frame og-net accepts a connection. Before the
Join UE may have sampled no ack, so the base's read answers `AccumulatorEmpty`, and its warning is one-shot per cause: the
server logs a false `[RttSample] SERVER tier RTT source unavailable (... accumulator is empty (no ack sampled yet))` line
before the first login, and a later real `AccumulatorEmpty` outage is silent for the rest of the session.

**What breaks if it moves.** Nothing at compile time, and the published values do not change. A dedicated server's log
carries the false warning shortly before its first `Login request`.

Was `UESimNetLanes-guards.md` G-04 (og-session-runtime task 28's step (a) moved the function unchanged from `UESimNetLanes.h`).

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-03 — RETIRED by og-session-runtime task 28 (2026-11-25): moved to `SimNetLink-guards.md` G-03 with the call (og-sim-net's
`SimNetLinkClientHost::onRosterRemoved`)

**Was:** *"a registered character is unregistered inside `onRosterRemoved`"*. Its tag sat in `UESimNetHost.h`'s client host's
`onRosterRemoved`, above the unregister handler's call.

### G-04 — RETIRED by og-session-runtime task 28 (2026-11-25): moved to `SimNetLink-guards.md` G-05 with the unconditional call
(og-sim-net's `SimNetLinkClientHost::onWireMismatch`); the line and the toast stay in `UESimNetLinkServices::onWireMismatch`

**Was:** *"the wire-format mismatch is reported with no session bound"*. Its tag sat in `UESimNetHost.h`'s client host's
`onWireMismatch`, above its `UE_LOG`.

### G-05 — RETIRED by og-session-runtime task 28 (2026-11-25): moved to `SimNetLink-guards.md` G-04 with the call (og-sim-net's
`SimNetLinkClientHost::onRelayFloor`)

**Was:** *"a latched floor is applied as received: it pays for an increase"*. Its tag sat in `UESimNetHost.h`'s client host's
`onRelayFloor`, above the session's floor call.
