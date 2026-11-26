<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionLogRoutes.h` — rationale

`SessionLogRoutes.h` holds og-sim-session's own log-route rows, `kSessionLogRoutes`, and the engine-free composition of every
engine-free module's rows, `kComposedLogRoutes`: og-simulation's, then og-sim-net's, then og-sim-session's. An engine adapter
appends its own rows to that and routes each line by the first row whose prefix begins it. It is also the docs pointer target
of the two generated test headers, `tests/SessionLogRoutesGolden.h` and `tests/SessionLogRoutesEmittedTags.h` (§6). The header
states no prohibition (its rules are `static_assert`s), so there is no guards file.

**If this file and `SessionLogRoutes.h` disagree, the header is authoritative and this file is stale.**

---

## §1 What it is for

A line the session runtime writes begins with a tag (`[ConnectionTier] …`, `tryRegister: …`), optionally behind a
`[Verbose]`, `[Warning]` or `[Error]` token. The runtime does not decide the category a line lands in: the engine's sink does,
with a first-match table of prefixes (Unreal: `UESimulationLogRoutes.h`'s `kOGLogRoutes`). A tag with no row lands in the
engine's default category (Unreal: `LogOG`).

Until og-session-runtime task 36 every row for a tag the runtime emits sat in the Unreal adapter's own block, so a second
engine adapter would have had to copy them. Since task 36 **each module declares the rows for its own lines** (design §3.6,
C5), and this header puts them together:

| block | header | rows |
|---|---|---|
| og-simulation | `OGSimulation/SimulationLogRoutes.h`, `kSimulationLogRoutes` | 28 |
| og-sim-net | `OGSimNet/SimNetLogRoutes.h`, `ogsimnet::kSimNetLogRoutes` | 4 |
| og-sim-session | this header, `ogsession::kSessionLogRoutes` | 15 |
| the composition | this header, `ogsession::kComposedLogRoutes` | 47 |

Every row is og-simulation's `SimulationLogRouteEntry`: a `char` prefix, a `SimulationLogRoute` and a `HostLogLevel`. The
route vocabulary is og-simulation's `SimulationLogRoute` (`SimulationLogRoutes.h`); task 36 added `Mgmt` and `SimHost` to it
for this block's rows. An engine maps each route to one of its categories in one place (Unreal: `ogLogRouteOf`, whose
`ogLogRoutesAreAllMapped` check fails the build for a route it does not map).

## §2 This header's rows

| row | route | `plainLevel` | the module's emitters |
|---|---|---|---|
| `[ReleaseBatch]` | `Net` | `Verbose` | `SessionNetHooks.h` (`releaseForTick`, once per authority step) |
| `SimulationManager:` | `Mgmt` | `Log` | `SessionRuntime.h` (the manager-initialized line) |
| `tryRegister:` | `Mgmt` | `Log` | `SessionLifecycle.h` (its three `[Error]` lines) |
| `[SimHost.` | `SimHost` | `Log` | `SessionRuntime.h` (`[SimHost.Backend]`) |
| `[RelayDelayFloor]`, `[StateRotation]`, `[ResimGate]`, `[LatencyKnobs]`, `[ClockRecentre.Config]`, `[ClockSource.Config]`, `[Dilation.Config]` | `Net` | `Log` | `SessionKnobs.h` (the knob intake's start-up lines; `[RelayDelayFloor]` also `SessionClock.h`) |
| `[ConnectionTier]`, `[ClockSource.Window]`, `[Dilation.Window]`, `[Dilation.Authority]` | `Net` | `Log` | `SessionClock.h` |

A `Warning` line carries its own token, so every `plainLevel` is `Log` or `Verbose`.

**The knob table's tags** (task 35's lead item 3). og-simulation's `Config/KnobTable.h` prints the tag a row names; it owns no
tag itself. The one product row, `sessionKnobRows::kRecentreSettleSamples` (`SessionKnobs.h`), prints
`[ClockRecentre.Config]`, so its route is this block's `[ClockRecentre.Config]` row and its owner is og-sim-session. The test
rows' `[DummyKnob.Config]` (`tests/SessionKnobRowsTest.cpp`) has no row: a test line.

**How the rows were placed** (task 36). Each row of the old table went to the module whose code hands lines that match it
to a log sink:
- og-sim-net: `[SimNet.` and `[Roster.` (they were in og-simulation's block since og-input-latency P5-4), `[ReceiveCorrectionState]`
  and `[ClockSource.Held]` (they were in the adapter's block; og-sim-net's `SimNetClient.h` is their only emitter);
- og-sim-session: the rows tasks 23–31 added for the tags they moved into this module, and three older adapter rows whose
  tags tasks 26–31 moved here: `[ReleaseBatch]`, `SimulationManager:` and `tryRegister:`;
- **the tiebreak** (lead ruling, 2026-11-26 14:42): a row whose tag og-simulation or a module below it also emits, but that sat
  outside og-simulation's block, stays in the block it was in. So `[ResimProbe`, `[ServerReceive]` and `[RelayProbe` stay in the
  Unreal adapter's block (their categories, `LogOGResimProbe` and `LogOGRelayProbe`, are Unreal routes), and `[SimHost.`
  (og-tickdriver and og-sim-jolt emit it too) and `[Dilation.Window]` (og-simulation's `ClockDilationController.h` formats it;
  `SessionClock.h` hands it to the sink) stay with the rows tasks 23–31 added, here;
- the rest stay in the adapter's block: lines only the adapter or the game emits (`[ClientPrediction]`, `NewFramework:`), and
  four rows nothing in the tree emits (`[SendRemoteInputToClients]`, `[ReceiveCorrectionInput]`, `[InjectCorrectionInput]`,
  `[DrainOutOfOrder]`), kept for equivalence.

## §3 The composition and what the compiler checks

`sessionLogRoutes::concatenate` copies the three blocks, in module order, into one `std::array` at compile time.
`kSimulationBlockEnd`, `kSimNetBlockEnd` and `kSessionBlockEnd` are the blocks' ends in it.

| check | fails when |
|---|---|
| `blocksAreDisjoint` over each pair of the three blocks | a prefix in one block and a prefix in another begin with one another (ignoring case) |
| `allReachable(kComposedLogRoutes)` | a prefix begins with an earlier prefix of the composition, so the later row can never be taken |
| `plainLevelsAreLogOrVerbose(kSessionLogRoutes)` | one of this block's rows is at `Warning` |

og-simulation's and og-sim-net's headers assert their own blocks' reachability and levels the same way, and the Unreal
adapter asserts its concatenation again (`UESimulationLogRoutes-rationale.md` §9).

**Why the order of the blocks routes nothing differently.** A line can match two rows only if one row's prefix begins with the
other's, since both begin the line. If no prefix in one block begins a prefix in another, a line matches rows of one block at
most, and its first match is the same whichever block comes first. The table of task 36's start had two such pairs, both inside
og-simulation's block (`[Resim.Input]` / `[Resim.`, `[ReceiveLocalInput] DISCARD` / `[ReceiveLocalInput]`), longer prefix first,
and `allReachable` keeps them so. So re-blocking the rows by module changed their indexes and no route; the pin (§6) checks it.

## §4 Reading a row at compile time: `bodyOf`, `rowOf`

`bodyOf(line)` skips a leading `[Verbose]`, `[Warning]` or `[Error]` token, compared ignoring case; `rowOf(line)` is the first
row of `kComposedLogRoutes` whose prefix the rest begins with, or `nullptr`. Both are `constexpr`. `rowOf` gives a line's
engine-free route and plain level, and `nullptr` for a line whose row is the engine adapter's or that has none.

## §5 Skipping a line's format: `wants`

The session engine models `SessionLogFilter` (`SessionEngine.h`): `wants(route, level)`, const, returns whether a line of that
route and level would be printed. `SessionNetHooks::releaseForTick` asks it before it formats `[ReleaseBatch]`, the one line
the runtime writes on every authority step, with the route and level `rowOf` gives that format
(`sessionNetHooksLines::kReleaseBatchRoute`, which a `static_assert` requires to exist). With the level off the line is
neither formatted nor handed to the sink (`SessionNetHooks-rationale.md`). The runtime's other lines are rare (start-up, a
window a second, a refusal) and format unconditionally.

## §6 The pin and its golden (task 36)

`tests/SessionLogRoutesTest.cpp` (CTest and the Unreal low-level test runner) and the Unreal automation test
`OGBrawler.Session.LogRoutes.Pin` (`Source/OGBrawlerUnreal/UESessionLogRoutesTest.cpp`) compare the tables with two generated
headers:
- `tests/SessionLogRoutesGolden.h`, `sessionLogRoutesGolden::kRows`: the Unreal adapter's `kOGLogRoutes` as composed before task
  36 (56 rows: og-simulation's 30, then the adapter's 26), each row's index, prefix, route and level. It was generated from
  the source text of `SimulationLogRoutes.h` and `UESimulationLogRoutes.h` at the commit before task 36 by the task's
  `pre_and_golden.py`, which reproduces `ogConcatenateLogRoutes` and checks the row counts and both mappings; it is not edited
  by hand.
- `tests/SessionLogRoutesEmittedTags.h`, `sessionLogRoutesGolden::kEmittedLines`: the start of every string literal in the
  product tree that begins, after an optional token, with `[` and a capital letter or with a row's prefix (321 lines), each cut
  before its first `%`, generated by the task's `gen_emitted.py`.

The tests assert:
- **the golden re-blocked by owner, row by row.** Each golden index has an owner (og-simulation, og-sim-net, og-sim-session, the
  adapter; §2). The expected table is the golden's rows grouped by owner in module order, each group in the golden's order;
  `kComposedLogRoutes` equals its first 47 rows and the Unreal `kOGLogRoutes` all 56, at every index (prefix, route, level);
- **every golden prefix routes to the same row** it took before: `rowOf` (engine-free) and `ogLogRouteFor` (Unreal);
- **every emitted line routes to the same row** it took before: `rowOf` and the Unreal `ogLogRouteForLevelLine` and
  `ogLogRouteFor`. A line whose row is the adapter's, or none, has no engine-free row.

The engine-free test also routes the four `[SimNet.`/`[Roster.` lines og-simulation's `SimulationLogRoutesTest.cpp` resolved there
until task 36, and counts the `[ReleaseBatch]` formats (§5): 100 authority steps with Net `Verbose` off give 0 lines, with it
on 100.
