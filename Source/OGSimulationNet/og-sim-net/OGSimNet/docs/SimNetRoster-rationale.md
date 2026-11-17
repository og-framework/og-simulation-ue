<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetRoster.h` — rationale

The narrative behind `OGSimNet/SimNetRoster.h`: the roster message codec, the server's `RosterAuthority` and the client's
`RosterMirror`. The header keeps its licence, its docs pointer, its code and its tags; everything that explains it lives here.

**If this file and `SimNetRoster.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **Do not move a prohibition into this file.** Prohibitions with a site live in `SimNetRoster-guards.md`, reachable from the
`⛔G-nn` tag at that site.

**Provenance.** Written with the header on 2026-11-15. No code calls it yet: the server and client sessions of og-sim-net use
it once they exist.

---

## §1 What it is

The **roster** tells every client which characters the authority has registered: each character's sim id, its physics slot,
and, for the recipient's own characters only, which of its local players owns it. It is the source of a client's sim
identity, slot and local or remote role for each character.

- **`rosterCodec`** writes and reads one roster message: `encodeSnapshot`, `encodeAdd`, `encodeRemove`, and `decode`, which
  returns a `DecodedRoster` with a `RosterRefusal`. The codec is `constexpr`, so its encodings can be checked at compile time.
- **`RosterAuthority`** (server) keeps one `RosterRecord` per registered character and the roster revision. It builds every
  message a recipient gets, in that recipient's view, and answers the input-owner check.
- **`RosterMirror`** (client) applies the messages it receives, all or nothing, and reports each added and removed entry to
  its callbacks.
- Three formatters build the `[Roster.*]` log lines (§7).

Everything is header-only, with no out-of-line function, so the module needs no export marker for it. It allocates nothing
after construction except inside the `std::function` callbacks a caller sets.

The roster travels as the body of the envelope kind `SimMessageKind::Roster`, on the Control lane, from the server to a
client (`SimEnvelope.h`). This header handles the roster bytes only; the sessions add and strip the envelope.

## §2 The wire layout

Little-endian; version 1 (`kRosterWireVersion`):

```
RosterMessage := version:u8 (=1) │ kind:u8 │ revision:u16 │ body
  kind 1 Snapshot : count:u8 (0..8) │ Entry[count]   (ascending simId)
  kind 2 Add      : Entry
  kind 3 Remove   : simId:u8 │ reason:u8   (1 Left, 2 LocalPlayerLeft, 3 Despawned)
Entry := simId:u8 │ physicsSlot:u8 │ localIndex:u8
```

- **`simId`** is 1..255; 0 means none and is never sent. The authority assigns it, and never reuses one in its lifetime.
- **`physicsSlot`** is 0..7, or `kRosterNoSlot` (0xFF) for a character with no physics slot.
- **`localIndex`** is 0..7 for the recipient's own characters and `kRosterNotLocal` (0xFF) for every other one. It is a per-wire
  number, never a global index: two clients each have a local player 0. Its bound is the reception coordinator's player-slot
  bound, `ConnectionSlotKey`'s `kMaxPlayerSlot` (7). A `static_assert` ties the two, through a probe address type that
  satisfies `ConnectionAddress` only so that the template's constant can be named.
- **Sizes.** A full Snapshot is 5 + 8 × 3 = 29 B (`kRosterMaxMessageBytes`), an Add 7 B, a Remove 6 B. Each fits one SimInbox
  record with its 2-byte envelope (`kMaxSimMessageBytes`), so none is ever refused as too large, and none needs fragmenting.
- **`revision`** is the authority's roster revision. It goes up by one on every Add and every Remove, and wraps from 65535 to 0.
  A Snapshot carries the current value.
- **The envelope checks only the length its own constants imply** (`simEnvelope::roster`). The header asserts that those
  constants equal this layout's, so the envelope and this decoder never disagree on a message's length.
- **Adding a field** (a team, a name, a starting loadout) is a version bump with new golden vectors.

Golden vectors, reproduced byte for byte by the tests:

