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
Every other rule of the header is enforced by a `static_assert` or an `OG_CHECK`, whose messages name what they check.

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

## §R — retired ids

None.
