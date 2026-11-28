<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetConnections.h` — rationale

The server's sim code names a connection by a handle that can tell whether that connection is still the one it was made for.
`SimNetConnections.h` holds that handle, `SimNetConnectionHandle`, and the table it reads, `LivenessTable`. The header states
no prohibition, so it has a one-path docs pointer and no guards file.

**If this file and `SimNetConnections.h` disagree, the header is authoritative and this file is stale.**

---

## §1 The liveness table

- **Entries:** og-net connection id → `{epoch, up}`.
- **Fed from the inbox only.** Its one mutator is `apply(record)`, which takes an `InboundSimRecord`: a `Connected` record gives
  the id a new epoch and marks it up; a `Disconnected` record marks it down; a message changes nothing (`apply` returns false).
  Records reach a session only through `SimInbox::drain`, so the table is updated on the drain's thread, in the order the
  events happened, interleaved correctly with the connection's messages. The table has no lock and no atomics; it is read on
  the same thread (`isAlive`, `isUp`, `handleFor`, `upCount`).
- **Epochs are table-wide.** Each `Connected` takes the next value of one counter, which starts at 1 and skips
  `kNoConnectionEpoch` (0) when it wraps. og-net draws connection ids at random and keeps them unique only among live
  connections (`Handshake.cpp`, `ServerHandshake::drawConnectionId`), so an id can come back for another client. Its new epoch
  differs from every earlier one, so a handle of the earlier connection reads dead.
- **Ignored events:** a `Disconnected` for an id the table has not seen up, and any event for id 0 (a client's failed
  connect, which the transport pushes for the client session; `SimNetTransport-rationale.md` §4).
- **Bounded size.** Down entries stay, so `handleFor` of a closed id answers "dead". Once the table holds
  `kLivenessPruneEntries` (2,048) entries, the next `Connected` first erases every down entry. Erasing is safe because epochs
  never repeat: no handle can match a re-created entry. The map reserves that many entries in the constructor.
- **Not copyable or movable:** handles point at the table.

## §2 The handle

- `SimNetConnectionHandle {id, epoch, table}`. `isAlive()` is true when the table still has `id` up, at `epoch`. A
  default-constructed handle (no table) and one made by `handleFor` for an id that is down (epoch 0) are never alive.
- **Equality** compares all three fields, so two connections that shared an og-net id are two handles. **The hash** is the id's
  alone, which equality refines.
- **It is a `ConnectionAddress`** (og-simulation's `ConnectionTierTable.h`): regular, default-constructible, hashable, with
  `isAlive()`. That is the address type the server's reception coordinator and its tier and delay tables are templated on,
  and the reap reads `isAlive()` to drop a closed connection's state. The header's `static_assert` pins it.
