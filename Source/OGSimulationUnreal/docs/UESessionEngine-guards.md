<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESessionEngine.h` — guards

Prohibitions with a tagged site in `Plugins/OGSimulation/Source/OGSimulationUnreal/UESessionEngine.h`. Ids are opaque, stable, and
retired rather than reused. The reasoning lives in `UESessionEngine-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

G-01 and G-02 moved here from `UESimulationSession-guards.md` (G-22, G-28) when og-session-runtime task 31 moved the session's
diagnostic gates into this header; G-03 is new.

---

## G-01 — the field-diff predicate must ask the category and verbosity `RouteOGMessage` sends `[DivergenceProbe` to, live

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-22, with the code it marks. **Tag site:** `UESessionEngine.h`, directly above the `correctionFieldDiff::setEnabledPredicate(` call in `ueSessionEngine::bindDiagnosticGates`, which the runtime calls through the session's `bindDiagnosticGates` at the old position of `bindCorrectionFieldDiffGate`. **R0 (task 31):** the predicate and its route check moved unchanged; the function is inline (G-03). The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above the `correctionFieldDiff::setEnabledPredicate(` call in the private
static member `bindCorrectionFieldDiffGate`, which `beginPlay` calls at the end of each role branch.

**The prohibition, as it stood in the source:**

> ⛔ A PREDICATE, NOT A LATCHED BOOL, AND THAT IS WHY IT IS A LAMBDA. `UE_LOG_ACTIVE`
> re-reads the category's CURRENT verbosity, so `LogOGDivergenceProbe Verbose` typed into
> the console mid-session starts naming fields on the next correction, and Warning stops
> it again. Reading it once at BeginPlay would pin the session to whatever the ini said.
>
> ⛔ IT MUST NAME THE SAME CATEGORY `RouteOGMessage` SENDS `[DivergenceProbe` TO, at the
> SAME verbosity the line carries (`[Verbose]`). Re-route that prefix and this gate starts
> answering about a channel the line no longer uses — it would then pay for a walk whose
> result is dropped, or drop a walk whose line is printed. This predicate and
> `RouteOGMessage`'s `[DivergenceProbe` arm are the ONLY TWO SITES IN CODE that USE this
> category — one reads its verbosity, the other logs to it; the DECLARE, the DEFINE and the
> ini value are the only other mentions. Change one and you must change the other.

**The consequence.** A predicate asking a different category than the router uses either pays for a per-field walk whose
line is then dropped, or skips a walk whose line is printed without its `field=` tail. A latched bool loses the console
switch mid-session.

**What breaks if the tag moves.** The other half of this pair is the route: og-simulation's `kSimulationLogRoutes` row
for `SimulationLogRoute::DivergenceProbe`, which `ogLogRouteOf` maps to `OGLogRoute::DivergenceProbe`, the arm of
`RouteOGMessage` that logs to `LogOGDivergenceProbe` (`UESimulationLogRoutes.h`). The route table's `static_assert`s check
its rows, not that the route and this predicate name the same category; this tag is the only thing that says so at the
predicate. A re-route is typed at either of the two route sites.

*R0, task 27b:* verified: in code, `LogOGDivergenceProbe` is used at exactly two sites, this predicate's `UE_LOG_ACTIVE`
and `RouteOGMessage`'s `DivergenceProbe` arm; its other code mentions are the `DECLARE` (`UESimulationLogRoutes.h`), the
`DEFINE` (`UESimulationLogRoutes.cpp`) and a game's ini value (the brawler's `Config/DefaultEngine.ini` sets `Verbose`);
every other mention is a comment. og-simulation's predicate is empty until a host sets one, and
`correctionFieldDiff::enabled()` is false while it is empty.

