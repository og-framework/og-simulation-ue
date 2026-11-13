<!-- SPDX-License-Identifier: MPL-2.0 -->
# `EngineLaneDriverCore.h` — rationale

`ognetunreal::EngineLaneDriverCore<HostT, BackendT>` is the engine-free half of the Unreal engine-lane driver: every policy the
driver needs between an og-net endpoint's engine lane (`ognet::EngineLane`) and the engine's own connections, with no engine
header. The Unreal driver (UOGNetDriver, task P4-4) holds one core and turns its callbacks into engine calls; the policies themselves are
tested here, over og-net's virtual network, without an engine session. This file is the docs pointer of `EngineLaneDriverCore.h`
and of its test, `EngineLaneDriverCoreTest.cpp`. Prohibitions are in `EngineLaneDriverCore-guards.md`.

The header holds the licence, the docs pointer, code, four guard tags and these compile-time checks:

- `kPreConnectSlotBytes` equals `ognet::EngineLane::maxPacketBytes()` (1,189 B): a pre-connect slot holds exactly one packet the
  lane would take (§4);
- `kBatchEventBytes` equals `ognet::kEngineEventBytes` (1,232 B) and fits a `uint16_t`: a batch slot holds any event a ring slot
  holds (§6);
- `kBatchEvents` (256) is at most `ognet::kEngineRingSlots`;
- `kDrainedCloseSeconds` (0.2 s) equals the Engine lane's `maxQueueAgeSeconds` in `ognet::lanes::kDefaultLaneTable` (§7).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 Shape and threads

- **Header-only, std and og-net only,** namespace `ognetunreal`. `HostT` is an og-net host (`ognet::NetHostServices`) and `BackendT`
  a datagram backend (`ognet::DatagramBackend`); the core builds its own `ognet::NetEndpoint<HostT, BackendT>` in place, in a
  `std::optional`, at `start`. In the Unreal driver they are to be `FOGNetUnrealHost` and `SwitchableBackend` over
  `ognet::BsdDatagramBackend` (`SwitchableBackend-rationale.md`); in the tests, og-net's `TestHost` and `SwitchableBackend` over
  `VirtualBackend`.
- **One engine thread owns the core** (the game thread in the engine). Every member function, and every callback, runs on it.
  The engine lane fixes its poll thread at the first `poll` (`EngineLane-rationale.md` §3), so `tick` must always come from
  the same thread.
- **The one exception is the connection handler,** which og-net calls on its servicing thread: og-net's own thread in
  `IoMode::OwnThread`, or the thread that calls `serviceUntil` in `IoMode::HostServiced`. It touches nothing but the handler
  queue (§5), under `m_handlerMutex`.
- **No allocation after `start`.** The record table, the pre-connect buffer and the batch are allocated by `start`; `tick` and
  `send` only index into them.
- **Teardown.** The destructor resets the endpoint first. `ognet::EndpointCore`'s destructor calls `stop`, which in own-thread mode
  waits for og-net's thread to exit (`EndpointCore.cpp`, `EndpointCore::stop`), so the thread that calls the connection handler is
  gone before the handler queue's mutex is destroyed. The endpoint is also the last member, so the order holds without the
  explicit reset too.

## §2 The interface

| member | what it does |
|---|---|
| `start(host, backend, config, mode, callbacks)` | builds the endpoint, sets the connection handler, enables the engine lane (`engineLane()` before `start`, as og-net requires), starts it, and in `OwnThread` mode calls `startOwnThread()`. When og-net's `start` fails it returns false and nothing is left started: the endpoint is destroyed. A second `start` and a missing callback are `OGNET_CHECK`s. |
| `connect(address, engineDeadlineSeconds)` | client only: remembers both and issues og-net's connect. The deadline is the engine's own: the time its pending connection gives up (§5). |
| `send(id, bytes)` | §3's verdicts. |
| `tick(nowSeconds, clientConnectionPending)` | in order: the handler queue (§5), the ring (§6), the drained closes (§7). `clientConnectionPending` is whether the engine still waits for its server connection. |
| `engineClosed(id, nowSeconds)` | the engine has cleaned up its connection: §7. |
| `stop(nowSeconds, drainSeconds)` | §8. |
| `serviceUntil(deadline)` | `HostServiced` mode only (`OGNET_CHECK`): the caller services the endpoint, as og-net's mode B. |
| `stats()` | `DriverStats`, the core's counters (§9). |
| `connectionStats(id)`, `endpointStats()`, `engineLaneStats()`, `localAddress()` | og-net's own figures, passed through; `localAddress` exists when the backend reports one (`ognet::ReportsLocalAddress`). |
| `isStarted()`, `isStopped()`, `mode()`, `serverConnection()` | state; `serverConnection()` is the client's og-net id once it is up, else 0. |

