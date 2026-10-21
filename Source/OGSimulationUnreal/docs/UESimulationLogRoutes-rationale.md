<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: kOGGameLogRoutes -- retired name: the adapter table until task 19, renamed kOGAdapterLogRoutes (ruling Q4); it must not resolve -->
# `UESimulationLogRoutes.h` and `.cpp` — rationale

The Unreal side of og-simulation's log routes. `UESimulationLogRoutes.h` declares eight exported `LogOG*` categories,
holds the adapter's route rows (`kOGAdapterLogRoutes`), concatenates them after og-simulation's own rows
(`kSimulationLogRoutes`, og-simulation's `SimulationLogRoutes.h`) into one table, `kOGLogRoutes`, checks that table with
five `static_assert`s, and defines the two things that read it: the router `RouteOGMessage`, and the macro
`OG_LOG_ROUTED`. `UESimulationLogRoutes.cpp` defines the eight categories.

The source holds the licence, a docs pointer and code; the `static_assert` messages are code. This file carries the why.
It has no prohibition: `UESimulationLogRoutes-guards.md` has no live entry, because every ordering and level rule the
table has is a `static_assert` (§9).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. What it is

| name | what |
|---|---|
| `LogOGSim`, `LogOGSimTick`, `LogOGMgmt`, `LogOGNet`, `LogOG`, `LogOGRelayProbe`, `LogOGDivergenceProbe`, `LogOGResimProbe` | the eight routed categories, `OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(…, Log, All)` in the header, `DEFINE_LOG_CATEGORY` in the `.cpp` (§3, §11) |
| `OGLogRoute` | the seven routes a row can name, one per routed category except the fallback `LogOG` |
| `OGLogRouteEntry` | one row: a `TCHAR` prefix, an `OGLogRoute`, and `plainLevel` (§6) |
| `kOGAdapterLogRoutes` | the adapter's 14 rows (§8) |
| `ogLogRouteOf`, `ogLogVerbosityOf` | the two mappings from og-simulation's engine-free row fields to Unreal's (§8) |
| `kOGSimulationLogRoutePrefixes` | og-simulation's `char` prefixes widened to `TCHAR` at compile time (§8) |
| `kOGLogRoutes` | the concatenation, og-simulation's 25 rows first, then the adapter's 14 (§8) |
| `ogLogRouteFor` | the first row whose prefix begins a line, at compile time (§7) |
| `OG_LOG_ROUTED` | a direct log site that takes its level from its row (§7) |
| `RouteOGMessage` | the router: one og-simulation log string to one category at one level (§2) |

## 2. The router — `RouteOGMessage`

`RouteOGMessage` routes one SIMLOG string to one `LogOG*` category by its leading `[Tag]`. A game's host installs it as
og-simulation's loggers (each core component's `setLogger`, and the `simlog::setGlobal` sink), so the core's log lines
arrive here, and the host logs its own probe lines through it too.

**Tag-matching runs against `body`** — `fmsg` minus a leading `[Verbose]` or `[Warning]` token, both nine characters —
while the **full** `fmsg`, severity token included, is what is logged. So one message can carry both a severity
escalation and a routable tag.

**The walk.** It starts every line at `Log`, sets `Verbose` or `Warning` from a leading token, then walks `kOGLogRoutes`
in order and stops at the first row whose prefix `body` begins with (`FString::StartsWith`, which ignores case). On that
row it replaces a remaining `Log` with the row's `plainLevel` (§6), logs to the row's category, and returns. A line no
row matches is logged to `LogOG`, the fallback, at the level it carries.

**One `StartsWith` covers each probe family** — the rows `[ResimProbe`, `[RelayProbe` and `[DivergenceProbe` have no
closing bracket — so a future sub-tag needs no router edit and no new row.

**`inline`, in the header, optimized.** The router is a plain `inline` function, so it is compiled in the module that
calls it (a game module), from the header. It sits outside any `OGSIM_OPTIMIZE_OFF`/`ON` pair, so it compiles
optimized, as it did before it moved here (§12). Its `EMIT_OG` helper macro is defined inside its body and `#undef`'d at
its end, so it does not leak into the file that includes the header.

## 3. The categories

