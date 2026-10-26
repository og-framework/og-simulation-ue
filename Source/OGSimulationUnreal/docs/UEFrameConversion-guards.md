<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEFrameConversion` — guards

Every prohibition that governs a line of `UEFrameConversion.h` or `UEFrameConversion.cpp`. Each entry has an **opaque,
stable id**. In the source a single line `// ⛔G-nn  docs/UEFrameConversion-guards.md` sits directly above the
statement or definition where the forbidden edit would be typed, or, for an absence tag, on the line where the
forbidden addition would go.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

⛔ **Nothing in this file is a rationale.** What the header converts, and by which rule, lives in
`UEFrameConversion-rationale.md`.

---

## G-01 — `composeTransform` stays non-template, `FORCENOINLINE`, inside the `.cpp`'s `OGSIM_OPTIMIZE_OFF` pair

**Site:** the definition of `ueFrameDetail::composeTransform` in `UEFrameConversion.cpp`, between that file's
`OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON`.

**The prohibition.** ⛔ Do not make it a template or a member of `BasicUeFrame`, do not drop its `FORCENOINLINE`,
and do not move it out of the `OGSIM_OPTIMIZE_OFF` … `OGSIM_OPTIMIZE_ON` pair. Do not fold its three statements
into its callers (`BasicUeFrame::transformToSim`, `ueNative::toGLMMat4`). Moving its definition back into the header
is G-05.

**The consequence.** Its body is the body of the frame-free `uglm` conversion it replaced, toGLMMat4 (deleted, with
the rest of `uglm`, by og-sim-metres-yup task 9; it was compiled between the same pair and called out of line), so
at the shipped knobs the matrix it returns is bit-identical to that conversion's.
The rotation matrix and the two matrix products do arithmetic. Under `/fp:fast` an optimized, inlined copy may fold
`x + 0` and `0 * x` against the identity matrix and keep a `-0` that the unoptimized code turns into `+0`. The
static geometry's matrices (the importer's `checksum=` hashes their bits) and every Chaos body and query transform
would then no longer be bit-identical. MSVC compiles a template with the optimization state at the END of the
translation unit, so a template inside the pair is compiled optimized anyway.

**What breaks if the tag moves.** The tag marks the definition. `decomposeTransform` has its own entry (G-02).

---

## G-02 — `decomposeTransform` stays non-template, `FORCENOINLINE`, inside the `.cpp`'s `OGSIM_OPTIMIZE_OFF` pair

**Site:** the definition of `ueFrameDetail::decomposeTransform` in `UEFrameConversion.cpp`, between that file's
`OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON`.

**The prohibition.** ⛔ Do not make it a template or a member of `BasicUeFrame`, do not drop its `FORCENOINLINE`,
and do not move it out of the pair. Do not fold its reads into its callers (`BasicUeFrame::transformToUe`,
`ueNative::toFTransform`). Moving its definition back into the header is G-05.

