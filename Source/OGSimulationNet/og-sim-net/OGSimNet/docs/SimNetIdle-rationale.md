<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetIdle.h` — rationale

`SimNetIdle<EndpointT>` is the authority loop's idle hook over og-net. The tick driver's `FreeRunning` arm idles between its
step deadlines by calling the game's `idleUntil(deadlineSeconds)` (`TickDriverIdleGame`, og-simulation's `TickDriverConcepts.h`;
its result type `IdleWake` is in og-simulation's `TickDriverGameTypes.h`).
A server game that runs its sim traffic over og-net forwards that call to `SimNetIdle::idleUntil`, which services the server's
endpoint until the deadline and says why it returned. The header states no prohibition, so it has no guards document.

**If this file and `SimNetIdle.h` disagree, the header is authoritative and this file is stale.**

---

## §1 The mapping

`idleUntil(deadlineSeconds)` is one `serviceUntil(deadlineSeconds)` on the endpoint, and `idleWakeFor` turns its
`ognet::ServiceResult` into the driver's `IdleWake`:

| `ServiceResult` | `IdleWake` | why |
|---|---|---|
| `datagrams > 0` | `Activity` | og-net received and handled at least one datagram on this thread. Its handlers ran (the transport pushed what they carried into the session's inbox), and the commands they queued were flushed before the return. The driver re-pumps at once, so the session's `beforePump` drain sees the arrival in the same wake |
| `reachedDeadline` and no datagram | `Deadline` | og-net's clock reached the deadline with nothing received |
| `stopped` and no datagram | `Unavailable` | a stopped endpoint cannot wait; the driver sleeps on its runner host to the approach instead of calling again at once |

- **A datagram wins.** A pass that handled datagrams and then reached its deadline, or stopped, is still an `Activity` wake:
  the arrival has to be drained before the runner idles again. A stopped endpoint's next call returns `stopped` at once, which
  is then `Unavailable`.
- The three `static_assert`s on `idleWakeFor` pin the table at compile time.
- **Nothing else can come back.** og-net's service loop (`EndpointCore::serviceUntil`) leaves only after receiving at least
  one datagram, on reaching its deadline, or on stopping. `idleUntil` checks that with an `OG_CHECK`; where the check is
  compiled out, a result with none of the three maps to `Unavailable`, the one wake after which the driver never calls again
  without sleeping first.

## §2 The contract it meets

`TickDriverConcepts-rationale.md` states the hook's contract for the driver (og-simulation). `SimNetIdle` meets it through
og-net:

- **It blocks while the deadline is ahead.** `serviceUntil` waits in the backend's receive (on Win64 the high-resolution
  waitable timer, `BsdDatagramBackend`'s `hasHighResolutionWait`) and wakes early only to receive or for og-net's own timers,
  which it handles and keeps waiting.
- **A deadline at or before now is one non-blocking pass.** og-net then receives with a zero timeout and returns, as a
  datagram or as the deadline.
- **It never spins.** A persistent receive error no longer returns at once: og-net waits it out until its next timer or the
  deadline (`EndpointCore-guards.md` G-10, og-net task P1-9's second rework). So `SimNetIdle` needs no receive-error
  detection of its own, and returns `Unavailable` only for a stopped endpoint. (The P11 design's interim rule, one pass and
  `Unavailable` whenever `receiveErrors` grew, was for the og-net that still spun.)
- **`Deadline` is never early.** og-net reports `reachedDeadline` only once its clock is at or past the deadline, so the
  driver's early-wake fallback (three early `Deadline` returns, then a sleep) does not fire on this idle while both use one
  clock (§3).
- **The slack.** With `idleSlackSeconds > 0` the driver idles to the deadline minus the slack, then polls `idleUntil(now)`
  to the deadline. Each poll is one non-blocking og-net pass, so arrivals are still handled at once while the runner lands on
  the deadline by polling: the spin-precise idle that the hidden timing case measures (§5).

## §3 One clock, one thread

- **The clock.** The deadline is absolute seconds on the tick driver's engine clock. og-net compares it with its own host's
  `nowSeconds`. The composition that builds the endpoint must give og-net that same clock; with two clocks a deadline would be
  early or late by their offset.
- **The thread.** og-net fixes an endpoint's servicing thread at its first `serviceUntil` and refuses a second thread with
  a check (mode B). On the server the authority loop's runner services the endpoint, through this idle, and no other thread
  calls `serviceUntil` on it. The endpoint never starts its own thread there.

## §4 What it requires of the endpoint

`SimNetIdleEndpoint<E>`: `serviceUntil(double)` returning `ognet::ServiceResult`. og-net's `NetEndpoint` has it; so does the
Unreal plugin's `SimLaneEndpoint`, which forwards it to the og-net core of the engine-lane driver. The compile-time
self-checks (`ogsimnet::simNetIdleSelfCheck`, in `tests/SimNetAuthorityLoopTest.cpp`, §5) pin that an endpoint without the
member, or one that returns anything but a `ServiceResult`, is refused, that `idleUntil` returns `IdleWake`, and that a game
which forwards `idleUntil` to a `SimNetIdle` satisfies `TickDriverIdleGame`.

They live in the test, not the header, since og-session-runtime task 18: the last one needs og-tickdriver's concepts header
(`TickDriverIdleGame`, and `tickDriverGameSelfCheck::Conforming` as the game's base), and og-sim-net's headers depend only on
og-sim's vocabulary (`IdleWake`, in `TickDriverGameTypes.h`), not on og-tickdriver.

## §5 Tests

`SimNetAuthorityLoopTest.cpp` (`[NetTransport]`):

- at compile time, the self-checks of §4 (`static_assert`s at namespace scope; the test target does not build if one fails);
- each `ServiceResult` maps to its wake, and the deadline reaches `serviceUntil` unchanged;
- over `VirtualNetwork` in virtual time, a `FreeRunning` driver whose game forwards `idleUntil` here relays a SimInput that
  arrives mid-idle in the same wake (relay latency 0); that case is the mapping's seen-failing
  arm (`Activity` mapped to `Deadline` makes the relay wait for the next deadline);
- a hidden `[.][OGSimDeath]` case: a result with no wake reason fires the `OG_CHECK`;
- a hidden `[.][OGSimTiming]` case over the real BSD backend on loopback, with a 2 ms slack: the runner's `lateUs` p99 is at
  most 100 µs.