| message | bytes |
|---|---|
| Snapshot, revision 5, {1, slot 0, not local} and {2, slot 1, local 0} | `01 01 05 00 02 01 00 FF 02 01 00` |
| Add, revision 6, {3, slot 2, local 1} | `01 02 06 00 03 02 01` |
| Remove, revision 7, sim id 2, Left | `01 03 07 00 02 01` |
| refused: version 2 | `02 01 05 00 00` |
| refused: sim id 0 | `01 01 05 00 01 00 00 FF` |
| refused: slot 8 | `01 02 06 00 03 08 01` |
| refused: truncated Remove | `01 03 07 00 02` |

## §3 What the decoder refuses

`rosterCodec::decode` refuses, and returns nothing to apply, for:

- a version other than 1 (`Version`), or a kind other than 1..3 (`Kind`);
- a message shorter or longer than its kind and count imply (`Truncated`, `Overlong`);
- a Snapshot `count` above 8 (`Count`), checked before the length, so a count of 9 with the bytes for nine entries is still
  `Count`;
- a sim id of 0 in any kind (`SimIdZero`), or the same sim id twice in a Snapshot (`DuplicateSimId`);
- a slot of 8 or more that is not 0xFF (`Slot`), or a local index above 7 that is not 0xFF (`LocalIndex`).

It also refuses four cases the layout makes invalid, so that nothing the authority could never send is applied:

- a Snapshot whose sim ids do not ascend (`SnapshotOrder`): the layout writes them in ascending order;
- two entries of one Snapshot with the same physics slot (`DuplicateSlot`; 0xFF may repeat);
- two of the recipient's own entries with the same local index (`DuplicateLocalIndex`; 0xFF may repeat);
- a Remove reason other than 1..3 (`RemoveReason`).

Checks run in the order the bytes are read: header, kind, count, length, then each entry in turn (its own fields, then against
the entries before it). The first failing check names the refusal.

## §4 `RosterAuthority`

