<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetBindings.h` — rationale

The narrative behind `OGSimNet/SimNetBindings.h`: the client's gate between a roster entry and the pawn that will simulate it.
The header keeps its licence, its docs pointer, its code and its tags; everything that explains it lives here.

**If this file and `SimNetBindings.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **Do not move a prohibition into this file.** Prohibitions with a site live in `SimNetBindings-guards.md`, reachable from
the `⛔G-nn` tag at that site.

**Provenance.** Written with the header on 2026-11-17. No code calls it yet: the engine host's client binding uses it once the
sessions run over og-sim-net.

---

## §1 What it is

On a client, two things must exist before a character can be registered with the simulation: the **roster entry** for its sim
id, which arrives from the server on the Control lane, and the **pawn** that carries that sim id as its binding key, which the
engine spawns. Either may come first. `SimNetBindings` holds the roster side of that meeting and answers one question for the
pawn side: *may this sim id be registered now, and with which slot and role?*

- The entry is the only source of the character's **physics slot**, its **local index** and so its **role** (local or remote).
  The engine's own idea of the role is a cross-check, never an input.
- A small **ledger** records which sim ids the host has registered, so that a character is unregistered exactly once, by
  whichever comes first of its roster Remove and its pawn's EndPlay.

It is engine-free (std and og-sim headers only) and holds no pointers: the host keeps its own pawns and channels.

## §2 The state

- `m_entries`: one `RosterEntry` per possible sim id, indexed by the id itself; an entry whose `simId` is 0 is "no entry"
  (sim id 0 is None, and the roster never sends it).
- `m_registered`: one flag per possible sim id.
- Both tables cover the whole `uint8_t` range (`kSimIdCount` = 256), so every lookup is a direct index with no bounds check; a
  `static_assert` holds that. About 1 KiB in all.
- **Invariant:** a registered id always has an entry. `markRegistered` checks it, and a Remove clears both together.

## §3 The verdict

`verdict(simId, engineSaysLocal)` returns a `BindVerdict`: a `BindStep` plus the sim id, the entry's physics slot, its local
index, and `isLocal` (the entry's role).

| entry | `engineSaysLocal` vs the entry's role | step | slot, local index, `isLocal` |
|---|---|---|---|
| none | — | `Wait` | `kRosterNoSlot`, `kRosterNotLocal`, false |
| present | agree | `Ready` | the entry's |
| present | disagree | `RoleMismatch` | the entry's |

- **`Wait`** means "not yet": the host retries the registration later, inside its own bounded retry loop.
- **`Ready`** gives the host everything it registers with: the slot, and whether the character is one of this client's local
  players (and which one).
- **`RoleMismatch`** carries the entry's values so the host can log `[Roster.BindMismatch]` with the text from
  `SimNetRoster.h`'s `formatRosterBindMismatch(verdict.simId, verdict.isLocal, engineSaysLocal)`, then fail its check. With
  checks compiled out, the host is to abandon that registration.
- The verdict is a pure read: asking it changes nothing, and it does not consult the ledger. A registered id still reads
  `Ready`; it is the host's job not to register it twice (`markRegistered` checks that).

## §4 The registration ledger

- `markRegistered(simId)` after the host registered the character; `isRegistered(simId)` reads the flag.
- `onRosterRemoved(simId)` drops the entry and returns **true once** if the id was registered, clearing the flag; the host then
  unregisters the character at once.
- `markUnregistered(simId)` is the pawn's EndPlay: it returns **true once** if the id was registered, clearing the flag, and
  `false` otherwise, so EndPlay is idempotent.

The two orders:

| order | first call | second call |
|---|---|---|
| Remove, then EndPlay | `onRosterRemoved` → true: unregister | `markUnregistered` → false: nothing |
| EndPlay, then Remove | `markUnregistered` → true: unregister | `onRosterRemoved` → false: nothing |

- After EndPlay-then-Remove the entry stays until the Remove, so a new pawn with the same key would read `Ready` again until
  then. After the Remove the id reads `Wait`.
- A sim id added again after its Remove (a re-Snapshot that moves it, reported by the mirror as removed then added) starts
  unregistered and reads `Ready` with its **new** slot.
- An id that was never registered unregisters nothing on either call.

## §5 Seeding

`seedFrom(mirror)` replaces every entry with the `RosterMirror`'s current entries, for a host that starts listening after the
mirror already holds some. The result equals what the same messages would have built through the mirror's callbacks.

It must run before the first registration: an `OG_CHECK` refuses a seed while any id is registered, because a registered id the
mirror no longer lists would then never see its Remove.

## §6 Threads and wiring

`SimNetBindings` is not synchronized. It is used on one thread at a time: the thread that drains the client session and
runs the registration. The host must not read it from another thread while that thread can write it.

How the client session's host hooks map onto it:

- The session calls the host's `onRosterAdded(simId)` from inside the mirror's added callback, after the mirror's state was
  replaced, so `client.roster().entryFor(simId)` returns the new entry: pass it to `onRosterAdded`.
- The session calls the host's `onRosterRemoved(simId)` **before** it destroys that character's channel. A `true` from
  `onRosterRemoved` must therefore unregister the character inside that hook, while the channel still exists.
- A host that binds after the session already received its Snapshot calls `seedFrom(client.roster())` once, before its first
  registration.

## §7 Checks

| check | fires when |
|---|---|
| `onRosterAdded`, sim id 0 | the entry's sim id is 0 (the decoder refuses it, so this is a host bug) |
| `onRosterAdded`, live id | the id already has an entry: the mirror's added and removed callbacks went out of step |
| `seedFrom` | any id is registered |
| `markRegistered`, no entry | the host registered without a `Ready` verdict |
| `markRegistered`, twice | the id is already registered |

## §8 Tests

`SimNetBindingsTest.cpp`, tags `[NetTransport][SimNetBindings]`, in the og-simulation test target:

- `Wait` before the Add (and for sim id 0), then `Ready` with the slot and local index, for a local and a remote entry;
- `RoleMismatch` both ways, with the entry's values and the `[Roster.BindMismatch]` text;
- Remove-then-EndPlay and EndPlay-then-Remove each unregister exactly once; ids never registered unregister nothing;
- an id added again after its Remove gets the new slot, directly and through a mirror's second Snapshot;
- `seedFrom` equals the incremental result after each message of a Snapshot, Add, Add, Remove, Remove script built by a
  `RosterAuthority`, and replaces entries the bindings held before;
- hidden `[.][OGSimDeath]` cases, one per `OG_CHECK` of the header: each calls the header so that exactly that check fires, and
  ends in a `FAIL` that runs only if it did not.
