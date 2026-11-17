<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetChannel.h` — guards

Every prohibition that governs `ogsimnet::SimNetChannel`, the per-character owner adapter in `SimNetChannel.h`. Each
entry has an **opaque, stable id**. A single line `// ⛔G-nn  docs/SimNetChannel-guards.md` sits exactly where the
forbidden edit would be typed.

**If this file and `SimNetChannel.h` disagree, the header is authoritative and this file is stale.** Fix this file; do
not soften the header to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to §R and its
number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn` **tag**.

⛔ **Nothing in this file is a rationale.** The narrative lives in `SimNetChannel-rationale.md`. A guard is a
prohibition plus the consequence of ignoring it.

⚠ **og-sim-net ships under MPL-2.0, engine-free.** Every guard below is true and actionable to a reader with no game
engine and no other repository open.

---

## G-01 — the mutable correction accessor marks the channel dirty

**Tag site:** `SimNetChannel.h`, the non-const `getSyncedCorrectionStateBuffer()`, immediately above
`m_dirty = true;`.

**The prohibition.** Do not delete the mark. Do not move it to the const overload, into `storeCorrection`, or after
the `return`. Do not replace it with a check of the buffer's contents.

**Consequence.** `SimulationNetSync::sendCorrectionAll` publishes a correction by writing through this accessor, and
that write is the only sign a correction was published. The session's flush sends the corrections of the channels
whose `takeDirty()` is true. Without the mark, `isDirty()` never turns true, and no correction is ever sent. Nothing
fails: the server keeps writing its buffers, and every client keeps predicting with no correction.

**What breaks if it moves.** `SimNetChannel.ANetSyncScriptGivesTheMockOwnersSequence` fails at the first correction
the mock owners record (`wire write id=7 tick=100 ...`), and `SimNetChannel.OnlyTheMutableCorrectionAccessorMarksDirty`
fails on `isDirty()` and `takeDirty()`.

## G-02 — an inbound correction is assigned to the member, never through the mutable accessor

**Tag site:** `SimNetChannel.h`, `storeCorrection`, immediately above
`if (!m_correctionState.assignFromWire(wire))`.

**The prohibition.** Do not write the received bytes through `getSyncedCorrectionStateBuffer()`, and do not mark the
channel dirty anywhere in `storeCorrection`.

**Consequence.** The mutable accessor marks the channel dirty (G-01). A received correction would then report itself
as one this process published, and any flush that reads `takeDirty()` on that channel sends the received bytes back
out as a correction of its own.

**What breaks if it moves.** `SimNetChannel.OnlyTheMutableCorrectionAccessorMarksDirty` fails on the receiver's
`CHECK_FALSE(receiver.isDirty())`, and `SimNetChannel.InboundDoorsAssignThenCallAndRefuseOversize` fails on
`CHECK_FALSE(channel.isDirty())` after a refused correction: the accessor marks before the refusal.

## G-03 — setting the relay callback does not replay the current ring

**Tag site:** `SimNetChannel.h`, `setOnRelayedInputReceivedCallback`, the blank line under
`m_onRelayedInputReceived = std::move(fn);` (an absence tag).

**The prohibition.** Do not add a call of the new callback with the channel's current ring when it is set.

**Consequence.** `SimulationNetSync`'s prediction registration sets this callback and then reads the ring itself,
once, through the same by-id ingest. A replay on set would run the arrival path for that bind-time ring as well:
`onRelayedInputReceived`, which feeds the relay arrival probe a sample that is not an arrival. NetSync keeps its own
bind-time read out of that probe on purpose. The ring would also be ingested twice at every registration.

**What breaks if it moves.** `SimNetChannel.ANetSyncScriptGivesTheMockOwnersSequence` fails: the channel trace gains a
`[RelayProbe.Ingest] id=9 newest=99 new=3` line at registration, its first line, that the mock owners do not produce. And
`SimNetChannel.InboundDoorsAssignThenCallAndRefuseOversize` fails on `CHECK(rings.empty())`.

## G-04 — the send narrows the redundancy depth to a byte before it clamps it

**Tag site:** `SimNetChannel.h`, `sendLocalInputToAuthority`, immediately above
`const std::uint8_t requested = static_cast<std::uint8_t>(redundancyDepth);`.

**The prohibition.** Do not clamp `redundancyDepth` to `InputBundleBuffer::kMaxSlots` before the narrowing, and do not
widen `requested`. The order is narrow first, then clamp.

**Consequence.** The order is the one the Unreal owner's send uses: it narrows the depth to `uint8` and hands it to its
bundle builder, which clamps to `kMaxSlots`. The bundle bytes are the equivalence claim the channel rests on. With the
clamp first, a depth of 256 or more sends `kMaxSlots` slots where today's owner sends `depth mod 256`, clamped. No
shipped configuration reaches 256, so the difference would appear only in a configuration nobody has run.

**What breaks if it moves.** `SimNetChannel.BrawlerBundleBytesEqualBuildRedundancyBundle` (og-brawler-tests) fails at
depths 256 and 259.

---

## §R Retired ids

None.
