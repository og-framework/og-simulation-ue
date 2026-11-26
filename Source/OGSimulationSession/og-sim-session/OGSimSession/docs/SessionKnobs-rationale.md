<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: onRelayDelayFloorReplayed -- deleted by og-input-latency task P5-9 (the relay actors, their listeners and the session's two replay members); kept in the history -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (§2.3, G-03): the ini key that was built and removed on a ruling. It must NOT resolve -->
# `SessionKnobs.h` — rationale

`SessionKnobs.h` is og-sim-session's knob intake (og-session-runtime design §3.4, the `SessionKnobs` row): it reads the session's
`[OGNetcode]` keys through a `ConfigSource`, clamps and validates them, applies them to the manager (and, on the authority, the
floor to og-sim-net's server), and writes their start-up lines through a `SessionLogSink`. It is the docs pointer target of
`SessionKnobs.h`; its prohibitions are `SessionKnobs-guards.md`.

**If this file and `SessionKnobs.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Where it came from (og-session-runtime task 23)

Until og-session-runtime task 23 the intake was members of the Unreal session (`UESimulationSession.h`), reading `GConfig` and
logging with `UE_LOG(LogOGNet, …)`. Task 23 moved it in two commits (TeamDescription W6):

- **(a) the move** (root `4006cac`, OGSimulation `0eb05d0`): `clockDilationDoors` with its four `static_assert`s, the
  `SessionKnobs` struct and the seven function definitions were cut from `UESimulationSession.h` and pasted unchanged into this
  header (98.9 % of the changed lines are moved lines). At (a) this header was a fragment of the Unreal session, included only by
  it.
- **(b) the adaptation**: everything is in `namespace ogsession`; the functions are free function templates over their ports
  (§5); `GConfig` became a `ConfigSource`; `FString` became `std::string`; `FString::Equals(…, IgnoreCase)` became
  `sessionKnobsLines::equalsIgnoringCase` (§7); `FMath::Max` and `FMath::IsFinite` became `std::max` and `std::isfinite`; each
  `UE_LOG` became one formatted line handed to `logRouted` (§6); the `checkf` on the og-sim-net server became an `OG_CHECK`.
  The statements' order, the keys, the clamps, the role rules and the lines are unchanged, which the tests prove (§8).

Since og-session-runtime task 31 the calls are the runtime's: `SessionRuntime::start` reads the knobs once
(`readSessionKnobs`) and applies them (`applySessionKnobs` on the authority, then `applyResimTriggerPolicy` after both role
branches, then `applyLatencyKnobs`; `SessionRuntime-rationale.md`), logging through the runtime's clock engine. The Unreal
adapter's side is the `ConfigSource` it passes to `start` (`UEConfigSource`, `UEConfigSource-rationale.md`) and the log
routes the lines reach (`UESimulationLogRoutes-rationale.md`); `UESessionLogSink` is the routes' `SessionLogSink`, which the
game's `UESessionKnobsLineRouteTest.cpp` drives. Until task 31 the calls were three in the Unreal session's `beginPlay`.

## §2 The session knobs

Fourteen `[OGNetcode]` keys are read once, at composition, by `readSessionKnobs`, and applied by four functions, which the
Unreal session calls at the positions they had as its members:

| function | what it holds | where the Unreal session calls it |
|---|---|---|
| `readSessionKnobs(config, worldIsAuthority)` | the intakes (`⛔G-01`, `⛔G-02`, `⛔G-03`, `⛔G-04`), `RecentreSettleSamples` through its knob row's intake (§2.5); it reaches no session state and returns the requests in a `SessionKnobs` value | after the latency member's begin, before the backend builds its world: both roles |
| `applySessionKnobs(engine, manager, floorServer, knobs, logRelayDelayFloorAdvisory)` | the floor's clamp, set, ini line and advisory; the rotation width's clamp and set; the rotation width's proof line; the floor's publish through og-sim-net's server (`publishFloor`) | in the authority branch, after the neutral input and the og-sim-net server's start, before the tier consumer's start |
| `applyResimTriggerPolicy(engine, manager, knobs)` | the policy's parse, apply and proof line (`⛔G-05`) | after both role branches |
| `applyLatencyKnobs(engine, manager, knobs, clockDilation, clockDilationWindow)` | the two latency knobs' clamp and set, and their one proof line; the re-centre window's clamp, set and proof line, through its knob row (§2.5); the clock's jitter multiplier, then `applyClockRttSource` (§3); last, `applyClockDilation` (§4), which applies the dev doors through `applyClockDilationDoor` (§5) | after both role branches, directly after `applyResimTriggerPolicy` |

**Every knob takes the same four steps:**

| step | what | why it is not optional |
|---|---|---|
| 1 INTAKE | read the ini once, before the manager exists where possible | a value that arrives after the first publish has to be absorbed as a *change* |
| 2 CLAMP / VALIDATE | out of range is **reported**, never silently corrected | a floor that big is a sizing mistake the operator needs to see |
| 3 SET | stamp the effective value into the one shared `TimeConfig` | every derivation site must read one number |
| 4 PROVE | an **unconditional** `Warning` line naming the value actually stored | see below |

**Step 4 is at `Warning`, not `Log`**, because a game runs `LogOGNet` at `Warning` (the brawler project's
`Config/DefaultEngine.ini` sets it there), and a `Log` line does not exist at `Warning`. ⚠ **Not every knob meets step 4:**
the relay-delay floor's ini line is at `Log` and inside the key-present branch. The Unreal adapter's source accepts both ini homes
for every key, the game ini first, then the engine ini (`UEConfigSource-rationale.md`). **And the proof line is unconditional**,
because a line that is absent both when the key was read and when it was not cannot distinguish those two cases, which is the
whole question a proof line exists to answer. The rotation width's and the policy's proof lines report values **read back from
`TimeConfig`**, never the parsed request, so they cannot claim a setting the manager did not store; the floor's ini line prints
the clamped request it is about to store (the setter's clamp is idempotent, so the stored value is the same number, but that line
is not a read-back).

**None of these may become a cvar** (`⛔G-03`). The rotation width, because a run's probe output is read *against* the cadence
and a value that moves mid-run makes those readings unattributable. The resim policy, because it is pushed into every correction
cache and read at the correction-landing site (a client's drain, on the step's thread) **with no synchronization**, which is sound
only because it is written once at composition, before any correction can land.

*R0, task 26b (carried from the Unreal session's rationale §9.8 by og-session-runtime task 23):* the moved text said these are
"the only `GConfig` uses in this codebase"; the brawler's game module reads `GConfig` in two other places (its join screen's
recent address and its movement-scheme cvar), so they are the only ones in the session's start-up. It said the reason for
`Warning` was an ini setting that was false at the brawler's `b9f6d81` (`Verbose` then, the brawler's §11 C17); the brawler's
ini sets `Warning` again today. And it said every proof line reports values read back from `TimeConfig`: the floor's ini line
prints the clamped request (`clampedFloor`) it is about to store.

*R0, og-session-runtime task 23:* re-verified: `Config/DefaultEngine.ini` sets `LogOGNet=Warning`; `GConfig` is read by the
Unreal adapter's `UEConfigSource` for the knobs, and this header names no engine at all.

*Moved from the brawler's `OGBrawlerSimulationManager-rationale.md` §3 ("The session knobs") to the Unreal session's
rationale §9.8 by og-ue-simulationcomponentandmanagersplit task 26b, made game-neutral; moved here by og-session-runtime task
23. Its history (the brawler's §11 C1, C17, C18) stayed in the brawler's file.*

### 2.1 `RelayDelayFloorTicks`: the session floor

**Authority only, and that is correctness rather than economy** (`⛔G-01`). The floor is server-owned session state that is
*replicated* to clients. A client reading its own ini could disagree with the server, which is the two-ends-diverge failure the
tier ruling exists to avoid.

The clamped value is stamped into `TimeConfig` and then published through og-sim-net's server (`publishFloor`), which the Unreal
session emplaces just before the knobs are applied, so the publish always has a server (an `OG_CHECK` says so; the Unreal session
passes `nullptr` when it has none). The server sends the floor to every attached connection and to each connection that attaches
later; nothing publishes the floor after start-up.

Two intakes share one clamp (`clampRelayDelayFloorTicks`): this ini override, and the floor's receipt from the server (the Unreal
session's `onRelayDelayFloorReceived`; its replay twin `onRelayDelayFloorReplayed` was deleted by og-input-latency task P5-9),
where the value arrives off the wire as a `uint8` and is clamped rather than trusted. The manager's setter clamps again; the call
sites exist so an out-of-range value is *visible* in the log rather than silently corrected.

The floor's advisory (whether the floor sits below the hiccup baseline, or at the uniform-D level) is the caller's: the fifth
argument of `applySessionKnobs`, called with the clamped floor after the floor's ini line. The Unreal session passes its
`logRelayDelayFloorAdvisory`, which the floor's receipt calls too (`UESimulationSession-rationale.md` §9.4, *The floor advisory*).

### 2.2 `CorrectionRotationK`: the state rotation width

Controls how many characters' correction-state buffers `SimulationNetSync::sendCorrectionAll` writes per tick, round-robin, so
each character's state replicates at the tick rate times K over N. **Authority only, but not for the floor's reason:** `K` is
never replicated; only the authority sends corrections, and a client read would have no reader.

> ⚠ **The compiled default is `TimeConfig::correctionRotationK` and this document does not restate its value.** Read it at the
> declaration.

**Sentinel collision, harmless and knowingly.** An ini that literally reads `CorrectionRotationK=-1` cannot be told from an absent
key, because `-1` is also the intake's own "not present" sentinel. The proof line reports the effective number either way.

### 2.3 `ResimTriggerPolicy`: the resim-gate policy

Decides which landed corrections open the resim gate: `FrontierExact`, the compiled default, or `OnDisagreement`, the designed
trigger. The field lives on `TimeConfig`; the mechanism is og-simulation's `ResimGatePolicy.h` and `StateCorrectionCache`. The
brawler ships `ResimTriggerPolicy=OnDisagreement` in its `Config/DefaultEngine.ini`.

**There is deliberately no `ResimCooldownTicks` key** (`⛔G-03`). **Not authority-gated**: the resim gate exists only on a
predicting client; applying it on both roles keeps one `TimeConfig` shape and costs nothing. **Presence is a `bool`**, the value
a string; the parse is case-insensitive (§7), and an unrecognised policy string is **reported with the compiled default kept**
(`⛔G-05`).

For this knob the proof line is also the **behaviour-neutrality receipt**: every claim made from a later run is a claim about
*which policy was live*. `depthPolicy` therefore carries `(inert under FrontierExact)` rather than falling silent, and
`rateLimit = none (structural)` is stated rather than omitted. The depth text under `OnDisagreement` is formatted on its own
(`kResimGateDepthSkip`, `skip beyond <rollbackWindowTicks> ticks`) and then printed into the line, as the Unreal session's inner
`FString::Printf` did.

### 2.4 `RedundancyFrames` and `RelayResendEntries`: the latency knobs

Two integer keys, read by one intake and applied by `applyLatencyKnobs`. They feed `TimeConfig::redundancyFrames` (the client's
redundancy window, in frames) and `TimeConfig::relayResendEntries` (already-flushed relay entries the authority re-sends per
flush); og-simulation's `TimeConfig-rationale.md` §12 has what each does. **Not authority-gated** (`⛔G-04`). An out-of-range
value is reported by `[LatencyKnobs] ini [OGNetcode] … out of range, clamped to …`, clamped by the shared `latencyKnobs::`
function and set through the manager's setter, which clamps again. The proof line,
`[LatencyKnobs] redundancyFrames=… relayResendEntries=… (ini|compiled)`, is unconditional, at `Warning`, read back from
`TimeConfig`; its token is `ini` when either key was present.

### 2.5 `RecentreSettleSamples`: the clock's re-centre window

One integer key, applied after the two latency knobs; it feeds `TimeConfig::recentreSettleSamples` (og-simulation's
`TimeConfig-rationale.md` §14). Not authority-gated, for `RedundancyFrames`' reason. A negative value is reported
(`[ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=… out of range, clamped to 0`) and set to 0. The proof line,
`[ClockRecentre.Config] recentreSettleSamples=… (ini|compiled)`, is unconditional, at `Warning`; its tag is not the clock's
`[Clock.Recentre]`, which a client logs once per arm.

**Since og-session-runtime task 35 this key is a knob row** (og-simulation's `Config/KnobTable.h`, `KnobTable-rationale.md`),
`sessionKnobRows::kRecentreSettleSamples` in this header: tag `ClockRecentre.Config`, name `recentreSettleSamples`, clamp
`clockRecentre::clampSettleSamples` (og-simulation's `TimeConfig.h`, the hand-written `std::max(requested, 0)` it replaced),
apply `setRecentreSettleSamples`, read-back `getTimeConfig().recentreSettleSamples` as an `int32_t` (a `static_assert` in
`TimeConfig.h` keeps the compiled default inside `int32_t`; the ini path stores only clamped `int32_t` values, so `%d` and the
old `%u` print the same digits). `readSessionKnobs` reads it through the row's intake (`kRecentreSettleSamplesIntake`,
`knobTable::readKnob`) at the old read's position and keeps the reading in the same two `SessionKnobs` fields;
`applyLatencyKnobs` applies it with `knobTable::applyKnob` at the old block's position. The table's two lines are the old
lines byte for byte: `[<tag>] ini [<section>] <key>=<value> out of range, clamped to <clamped>` and
`[<tag>] <name>=<stored> (<ini|compiled>)`, `Warning` both. The two formats the block used, `kRecentreClamped` and
`kRecentreSession`, stay in `sessionKnobsLines` as the reference the row is checked against (§8); the intake no longer
formats them.

**The other thirteen keys stay hand-written**, because no generic row prints their lines (`KnobTable-rationale.md` §5):

| key | kept as written because |
|---|---|
| `RelayDelayFloorTicks` | its proof line is `[RelayDelayFloor] session floor = <n> ticks (ini override)`, at `Log`, printed only when the key was read, and followed by the caller's advisory |
| `CorrectionRotationK` | its proof line is `[StateRotation] session K = <k> (ini override\|compiled default)`, and `-1` is its absence sentinel |
| `ResimTriggerPolicy` | a word (`FrontierExact\|OnDisagreement`) with an `unrecognised` Warning, and a proof line that also carries `depthPolicy` and `rateLimit` |
| `RedundancyFrames`, `RelayResendEntries` | two keys share one proof line, `[LatencyKnobs] redundancyFrames=… relayResendEntries=… (ini\|compiled)`, with one token for both |
| `ClockRttSource` | a word (`Engine\|SimClock`), and its value shares the `[ClockSource.Config]` line with the jitter multiplier |
| `JitterMultiplier` | one value sets two fields, both printed on the `[ClockSource.Config]` line; its not-finite line says "defaults" |
| `ClockDilation` | a word (`Off\|Shadow\|On`), applied only on a client (the authority prints `mode=n/a`), its proof line carrying the setpoint, gain and deviation |
| `ClockDilationGainPerTick`, `ClockDilationMaxPercent`, `ClockDilationTargetOffsetTicks`, `ClockDilationCommonLeadTicks` | the doors: one `doors` line carries all five values with their tokens; `ClockDilationMaxPercent` is a percent stored as a fraction |
| `ClockDilationSetpoint` | a word (`OwnRtt\|Common`) whose `unrecognised` Warning names the setpoint it runs, and a field of the doors line |

### 2.6 The order of the four functions' lines

On the authority: the floor's lines and advisory (when the key is present), the rotation width's clamp line, `[StateRotation]
session`, the publish; then on both roles `[ResimGate] session`, `[LatencyKnobs]`, `[ClockRecentre.Config]`,
`[ClockSource.Config]`, and `[Dilation.Config]` (`mode=n/a` on the authority; on a client the proof line and the doors line). Each
clamp, "not a finite number" or "unrecognised" line comes before the proof line it qualifies. The transcription test compares this order on every
scenario (§8).

### 2.7 The retired fourth knob

There was a fourth key: a session-configurable **relay-ring retention depth**. The flush-on-poll write path replaced the mechanism
it sized (the stage capacity is `relayedInputRing::kMaxDepth`, a compile-time constant with no ini key to feed), and the key, its
clamp intake, its setter and its `[RelayDepth]` startup proof line were removed together. Guard **G-02** marks where the intake
used to sit, in `readSessionKnobs` between the floor's intake and the rotation width's. **It is an absence fence:** the
identifier it is about occurs nowhere in this tree, so the guard is the only record.

## §3 The clock's RTT source and jitter knobs

- **`ClockRttSource=Engine|SimClock`** (case-insensitive; the Unreal source trims surrounding blanks), read on both roles: it acts
  only on a predicting client, which reads its own ini. `applyClockRttSource` runs at the end of `applyLatencyKnobs` and calls
  the manager's `setClockRttSource`. An unrecognised value is reported (`[ClockSource.Config] ini [OGNetcode] ClockRttSource='…'
  unrecognised (expected Engine or SimClock), keeping the compiled default`) and the compiled default kept. The proof line,
  `[ClockSource.Config] clockRttSource=<Engine|SimClock> (<ini|compiled>) jitterMultiplier engine=<m> simClock=<m>
  (<ini|compiled>)`, is unconditional, at `Warning`, read back from `TimeConfig`. The setter runs before any sample can reach the
  estimator: the Unreal session configures before `startStepping` (og-simulation `SimulationManager-rationale.md` §16).
- **`JitterMultiplier=<number>`**, read on both roles and applied in `applyLatencyKnobs` after `RecentreSettleSamples`, before
  `applyClockRttSource`. One value sets **both** sources' multipliers through the manager's `setJitterMultiplier`
  (`jitterMultiplier` and `simClockJitterMultiplier`, og-simulation `TimeConfig-rationale.md` §17). A value that is not a finite
  number is reported (`[ClockSource.Config] ini [OGNetcode] JitterMultiplier is not a finite number, keeping the compiled
  defaults`) and both defaults kept; one outside `clockJitter::clampJitterMultiplier`'s 0 to 4 is reported as `… out of range,
  clamped to …` and clamped. The result is printed on the `[ClockSource.Config]` line; its trailing token is `ini` only when the
  key was applied.

## §4 The clock dilation knob and its proof line

`ClockDilation=Off|Shadow|On`, case-insensitive. `applyClockDilation` applies it at the end of `applyLatencyKnobs`, after
`applyClockRttSource`.

- **The authority** (`!manager.runsPrediction()`) parses nothing and builds nothing: it prints `[Dilation.Config] mode=n/a` and
  returns. Its step rate is the reference every client's lead is measured against.
- **A client** maps `Off`, `Shadow` and `On` to their modes. Any other value logs one Warning, `[Dilation.Config] ini
  [OGNetcode] ClockDilation='…' unrecognised (expected Off, Shadow or On), running Off`, and runs the compiled default, `Off`.
- **The proof line**, unconditional on a client, at `Warning`: `[Dilation.Config] mode=<Off|Shadow|On> setpoint=<OwnRtt|Common>
  gainPerTick=<g> maxDeviation=<d> (<ini|compiled>)`. Every value is read back from the controller's stored `config()`. The
  trailing token is the mode's. og-time-dilation's `lead_split.py` parses this line with a pattern anchored at the token.
- **The doors line** follows it: `[Dilation.Config] doors gainPerTick=<g> (<src>) maxPercent=<p> (<src>) targetOffsetTicks=<o>
  (<src>) setpoint=<s> (<src>) commonLeadTicks=<x> (<src>)`, each value read back with its own token (§5).
- **The controller and the window** are the caller's `std::optional<ClockDilationController>` and
  `std::optional<ClockDilationWindow>`, emplaced here on every client session, `Off` included, and on no authority. The Unreal
  session passes its `m_clockDilation` and `m_clockDilationWindow`; its client configures on the game thread, the thread that
  later runs the stage (`UESimulationSession-rationale.md` §22).

## §5 The dev doors and their clamps

Five more keys, read on both roles and applied only on a client by `applyClockDilation`, into the config the controller and the
window are built from. They apply in `Shadow` as in `On`; `Off` never calls the controller.

| key | config field | door |
|---|---|---|
| `ClockDilationGainPerTick` | `gainPerTick` | clamped to [0, 0.05] (`clockDilationDoors::clampGainPerTick`) |
| `ClockDilationMaxPercent` | `maxDeviation` = percent / 100 | clamped to [0, 2] % (`clampMaxPercent`) |
| `ClockDilationTargetOffsetTicks` | `targetOffsetTicks` | clamped to [−2, 2] ticks (`clampTargetOffsetTicks`) |
| `ClockDilationSetpoint` | `setpoint` | `OwnRtt` or `Common`, case-insensitive; any other value logs one Warning, `[Dilation.Config] ini [OGNetcode] ClockDilationSetpoint='…' unrecognised (expected OwnRtt or Common), running <the compiled setpoint>`, and runs the compiled setpoint (`OwnRtt`) |
| `ClockDilationCommonLeadTicks` | `commonLeadTicks` | clamped to [0, 10] ticks (`clampCommonLeadTicks`); the compiled default is og-simulation's, 3.65; only `Common` reads it |

- **The four number doors** go through `applyClockDilationDoor`. A value that is not finite logs `[Dilation.Config] ini
  [OGNetcode] <key> is not a finite number, keeping the compiled default` and keeps it (token `compiled`). An out-of-range value
  logs `[Dilation.Config] ini [OGNetcode] <key>=<v> out of range, clamped to <c>` and runs the clamped value (token `ini`).
- **The clamps are the controller's constraints in runtime form**, checked by the four `static_assert`s beside
  `clockDilationDoors`: the `ClockDilationMaxPercent` ceiling keeps `maxDeviation` under the render bracket's bound, 0.2 (og-simulation's
  `ClockDilationController-rationale.md` §7); neither the gain nor the deviation door admits a negative value; each compiled
  default lies inside its own door; the `Common` lead's door admits no negative lead.
- **What no door reaches** keeps its compiled values and their `static_assert`s. **One compiled check the door does not keep:**
  og-simulation's `slewPerCall ≤ maxDeviation`; below 0.2 % the clamp binds rather than the slew, and 0 % turns the law off.
- **The gain's ceiling, 0.05**, is ten times the default; the loop's per-frame step `f · K_p · Δt` stays small against 1
  (og-simulation's `ClockDilationController-rationale.md` §8). **The `Common` lead's range, [0, 10] ticks**, is a dev door's;
  each client's band guard still clamps the target into its own band. **The offset's range, ±2 ticks**, is for
  og-time-dilation's G7 seen-failing arm.

## §6 The lines: one format each, a level token, a sized buffer

- **The formats are named constants** in `sessionKnobsLines` (22 lines and the depth text), so the Unreal parity test can
  format each one beside its old `UE_LOG` format. Since og-session-runtime task 35 two of them, `kRecentreClamped` and
  `kRecentreSession`, are the reference shapes of the `RecentreSettleSamples` row (§2.5): the row composes those two lines, and
  `SessionKnobRowsTest.cpp` checks they equal these formats (§8).
- **A line carries its level.** A line logged at `Warning` before task 23 starts with the token `[Warning]`; the floor's ini line,
  at `Log`, carries none. That is og-sim-session's `logRouted` contract (`SessionEngine-rationale.md`): one sink for lines that
  carry their own `[Level][Tag]` prefix. The Unreal adapter's sink reads the token for the level, picks the category from the
  line's tag through the route table (each of the seven tags has a row: `LogOGNet`, plain level `Log`), and prints the line
  **without** the token, so the log reads byte for byte as the old `UE_LOG` did (`UESimulationLogRoutes-rationale.md`). A
  `static_assert` in the Unreal session checks each of the 22 formats' category and level against its old `UE_LOG`.
- **A sized buffer.** `formatLine` measures the line with `snprintf(nullptr, 0, …)` and formats it into a `std::string` of that
  size, so no line is cut: a 309-digit `%.4f` of a finite out-of-range value, or a long unrecognised word, prints whole, as
  `FString::Printf` did. A line without arguments is passed through unformatted (neither such line contains a `%`).
- **`%hs` became `%s`**: the old Unreal formats printed the og-simulation words (`modeWord`, `setpointWord`) with `%hs`; every
  argument is a narrow string now.
- **The em dash** in the `ResimTriggerPolicy` line is written as its UTF-8 bytes (`\xE2\x80\x94`), so the line does not depend
  on the compiler's execution character set; the Unreal router decodes each line as UTF-8.

## §7 The case-insensitive compare

`FString::Equals(…, ESearchCase::IgnoreCase)` compares through `FGenericPlatformStricmp`, which folds a pair of characters only
when **both** are ASCII; any other pair must be equal. Every word the intake accepts is ASCII, so the byte-wise ASCII fold of
`equalsIgnoringCase` on the UTF-8 value is the same relation: a non-ASCII byte never equals an ASCII byte. A `static_assert` beside
it pins a mixed-case match, a trailing blank, a digit for a letter and a non-ASCII byte.

## §8 The ports, and how the tests prove the move

**The ports.** `readSessionKnobs` takes a `ConfigSource` (`SessionEngine.h`): `readInt`, `readDouble` and `readString`, each
`std::optional`, absent when the key is. Every apply function takes a `SessionLogSink` (`SessionEngine.h`: only `logRouted`), the
manager (any type with the setters and `getTimeConfig`; the Unreal session passes its `SimulationManager`) and what it writes:
the floor's server (any type with `publishFloor(uint8_t)`, as a pointer, `OG_CHECK`ed), the advisory callable, and the two dilation
optionals. They are template parameters constrained only by the calls made on them.

**The tests.**
- `og-sim-session/tests/SessionKnobsTest.cpp` (`[Session][SessionKnobs]`): a **transcription** of the seven functions as they
  stood at the (a) commit (OGSimulation `0eb05d0`, `SessionKnobs.h`, each cited by line), with `UE_LOG` replaced by a recorder,
  `GConfig` by a two-ini map (game, engine) with `GetInt`, `GetDouble` and `GetString`, `FString` by `std::string`, and its own
  copy of the doors. For each of the 14 keys, every value of its list (absent, in range, out of range on each side, `-1`,
  non-finite, unrecognised words, mixed case, padded, non-ASCII), placed in the game ini, the engine ini and both, on both roles,
  plus no `GConfig`, every key absent, the tree's own `[OGNetcode]` and every key at once (1,838 scenarios, 12,666 events): the
  lines (level and text), their order, the advisory and the publish, the knobs as read and after the apply, and the manager's
  values and the dilation config are equal. A hidden `[.][SessionKnobsSurvey]` case prints the per-key counts the main case
  checks are not zero.
- `Source/OGSimulationTests/SessionKnobsFormatParityTest.cpp`: every moved format through `FString::Printf` (the old `TEXT`
  literal) and through `formatLine`, compared byte for byte in UTF-8 (4,271 lines: integer extremes, `-0.0`, subnormals,
  `DBL_MAX`, half-way roundings, non-ASCII, `%` in a word, a 3,000-character word), and each `[Warning]` token against the old
  level.
- `Source/OGBrawlerUnreal/UESessionKnobsLineRouteTest.cpp` (`OGBrawler.Session.KnobLineRoute`, automation): each of the 22 lines
  logged the old way and through `UESessionLogSink`, captured from `GLog`: equal text, category and level.
- `og-sim-session/tests/SessionKnobRowsTest.cpp` (`[Session][SessionKnobs]`, og-session-runtime task 35): the
  `RecentreSettleSamples` row's lines, on both roles, absent and for ten values from `INT32_MIN` to `INT32_MAX`, equal
  `formatLine(kRecentreClamped, …)` and `formatLine(kRecentreSession, …)` byte for byte, so the parity test's pins on those two
  formats still describe the lines the session prints; and a dummy key read through `MapConfigSource` by a test-local table
  (`KnobTable-rationale.md` §6), with its negative: without the row the key is not read. The transcription test above is
  unchanged and passes, so the moved key's lines, their order and the stored values are the transcription's.