- **Records.** `RosterRecord` is `{simId, physicsSlot, owner, localIndex}`. `owner` is the owning connection, or none for an
  authority-local character (a listen host's own player). The owner is never sent; it decides each recipient's view.
- **`add(record)`** registers a character and goes up one revision. Its `OG_CHECK`s refuse, at the server: sim id 0; a sim id
  used before in this authority's lifetime; an invalid slot or local index; an owner of `kInvalidSimConnection`; an owner with
  no local index; a slot another entry holds; an owner and local index another live entry has; and a ninth character
  (`kRosterMaxEntries`, the world's slot count; the host's admission must refuse a join before it reaches registration).
- **`remove(simId, reason)`** goes up one revision and marks the entry *Remove pending*. The entry leaves every view and the input
  check at once, but its slot stays held.
- **`removeFor(simId)`** returns the Remove message, which is the same for every recipient, and frees the slot. This is the
  point the Remove counts as queued. **A slot is reusable only after its Remove was queued**: `add` into a slot whose Remove is
  still pending fails an `OG_CHECK`. One sender sends both on one ordered lane, so the slot's new Add can never reach a client
  before the Remove that freed it.
- **Views.** `viewFor(recipient)` lists the live entries in ascending sim id, each with the recipient's local index or 0xFF
  (guard G-01). `snapshotFor(recipient)` encodes that view at the current revision. `addFor(recipient, simId)` encodes one
  entry in that view at the revision its `add` produced, so an Add built late still carries its own revision.
- **`snapshotFor` with a Remove pending fails an `OG_CHECK`.** The Snapshot's revision already counts that Remove, and the
  entry is gone from its view. If the Remove were then sent to the new connection too, its revision would not be the
  Snapshot's plus one, and the client would refuse it as a gap.
- **The input check.** `checkInput(sender, simId)` is `Accepted` only when the live entry's owner is
  the sender. It is `UnknownSim` for an id with no live entry (a pending Remove included) and `NotOwner` for one owned by
  another connection or by the authority itself. `ownerOf` and `localIndexOf` return the owner and local index of a live entry
  with an owner, and nothing otherwise.
- `recordFor`, `revision` and `liveCount` are read-only views for the sessions and tests.

## §5 `RosterMirror`

- **`apply(bytes)`** decodes, then checks the decoded message against the mirror's state, then commits. It returns a
  `RosterApplyResult` (`applied()`, or the refusal, and the kind). **It never applies partially:** every check runs on a copy,
  and the mirror's state is replaced only after the last check passed. A refused message leaves the entries, the revision and
  the callbacks untouched. The caller is to disconnect on any refusal, with the reason logged.
- **A Snapshot** replaces the whole state and sets the revision. A second Snapshot (after a re-attach) is accepted the same way.
- **An Add or a Remove** is refused before any Snapshot (`NotAttached`), and unless its revision is the mirror's plus one,
  modulo 2¹⁶ (`RevisionGap`). The lane is reliable and ordered, so a gap is a bug, never loss. Then:
  - an Add whose sim id is live is `AddLive`; one whose slot or local index a live entry holds is `DuplicateSlot` or
    `DuplicateLocalIndex`; a ninth entry is `Count`;
  - a Remove whose sim id is not live is `RemoveNotLive`.
- **Callbacks.** After a commit, the mirror compares the old and new entries: every old entry not identical in the new state
  goes to the Removed callback, then every new entry not identical in the old state goes to the Added callback, both in
  ascending sim id. An Add or a Remove therefore reports exactly its own entry. A re-Snapshot that changes an entry reports it
  removed, then added. The callbacks run after the state is replaced, so a callback that reads `entryFor` sees the new state.
- `entryFor(simId)`, `entries()` (ascending sim id), `attached()` and `revision()` read the state.

## §6 What the sessions must do in order

The authority builds messages; the server session sends them. These rules cannot be checked inside the authority, because it
does not know which connections exist:

- send a connection its Snapshot (`snapshotFor`) before any Add or Remove to it, and never an Add or Remove whose revision the
  Snapshot already counts;
- send every connection that has its Snapshot each Add and Remove, in revision order, with nothing skipped;
- call `removeFor` right after `remove`, before the next `add` or `snapshotFor`, on the same thread.

A client that receives anything else refuses it and disconnects, so a mistake here is loud, not silent.

## §7 The `[Roster.*]` lines

| formatter | text |
|---|---|
| `formatRosterInputRefused(conn, simId, verdict)` | `[Roster.InputRefused] conn=<id> simId=<id> reason=<NotOwner or UnknownSim>` |
| `formatRosterRefused(refusal)` | `[Roster.Refused] reason=<refusal name>` |
| `formatRosterBindMismatch(simId, rosterLocal, engineLocal)` | `[Roster.BindMismatch] simId=<id> roster=<Local or Remote> engine=<Local or Remote>` |

- Each returns a fixed `RosterLogLine` (128 chars). A `static_assert` checks that the longest line of each fits. The host picks
  the severity and the route.
- `formatRosterInputRefused` with `Accepted` fails an `OG_CHECK`: an accepted bundle is not refused.
- `[Roster.BindMismatch]` is for the client host: the role a pawn's engine side reports disagrees with the roster entry its key
  selects. The host fails its own check after logging it.
- The refusal and verdict names are `rosterRefusalName` and `rosterInputVerdictName`.

## §8 Tests

`SimNetRosterTest.cpp`, tags `[NetTransport][SimNetRoster]`, in the og-simulation test target:

- the golden vectors, encoded (also at compile time, with `STATIC_CHECK`) and decoded;
- the four refused vectors, and every refusal of §3 and §5, each with a nearby accepted case;
- a refused message leaves the mirror and its callbacks untouched;
- the gap 5 → 7, a stale revision, and the wrap from 65535 to 0;
- the views of 3 connections with 2 local players each, a stranger's view, and per-recipient Adds;
- a Snapshot, an Add and a Remove leave the mirror equal to the authority's `viewFor` that recipient;
- the input check, the revisions, slot reuse after `removeFor`, and a second Snapshot's callbacks;
- callbacks that read the mirror see the state after the message, and a cleared callback is not called;
- the tag texts;
- hidden `[.][OGSimDeath]` cases, one per `OG_CHECK` of the header: each calls the header so that exactly that check fires, and
  ends in a `FAIL` that runs only if it did not.
