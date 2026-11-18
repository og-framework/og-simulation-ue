<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetRoster.h` — guards

Every prohibition that governs a line of `SimNetRoster.h`. Each entry has an **opaque, stable id**. In the header a single
line `// ⛔G-nn  docs/SimNetRoster-guards.md` sits exactly where the forbidden edit would be typed.

**If this file and `SimNetRoster.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to §R and its number is
spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn` **tag**.

⛔ **Nothing in this file is a rationale.** The narrative lives in `SimNetRoster-rationale.md`. A guard is a prohibition plus
the consequence of ignoring it.

**Provenance.** Written with the header on 2026-11-15. The roster is new code, so no entry was carried from an older comment.
Every other rule of the header is enforced by a `static_assert` or an `OG_CHECK`, whose messages name what they check, or
(from og-input-latency task P5-23, G-02) by a refusal that `RosterAuthority::add` returns.

---

## G-01 — a recipient sees a local index only for the characters it owns

**Tag site:** `SimNetRoster.h`, in `RosterAuthority::entryFor`, immediately above its `return`.

**The prohibition.** Do not write the record's `localIndex` into the entry unconditionally, and do not widen the condition
beyond "the record's owner is this recipient". Every other recipient, and every recipient of an authority-local character
(no owner), gets `kRosterNotLocal` (0xFF).

**Consequence.** A client takes the local or remote role of each character from its roster entry: an entry with a local index
is one of its own players. With the owner's local index in every recipient's view, every client reads the other clients'
characters as its own local players, and predicts and drives them as such. The owner connection itself is never sent, so
this per-recipient choice is the only place the wire says whose character it is.

**What breaks if it moves.** `snapshotFor` and `addFor` both build their entries through this function, and `viewFor` is the
view tests and sessions compare a mirror against. A second copy of the rule elsewhere would let the Snapshot and the Add of one
character disagree for the same recipient.

---

## G-02 — `add` refuses a full roster or a live (owner, localIndex) before it changes anything

**Tag site:** `SimNetRoster.h`, in `RosterAuthority::add`, immediately above `if (refusal != RosterAddRefusal::None)`.

**The prohibition.** Do not delete this return, do not replace it (or the `Full` and `DuplicateOwnerSlot` assignments above it)
with an `OG_CHECK`, and do not move it below `m_everAdded.set`, `++m_revision` or the write through `freeSlot`.

**Consequence.** These two refusals were `OG_CHECK`s until task P5-23. Where `OG_CHECK` is compiled out (Shipping, and the
standalone build with `NDEBUG`), a ninth character wrote through a null `freeSlot`, and a second live `(owner, localIndex)` was
added silently, so one local player of a connection owned two sim ids. Where the checks are live, a host's mistake stopped the
program instead of being refused. Below the writes, a refused add would spend the sim id (a later add of it then fails the
never-reused check) and raise the revision with no message to carry it, so every attached client refuses the next Add or
Remove as a `RevisionGap`.

**What breaks if it moves.** `SimNetRoster.AnAddPastCapacityIsRefusedAsFullAndChangesNothing` and
`SimNetRoster.ASecondLiveOwnerAndLocalIndexIsRefusedAndChangesNothing` fail: with checks compiled out the first crashes in the
null write and the second sees `None` and an extra live entry; with checks live both fail in the `OG_CHECK`.
`SimNetServer::addCharacter` returns `nullptr` on these refusals (`SimNetServer-guards.md` G-11).

---

## §R — retired ids

None.
