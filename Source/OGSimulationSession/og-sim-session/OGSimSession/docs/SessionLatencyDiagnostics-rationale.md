<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: FPlatformTime::Seconds -- Unreal Engine function, outside every scan root -->
<!-- lint-external-ref: UWorld::OnPostTickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root -->
<!-- lint-external-ref: FString::Printf -- Unreal Engine function, outside every scan root -->
# `SessionLatencyDiagnostics.h` — rationale

Companion to `SessionLatencyDiagnostics.h`. The header carries no prose: a docs pointer, code, assertion messages and one-line
tags `⛔G-nn` → an entry in `SessionLatencyDiagnostics-guards.md`.

> **Licence.** This tier is **MPL-2.0**, matching the code it describes.

* **§1** what the class is, and what the Unreal adapter keeps;
* **§2** the two arms, `kEnabled`, and who picks one;
* **§3** the port: the clock, the log sink and the thread checks;
* **§4** the doors, what each posts, and its thread row;
* **§5** the mailbox and the window tick;
* **§6** reading production state;
* **§7** the window lines;
* **§8** the tests;
* **§9** provenance.

---

## §1 What it is

`ogsession::SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, kEnabled>` runs og-simulation's latency-budget probe
(`latencyBudget::LatencyBudgetProbe`, `OGSimulation/LatencyBudgetProbe.h`, whose rationale explains keys, joins, counters,
windows and percentiles) for one session. It is the engine-free half of what was Unreal's `UELatencyBudgetDiagnostics`, moved
here by og-session-runtime task 37:

- **the doors**: the capture wrapper (`wrapInputProvider` and the lambda it returns), `forgetCharacter`, `noteWireSample`,
  `post_Internal`, `stampClientReceived_Internal`, `noteWireSample_Internal`, `stampHandedAndLeft_Internal`,
  `stampServerReceived_Internal`, `stampCorrectionHanded_Internal`, `stampAfterStep_Internal` (with the relayed-read shadow,
  `postRelayedReads_Internal`) and `stampStepEnd_Internal`;
- **the mailbox** (`PostedEvent`, the event and clock mailboxes, the drop count) and the probe they feed, created by
  `begin(isAuthority)` with the role's hop masks;
- **the frame-thread work**: `onPostTickFlush` (the `H3` and `H3c` end) and `tickWindow_Frame` (the drain, the `Rendered`
  stamp, the window and its lines), and the clock-count window (`clockCountsInWindow`, `restartClockWindow`).