**The consequence.** Its body is the decomposition of the frame-free `uglm` conversion it replaced, toFtransform
(deleted with the rest of `uglm`; it was compiled between the same pair and called out of line): the translation
column, `glm::quat_cast` and the three column lengths. `quat_cast` takes a square root and divides; an optimized,
inlined copy under `/fp:fast` may use a reciprocal or reassociate, so the Chaos transform writes (`setBodyTransform`,
the query adapter's overlap and sweep poses) would no longer be bit-identical to the conversion they replaced.

**What breaks if the tag moves.** The tag marks the definition. `composeTransform` has its own entry (G-01).

---

## G-03 — the vector `narrow` stays `FORCENOINLINE`; every vector narrowed to og passes through it

**Site:** the definition of `ueFrameDetail::narrow(double x, double y, double z)` in `UEFrameConversion.cpp`,
between that file's `OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON`.

**The prohibition.** ⛔ Do not drop its `FORCENOINLINE`, do not move it out of the pair, and do not replace a call to it
(`BasicUeFrame::narrowed`, `ueNative::toGlm`) with an inline `static_cast<float>` or a `glm::vec3` constructor from
doubles. Moving its definition back into the header is G-05.

**The consequence.** Under `/fp:fast` MSVC may omit the rounding of a `double` to `float` when the float is widened
back to `double` in the same inlined code, so a "converted" component keeps the Unreal double's extra bits. Measured
in this header's own test (2026-10-25, the real `OGSimulationTests` build): with the conversions inline,
`rotationToUe(rotationToSim(FQuat(0.1, 0.2, 0.3, 0.9)))` returned `W = 0.90000000000000002`, the double, where
`double(0.9f) = 0.89999997615814209`. The `uglm` conversions these functions replaced were out-of-line calls that
always returned rounded floats, so an inline narrowing would make a captured Chaos state, or a log line that prints it,
differ from today's wherever the caller widens the value again (a set-back to Chaos, a `%f` argument).

**What breaks if the tag moves.** The tag marks the definition. The quaternion overload has its own entry (G-04).
`lengthToSim` is a scalar `static_cast` on purpose: it replaced inline casts, not out-of-line calls, so it keeps
their inline form (`UEFrameConversion-rationale.md` §5).

---

## G-04 — the quaternion `narrow` stays `FORCENOINLINE`; every rotation `rotationToSim` returns passes through it

**Site:** the definition of `ueFrameDetail::narrow(double w, double x, double y, double z)` in
`UEFrameConversion.cpp`, between that file's `OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON`.

**The prohibition.** ⛔ Do not drop its `FORCENOINLINE`, do not move it out of the pair, and do not replace a call to it
(`BasicUeFrame::rotationToSim`, `ueNative::toGlmQuat`) with an inline `glm::quat` constructor from doubles. Moving its
definition back into the header is G-05.

**The consequence.** As G-03, for the four quaternion components; the measured case of G-03 is this overload's.

**What breaks if the tag moves.** The tag marks the definition. The vector overload has its own entry (G-03).

---

## G-05 — the header holds no `OGSIM_OPTIMIZE_OFF` region: the four `ueFrameDetail` helpers are only declared here

**Site:** an absence tag at the end of `UEFrameConversion.h`'s include block, where the header's
`OGSIM_OPTIMIZE_OFF` … `OGSIM_OPTIMIZE_ON` region and its four definitions stood until og-sim-metres-yup task 7b.

**The prohibition.** ⛔ Do not add an `OGSIM_OPTIMIZE_OFF` region to this header, and do not move the definition of
`narrow`, `composeTransform` or `decomposeTransform` back into it: they are defined in `UEFrameConversion.cpp`
(G-01 to G-04) and exported with `OGSIMULATIONUNREAL_API`.

**The consequence (measured).** With the helpers defined in the header between the pair, every translation unit that
includes the header compiles their bodies, and so instantiates the glm templates they use (`glm::translate`,
`glm::mat4_cast`, `glm::scale`, `glm::quat_cast`, `glm::length`) inside `OGSIM_OPTIMIZE_OFF`. MSVC then stopped
inlining some of those templates in the same translation unit's optimized code. Measured by partition-control
disassembly of the Editor builds (og-sim-metres-yup tasks 8 and 7b):
- **Jolt (task 8).** Including the header in `BrawlerJoltBindingUImpl.cpp`, the translation unit that holds the Jolt
  host's explicit instantiation, made `dAttackRadialSimulation::setInitialConditions<JoltPhysicsBodyAdapter>` call the
  generic `glm::translate` instead of inlining and folding `glm::translate(glm::mat4(1.f), rootTranslation)` (196 → 184
  instructions). The folded copy only places the translation; the generic one computes `m[0]*x + m[1]*y + m[2]*z + m[3]`,
  which turns an exact `-0` component into `+0`, and a non-finite one into NaN, in the weapon body's transform. The
  shadow-pose lambda of `overwriteShadowPoses_GameThread` stopped inlining `glm::mat4_cast` (196 → 129 instructions),
  and the inlining in `onPostPhysicsStep` and `beforePresent_GameThread` changed with it.
- **Chaos (task 7b).** The Chaos host's instantiation translation unit, `ChaosTickBackendUImpl.cpp`, includes the header
  through the Chaos adapter headers. On the tree before task 7b its `setInitialConditions<ChaosPhysicsBodyAdapter>`
  called `glm::translate` in the same way (184 instructions). With the definitions in the `.cpp` it inlines it again (196),
  instruction for instruction like the Jolt instantiation in a translation unit that does not see the region
  (only the adapter's name and its call form differ).
- **Seen failing (task 7b).** `composeTransform` alone, put back into the header between the pair, brought back all of
  the Jolt changes above.

So "a pure include, no code change" stops being bit-identical for the simulation, and no test target sees it: the
golden trace and the suites do not build the Unreal modules.

**What breaks if the tag moves.** Nothing checks it: the header compiles either way, and only a per-function
disassembly of an includer in the Unreal modules shows the change. `BrawlerJoltBindingUImpl-guards.md`
G-01, which forbade the include in the Jolt host's translation unit for this reason, is retired with this entry as its
successor.

---

## R. Retired ids

*(None yet.)*
