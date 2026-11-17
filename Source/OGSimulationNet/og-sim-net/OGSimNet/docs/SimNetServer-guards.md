<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetServer.h` — guards

Every prohibition that governs a line of `SimNetServer.h`. Each entry has an **opaque, stable id**. In the header a single
line `// ⛔G-nn  docs/SimNetServer-guards.md` sits exactly where the forbidden edit would be typed.

**If this file and `SimNetServer.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to §R and its number is
spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn` **tag**.

⛔ **Nothing in this file is a rationale.** The narrative lives in `SimNetServer-rationale.md`. A guard is a prohibition plus
the consequence of ignoring it.

---

## G-01 — a bundle the roster does not accept goes no further

- **Site:** the `if (verdict != RosterInputVerdict::Accepted)` in `onBundle`.
- **Prohibition:** do not delete the check, and do not widen what passes it (for example, accept `UnknownSim`, or accept a
  bundle whose sender is not the character's owner).
- **Consequence:** any connection could drive any character. Its bundles would be stamped, sampled into the tier of the wire
  that sent them, parked under the victim's sim id and relayed to every other connection as that character's input. The
  park key is the sender's wire and the victim's local index, so it can also replace the claim of the sender's own character
  that has the same local index.
- **What breaks if it moves:** it must run before the stamps, the RTT sample and `receiveInputBundle`. Each of them changes
  state for the sim id it is given.

## G-02 — one RTT sample per character bundle, never per slot

- **Site:** the `noteRttSample` call in `receiveBundle`.
- **Prohibition:** do not move the call into the per-slot stamp walk above it, and do not call it once per slot anywhere else.
- **Consequence:** the tier table counts samples, not time: each sample adds to the smoothed RTT and to the dwell count. A
  bundle of three slots would count three times, so a wire's tier would change after a third of the dwell and its smoothing
  would depend on the redundancy depth. That is the coordinator's own "once per bundle, never per slot" rule; the reception
  path the session replaces samples once per bundle.

## G-03 — `remove` and `removeFor` are one statement

- **Site:** the `return` in `takeRemove`.
- **Prohibition:** do not split the two calls, and do not put another roster call, a send or a drain between them.
- **Consequence:** between `remove` and `removeFor` the roster holds a pending Remove. A `snapshotFor` in that window fails
  the roster's check (a new connection would get a Snapshot whose revision already counts a Remove it never receives), and an
  `addFor` sends an Add whose revision is newer than the Remove that follows it, which every client refuses as a revision
  gap and disconnects on.

## G-04 — a character's relay skips its owner

- **Site:** the `m_transport.relay(character.owner, …)` call in `flushRelays`.
- **Prohibition:** do not pass anything but the character's owner as the connection to skip.
- **Consequence:** with `kInvalidSimConnection` (or another id) the owner receives the relay of its own input every frame.
  The relay this replaces is never sent to its owner (the UE relay actor replicates its ring with `COND_SkipOwner`), so no
  client path has ever received its own character's ring; the owner's SimRelay budget pays for it either way.

## G-05 — the flush reads a correction through the const accessor

- **Site:** the `std::as_const(channel).getSyncedCorrectionStateBuffer()` read in `flushCorrections`.
- **Prohibition:** do not read the buffer through the non-const accessor (drop the `std::as_const`).
- **Consequence:** the non-const accessor marks the channel dirty; that is how a NetSync write becomes a send. A flush that
  used it would re-mark every correction it just sent, so each one would be sent again on every later frame, to every
  connection, until the character leaves. Nothing fails: the clients drop the repeats as stale, and only the SimState budget
  shows it.

## G-06 — the return after the selection check stays

- **Site:** the `if (count >= indices.size() || …)` return in `CorrectionSelection::select`, after its `OG_CHECK`.
- **Prohibition:** do not delete the return.
- **Consequence:** it runs only where `OG_CHECK` is compiled out: a Shipping build, and the standalone build with `NDEBUG`,
  where `OG_CHECK` is `assert`. There a policy that selects more than `kMaxCorrectionCandidates` times would write past the
  end of `indices`. Where the check is live it stops the program, or ends the case in a test runner that reports a failed
  check as a failed case, so no test can reach the return and its deletion passes every test.

## G-07 — the return after the removal check stays

- **Site:** the `if (found == m_characters.end())` return in `removeCharacter`, after its `OG_CHECK`.
- **Prohibition:** do not delete the return.
- **Consequence:** as G-06: only a build without live checks reaches it, and there `removeCharacter` of a sim id with no
  character would dereference the end iterator of the character map. No test can reach it.

## G-08 — the skip after the selected-index check stays

- **Site:** the `if (index >= count)` `continue` in `flushCorrections`, after its `OG_CHECK`.
- **Prohibition:** do not delete the `continue`.
- **Consequence:** as G-06: only a build without live checks reaches it, and there a policy that selects an index past this
  frame's candidates would read a candidate slot that holds no correction. The envelope writer refuses its sim id 0 and
  returns size 0, so og-net would be handed an empty SimState message, and a send for sim id 0 would be recorded. No test
  can reach it.

## G-09 — the one-shape check folds over a variable template

- **Site:** the `static_assert((kSimHasShape<SimTs, Shape> && ...), …)` in the class.
- **Prohibition:** do not write the comparison inline in the fold (for example
  `(std::is_same_v<simEnvelope::ShapeFor<typename SimTs::InputType, typename SimTs::StateType>, Shape> && ...)`), and do not
  spell the alias out inside it either.
- **Consequence:** MSVC 19.38 (the UBT toolchain 14.38.33130) passes both inline forms for a pack whose shapes differ, so a
  game with two input sizes compiles and every input set from it is walked with the wrong stride for one of its types. The
  same fold over a variable template of one simulatable is evaluated, and fails. This was measured: the inline form was the
  first version of this assert, and its poison arm (a game with a 5 B and a 12 B input) compiled.

---

## §R Retired ids

None.