The probe's only output is its window lines; nothing reads them back. Deleting the class and every caller changes no
production behaviour (og-simulation's `DiagnosticsConventions.md` §2).

**What the Unreal adapter keeps** (`OGSimulationUnreal/UELatencyBudgetDiagnostics.h`, its rationale §15): the class derives
from this one with its port, `UELatencyBudgetPort`, and keeps what names an Unreal type: the gate macro
`OG_LATENCY_BUDGET_DIAGNOSTICS`, which picks the arm (§2); the log category `LogOGLatencyBudget` behind the sink (§3);
`begin(UWorld&, bool)`, which calls this class's `begin` and binds `onPostTickFlush` to `UWorld::OnPostTickFlush` with the
session's actor as the weak object; `end`, which unbinds it; `tick_GameThread`, the frame-thread entry the session calls in its
post-physics pass, which calls `tickWindow_Frame`; and `instanceFor` / `withInstance`. Every call site in Unreal
(`UESimNetHost.h`'s stamp calls, the session's step hooks through og-sim-session's `SessionTickGame` and `SessionNetHooks`, the
lifecycle's wrap) reaches the doors through the derived class, so it calls this class's members.

---

## §2 The two arms: `kEnabled`

`SessionLatencyDiagnostics<…, true>` is the feature; `SessionLatencyDiagnostics<…, false>` is the diagnostics-off arm. The
engine decides which, at compile time; on Unreal the adapter passes `OG_LATENCY_BUDGET_DIAGNOSTICS != 0`, so the macro's rule is
unchanged (`(!UE_BUILD_SHIPPING)` by default, per including module; `UELatencyBudgetDiagnostics-rationale.md` §3).

**The off arm compiles to the same no-ops the Unreal class's off arm was** (lead ruling under task 37, item 1):

- it has no data member (`std::is_empty_v`, asserted in the tests), so a member that only the feature has cannot be read in
  production code that compiles at 0;
- every door is an inline no-op with the same signature, so no call site carries an `#if`;
- `wrapInputProvider` keeps its presence check in both arms: the off arm's private `wrap` returns the provider unchanged, and
  the `OG_CHECK` holds that a present provider stays present and an empty one empty. Provider presence is the core's
  local-versus-remote identity test, so the wrapper may never change it, in either arm;
- it never calls the port, so a diagnostics-off build reads no clock and checks no thread for the diagnostics.

**The Unreal adapter owns the arm's choice; this header owns both arms' code.** The Unreal class's own off arm is now only the
four Unreal-typed members (`begin(UWorld&, bool)`, `end`, `tick_GameThread`, `withInstance`), each empty. Seen: the fence build
of the game module at `OG_LATENCY_BUDGET_DIAGNOSTICS=0` (`UELatencyBudgetDiagnostics-rationale.md` §5, step 2) compiles
(og-session-runtime task 37's notes).

---

## §3 The port: `SessionLatencyPort`

Three requirements, each on a `const` port, each with one negative beside the concept (og-session-runtime task 33b's pattern):

| requirement | what it is | negative |
|---|---|---|
| `nowSeconds()` → `double` | **the clock** every stamp the doors take at "now" reads | `NowSecondsReturnsFloat` |
| `logLatency(line)` → `void` | **the log sink**: one finished window line | `LogLatencyReturnsBool` |
| `checkOp(op)` → `void` | **the thread checks**: the door's `SessionOp` row | `CheckOpReturnsBool` |
| all three on a `const` port | the doors run on the step's thread and the frame thread alike | `NowSecondsOnAMutablePort` |

**One clock.** The doors' times are compared, in the probe, with times other code took: og-net's `recvSeconds` (the
`NetReceived` stamps the hosts pass in), the game's `inputSampledSeconds` (the `H1` start) and, on a client, the step start the
tick driver passes to `stampAfterStep_Internal`. Each hop that mixes two sources is only right if they read one clock. So the
port's clock **is** the engine's clock those use: on Unreal `UELatencyBudgetPort::nowSeconds` is exactly
`FPlatformTime::Seconds()`, the call the doors made before the move (lead ruling under task 37, item 2). A test may inject a
clock (`SessionLatencyDiagnosticsTest.cpp`'s port); production may not give this class a second one.

**The log sink keeps the category and the level.** On Unreal, `logLatency` is `UE_LOG(LogOGLatencyBudget, Warning, TEXT("%s"),
ANSI_TO_TCHAR(line))`: the lines keep their own category and `Warning`, as before. They are not routed through `RouteOGLine` and
have no row in the composed log-route table (`SessionLogRoutes-rationale.md`): a row would be a second home for the level, and
`latency_anatomy.py` keys on the text, not the route (lead ruling under task 37, item 3).

**The thread checks.** Each door calls `m_port.checkOp(<its row>)` first. On Unreal the port asks the runtime's
`SessionRuntime::checkDiagnosticsOp(op)` (`SessionRuntime-rationale.md`):

- **before the runtime has started** there is no thread table yet (it is built from the role at `start`), and the call checks
  nothing. The Unreal session calls `begin` and then `start` in one frame-thread call (`beginPlay`), so no door runs between them;
- **during a client's stopped drain** (`SessionThreads-rationale.md` §8) it checks `DrainStopped` instead of the door's row: that
  drain runs og-sim-net's receive handlers once on the frame thread after the steps stopped, and they reach
  `stampClientReceived_Internal` and `noteWireSample_Internal` there. The flag is the session clock's
  (`SessionClock::inStoppedDrain`), set by the scope that drain opens;
- otherwise it is `SessionThreadChecks::checkOp(op)`, an `OG_CHECK` (on Unreal a `checkf`, live in Development).

---

## §4 The doors

Each door has its own `SessionOp` row (`SessionThreads.h`, og-session-runtime task 37). A thread "—" means the role never calls the
door, so the check stops a call there.

| door | posts (through the mailbox unless said) | row | client | authority | the caller |
|---|---|---|---|---|---|
| `wrapInputProvider` | nothing; resolves the input-sample source once (G-02) | `LatencyWrap` | frame | — | the client lifecycle's `tryRegister` (`SessionLifecycle.h`), through the runtime host's wrap |
| the wrapped provider | `InputSampled` (when the game's time is above 0), then `Captured` at now (Input) | `LatencyCapture` | step | — | the core's input collection, at capture |
| `stampClientReceived_Internal(stream, lane, tick, recv)` | `ClientReceived` at now; for State, a heal-by-correction item when the relay store has a newest arrival; then `NetReceived` at `recv` | `LatencyClientReceived` | step | — | `UESimNetLinkSession::stampClientReceived` (`UESimNetHost.h`), og-sim-net's client drain |
| `noteWireSample_Internal(rtt)` | a `Wire` sample, half the round trip | `LatencyClientWireSample` | step | — | `SessionRuntime::onTimingInfoReceived` (og-sim-net's client drain) |
| `noteWireSample(rtt)` | a `Wire` sample straight to the probe (no mailbox) | `LatencyServerWireSample` | — | frame | the base server host's `rttMs` (`UESimNetHost.h`), from the RTT publish |
| `stampHandedAndLeft_Internal(lane, captureTick)` | `Handed` and `LeftProcess` at now (Input) | `LatencyHanded` | step | runner | a client channel's handed callback (the runtime's `installInputStamps`); the server host port's `stampHanded` |
| `stampServerReceived_Internal(lane, captureTick, recv)` | `ServerReceived` at now, then `NetReceived` at `recv` (Input) | `LatencyServerReceived` | — | runner | the server host port's `stampServerReceived` (`UESimNetHost.h`) |
| `stampCorrectionHanded_Internal(lane, tick, handed)` | `Handed` and `LeftProcess` at `handed` (State), once per new correction tick of the lane | `LatencyCorrectionHanded` | — | runner | `SessionNetHooks::stampCorrectionsHanded`, after each authority batch |
| `stampAfterStep_Internal(start)` | authority: `Consumed` per used capture; client: the pending `ClientReceived` → `Consumed` (State), the relayed reads, the clock counts on a change | `LatencyAfterStep` | step | runner | `SessionTickGame::onStepStarted` |
| `stampStepEnd_Internal()` | `StepEnd` at now per simulatable (State) | `LatencyStepEnd` | step | runner | `SessionTickGame::onStepEnded` |
| `forgetCharacter(id)` | drops the probe lane; touches no step-thread map (G-04) | `LatencyForget` | frame | frame | the Unreal session's unregistration |
| `onPostTickFlush()` | the probe's pending `Handed` → `LeftProcess` at now, both streams (no mailbox) | `LatencyPostTickFlush` | frame | frame | `UWorld::OnPostTickFlush`, bound by the Unreal `begin` |
| `tickWindow_Frame()` | drains, stamps `Rendered`, closes the window (§5) | `LatencyWindow` | frame | frame | the Unreal `tick_GameThread`, from the session's post-physics pass |

`post_Internal` and `postRelayedReads_Internal` are helpers of the doors above and check no row of their own. `SessionThreads.h`'s
`latencyDoorsKeepTheirSide` holds the table's shape: the step doors never name the frame thread, and the frame doors never name
another. The rows were confirmed live by the smoke after the move (0 asserts on every process; task 37's notes).

The stamps' meaning, the hops they start and end and their measurement error are unchanged by the move; they are
`UELatencyBudgetDiagnostics-rationale.md` §6, §11, §13 and §14.

---

## §5 The mailbox and the window tick

Every step-thread door posts a `PostedEvent` (an `Event` plus the heal-by-correction delay and flag, trivially copyable) into
an `SpscMailbox` of 4096; the client clock's counts go into a second one of 64 when they change. On each role every producer
is one thread: a client's step, or the authority's runner (`UELatencyBudgetDiagnostics-rationale.md` §13, §14); the frame thread
is the only consumer and the probe's only owner.

`tickWindow_Frame` keeps `tick_GameThread`'s order and returns exactly (lead ruling under task 37, item 4):

1. its row's check; then return without a probe, a mailbox or a manager;
2. drain the event mailbox into the probe (`stampHealedByCorrection` for a flagged item, `apply` for the rest), then add the
   dropped count; drain the clock mailbox into the latest counts and add its dropped count;
3. read now and the role (`runsPrediction`);
4. stamp `Rendered` on the newest pending `StepEnd` per lane;
5. close the window if due; if not, return;
6. one line per enabled hop, then the window line, each to the sink; reset the dropped count only after the window line.

---

## §6 Reading production state

The class reads its owner's runtime through **one const reference** (`m_owner`), and only through the runtime's public const
accessors (the Unreal session declares no friend since og-session-runtime task 31): `getManager()` (`currentIntegratedTick`,
`runsPrediction`, `getClientClock`), `storage()` (`forEachSimulatable`) and `inputResolution()` (`getLastUsedCaptureTick`,
`getDiagnostics().relayedReadObservations`, `findRemoteInputCache`). `OwnerT` is whatever has `runtime()`; on Unreal the session.
A `static_assert` beside `m_owner` requires the referenced type to be `const`. Every one of those reads is on the thread that
writes it (`UELatencyBudgetDiagnostics-rationale.md` §6, "Threads").

---

## §7 The window lines

The three formats are `sessionLatencyLines::kHopLine`, `kHealHopLine` (the `H6L` line, with the two heal counters appended) and
`kWindowLine`, formatted with `std::snprintf` into a 2048-byte line by `formatHop` and `formatWindow`. They are the Unreal
adapter's `TEXT()` formats byte for byte: the role (`Client`/`Server`) and the hop name as `%s`, each `uint32_t` counter as `%u`,
the milliseconds as `%.3f` of the seconds times 1000, the window's length as `%.2f`, and `mailboxDropped` as `%llu` of an
`unsigned long long` cast. The sink prints the line with `%s`. Examples are `UELatencyBudgetDiagnostics-rationale.md` §7.

---

## §8 The tests

- **`og-sim-session/tests/SessionLatencyDiagnosticsTest.cpp`** (`[Session][SessionLatency]`; CTest and the Unreal LLT runner):
  one case per door, transcribed from `UELatencyBudgetDiagnostics.h` at root `7d58a26` (each case names its file:line): the
  wrapper (presence, the source resolved once, `InputSampled` then `Captured`; a zero sample time), the mailbox before and after
  `begin`, the client receipt (Input; State with and without a valid relay store), both wire samples, the hand-over, the
  server's receipt, the correction dedup per lane, the authority's and the client's step start (the pending stamp, the read
  shadow's prime, use and rewrite, the clock counts on a change), the shadow's bound, the step end, the forget (which leaves the
  step-thread maps), the window (its returns, the drain, the `H2` it closes, the window line's text), the clock window, the rows
  each door asks, the off arm (empty, presence kept, the port never called), and the three formats against the old text. A
  test-only friend, `sessionLatencyTest::Inspector`, drains the mailbox to compare each posted event.
- **`Source/OGSimulationTests/SessionLatencyFormatParityTest.cpp`** (the Unreal LLT runner): `FString::Printf` with the old
  `TEXT()` formats against `formatHop` / `formatWindow` through the sink's `%s`, both roles, every hop, edge values (0, −0,
  rounding edges, `DBL_MAX`, infinity, `UINT32_MAX`, `UINT64_MAX`).
- **`SessionThreadsTest.cpp`**: the thirteen rows in the expected table, and the counts of rows without a thread.

---

## §9 Provenance

Written by og-tickdriver task 18 as a game-module class; made a template in OGSimulationUnreal by
og-ue-simulationcomponentandmanagersplit task 19a; its doors and the step and runner sides grew through og-input-latency tasks 6,
P5-5, P5-17 and P5-20. og-session-runtime task 37 moved the engine-free half here in two steps (W6): **(a)** the twelve out-of-line
door definitions, byte for byte, included where they stood (root `d6e247f`); **(b)** this class: the in-class mailbox types and
state, `ClockCounts`, `wrapInputProvider` and `begin`'s probe and mailbox construction quoted from the root; `FPlatformTime::Seconds`
→ the port's clock; `checkf(IsInGameThread())` and the presence `checkf` → the thread rows and `OG_CHECK`; the three `UE_LOG`s →
the formats and the sink; `uint32` → `uint32_t`; the client clock read through `const auto&` instead of naming
`ClientPredictionClock`. The guards moved with their sites (`SessionLatencyDiagnostics-guards.md`, each entry names its old id).