The callbacks are plain function pointers with one context (`DriverCallbacks`): `accept`, `deliver`, `closed`,
`serverConnectionUp` and `connectEnded`. All five must be set, whatever the role, because one driver class serves both roles.

## §3 `send` and its verdicts

| case | verdict | counted in |
|---|---|---|
| the core is not started, or has stopped | `Dead` | — |
| client, server connection not up, not ended: room in the buffer | `Buffered` | `preConnectBuffered` |
| the same, buffer full | `BufferFull` (the new packet is dropped, the oldest kept) | `preConnectDropped` |
| the same, packet over 1,189 B | `Refused` | `sendsRefused` |
| client after its connection ended (§5, §7) | `Dead` | — |
| an id that is not a live connection of the core | `Dead` | — |
| a live id, and the engine lane refused the packet | `Refused` | `sendsRefused` |
| a live id, and the lane took it | `Sent` | — |

- **`Dead` covers every id that is not live** (closed by the peer, passed to `engineClosed`, rejected, still pending, or never
  known), not only the ids the core has closed. The engine sends only on connections it has: its connections are created from
  `accept`, so an id it never knew does not reach `send`. A send after a close is normal engine behaviour, and it must not count as
  a refusal: `sendsRefused` means "the lane dropped a packet", and the Ph2 gate's transport-health check expects it to stay 0.
- **A refused send is dropped.** The engine's own reliability resends what matters, as after a UDP loss.

## §4 The client's pre-connect buffer

- **Why.** Without a packet handler, the engine sends its first join packet as soon as its pending connection begins the
  handshake (UPendingNetGame::BeginHandshake calls SendInitialJoin at once when the connection has no handler,
  PendingNetGame.cpp, lines 112-125), which is before og-net's handshake has ended; and the engine lane refuses an id it does not
  know (`EngineLane-rationale.md` §1). So the core keeps such packets: 32 slots (`kPreConnectSlots`) of up to 1,189 B, in arrival
  order.
