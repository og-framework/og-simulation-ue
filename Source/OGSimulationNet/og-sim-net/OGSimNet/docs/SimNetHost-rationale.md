<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetHost.h` — rationale

`SimNetHost.h` names the services a sim session needs from the program that runs it: a clock, an RTT reading, the server
tick, a log, and the latency-budget stamps. The server session (`SimNetServer.h`) takes a `SimNetServerHost`; the client
session takes a `SimNetClientHost`. In the game, P5's UE host implements them; in the tests, a fake does.

**If this file and `SimNetHost.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Why a concept and not an interface

og-sim-net is engine-free and header-only. The sessions are templates over their host, so a call to a host service is a
direct call the compiler can inline, and a missing or mistyped service is a compile error at the session, not a runtime
failure. The header carries a reference shape of each host, declared and never defined, and `static_assert`s that it
satisfies its concept; this is the `TickDriverConcepts.h` pattern of og-simulation.

Every requirement is an exact return type (`std::same_as`), so a host cannot return a narrower type by accident. One
`static_assert` shows it for the RTT: a host whose `rttMs` returns `float` is not a `SimNetServerHost`.

## §2 `SimNetServerHost`

| service | what the server session uses it for |
|---|---|
| `nowSeconds() → double` | the `[SimNet.Window]` period (10 s of this clock) |
| `rttMs(conn) → double` | the RTT sample for each character bundle from `conn`, in milliseconds. A negative value means "no reading yet", which the coordinator never samples. Ph3 keeps the engine's ping as the source |
| `receptionTick() → int32_t` | the server tick passed with each RTT sample, the value today's input path reads from the server clock (`getServerReceptionTick`) |
| `log(const char*)` | every line the session and the objects it owns emit: the coordinator's, the stagers' `[RelayFlush]`, `[Roster.InputRefused]` and `[SimNet.Window]`. A leading `[Warning]` or `[Verbose]` is the og-simulation severity convention; the host routes by tag |
| `stampServerReceived(simId, captureTick, recvSeconds)` | once per slot of every accepted bundle, before the coordinator sees it. `recvSeconds` is og-net's receive time for the whole message, for the host's NetReceived point |
| `stampHanded(simId, captureTick)` | once per entry of every relay ring the session sends, when it sends it |

The RTT stays a `double` end to end: the coordinator smooths it as a `double`, and narrowing it changes tier transitions.

## §3 `SimNetClientHost`

The client session's services. The server session does not use them; they are named here so both sessions share one
header.

| service | meaning |
|---|---|
| `nowSeconds() → double` | the host clock |
| `serverRttSeconds() → double` | the RTT to the server, in seconds, passed with each clock sample |
| `onTimingInfo(tick, rttSeconds)` | a server clock sample was received |
| `onRelayFloor(floor, replayed)` | the relay floor arrived, or was replayed to a listener bound after it arrived |
| `onConnectionTier(old, new, replayed)` | this connection's tier changed, or was replayed |
| `onRosterAdded(simId)`, `onRosterRemoved(simId)` | the roster mirror gained or lost a character |
| `onWireMismatch(received, expected)` | a correction carried another correction version than this build's |
| `log(const char*)` | as on the server |
| `stampClientReceived(stream, simId, tick, recvSeconds)` | the ClientReceived stamp: `SimNetStream::Input` for a relay ring's entries, `SimNetStream::State` for a correction |

`SimNetStream` is the session's own two-value enum, so the header names no type of og-simulation's latency probe.

## §4 The optional delivery hook, `noteDelivered`

| service | meaning |
|---|---|
| `noteDelivered(lane, recvSeconds)` | optional, on either host: one message record was drained, on `lane`, received by og-net at `recvSeconds`. Called once per `Message` record, before the session decodes it, and never for a connection event |

- **`SimNetDeliveryHook<H>`** is true when a host has it, with exactly that call shape and a `void` result. Each session calls it
  under `if constexpr`, so a host without it compiles unchanged and pays nothing.
- **An optional member fails silently when it is misspelled**, so the header also checks the other direction.
  `SimNetHostHasNoDeliveryMember<H>` is true when `H` has no member named `noteDelivered` of any kind or access: it derives a
  probe from `H` and a struct with one `int noteDelivered`, and takes `&Probe<H>::noteDelivered`, which is ambiguous, and so
  ill-formed, exactly when `H` has such a member too. `SimNetDeliveryHookWellFormed<H>` is "has the hook, or has no member of
  that name", and each session `static_assert`s it for its host. So a host whose `noteDelivered` takes other arguments (for
  example og-net's endpoint shape, which also takes the current time), returns a value, or is private, does not compile
  instead of never being called.
- The probe derives from `H`, so a host class must not be `final`.
- **Self-check:** reference hosts with the hook satisfy their role's concept and the hook; the plain reference hosts are well
  formed without it; a host with a three-argument `noteDelivered`, and one with a private `noteDelivered`, are not well formed.

