<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetBindings.h` — guards

Every prohibition that governs a line of `SimNetBindings.h`. Each entry has an **opaque, stable id**. In the header a single
line `// ⛔G-nn  docs/SimNetBindings-guards.md` sits exactly where the forbidden edit would be typed.

**If this file and `SimNetBindings.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to §R and its number is
spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn` **tag**.

⛔ **Nothing in this file is a rationale.** The narrative lives in `SimNetBindings-rationale.md`. A guard is a prohibition plus
the consequence of ignoring it.

**Provenance.** Written with the header on 2026-11-17. The bindings are new code, so no entry was carried from an older
comment. Every other rule of the header is enforced by a `static_assert` or an `OG_CHECK`, whose messages name what they check.

---

## G-01 — the call that reports a registration also clears it

**Tag site:** `SimNetBindings.h`, in `SimNetBindings::takeRegistration`, immediately above its `return`.

**The prohibition.** Do not return the registration bit without clearing it in the same statement, and do not clear it in a
separate step that one caller can skip. `onRosterRemoved` and `markUnregistered` both answer through this function, and each
`true` it returns tells the host to unregister the character once.

**Consequence.** A character is unregistered by whichever comes first of its roster Remove and its pawn's EndPlay; the second
must find it unregistered. If the bit survived the first, the second would unregister it again: after a Remove the client
session has already destroyed that character's channel, so the host would unregister a channel that no longer exists.

**What breaks if it moves.** The two callers share this one read-and-clear. A copy of the rule inside each caller could let one
of them stop clearing the bit, and `RemoveThenEndPlayUnregistersExactlyOnce` and `EndPlayThenRemoveUnregistersExactlyOnce` are
the cases that would then fail.

---

## G-02 — the slot and the role come from the roster entry; the engine only cross-checks

**Tag site:** `SimNetBindings.h`, in `SimNetBindings::verdict`, immediately above its last `return`.

**The prohibition.** Do not take `isLocal`, the physics slot or the local index from anything but the roster entry, and do not
return `Ready` when the entry's role (`localIndex != kRosterNotLocal`) and `engineSaysLocal` disagree.

**Consequence.** The roster entry is the client's only source of a character's identity, slot and role, so every peer binds the
same id to the same slot. The engine's role is an independent second opinion. If `verdict` took the role from the engine, or
returned `Ready` on a disagreement, a pawn the engine wrongly reports as locally controlled would be predicted and fed local
input as one of this client's players, and the mismatch would never be logged or caught.

**What breaks if it moves.** The host acts on the verdict without looking at the entry again: it registers on `Ready` with the
verdict's slot, and on `RoleMismatch` logs `[Roster.BindMismatch]` and fails its check. `RoleMismatchBothWays` is the case
that fails when the comparison is dropped.

---

## §R — retired ids

None.