Seven categories take routed lines and one, `LogOG`, takes the rest.

| category | what lands there |
|---|---|
| `LogOGSim` | rare simulation lifecycle: `[TimeResync.`, `[Resim.` (but not `[Resim.Input]`), `[ResimCheck.Divergence]`, `[ResimCheck.PrepareRestore]` |
| `LogOGSimTick` | per-tick simulation lines: `[Resim.Input]`, `[ResimCheck.Check]`, `[ResimCheck.IsSimilar]`, `[ResimCheck.TriggerRewind]`, `[AuthoritySimulation]`, `[ClientPrediction]`, `[PredictionSimulation]`, `[PostPrediction]`, `[CollectInput]` |
| `LogOGMgmt` | manager and simulatable lifecycle: `SimulationManager:`, `tryRegister:`, `NewFramework:` |
| `LogOGNet` | the replication channel and the reception coordinator: every `OGLogRoute::Net` row (§6) |
| `LogOGRelayProbe`, `LogOGResimProbe`, `LogOGDivergenceProbe` | the three probe families (below; members in §5) |
| `LogOG` | the fallback: any line whose tag has no row, at the level its token gives it (`Log` without one) |

### Three probe families, three categories

`LogOGRelayProbe`, `LogOGDivergenceProbe` and `LogOGResimProbe` each own a category, and the reason is the same all three
times: **both severities ride one tag family** — per-window summaries at `Warning`, per-event detail at `Verbose` — so
its own category is the only thing that lets `LogOGRelayProbe=Warning` (the default the game's committed ini ships) keep
the summaries while dropping the detail, and `LogOGRelayProbe=NoLogging` drop both, without disturbing any other
channel. The same mechanism holds for the other two at `Warning` (the committed ini ships both at `Verbose`). Routed to
`LogOGNet` instead, a family would be inseparable from the whole replication bucket; left unrouted, its `Verbose` half
would fall to `LogOG=Warning` and be unreachable with no way to turn it on.

**No probe family may be filed under `[Resim.` or `[ResimCheck.`.** The first inherits whatever `LogOGSim` is set to, so
the probe could not be switched on its own. *(It said "inherits `LogOGSim=Verbose`" until og-netcode-v2-field-defects
task 11; `LogOGSim` shipped at `Verbose` when T19 measured it, and the committed ini sets `Log` now.)* The second is split
across `LogOGSim` and `LogOGSimTick`, so a family filed under it could not be switched on or off as one thing.
*(Until task 19 this paragraph ended "which is why `[ResimCheck.IsSimilar]` has zero occurrences in every log on disk".
That is false twice: the line is routed to `LogOGSimTick` alone, and its absence from the committed configuration's logs
comes from `LogOGSimTick=Warning` hiding a `Verbose` row; a harness session at `LogOGSimTick=Verbose` logs it about 57
times a second per client, 8,654 lines in B1's second Jolt capture's client 1.)* No assertion enforces this rule: a new
family is new rows, and nothing in a row says "probe".

`[RelayProbe.Frame]` is the relay family's original **server-side** member; `.Write` and `.Budget` are server-side too.
It shares the category deliberately: it measures the *cause* of the cadence `[RelayProbe.Arrival]` measures the
*effect* of, and the two are only interpretable together, so one knob should turn both on.

`[DivergenceProbe.*]`'s signal is not new — `StateCorrectionCache::tryInsertingCorrectState` has computed the verdict on
every correction for as long as that method has existed. What was missing was a **route**: the cache's own line
(`inserting correction at tick=…`) carries no tag, so it lands on the `LogOG` fallback at `Log` severity and
`LogOG=Warning` suppresses it. The `[DivergenceProbe` row is the whole of that fix. The game's host may also read this
category's verbosity to decide whether the cache's per-field walk runs; that half is the game's.

## 4. Order is load-bearing

`[Resim.Input]` must be matched **ahead of** the `[Resim.` catch-all, because the catch-all is a prefix of it.
`[Resim.Input]` is a per-character-per-resim-tick line — the same volume class as `[CollectInput]` — and it landed in the
rare-lifecycle bucket purely by inheriting the `[Resim.` prefix from `[Resim.Pre]` / `[Resim.Post]`, which really are
rare, while `LogOGSim` shipped at `Verbose`. So the highest-frequency line in the system was filed as a lifecycle event
and printed by default.

It was **re-routed rather than renamed.** Both were available, and renaming would have silenced it just as well — but
`[Resim.Input]` is the name the impl notes, the PIE scripts and the spectrum design all use when they tell an operator
what to grep for. A rename invalidates every one of those instructions for a cosmetic gain. Re-routing keeps the string
an operator already knows and changes only which knob controls it: `LogOGSimTick=Verbose` shows the resim table, exactly
as it shows `[CollectInput]`. *(Until task 19 this said `[CollectInput]` "is routed there under the comment 'dominates
log volume'". No such comment is in the tree: the router's comments went when its table became `constexpr`.)*

Both rows are og-simulation's now, so this order is og-simulation's table's (its `allReachable`), and it is checked
again over the concatenation here (§9).

`[ResimProbe` is the first row of the adapter's block, so it sits after the `[Resim.` catch-all, which is in
og-simulation's block (§8). The two cannot collide: `[Resim.` requires the dot and `[ResimProbe` has a `P` there. The one
edit that would make them collide, widening the catch-all to `[Resim`, is a compile error, not a silent reroute: in
og-simulation it fails `allReachable` (the `[ResimCheck.` rows after it begin with `[Resim`) and here
`ogLogRouteBlocksAreDisjoint` (`[ResimProbe` begins with it); and a `[Resim` row in the adapter's block fails
`ogLogRouteBlocksAreDisjoint`. *(Until og-tickdriver task 12 the single table put `[ResimProbe` ahead of `[Resim.`,
defensively, against that edit.)*

⭐ **The order is checked by the compiler, not by this section.** `RouteOGMessage` walks the `constexpr` table
`kOGLogRoutes` in order, and `ogLogRoutesAreAllReachable` fails the build if any prefix begins with an earlier one,
compared case-insensitively as `FString::StartsWith` compares.

## 5. The tag families, in full

The rows name each family by its prefix; the members in the tree are:

| category | tags |
|---|---|
| `LogOGRelayProbe` | `[RelayProbe.Read]`, `[RelayProbe.Arrival]`, `[RelayProbe.Stale]`, `[RelayProbe.Miss]`, `[RelayProbe.Delta]`, `[RelayProbe.Frame]`, `[RelayProbe.Write]`, `[RelayProbe.Budget]`, `[RelayProbe.Ingest]`, `[RelayProbe.ReadTick]` |
| `LogOGResimProbe` | `[ResimProbe.Gate]`, `[ResimProbe.Chaos]`, `[ResimProbe.Apply]`, `[ResimProbe.Landing]`, `[ResimProbe.Request]`, `[ResimProbe.Stranded]`, `[ResimProbe.Session]`, `[ResimProbe.Frame]`, `[ResimProbe.PushTarget]`, `[ResimProbe.PushVerdict]`, `[ResimProbe.SlotMap]` |
| `LogOGDivergenceProbe` | `[DivergenceProbe.Correction]`, `[DivergenceProbe.Window]` |

*(Until task 19 the relay row listed eight members. `[RelayProbe.Ingest]` and `[RelayProbe.ReadTick]` are emitted too,
both `Verbose`, by og-simulation's net-sync and input-resolution telemetry.)*

`[RelayProbe.Miss]` says *why* each miss missed — an in-span coverage hole, asking above the newest arrival, or below the
oldest; `[RelayProbe.Delta]` is the signed probe-tick-to-newest distribution; `[RelayProbe.Stale]` is the longest
consecutive fallback run. `[ResimProbe.Chaos]` is the request/grant/refusal ledger and the requested-vs-granted pin;
`[ResimProbe.Apply]` is the apply edge and the replay span; `[ResimProbe.Landing]` is the frontier-landing split;
`[ResimProbe.Stranded]` is the per-event `Verbose` companion to it.

## 6. Levels — the `plainLevel` column (og-simulationscheduler-withjolt task 80)

**Every row carries a third field, `plainLevel`: the level of a routed line that carries no `[Verbose]` or `[Warning]`
token.** A token always wins (§2): the per-window `[Warning]` summaries and the per-event `[Verbose]` probe detail are
logged exactly as their emitters mark them, whatever the row says. Every `[DivergenceProbe` line carries a token, so
that row's `plainLevel` never applies to one.

The table is in the header, not in a `.cpp`, so that log sites in other files can read a row at compile time (§7).

**Why.** With a visible `-log` console, every log line costs a synchronous console write on the thread that logs it
(the research note on the gate-22 dedicated-server frame rate: about 1.4 ms per line at gate 22, 12–20 ms step outliers,
and a 3.2 s join hitch when one unbounded receive loop logged 761 packets). At `LogOGNet=Log` and `LogOGSimTick=Log`
every tick and every received packet wrote a line. Now `Log` carries only rare events, and the per-tick lines need
`Verbose`.

**The rule.** A row whose token-less lines arrive per tick, per packet or per relay (about 10 lines per second per peer
or more in normal play) is `Verbose`. A row for rare events (gaps, drops, delay shifts, stats windows, resyncs, resim
prepare and finish, join and leave) is `Log`. `ogLogRoutePlainLevelsAreLogOrVerbose` admits only those two values: a line
that must reach `Warning` says so with its own token.

**The table, with the rates that classified it.** Rates are token-less lines per second in the gate-22 Jolt packaged
final run (`LogOGNet=Log`, `LogOGSimTick=Log`; server 234 s with up to three clients, client 1 191 s, client 3 76 s).
"token" means every line of that row carries `[Verbose]` or `[Warning]`, so `plainLevel` does not apply to it. Before
task 80 every row behaved as `Log`.

| row | category | rate (server / client) | `plainLevel` |
|---|---|---|---|
| `[Resim.Input]` | `LogOGSimTick` | token (`[Verbose]`) | `Verbose` |
| `[ResimProbe` | `LogOGResimProbe` | token | `Log` |
| `[TimeResync.` | `LogOGSim` | – / ≤ 0.7 (join) | `Log` |
| `[Resim.` | `LogOGSim` | – / ≤ 5.0 (`[Resim.Post]`) | `Log` |
| `[ResimCheck.Divergence]` | `LogOGSim` | – / ≤ 1.0 | `Log` |
| `[ResimCheck.PrepareRestore]` | `LogOGSim` | – / ≤ 2.8 | `Log` |
| `[ResimCheck.Check]` | `LogOGSimTick` | – / 142–172 | `Verbose` |
| `[ResimCheck.IsSimilar]` | `LogOGSimTick` | – / 57–59 | `Verbose` |
| `[ResimCheck.TriggerRewind]` | `LogOGSimTick` | Chaos only: one per physics step | `Verbose` |
| `[AuthoritySimulation]` | `LogOGSimTick` | 60 / – | `Verbose` |
| `[ClientPrediction]` | `LogOGSimTick` | – / 57–59 | `Verbose` |
| `[PredictionSimulation]` | `LogOGSimTick` | – / 57–59 | `Verbose` |
| `[PostPrediction]` | `LogOGSimTick` | – / 142–172 | `Verbose` |
| `[CollectInput]` | `LogOGSimTick` | 116 / 142–172 | `Verbose` |
| `[ServerReceive]` | `LogOGNet` | 573 / – | `Verbose` |
| `[ReceiveLocalInput] DISCARD` | `LogOGNet` | 0 (rare: a too-far-future input) | `Log` |
| `[ReceiveLocalInput]` | `LogOGNet` | 116 / – | `Verbose` |
| `[SendCorrectionStateToClients]` | `LogOGNet` | 49 / – | `Verbose` |
| `[SendRemoteInputToClients]` | `LogOGNet` | no emitter | `Log` |
| `[SendLocalInputToServer]` | `LogOGNet` | – / 94–99 | `Verbose` |
| `[ReceiveCorrectionState]` | `LogOGNet` | – / 57–59 | `Verbose` |
| `[ReceiveCorrectionInput]` | `LogOGNet` | no emitter | `Log` |
| `[InjectCorrectionState]` | `LogOGNet` | – / 57–59 | `Verbose` |
| `[InjectCorrectionInput]` | `LogOGNet` | no emitter | `Log` |
| `[DrainOutOfOrder]` | `LogOGNet` | no emitter | `Log` |
| `[InputGap]`, `[InputDrop]`, `[DelayShift]`, `[InputStats]`, `[InputDomain]` | `LogOGNet` | token (`[Warning]`) | `Log` |
| `[Park]` | `LogOGNet` | 116 / – | `Verbose` |
| `[Release]` | `LogOGNet` | 116 / – | `Verbose` |
| `[ReleaseBatch]` | `LogOGNet` | 60 / – | `Verbose` |
| `[RelaySkip]` | `LogOGNet` | token | `Log` |
| `[RelayProbe` | `LogOGRelayProbe` | token | `Log` |
| `[DivergenceProbe` | `LogOGDivergenceProbe` | token | `Log` |
| `SimulationManager:`, `tryRegister:`, `NewFramework:` | `LogOGMgmt` | ≤ 0.04 | `Log` |

Notes on the rows:

* **`[ReceiveLocalInput] DISCARD` is its own row, ahead of `[ReceiveLocalInput]`.** The core emits both from one
  callback (og-simulation's `SimulationNetSync.h`): the plain line once per input, the `DISCARD` line for the rare
  too-far-future anomaly, with no token. The more specific row keeps the anomaly at `Log`. The reachability
  `static_assert` keeps it ahead: below `[ReceiveLocalInput]` it could never be taken.
* **`[ReleaseBatch]` is a row although `RouteOGMessage` never sees it.** A game logs it directly, with `OG_LOG_ROUTED`,
  and its row exists so that site reads its level from the table.
* **`[Resim.Input]` and the token rows.** Their `plainLevel` has no effect today. `[Resim.Input]` is `Verbose` because it
  is a per-replayed-tick line; the token rows are `Log` so a future token-less line there is not hidden by accident.
* **Four rows have no emitter in the tree:** `[SendRemoteInputToClients]` (its write is retired in og-simulation's
  `SimulationNetSync.h`), `[ReceiveCorrectionInput]`, `[InjectCorrectionInput]` and `[DrainOutOfOrder]`. With no rate to
  classify them they stay `Log`. They are candidates for deletion; the user ruled that they move unchanged in task 19
  and that retiring them is a follow-on (Q10).
* **The `LogOGSim` rows stay `Log`.** Resim lifecycle lines are at most about 5 per second on a client.

**What an operator types.** Per-tick and per-packet diagnostics need `LogOGNet=Verbose` and `LogOGSimTick=Verbose` (on
the command line, `-ini:Engine:[Core.Log]:LogOGNet=Verbose`, or `-LogCmds="LogOGNet Verbose"`). At `Log` those
categories carry only the rare lines. Lines read at `Verbose` are prefixed `Verbose: ` after the category.

## 7. Direct log sites read the same table: `OG_LOG_ROUTED`

A per-tick or per-packet line a game logs with `UE_LOG` directly, rather than through `RouteOGMessage`, uses
`OG_LOG_ROUTED(Category, Format, ...)`. The macro looks up the row whose prefix begins `Format` at compile time
(`ogLogRouteFor`, a `constexpr` walk of `kOGLogRoutes`, so the first match as the router would take it), fails the build
if there is none, and expands to `UE_LOG(Category, Verbose, ...)` or `UE_LOG(Category, Log, ...)` from that row's
`plainLevel`. So the level is set in one place for routed and direct lines alike.

The category stays at the site. Nothing checks that it is the row's category.

A direct site whose tag is shared by lines of different rates (a rare line and per-step lines that differ only at the
end of the text, where no prefix row can reach) stays a `UE_LOG` at a fixed level; so does a tag with no row, which needs
none.

## 8. Two blocks: og-simulation's rows first, then the adapter's (og-tickdriver task 12)

**`kOGLogRoutes` is the concatenation of two tables.** The rows whose tags only og-simulation emits are engine-free
data in og-simulation: `kSimulationLogRoutes` in its `SimulationLogRoutes.h`, with plain `char` prefixes, a
`SimulationLogRoute` and a `HostLogLevel`. The rest are here, in `kOGAdapterLogRoutes`. `ogConcatenateLogRoutes` builds
`kOGLogRoutes` at compile time: og-simulation's 25 rows first, then the adapter's 14, each block in the order the
single table had before task 12. `RouteOGMessage`, `OG_LOG_ROUTED` and `ogLogRouteFor` read `kOGLogRoutes`.

**Which rows are og-simulation's** was decided by the emitters (og-tickdriver task 12): a row went to og-simulation when
og-simulation alone emits its tag; its emitting files are listed in og-simulation's `SimulationLogRoutes-rationale.md`
§5. The adapter's 14 rows are those a game's host also emits or alone emits (`[ResimProbe`, `[ServerReceive]` and
`[RelayProbe` are emitted by both), plus the four with no emitter. All 14 moved here from the first game's module
unchanged in task 19 (Q5: the router stays one plain function and no game block exists); the table was renamed from
`kOGGameLogRoutes` (Q4).

**Why the concatenation changes no route.** A line can match two rows only if one row's prefix begins with the other's,
because both begin the line. So if no prefix in one block begins with a prefix in the other, a line matches rows of one
block at most, and its first match is the same whichever block comes first; within each block the order is the old one.
`ogLogRouteBlocksAreDisjoint` checks that at compile time (§9). A later row added to either block is held to it.

The two mappings live in one place each: `ogLogRouteOf` (`SimulationLogRoute` to `OGLogRoute`) and `ogLogVerbosityOf`
(`HostLogLevel` to `ELogVerbosity`). Each og-simulation prefix is widened to `TCHAR` one `char` at a time
(`kOGSimulationLogRoutePrefixes`, each entry sized by `ogLongestSimulationLogRoutePrefix`), which is exact for ASCII only.

## 9. What the compiler checks

Five `static_assert`s at namespace scope, and one inside every `OG_LOG_ROUTED` expansion. Each message is code.

| check | fails when |
|---|---|
| `ogSimulationLogRoutePrefixesArePrintableAscii` | an og-simulation prefix holds a character outside printable ASCII (the widening is exact for ASCII only) |
| `ogLogRoutesAreAllMapped` | an og-simulation row's `SimulationLogRoute` has no `OGLogRoute` in `ogLogRouteOf`, so the router would drop its lines |
| `ogLogRouteBlocksAreDisjoint` | a prefix in one block and a prefix in the other begin with one another, compared ignoring case |
| `ogLogRoutesAreAllReachable` | a prefix begins with an earlier prefix, so the later row can never be taken. Its message says it replaced the ORDER IS LOAD-BEARING fence |
| `ogLogRoutePlainLevelsAreLogOrVerbose` | a row's `plainLevel` is neither `Log` nor `Verbose` |
| `OG_LOG_ROUTED`'s row lookup | the format's leading tag has no row |

og-simulation asserts its own block's reachability and levels the same way (`allReachable`, `plainLevelsAreLogOrVerbose`).

**Seen failing.** Each arm was seen to fail when it was written (og-simulationscheduler-withjolt task 80, og-tickdriver
task 12), and the reachability arm again for `[Resim.` moved above `[Resim.Input]` and widened to `[Resim`. Task 19
re-ran one arm from a game-module translation unit after the move: the adapter's `[ReleaseBatch]` row's prefix changed
to `[Re` fails the build twice, with `ogLogRouteBlocksAreDisjoint`'s message (og-simulation's `[Resim.Input]`,
`[ReceiveLocalInput]` and `[Release]` rows, among others, begin with it) and with `ogLogRoutesAreAllReachable`'s (the
adapter's later `[RelayProbe` row begins with it).

## 10. The router's per-line notes, carried

*The router was once a chain of `StartsWith` branches, each with a comment beside it. Since it became a `constexpr`
table with a `static_assert` that no prefix begins with an earlier one (§9), those notes have no line to stand beside.
They are quoted here as they shipped; section numbers in the quotes are the game manager document's of that time. The
corrections beneath them are R0 findings and do not edit the quotes.*

> [InputGap]/[InputDrop]/[DelayShift]/[InputStats] carry [Warning]; [Park]/[Release] do not.

> The out-of-domain gate: one [InputDomain] per burst, naming a client outside our domain.

> The relay tap: Verbose per skipped capture tick, Warning per [InputStats] window.

> The relay family: [RelayProbe.Read], .Arrival (in CAPTURE ticks), .Stale, .Miss, .Delta, .Frame. §8
>
> ⚠ .Frame IS THE ONE SERVER-SIDE MEMBER: it measures the CAUSE of .Arrival's EFFECT.
>
> ⛔ Under LogOGNet it would be inseparable; unrouted its Verbose half is unreachable. §4

⚠ **Correction C13-3.** The relay family has **ten** members in the tree, not six: `[RelayProbe.Write]` and
`[RelayProbe.Budget]` are emitted by a game's host, both **server-side**, and `[RelayProbe.Ingest]` and
`[RelayProbe.ReadTick]` by og-simulation. So `.Frame` is not the one server-side member. §3 and §5 list them all.
*(Before task 19 this correction counted eight and said the family table and the `.Frame` sentence carried the same
omission; both were fixed earlier and listed `.Write` and `.Budget`, but neither listed `.Ingest` or `.ReadTick`.)*

> The RESIM-GATE family: [ResimProbe.Gate], .Chaos, .Apply, .Landing, .Request, .Stranded. §8
>
> ⛔ AHEAD OF `[Resim.`, DEFENSIVELY: widening it drops this family into LogOGSim=Verbose.
>
> ONE StartsWith COVERS THE FAMILY: a future sub-tag needs no router edit.

⚠ **Correction C13-2.** The resim-gate family has **eleven** members in the tree: `.Session`, `.Frame`, `.PushTarget`,
`.PushVerdict` and `.SlotMap` are also emitted (§5). `LogOGSim` is `Log` in the committed ini, not `Verbose`. The
"ahead of `[Resim.`" placement is gone since og-tickdriver task 12, and the widening it defended against is a compile
error (§4). *(Before task 19 this correction said the family table omitted `.SlotMap`, which `CorrectionCache.h` emits.
The table lists it, and og-simulation's `SimulationReconciliation.h` emits it; `CorrectionCache.h` names it only in
comments.)*

## 11. Exports, build mode, and what that does to the code

**Exports.** The eight categories are `OGSIMULATIONUNREAL_API`, as `LogOGSimHost` is in `OGSimulationUnreal.h`: their
objects live in this module and the game's code logs to them. In the modular Editor build, a game module's references
to them are imports, so a log site there loads the category's address through the import table first (task 19 measured
it in the router: one `__imp_` load per category, the same control flow and calls); in the monolithic Game and Server
builds the export macro is empty and the code is unchanged. Dropping the macro from one category fails the game module's Editor link with an unresolved
external (task 19, seen failing).

**Build mode.** The module is built one translation unit per file (`bUseUnity = false` in `OGSimulationUnreal.Build.cs`;
`OGSimulationUnreal-rationale.md` §4), so this file's `.cpp` is compiled on its own: it holds the eight definitions and
nothing else.

**No thread or mask.** The `.cpp` defines log categories only. It touches no tick-driver thread mask, so the one-module
rule (the masks get one copy per DLL) is unaffected.

## 12. Provenance

* **og-tickdriver task 12** split the single table into og-simulation's block and the game module's block, and added
  `ogLogRouteBlocksAreDisjoint`, `ogLogRoutesAreAllMapped` and `ogSimulationLogRoutePrefixesArePrintableAscii`.
* **og-simulationscheduler-withjolt task 80** added the `plainLevel` column, its assertion and `OG_LOG_ROUTED`.
* **og-ue-simulationcomponentandmanagersplit task 19** moved the eight categories, the table and its checks,
  `ogLogRouteFor`, `OG_LOG_ROUTED` and `RouteOGMessage` here from the first game's module (BUSL-1.1), re-licensed MPL-2.0
  (ruling Q1). The statements are the old ones, in the old order. The changes are: the categories gained
  `OGSIMULATIONUNREAL_API`; the adapter table is named `kOGAdapterLogRoutes` (was `kOGGameLogRoutes`); the router, which
  was a function with internal linkage in an anonymous namespace of the game's implementation file, compiled optimized
  there, is an `inline` function in this header, still outside any optimize pair. Measured: no tag changed its category or
  level on any process of that task's harness sessions, and in the monolithic Game build the router's optimized code is
  the old code instruction for instruction (its name aside).