*Moved from `OGBrawlerSimulationManager-guards.md` G-50 (the brawler's task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 27b, with the function
it marks: the brawler's file-local `bindCorrectionFieldDiffGate` became the session's private static member, unchanged,
and the start-up that calls it became `beginPlay`.*

*R0, og-input-latency task 2h:* two statements above are no longer true, and one never was. Since task 2h the category
has a THIRD use in code, the margin predicate below this one (`UE_LOG_ACTIVE(LogOGDivergenceProbe, Log)`, `⛔G-28`), so
the quoted "ONLY TWO SITES" and 27b's "exactly two sites" now read "three". And the brawler's `Config/DefaultEngine.ini`
sets the category to `Warning`, not `Verbose`, at `HEAD` and in today's tree (`LogOGDivergenceProbe=Warning`). The
prohibition itself is unchanged.

---

## G-02 — the margin predicate must ask the category `RouteOGMessage` sends `[DivergenceProbe` to, at `Log`, live

**Moved by og-session-runtime task 31 (2026-11-26)** from `UESimulationSession-guards.md` G-28, with the code it marks. **Tag site:** `UESessionEngine.h`, directly above the `correctionVerdictMargin::setEnabledPredicate(` call in `ueSessionEngine::bindDiagnosticGates`. **R0 (task 31):** unchanged. The text below is the entry as it was; where it names the Unreal session's members, read the runtime's (the same names).

**Tag site:** `UESimulationSession.h`, directly above the `correctionVerdictMargin::setEnabledPredicate(` call in
`bindCorrectionFieldDiffGate`, the second predicate install, after G-22's and a `static_assert` on the route.

**The prohibition.** ⛔ The margin predicate is G-22's twin at another level: a lambda that re-reads
`UE_LOG_ACTIVE(LogOGDivergenceProbe, Log)` on every call, never a bool latched at `beginPlay`, and never a level other
than the one the `[DivergenceProbe.Margin]` line is printed at. That line carries no severity token, so it goes out at
the `[DivergenceProbe` route's plain level, `Log`. Do not change the predicate's category or level without the route,
and do not raise it to `Verbose` to share G-22's answer.

**The consequence.** A predicate asking another category, or a higher level, pays for a walk of every landed verdict
whose line is dropped, or drops the walk while the line would print: `LogOGDivergenceProbe=Log` then shows no Margin
lines at all. A latched bool loses the console switch mid-session. At `Verbose` the margin would run only when the
per-correction Verbose lines also run, which is the cost the second predicate exists to avoid.

**What breaks if the tag moves.** Half of the pair is checked by the compiler: the `static_assert` above the call fails
if `[DivergenceProbe.Margin]` stops routing to `OGLogRoute::DivergenceProbe` at plain level `ELogVerbosity::Log`. The
other half is not: that `OGLogRoute::DivergenceProbe` logs to `LogOGDivergenceProbe` is a `switch` arm in
`RouteOGMessage` (`UESimulationLogRoutes.h`), which is not `constexpr`. This tag is what says so at the predicate.

*R0, og-input-latency task 2h:* written with the code. `ogLogRouteFor(TEXT("[DivergenceProbe.Margin]"))` finds the row
whose prefix is `[DivergenceProbe` (`kSimulationLogRoutes`, plain level `HostLogLevel::Log`, mapped to
`ELogVerbosity::Log`). `RouteOGMessage` strips only `[Verbose]` and `[Warning]`, then matches the prefix and sends a
`DivergenceProbe` line to `LogOGDivergenceProbe`. The brawler's `Config/DefaultEngine.ini` ships the category at
`Warning`, so the predicate answers false there. og-simulation's `correctionVerdictMargin::enabled()` is false while
no predicate is installed.

---

## G-03 — the diagnostic gates are set from an inline header function, never from `UESessionEngine.cpp`

**Tag site:** `UESessionEngine.h`, directly above `inline void bindDiagnosticGates()`.

**The prohibition.** ⛔ Keep `ueSessionEngine::bindDiagnosticGates` inline in the header (or in a template the game module
instantiates). Never move it into an exported `.cpp`.

**Why.** og-simulation's two predicates (`correctionFieldDiff::g_enabledPredicate`, `correctionVerdictMargin::g_enabledPredicate`,
`SimulationComparison.h`) are `inline` variables in a header: in a modular Unreal build each module that includes the header
has its own copy. The correction cache that reads them is instantiated in the game module (where the session template is).
Set from OGSimulationUnreal's DLL, the game module's copy stays empty and every `[DivergenceProbe.*]` line disappears; the
build and every unit test pass.

**Seen failing (task 31).** The first form of this function was out of line in `UESessionEngine.cpp`: the smoke
`SR5_t31_b3_smoke`'s tagset reported `[DivergenceProbe.Margin] before=110 after=0 (REMOVED)` on client1 (54 and 40 on
client2a and client2b). Inline, `SR5_t31_b4_smoke` has 0 findings.

*Added by og-session-runtime task 31.*