- **When the server connection comes up** (the client's `Connected` from the ring), the core records the id, sends the whole buffer
  through the lane oldest first, and only then calls `serverConnectionUp(id)`. A packet sent from inside that callback is then
  already behind the buffered ones on the wire.
  `EngineLaneDriver.PacketsSentBeforeTheServerConnectionIsUpArriveFirstInOrderAndByteExact` checks both, with one packet sent from
  inside the callback.
- **Full:** the new packet is dropped and counted (`BufferFull`); the oldest are kept, because the engine's handshake packets come
  first (`EngineLaneDriver.AFullPreConnectBufferKeepsTheOldestAndCountsTheDrop`: of 33, the server gets the first 32).
- **Cleared** when the client's connection ends before it was up (`connectEnded`, or `engineClosed` before up): nothing would ever
  send it.

## §5 The handler queue and the connect retries

- **Where a failed connect is reported.** og-net reports a client handshake that ended without a connection (rejected, failed,
  timed out) only to the connection handler, as a `Disconnected` with id 0 (`EndpointCore.cpp`, `EndpointCore::handleClientStep`);
  the engine ring never sees it, because there was never a connection. The handler pushes the reason of each such event into a
  mutex-guarded queue of 64 (`kHandlerQueueSlots`); a full queue drops the event and counts it (`handlerQueueDropped`). Every other
  connection event is ignored there: the ring carries `Connected` and `Disconnected` for real connections, in order with their
  datagrams.
- **`tick` drains the queue first.** While the client is connecting:
  - `ConnectFailed` while `clientConnectionPending` and before the engine's deadline: og-net's connect again (`retries`). og-net's
    handshake gives up after 10 s (`ognet::handshake::kConnectTimeoutSeconds`), while the engine's pending connection lives for its
    own initial-connect timeout (60 s in the engine's base configuration, BaseEngine.ini, line 1764): without the retry, a server that starts listening a few seconds late is never
    reached, though the engine is still waiting.
  - anything else (`Rejected`, `ServerKeyMismatch`, a `ConnectFailed` after the deadline or when the engine no longer waits):
    `connectEnded(reason)`, once. The client is then ended: later queue events are ignored and `send` gives `Dead`.
- `EngineLaneDriver.AClientWhoseConnectFailsBeforeTheDeadlineReconnectsAndALaterServerAcceptsIt` (failure at 10 s, one retry, a
  server started at 15 s accepts) and `EngineLaneDriver.AClientWhoseConnectFailsAfterTheDeadlineEndsOnceWithoutARetry` (deadline
  5 s: one `connectEnded(ConnectFailed)`, no retry, nothing more by 25 s).
- **Beyond the specification:** a client `Connected` that arrives after the client has ended is disconnected at once, so og-net does
  not keep a connection the engine no longer has.

## §6 The ring, in batches

- `tick` polls the engine lane for at most 256 events at a time (`kBatchEvents`) into a preallocated batch of slots
  `{kind, id, reason, size, bytes[1232]}`. The poll callback only copies. The callbacks run after `poll` has returned, slot by slot,
  and the core polls again while a batch came back full.
- **Why not dispatch inside the callback.** og-net forbids `poll` from its own callback (an `OGNET_CHECK`), and the event's bytes are
  valid only inside it (the ring slot is wiped after the callback returns, `EngineLane-rationale.md` §1, §3). Engine code reached
  from a callback can do anything, including stopping the driver. With dispatch inside the callback, a `deliver` that stopped the
  core did not stop the poll: the four remaining datagrams of the batch and the stop's own `Disconnected` were still delivered
  (5 callbacks after the stop, P4-3's arm D). Guard G-01.
- **Per event:**

  | event | server | client |
  |---|---|---|
  | `Connected` | `accept(id)`: **Accept** → a live record, `accepted`; **Reject** → og-net's `disconnect(id)`, `rejected`; **Ignore** → a pending record | the server connection is up: §4 |
  | `Datagram`, live id | `deliver` | `deliver` |
  | `Datagram`, pending id | `accept` again: Accept → live, `accepted`, then `deliver`; Reject → `disconnect`, `rejected`, the datagram dropped; Ignore → dropped. Both drops count in `ignoredDatagrams` | — |
  | `Datagram`, any other id (unknown, or draining after `engineClosed`) | dropped, `unknownDatagrams` | same |
  | `Disconnected`, live id | the record goes, then `closed(id, reason)` | same, and the client is ended |
  | `Disconnected`, pending or draining id | the record goes, no callback: the engine never had, or no longer has, the connection | same |

- **Ignore** is the engine's answer while the server switches levels (UWorld::NotifyAcceptingConnection, World.cpp, lines
  6788-6801). The core asks again on every later datagram (`EngineLaneDriver.AnIgnoredConnectionDropsItsDatagramsUntilItIsAccepted`: three Ignores,
  two datagrams dropped, the rest delivered in order). **Reject** closes the connection from the server, so the client sees
  `closed(ClosedByPeer)` (`EngineLaneDriver.ARejectedConnectionIsClosedByThePeerAndDeliversNothing`).
- `EngineLaneDriver.SixHundredDatagramsQueuedBetweenTwoTicksArriveInOrderInBatchesOf256`: 600 datagrams in one tick, in order, over
  3 batches, `maxBatch == 256`.
- **The record table** holds one entry per og-net connection the core knows (pending, live, or draining), sized to the endpoint's
  connection slots (`maxConnections` on a server, 1 on a client). og-net delivers a slot's `Disconnected` before the next
  `Connected` of that slot (`EngineLane-rationale.md` §4), so the table cannot overflow; `insert` checks it (`OGNET_CHECK`).

## §7 The drained close

- `engineClosed(id, now)` marks the record draining and stamps the time; it does not disconnect. Each `tick` then calls og-net's
  `disconnect(id)` once `queuedBytes(id) == 0` (`closesDrained`), or once `now` is 0.2 s past the `engineClosed` stamp (`closesTimedOut`).
- **Why.** When the engine closes a connection it sends a last packet first: UNetConnection::Close sends the close reason and
  flushes (NetConnection.cpp, lines 1106-1119). A closing og-net connection seals its
  Close frame first in every datagram it still sends (`Connection.cpp`, `Connection::sealNext`). Disconnecting in the same tick as
  that last send lost it: with an immediate disconnect the client received no packet before its `closed` (P4-3's arm B,
  `EngineLaneDriver.ALastPacketSentWithEngineClosedReachesThePeerBeforeItsClose`). Guard G-02.
- **0.2 s** is the Engine lane's queue age limit: a packet still queued after it has been dropped by the lane anyway, so waiting
  longer gains nothing (the `static_assert`).
- After `engineClosed`, `send` to the id gives `Dead`, its datagrams count as unknown, and its og-net `Disconnected` brings no
  `closed` callback: the engine has already cleaned it up.
- **Beyond the specification:** `engineClosed` on a client whose server connection is not up yet (the engine dropped its pending
  connection, with id 0) ends the client: the buffer is cleared, no retry follows, and a late `Connected` is disconnected (§5).

## §8 Stop and re-entrancy

- `stop(now, drainSeconds)` marks the core stopped, then in `OwnThread` mode polls `queuedBytes` of every live and draining
  connection every 1 ms (`kStopDrainPollInterval`) until all are 0 or `drainSeconds` have passed on the host's clock, then calls
  og-net's `stop` (which in own-thread mode waits for og-net's thread to exit). In `HostServiced` mode there is no drain: nothing
  services the endpoint while the caller waits in `stop`. A second `stop` does nothing.
- **A callback may call `send`, `engineClosed` or `stop`.** `send` from `serverConnectionUp` is §4's case. After a `stop`, no
  further callback of that `tick` runs: the batch loop checks after every event (guard G-04), the handler queue and the drained
  closes stop at it. Without the check, a stop from `deliver` was followed by four more `deliver`s from the same batch (P4-3's arm E,
  `EngineLaneDriver.ADeliverThatStopsTheCoreRunsNoFurtherCallback`).
- `EngineLaneDriver.OnItsOwnThreadEachSideSendsFiveHundredInOrderAndStopDrainsTheLastPacket`: real threads, a 10 ms tick, 500
  packets each way in order; the client sends its last packet and calls `stop(…, 0.2)` in the same frame, and the server delivers
  that packet before its `closed(ClosedByPeer)`.

## §9 `DriverStats`

`accepted`, `rejected`, `ignoredDatagrams`, `unknownDatagrams` (§6); `preConnectBuffered`, `preConnectDropped`, `sendsRefused` (§3,
§4); `retries` (§5); `closesDrained`, `closesTimedOut` (§7); `handlerQueueDropped` (§5, read under the handler mutex); `batches`
(non-empty polls) and `maxBatch` (§6). The driver's `closed` line and Ph2's transport-health check read them.

## §10 Tests

`EngineLaneDriverCoreTest.cpp` (`[OGNet][EngineLaneDriver]`, run by `OGNetTests`). The rig is og-net's `VirtualNetwork` in virtual
time with `TestHost`, both cores in `HostServiced` mode over `SwitchableBackend<VirtualBackend>` without an emulator: each 1 ms step
calls `serviceUntil(now)` on every started core, then `tick(now, …)` on every core that ticks. The steady case runs real threads in
`OwnThread` mode on the network's steady clock, with a 30 s give-up. One more case runs a client whose emulator drops every
outbound datagram: the server never sees a datagram (`EngineLaneDriver.AClientWhoseEmulatorDropsEveryOutboundDatagramNeverReachesTheServer`).
