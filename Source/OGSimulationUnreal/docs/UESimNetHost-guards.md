<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: onRelayDelayFloorReplayed -- deleted by og-input-latency task P5-9 (the session's two replay members); kept in the history -->
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

**Tag site:** `UESimNetHost.h`, `UESimNetClientHost::serverRttSeconds`, the line above `return kNoRttReadingSeconds;`.

**The prohibition.** Do not return `0.0`, or any other non-negative value, when the server connection's ping has no reading.

**The consequence.** The value is the round trip `onTimingInfoReceived` hands to the network time estimator, which rejects
and counts a negative sample. A `0.0` is taken as a real 0 ms round trip: once the estimator has a reading above 0 (a 0 does
not seed it, og-input-latency P5-28), every `0.0` pulls the smoothed RTT toward 0, and the prediction offset falls to its floor
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

**A client** (og-input-latency task P5-17) makes the call here, at once, inside the drain on the step's thread.
og-sim-net has already unregistered a queue-registered character before it (`SimNetClient-guards.md` G-12), so the session's
handler runs no og-sim unregistration; it posts the host's own game-thread work to the game thread
(`UESimulationSession-rationale.md` §18.5). Since og-session-runtime task 12 every client registers through that queue (until
then a frame-locked client's handler unregistered against the channel here, which a deferral broke).
So the destroyed-channel consequence above has no path left. The call still stays here, at once: the handler erases the id from
the session's step-thread set (`m_systemsNotified_Step`), which the step's thread owns while the steps run.

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

## G-05 — a latched floor is applied as received: it pays for an increase

**Tag site:** `SimNetLink.h`, `UESimNetLanesClientHost::onRelayFloor`, the line above `m_session->onRelayDelayFloorReceived(floorTicks);`.

**The prohibition.** Do not route a replayed floor (`replayed == true`) to the session's `onRelayDelayFloorReplayed`, and do not
make the call depend on `replayed`.

**The consequence.** og-net delivers the floor on the server's attach, before the client's world exists, so every floor a client
session hands over at its bind is a replay. Applied as a replay it never requests a prediction stall, so the client's join
skips the stall today's client pays when its floor arrives after the bind (the rationale §6): the client's prediction lead at
join changes, and Ph3's identical row `[RelayDelayFloor] floor = 6 ticks, UPWARD` is absent from every client log.

**What breaks if it moves.** Nothing at compile time and no test; the gate's start-up comparison shows it.

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

## R. Retired ids

None.
