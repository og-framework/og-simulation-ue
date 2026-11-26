<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: UEConnectionHandle.h -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: IndexOfByKey -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: AssignConnectionHandle -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: UChildConnection::InitChildConnection -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: Engine/Private/NetConnection.cpp -- Unreal Engine symbol or file, outside every scan root -->
<!-- lint-external-ref: readRoundTripMs -- the server's engine-ping read, deleted by og-session-runtime task 39 (og-sim-net's SimNetServerHostCore::rttMs reads og-net's stats); named in this file's retired entries -->
<!-- lint-external-ref: kNoRttReadingMs -- the server's ms sentinel, og-sim-net's kSimNetServerNoRttMs since og-session-runtime task 39; named in this file's retired entries -->
<!-- lint-external-ref: publishedRttMs_GameThread -- the per-connection reader of the runner's RTT publish, deleted by og-session-runtime task 39; named in this file's retired entries -->
# `UESimNetHost.h` — guards

Every prohibition that governs a line of `UESimNetHost.h`. Each entry has an **opaque, stable id**. In the source a single
line `// ⛔G-nn  docs/UESimNetHost-guards.md` sits directly above the statement where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not soften
the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `UESimNetHost-rationale.md`.

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

### G-01 — RETIRED by og-session-runtime task 39 (2026-11-26): moved to `SimNetServerHost-guards.md` G-03 with the read

**Was:** *"the server's "no connection" RTT is the negative sentinel, never 0"*. Its tag sat in `UESimNetServerHost::rttMs`,
above `return kNoRttReadingMs;` for an og-net id with no Unreal connection. The authority's round trip is og-sim-net's
`SimNetServerHostCore::rttMs` since, from og-net's stats, and its no-reading return is og-sim-net's G-03.

### G-09 — RETIRED by og-session-runtime task 39 (2026-11-26): the server's engine-ping read is deleted

**Was:** *"the RTT is read from `EPingType::RoundTrip`; the "ExclFrame" type is not the fix it looks like"*. Its tag sat in
`readRoundTripMs`, above its `GetPingValues(EPingType::RoundTrip)`. The derivation still holds for the client's engine-ping shadow
figure, `UESimNetLinkServices::readServerRoundTrip`, which reads the same type for the same reason, untagged
(`UESimNetHost-rationale.md` §11.5).

### G-10 — RETIRED by og-session-runtime task 39 (2026-11-26): moved to `SimNetServerHost-guards.md` G-04 with the read

**Was:** *"the server's RTT is the engine's raw reading, with nothing smoothed on top"*. Its tag sat in `readRoundTripMs`, above
its write of the ping's current value times 1000 into the out parameter. The rule (nothing between the source's reading and the return but the unit) is og-sim-net's
G-04 over og-net's smoothed round trip.

### G-11 — RETIRED by og-session-runtime task 39 (2026-11-26): its premise, the engine ping of a closing connection, is gone

**Was:** *"a closing connection is skipped before the read, with no warning and no wire sample"*. Its tag sat in
`UESimNetServerHost::rttMs`, above `if (found.connection->IsClosingOrClosed())`. og-net's stats of a closing connection still
carry its last smoothed round trip, a real reading; there is no engine ping to lose and no cause to warn.

### G-12 — RETIRED by og-session-runtime task 39 (2026-11-26): its premise, the driver's connection lookup, is gone

**Was:** *"the server finds a connection through the host's lookup, never the driver's `FindConnection` directly"*. Its tag sat in
`UESimNetServerHost::rttMs`, above its `m_connections.find(m_driver, conn, nowSeconds())`. The round trip is og-net's stats of the
og-net id; an id og-net no longer publishes reads the sentinel with no warning.

### G-13 — RETIRED by og-session-runtime task 39 (2026-11-26): the runner's RTT publish is deleted

**Was:** *"the loop's RTT publish asks the base host only from a connection's Join"*. Its tag sat in the port's
`publishedRttMs_GameThread`, above `if (connection.ClientLoginState == EClientLoginState::ReceivedJoin)`. Was
`UESimNetLanes-guards.md` G-04 before og-session-runtime task 28. The runner reads og-net's stats itself; nothing is published.

