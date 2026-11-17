<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetTransport.h` — rationale

`SimNetTransport<EndpointT, InboxN>` is the sim's transport port over an og-net endpoint. Inbound, it copies every message
on the five sim lanes, and every connection event it is handed, into one `SimInbox`. Outbound, it maps the sim's sends onto
the endpoint's `send` and `broadcastExcept`, either at once or held until the host's frame flush. The prohibitions that govern
its lines are in `SimNetTransport-guards.md`. The connection handles the sessions build from the inbox's events are in
`SimNetConnections.h`.

**If this file and `SimNetTransport.h` disagree, the header is authoritative and this file is stale.**

---

## §1 What the transport is, and what it is not

- **Generic over the endpoint.** `EndpointT` is anything that satisfies `SimNetEndpoint`: `send`, `broadcastExcept`,
  `setLaneHandler`, `setConnectionHandler` and `disconnect` (§10), with og-net's signatures. In the game that is og-net's `NetEndpoint`; in tests
  it can be a recording fake. The transport never calls `setConnectionHandler` (§3); the concept still names it, because the
  transport is written for an endpoint that has a connection handler somebody else sets.
- **It owns the inbox** (`SimInbox<InboxN>`), the hold arena (§7), the client's pending input set (§5) and the table that gives
  each connection its inbox slot and quotas (§10) and records whether its events were announced (§11). It does not drain the inbox, decode a message, or keep a session's connection table.
  The server and client sessions do that on their own thread, from `inbox()`.
- **It knows no role.** One class serves both ends: the server session calls the server sends (§6), and the client session
  hands each character a `SimNetChannelTransport` (§5).
- **Lifetime.** og-net calls the transport's handlers through a `this` pointer, so the transport is neither copyable nor
  movable. It must also outlive the endpoint: og-net's destructor stops the endpoint and reports every announced connection
  `Disconnected` through the connection handler (og-net's `NetEndpoint-rationale.md`, §1). A host that owns both as members
  declares the transport first.

## §2 Lanes and connection ids are og-net's

`SimLane` (og-simulation's `SimTransport.h`) has the values of og-net's first five `Lane`s, and `SimConnectionId` is the same
32-bit type as og-net's `ConnectionId`, with 0 invalid on both sides. The header's `static_assert`s pin both, so a lane or id
crosses between the two with a cast and no lookup. og-simulation names no og-net type; this header is where the two meet.

## §3 Installing the handlers

- **`installHandlers(endpoint)`** binds the endpoint and sets a lane handler on each of the five sim lanes. og-net's other
  lanes (Voice, Engine) are left alone: the engine lane has its own consumer.
- **Before the endpoint starts.** og-net takes lane handlers before `start` only, and fails its own check otherwise
  (`EndpointCore.cpp`, line 319). The transport checks first, so the failure names the transport: the host calls
  `markEndpointStarted()` right after a successful `start`, and an `installHandlers` after it fails an `OG_CHECK`. The hidden
  case `SimNetTransport.Death.InstallHandlersAfterTheEndpointStartedFiresTheCheck` shows it.
- **Never the connection handler** (G-02). In the game, P4's engine-lane driver core constructs the endpoint and sets its
  connection handler itself, in its `start` (`EngineLaneDriverCore.h`, line 111); it needs the client's failed-connect event.
  The host chains the transport behind it, calling `onConnectionEvent` from its own handler. A host with no other handler
  (the tests) installs `connectionHandler()`, a thunk to `onConnectionEvent`.

## §4 Inbound: copy into the inbox, nothing else

- **Messages.** A sim lane's handler hands the message to the transport's `receive`, which finds the connection's slot,
  refuses the message if the connection has none or is closing, and otherwise pushes it with `push(conn, lane, recvSeconds,
  bytes, slot)`, with og-net's receive time. A Control message over its slot's quota makes it ask the endpoint to disconnect
  the connection (§10). That is all it does (G-01): it runs on og-net's servicing thread, while every reader of sim state runs
  on the session's thread.
- **Too large.** A message over `kMaxSimMessageBytes` (512 B) is refused and counted by the inbox (`tooLarge`). Only the
  Control lane can carry one: og-net limits the unreliable lanes to 1,200 B, and Control to 256 KiB.
- **Connection events.** `onConnectionEvent` pushes `Connected` and `Disconnected` into the inbox's event ring (§11). A
  `Connected` first gives the connection its slot and is pushed paired, so that the inbox keeps a slot for its `Disconnected`;
  a `Disconnected` is pushed into that slot, with the cause `ControlQuota` when the transport closed the connection for its
  Control quota (§10), and then gives the connection's slot back. A `Connected` the event ring has no room for is not
  announced at all (§11). An event carries no receive time; its `recvSeconds` is `kNoReceiveSeconds` (−1). `AddressChanged` (a
  roaming peer) does not change which connection a message belongs to, so it is not pushed; it is counted
  (`addressChangesIgnored`).
- **A failed connect is pushed too.** og-net reports a client's failed handshake as a `Disconnected` with id 0
  (`EndpointCore.cpp`, lines 1375-1390; og-net's `EndpointCore-rationale.md` §2). The transport pushes it unpaired, since it
  has no `Connected` (§11), so the client session learns that its connection attempt ended.
- **One producer.** The inbox has exactly one producer thread. og-net runs every lane and connection handler on its servicing
  thread, so `onConnectionEvent` must be called from that thread too: in the game, from the handler that og-net calls. Called
  from any other thread, two threads would push into one single-producer ring. Nothing in the header can see the caller's
  thread, so this is a rule for the host and has no tag here.
- **Order.** Because a connection's `Connected` and its messages come from the same thread into one inbox, which hands records
  out in the order they were pushed even though Control and the events have rings of their own (og-simulation's
  `SimInbox-rationale.md` §3), the
  drain sees them in the order og-net raised them. og-net announces a connection just before it hands its first message to a lane
  handler (`EndpointCore.cpp`, line 1295), so `Connected` comes first
  (`SimNetTransport.AConnectedEventPrecedesItsConnectionsFirstMessageInDrainOrder`).

## §5 Client sends: one bundle set per connection per send

- **The adapter.** Each character's channel holds a `SimNetChannelTransport`, made by `channelTransport(simId)`. It satisfies
  og-simulation's `ClientSimTransport`: `sendInputBundle(bytes)` takes the codec's bundle bytes, with no sim id, and the
  adapter adds its sim id. The transport itself does not satisfy `ClientSimTransport` (a `static_assert` says so): its own
  `appendInputBundle(simId, bundle)` takes the id, so a channel cannot hold the transport by mistake.
- **The set.** `appendInputBundle` appends `simId │ bundle` to the pending `InputBundleSet` (og-simulation's
  `simEnvelope::writeInputBundleSetHeader` and `simEnvelope::appendInputBundle`). `endInputSends()`, from any adapter or the
  transport, sends the set as one SimInput message to the server connection and empties it. og-net's default lane table
  caps SimInput at 1,200 messages per second per connection, 300 per local player for 4 local players (`LaneConfig.h`,
  `kDefaultLaneTable`). One message per character would take four local players at 144 frames per second to 576 messages a
  second. One set per send takes them to 144 when the four bundles fit one message, and to 288 at the default 3 redundancy
  slots, where the set splits in two (below); at 8 slots every bundle fills a message of its own, so 576. All of these are
  within the cap. The set is kept because it takes fewer datagrams, and fewer of the server inbox's slots, which are shared
  out per connection (§10).
- **The server connection** is set by the client session with `setServerConnection`, from the `Connected` event it drains. A
  set ended before that is dropped and counted (`inputSetsWithoutServer`).
- **A set never outgrows one sim message.** The set buffer holds `kMaxSimMessageBytes`, the largest message the server's inbox
  accepts. When the next bundle does not fit, or the set already holds `simEnvelope::kMaxInputBundlesPerSet` (8) bundles, the
  pending set is sent first and a new one started (`inputSetSplits`). A brawler input slot is 43 B, so at the default of 3
  redundancy slots a bundle is up to 131 B, 132 B with its sim id. Three players fit one set (3 + 3 × 132 = 399 B); a fourth
  does not (531 B), so four local players send two messages. With 8 slots, the codec's maximum, a bundle is 347 B with its
  sim id, and every player sends one message. Without the split, an oversize set would be refused by the server's inbox as
  `tooLarge`.
- **Refused bundles** (counted `inputBundlesRefused`, nothing sent): sim id 0, fewer bytes than the bundle header, or a bundle
  too large for an empty set.

## §6 Server sends

| member | og-net call | lane |
|---|---|---|
| `relay(skipOwner, bytes)` | `broadcastExcept(skipOwner, …)` | SimRelay |
| `sendState(conn, bytes)` | `send(conn, …)` | SimState |
| `sendControl(conn, bytes)` | `send(conn, …)` | Control |
| `broadcastClock(bytes)` | `broadcastExcept(kInvalidSimConnection, …)` | SimClock |

`broadcastClock` skips id 0, which no connection has, so the clock reaches every connection. These four members are
og-simulation's `ServerSimTransport`, which the header's self-check asserts.

## §7 The frame hold (`SimNetConfig::immediate`)

- **`immediate = false`** (the default, and Ph3's): every send, client or server, is appended to the hold, a 64 KiB arena
  (`kHoldArenaBytes`) allocated and zeroed in the constructor. `flushFrame()` releases the held sends to the endpoint in the
  order they were made, and empties the hold. The host binds `flushFrame()` to the engine's flush point, so the sim's sends
  leave where they leave today. The hold is the owner thread's: sends and `flushFrame()` are made from one thread at a time.
- **An overflow keeps the order.** A send that does not fit the remaining arena first releases everything held, then goes to
  the endpoint at once (counted `holdOverflows`). Sending it alone would put it ahead of earlier sends of the same frame
  (G-03). Every send the hold refuses still leaves, in order, only earlier than its frame flush.
- **`immediate = true`:** every send goes straight to the endpoint, which takes sends from any thread (og-net's
  `EndpointCore-rationale.md` §2). Nothing is held, and `flushFrame()` has nothing to release.
- **The switch is fixed at construction** (`config()` reads it), so a session never mixes held and immediate sends.

## §8 Counters

`stats()` returns `SimNetTransportStats`, relaxed reads of atomics: `heldSends`, `releasedSends`, `directSends` (sent at once,
immediate or overflow), `holdOverflows`, `holdHighWaterBytes`, `inputBundles`, `inputBundlesRefused`, `inputSets`,
`inputSetSplits`, `inputSetsWithoutServer`, `addressChangesIgnored`, §10's `unslottedRefused`, `closingRefused`,
`controlQuotaDisconnects` and `noSlotConnects`, and §11's `unannouncedConnects` and `unannouncedRefused`. The inbox keeps its own
(`inbox().stats()`), among them the quota refusals per lane and the Control ring's high-water mark, which the sessions print on
their `[SimNet.Window]` line with the quota disconnects they drained, and the event ring's `eventsRefused` and
`eventHighWater`, which no line prints yet. `quotaRefused(conn)` gives one live connection's quota refusals per lane, and `slotOf(conn)` its
slot.

## §9 The fallback after the endpoint check

Every send reaches the endpoint through one private function. It `OG_CHECK`s that `installHandlers` bound an endpoint, then
returns if none is bound (G-04). The return runs only where the check is compiled out: a Shipping build, or the standalone
build with `NDEBUG`. There a send before `installHandlers` is dropped instead of dereferencing a null endpoint.

## §10 Connection isolation: the per-connection quotas and the Control ring

**Why.** The inbox is one per process, and on a server every connection's messages land in it. Shared first come, first
served, one connection that stays under every og-net cap could still fill it between two drains, so that the other connections'
messages were refused. Measured over og-net's real receive scheduler and the real inbox, with SimInput at 1,200 a second: two
such connections flooding every second, or one with a 100 ms server hitch, overflowed the 1,984 message slots and took other
players' messages. The SimInput and SimClock that are lost are repaired (later bundles repeat recent inputs; the next clock
publish supersedes), but a refused Control message is lost for good: og-net has already delivered and acknowledged it. Five
flooders overflow the shared ring with Control alone (5 × 456 = 2,280), at any SimInput cap.

**The sizing, from the endpoint.** `SimNetConfig` carries `maxConnections` and `controlReceiveDepth`, and
`simNetConfigFor(endpointConfig, immediate)` derives both from the og-net endpoint's config: a server takes its
`maxConnections` (16 by default), a client 1 (its one connection, to the server), and both take Control's receive depth from the
endpoint's lane table through og-net's `receiveDepthMessages`: 200 + 256 = 456 messages for the default table. The defaults of
`SimNetConfig` are a default server's. The host builds the transport's config with `simNetConfigFor`.
`simNetInboxConfig<InboxN>(config)` turns it into the inbox's config; the header's `static_assert`s pin a default server's
and a client's numbers.

| | server (16 connections, `InboxN` 2,048) | client (1 connection) |
|---|---|---|
| connection slots (inbox tags) | 16 | 1 |
| Control quota per slot | 456 undrained Control messages | 456 |
| Control ring | 16 × 456 = 7,296 slots, 3,910,656 B | 456 slots, 244,416 B |
| quota per slot, other lanes | ⌊1,984 / 16⌋ = 124 undrained messages | none: the shared ring's 1,984 message slots |

With the 2,048-slot shared ring (1,097,728 B) and the event ring (816 slots, 437,376 B; §11) a default server's inbox is
5,445,760 B, and a client's 1,346,968 B. A `maxConnections` of 0 is read as 1.
A client's one slot has no quota on the other lanes: a quota cannot isolate a connection from others it does not have, and
without one a full inbox counts its refusals in `dropped`, as it did before, not in `quotaRefused`. Its Control quota is the
server's: the bound on Control is per connection, and a client's one connection to the server has the same exposure.
The constructor checks that Control's depth is not 0 (an uncapped Control lane would give a quota of 0) and that the
other lanes' quota is at least 1 (more connections need a larger `InboxN`).

**The Control ring cannot overflow.** Every Control message goes to the Control ring, which only Control messages fill, and
each connection slot may hold at most 456 undrained Control messages there; the ring holds exactly the slots' quotas, so
however much the connections send between two drains, and however often they reconnect, they cannot fill it. A ring sized for
one og-net burst per connection, with no per-connection bound, did not hold: og-net admits at most 456 Control messages from a
connection in one burst, but its bucket refills at 200 a second, so between two drains Δ s apart a connection could deliver
456 + 200 × Δ, and a connection that reconnected came back with a full bucket while its old messages still waited. Sixteen
connections flooding at once overflowed 7,296 at any Δ > 0, and the ring's `OG_CHECK` (a Development server abort) was
reachable from the network.

**Over its Control quota, a connection is disconnected, with a stated reason.** A Control message that would take its slot
past 456 is refused by the inbox (`quotaRefused[Control]`, the result `ControlQuotaRefused`). The message is lost for good,
which a correct peer never comes near: a P2 client sends no Control message at all, and a server sends a connection Control
only when its roster, its relay floor or its tier changes. So the transport closes the connection:
- it asks the endpoint to `disconnect(id)` once, and counts `controlQuotaDisconnects`. og-net's `disconnect` takes no reason
  and is queued: the servicing thread applies it at its next command flush, after the burst it is receiving;
- until the connection's `Disconnected`, every further message from it, on any lane, is refused and counted in
  `closingRefused`: they are the offender's, still arriving while the close is queued;
- its `Disconnected` is pushed with the inbox's `cause` `ControlQuota`. The session that drains it logs
  `[Warning][SimNet.Disconnect] role=<server|client> conn=<id> cause=controlQuota (...)`, which names the connection and
  the cause, and counts it in its `[SimNet.Window]` line's `quotaDisconnects=` (`SimNetServer-rationale.md` §3,
  `SimNetClient-rationale.md` §4).

A client applies the same rule to its server connection.

**The quota on the other lanes.** A message on any lane but Control is refused while its connection's slot already holds 124
undrained messages in the shared ring. 16 × 124 = 1,984, so the connections together cannot fill the shared ring: a
connection's messages are refused only by its own quota, never because of another's traffic. A correct P2 client sends one
input set a frame, two when a set splits (§5), and nothing else upstream; with Ph4's clock messages added it stays under 30
between drains even across a 100 ms hitch. These
refusals are counted per lane and per connection and do not disconnect; the lanes' losses are repaired by later messages.
The inbox subtracts a message from its slot's counts when its drain hands it out, so whoever drains (a session, a test, a
host) gives the quota back.

**The slot table.** `maxConnections` atomics, one per slot, each naming the connection that holds it (0 when free), and a
closing flag per slot. Only og-net's servicing thread writes them; `slotOf(conn)` and `quotaRefused(conn)` read the owners
from any thread.
- A connection takes a slot at its `Connected`, or at its first message when the host pushed no `Connected` (a host that
  installs no connection handler), and gives it back after its `Disconnected` is pushed.
- **A slot given back is taken again at once, with its residue.** The closed connection's undrained messages still count
  against the slot's quotas until the drain hands them out, and the new connection starts with them against its own quotas
  (og-simulation's `SimInbox-rationale.md` §13). That is what keeps the bound under churn: a flooder that leaves and comes
  back cannot bring fresh room while its messages still wait. Of the free slots, a new connection takes the one with the
  least residue (the lowest index among equals), so a correct peer that joins inherits a flooder's residue only when every
  free slot holds one.
- **A connection with no slot is refused, never pushed untagged.** Its messages count `unslottedRefused`, so no message
  bypasses the quotas. og-net holds at most `maxConnections` connections at once and reports a connection's `Disconnected`
  before its slot can take another (`EndpointCore.cpp`, `reap`), so with a config from `simNetConfigFor` every connection
  finds a slot: no remote peer can make one slotless. A `Connected` that finds every slot taken therefore means a config
  that was not built from the endpoint's, and fails an `OG_CHECK`; where it is compiled out, the connection is asked to
  disconnect and counted in `noSlotConnects` (G-06).
- Finding a connection's slot is a scan of the table: 16 compares per message on a default server.

**Connection events** count against no slot's quota. They have a ring of their own, bounded by construction (§11).

## §11 Connection events: a ring of their own, sized from the slots and og-net's admission rate

**Why.** The events used to take the shared ring's slots, and its last 64 when the connections' messages had filled the rest.
With every slot at its 124-message quota (16 × 124 = 1,984) only those 64 were left, so more than 32 connections that each
connected and disconnected between two drains reached the inbox's event `OG_CHECK`: a Development server abort that remote
peers could trigger, since og-net admits up to 200 new connections a second.

**The event ring.** `simNetInboxConfig` gives the inbox an event ring (og-simulation's `SimInbox-rationale.md` §14) of
`simNetEventSlots(config)` slots: one per connection slot, for the `Disconnected` of the connection open there at the last
drain, and two for each connection that opens before the next drain, for its `Connected` and its `Disconnected`.

| | server (16 connections) | client (1 connection) |
|---|---|---|
| `connectsPerDrainGap` | 400 | 4 |
| event ring | 16 + 2 × 400 = 816 slots, 437,376 B | 1 + 2 × 4 = 9 slots, 4,824 B |

- **A server's 400** is every connection og-net can open in a drain gap of up to `kSimNetEventGapSeconds` (1 s). og-net opens a
  connection only on a cookie Hello, and takes each from one global bucket that holds one second of its 200 a second
  (`kCookieHellosPerSecond`; og-net's `Handshake.cpp`, `takeToken`): at most 200 at once and 200 a second after that, so 400 in
  any second. `kDefaultConnectsPerDrainGap` is computed from og-net's constant, and a `static_assert` pins it.
- **A client's 4.** A client's endpoint opens a connection only when its host connects, so its ring has room for four
  reconnects in one drain gap.
- `simNetConfigFor` sets the role's value, and the header's `static_assert`s pin both rings. The constructor `OG_CHECK`s that
  `connectsPerDrainGap` is not 0: with none, a client's ring of one slot could never hold a `Connected` and the slot it keeps.

**The events of a connection are a pair.**
- A `Connected` takes the connection's slot (§10) and is pushed with `pushPairedConnected`, which keeps a slot in the ring for
  that connection's `Disconnected`. The transport marks the slot announced.
- The connection's `Disconnected` goes into the kept slot with `pushPairedDisconnected`, which always finds it, and the
  connection slot is given back.
- So however many connections open and close between two drains, and however long the drain gap, the event ring cannot
  overflow and no announced connection's `Disconnected` is ever refused: no inbox `OG_CHECK` on events can be reached from the
  network. The argument is the inbox's (`SimInbox-rationale.md` §14): each pair keeps its slot, and the ring's records plus its
  kept slots never exceed its size.
- Other events are pushed unpaired (`pushEvent`), into room beyond the kept slots, and are refused, counted, when there is
  none: og-net's failed-connect `Disconnected` with id 0, which has no `Connected`; a `Connected` repeated for a connection
  already announced; the `Disconnected` of a connection that never had a `Connected` (a host that installs no connection
  handler, §10). og-net raises none of the last two: it announces a connection once, and raises its `Disconnected` only once
  it was announced (`EndpointCore.cpp`, `announce` and `reap`).

**Over the ring's size, a connection is not announced** (the coalescing rule). A `Connected` that finds no room for itself and
the slot it would keep (more than `connectsPerDrainGap` connections opened since the last drain) is refused by the inbox
(`EventRefused`, counted in `eventsRefused`). The transport then:
- marks the connection's slot unannounced, counts `unannouncedConnects`, and asks the endpoint to `disconnect` it once;
- refuses every message from it (`unannouncedRefused`), so none reaches the inbox;
- pushes nothing for its `Disconnected`, and gives the slot back.

So the consumer sees each connection that opens and closes inside one drain window either in full (`Connected`, its messages,
`Disconnected`, in push order) or not at all. Every connection it sees `Connected` for, it later sees `Disconnected` for
exactly once. A correct peer meets the rule only if more than 400 connections open in one drain gap; it is then closed before
any of its messages reach the session, and can reconnect.

**Measured** on the real transport and inbox (`SimNetConnectionEventTest.cpp`), every slot at its quota, 8 connections staying
and 8 slots churning:
- 200 connect and disconnect pairs a second for 2 s, drained every 100 ms (20 a gap): all 400 announced, each `Disconnected`
  drained exactly once, no message outside its connection's `Connected` and `Disconnected`. The event ring's high-water mark is
  64, exactly the old reserve: this rate alone did not overflow it;
- og-net's whole bucket in the first gap, then 200 a second (220, then 20 a gap): all 600 announced, high-water 464. On the old
  64-slot reserve this aborts on the inbox's event `OG_CHECK`;
- a 3 s drain gap that opens 800: 392 announced and 408 not, none of whose records reach the inbox, then all of the next
  gaps' connections announced; high-water 808, the ring's 816 less the 8 staying connections' kept slots.