## §4a The optional unregistering hook, `onLifecycleUnregistering`

| service | meaning |
|---|---|
| `onLifecycleUnregistering(simId)` | optional, on either host: a character the session registered through its lifecycle queue is about to leave the simulation. Called by the session's `unregisterLifecycle`, on the drain's thread, **before** the facade (`unregisterSimulatable`) runs, so the character is still in storage |

- **Why it exists.** A game's systems may hold pointers into a character's storage: the brawler's hit routing keeps the
  character's root body and its simulatable. They forget the character in the systems executor's unregister notify, which reads
  the character, so it must run before the facade. The session's own unregistrations can notify first (the host does it before it
  enqueues), but a roster `Remove` reaches the facade inside the session, with no host code before it. Without the hook such a
  removal left the systems pointing at freed storage (og-input-latency task P5-17's finding F1; added by task P5-19).
- **When it is called.** In `SimNetClient::unregisterLifecycle` (a roster `Remove` or a queued unregister of a queue-registered
  character) and in `SimNetServer::unregisterLifecycle` (`removeCharacter` or a queued unregister of one). A character the queue
  never registered (a frame-locked host registers on its own) does not reach it: `unregisterLifecycle` returns before the call.
- **Shape and checks.** The same pattern as `noteDelivered` (§4): `SimNetLifecycleUnregisteringHook<H>` is the call shape
  `onLifecycleUnregistering(uint8_t) -> void`, called under `if constexpr`; `SimNetHostHasNoUnregisteringMember<H>` is the name
  probe (a second `Names` struct, so the two probes stay independent); `SimNetLifecycleUnregisteringHookWellFormed<H>` is "has the
  hook, or has no member of that name", and both sessions `static_assert` it. A host whose member returns a value, or is private,
  does not compile.
- **Self-check:** reference hosts with the hook satisfy their role's concept and the hook; the plain reference hosts are well
  formed without it; a host whose `onLifecycleUnregistering` returns `bool`, and one with a private one, are not well formed.

## §4b The optional SimClock sample hook, `onClockSample`

| service | meaning |
|---|---|
| `onClockSample(const SimNetClockSample&)` | optional, client host only: one SimClock ping/pong measured by `SimNetClockClient` (`SimNetClock.h`: the RTT, theta, the server tick and the four stamps) was drained. Called by the client session on the drain's thread, ahead of the envelope decode, once per clockSample record, in drain order |

- **Why it exists.** The clock client turns each pong into a clockSample record and pushes it into the inbox on og-net's
  servicing thread (`SimNetClock-rationale.md` §6, §7). The record's version byte is one no envelope carries, so before this hook
  the client session's decode refused it and counted it as a `badDecode`, and a host that wanted the samples had to drain the
  records out of the inbox first (og-input-latency task P2-20's finding F1). The hook is the drain's own door for them: the host
  copies the sample into its clock's estimator on the thread that already writes the estimator (`onTimingInfo`'s), so no new
  thread crossing (og-input-latency task P3-10).
- **Shape and checks.** The same pattern as `noteDelivered` (§4) and `onLifecycleUnregistering` (§4a):
  `SimNetClockSampleHook<H>` is the call shape `onClockSample(const SimNetClockSample&) -> void`, called under `if constexpr`;
  `SimNetHostHasNoClockSampleMember<H>` is the name probe (its own `Names` struct); `SimNetClockSampleHookWellFormed<H>` is "has
  the hook, or has no member of that name", and the client session `static_assert`s it. This header names `SimNetClockSample`
  by a forward declaration only, so it stays free of the clock's og-net includes; a reference parameter of an incomplete type is
  enough for the concept.
- **Without the hook** a host compiles unchanged: the session still reads the record ahead of the decode and drops it, counted
  (`SimNetClientStats::clockSamples` and `clockSamplesUnheard`), never as a `badDecode`. The server session has no such record:
  the server's clock answers pings on the servicing thread and pushes nothing for its session.
- **Self-check:** a client host with the hook is still a client host, has the hook, and the probe sees it; the plain reference
  client host is well formed without it; a host whose `onClockSample` takes a second argument, and one with a private one, are
  not well formed.

## §5 Tests

The reference shapes are the header's self-check. `SimNetServerTest.cpp` instantiates the server session with a fake host
that records every stamp and line, and with a host that records `noteDelivered`; `SimNetClientTest.cpp` does the same for the
client session. `SimNetLifecycleUnregisteringHookTest.cpp` gives both sessions a host with `onLifecycleUnregistering` that records whether the
character is still in storage at the call (§4a). `SimNetClockSampleRouteTest.cpp` drives a real `SimNetClockClient` ping and pong
into a client session with and without `onClockSample` (§4b; `SimNetClient-rationale.md` §15).
