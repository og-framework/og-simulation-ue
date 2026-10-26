<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEFrameConversion` — rationale

`UEFrameConversion.h` is the Unreal edge of og-simulation's frame: every value that crosses between Unreal
(centimetres, left-handed, +X forward, +Y right, +Z up) and og-simulation (the units and axes `SimulationFrame.h`'s
`simFrame` names) is converted there, by the rule of its kind. `UEFrameConversion.cpp` defines the four out-of-line
helpers the header declares (§5). The sources hold the licence, a docs pointer, code and five guard tags (one in the
header, four in the `.cpp`); this file carries the why. The five prohibitions live in `UEFrameConversion-guards.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. Placement and what it depends on

The header depends on Unreal's `Core` (`FVector`, `FQuat`, `FTransform`, `checkf`), glm, and og-simulation's
`SimulationFrame.h` (the frame). It declares the four `ueFrameDetail` helpers and defines everything else inline.
`UEFrameConversion.cpp` defines those four, in the OGSimulationUnreal module, which exports them
(`OGSIMULATIONUNREAL_API`); it also includes `CompilerControl.h` (the `OGSIM_OPTIMIZE_OFF`/`ON` pair) and the glm header that
declares `glm::translate` and `glm::scale`. Neither file includes anything else of the OGSimulationUnreal module, so a target that depends
on `Core` and `OGSimulation` only can use them by path without linking the module: the root repository's
`OGSimulationTests` does (`Source/OGSimulationTests/UEFrameConversionTest.cpp`, with an include path to this plugin's
`Source` folder in `OGSimulationTests.Build.cs`). Such a target sees no `OGSIMULATIONUNREAL_API` (UBT defines,
for each module, the API macros of that module and of its dependencies only), so the test defines it empty and compiles
`UEFrameConversion.cpp` into its own binary by including it (§6).

## 2. The three namespaces

* **`BasicUeFrame<Frame>`** converts between Unreal and the og frame `Frame`. `ueFrame` is
  `BasicUeFrame<simFrame>`, the shipped frame. The template exists so a test can instantiate the right-handed Y-up
  metre frame (`BasicSimFrame<AxisFrame::YUpRightHanded, 1.f>`) before the knobs are flipped, and check every rule
  through the reflection while the shipped frame is still Unreal's own.
* **`ueNative`** copies between Unreal and glm types with no frame change: `toGlm`, `toFVector`, `toGlmQuat`,
  `toFQuat`, `toGLMMat4`, `toFTransform`. It is for glm maths that is in Unreal's frame by design (the humanoid mesh,
  the Unreal camera), and it keeps scale, which a frame conversion of a rigid transform does not carry (§4).
* **`ueFrameDetail`** holds the two narrowing helpers and the two transform helpers both of the others call, declared
  in the header and defined in `UEFrameConversion.cpp` (§5).

## 3. The rules

With M the axis map (M v puts Unreal's forward, right and up components on `Frame::kForward`, `kRight` and `kUp`,
each with its sign) and s the og length per Unreal centimetre (`1 / Frame::kUeCentimetresPerUnit`):

| kind | Unreal → og | og → Unreal | functions |
|---|---|---|---|
| position, displacement, velocity, acceleration, force, impulse | s·M v | Mᵀ p / s | `vectorToSim`, `vectorToSimD`, `vectorToUe` |
| direction, normal | M n | Mᵀ n | `directionToSim`, `directionToUe` |
| angular velocity, rotation axis | det(M)·M ω | det(M)·Mᵀ ω | `axialToSim`, `axialToUe` |
| torque, angular impulse | det(M)·s²·M τ | det(M)·Mᵀ τ / s² | `torqueToSim`, `torqueToUe` |
| quaternion (w, v) | (w, det(M)·M v) | (w, det(M)·Mᵀ v) | `rotationToSim`, `rotationToUe` |
| inertia diagonal | s²·\|M\| I | \|M\|ᵀ I / s² | `inertiaToSim`, `inertiaToUe` |
| box half extents | s·\|M\| h | \|M\|ᵀ h / s | `halfExtentsToSim`, `halfExtentsToUe` |
| scale | \|M\| k | \|M\|ᵀ k | `scaleToSim`, `scaleToUe` |
| scalar length | s·x | x / s | `lengthToSim`, `lengthToUe` |
| rigid transform | [M R Mᵀ \| s M t] | the inverse | `transformToSim`, `transformToUe` |

\|M\| is M with its signs dropped: a permutation, because an extent, a scale or an inertia diagonal has no direction.

* **Pseudovectors take det(M).** An angular velocity, a torque and a quaternion's vector part are cross products,
  which a reflection maps to minus their image. det(M) is +1 in Unreal's own frame and −1 in the right-handed Y-up
  frame, where `kReflects` is true; it is computed from the frame's own axes,
  `dot(cross(kForward, kRight), kUp) < 0`, so it does not depend on `kChartOrientation`, which describes the planar
  chart and changes when the chart does.
* **A rotation maps by conjugation.** With the quaternion rule, `rotationToSim(q) * directionToSim(v)` equals
  `directionToSim(q.RotateVector(v))`: the og rotation of the mapped vector is the mapped Unreal rotation (the test's
  quaternion case, §6). In the Y-up frame the rule is `glm::quat(W, −Y, −Z, X)` one way and `FQuat(z, −x, −y, w)` the
  other.
* **Lengths divide in double, then round, toward og; they multiply exactly in double toward Unreal.** A float times
  100 or 10,000 is exact in double, so og → Unreal → og returns every float bit for bit, and Unreal → og → Unreal returns
  every Unreal value that came from an og float. At the shipped knobs (s = 1) no division and no multiplication is
  made at all: each function is a float↔double copy, component for component, which is what the `uglm` conversions
  it replaced did. ⚠ At s = 0.01 the division is by a compile-time constant, and `/fp:fast` lets the compiler multiply
  by its reciprocal instead, so the double quotient may be one double ulp from the correctly rounded one. For an
  Unreal value that holds a float, the float result is still correctly rounded: such a value cannot be exactly 100
  (or 10⁴) times a midpoint between two floats, so the exact quotient lies about 2⁻³¹ of its magnitude or more from
  one, far outside that error (`SimulationFrame-rationale.md` makes the same argument for its wrappers). For an
  arbitrary double (a Chaos position) a quotient that close to a midpoint is possible, so a float result may, rarely,
  round one float ulp the other way; `vectorToSimD`, which returns the double, may differ in its last bit.
* **The signed index picks keep the sign of zero.** Every component is placed or read at its index with an exact
  negation where the axis is negative, never through a dot product, so in Unreal's own frame a `-0` stays `-0` and a
  `+0` stays `+0`, as with the copies these functions replaced.

## 4. Transforms carry no scale

`transformToSim` converts a rigid transform: it `checkf`s that every scale component is ±1 within
`UE_KINDA_SMALL_NUMBER`, because a non-unit scale has no single meaning on og's side (a shape bakes its scale into its
extents, as the static-geometry importer does). Every caller in this plugin passes a transform built from a rotation
and a translation only. A UE-frame matrix with scale goes through `ueNative::toGLMMat4`. `transformToUe` keeps the
scale the og matrix carries, permuted by \|M\|.

## 5. Out-of-line helpers: the narrowing (G-03, G-04), the transforms (G-01, G-02), and why they live in the `.cpp` (G-05)

**Every double-to-float narrowing of a vector or a quaternion goes through `ueFrameDetail::narrow`,** a
`FORCENOINLINE` function compiled between `OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON`, as the `uglm` copies it
replaced were (toGLMVec3 and toGLMQuat, now deleted: out-of-line, unoptimized). Under `/fp:fast` MSVC may omit the rounding of a
double to a float when the float is widened back to double in the same inlined code. This header's own test
measured it in the real `OGSimulationTests` build (2026-10-25): with the conversions inline, a quaternion taken to
og and back returned the original double `W = 0.90000000000000002`, not `double(0.9f) = 0.89999997615814209`. An
out-of-line function returns a rounded float, so every caller sees the float the old call returned. The scalar
`lengthToSim` stays an inline `static_cast`, because the code it replaced (the importer's radii and half-lengths,
the query adapter's penetration depth) was an inline cast too, in the same place.

**The transform helpers are the old conversions' bodies.**

`ueFrameDetail::composeTransform` and `decomposeTransform` are the bodies of the deleted `uglm` conversions
toGLMMat4 and toFtransform, statement for statement: translate the identity, multiply by the rotation matrix, scale; and the
translation column, `glm::quat_cast` and the three column lengths. `BasicUeFrame` converts the parts (the
translation, the rotation and the scale, each by its rule) and the helper composes them; at the shipped knobs the
parts are the copies the old functions made, so the matrices and transforms are the old ones bit for bit. They are
non-template, `FORCENOINLINE` and compiled between `OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON` because the old
functions were out-of-line unoptimized code; the guards say what breaks otherwise. The importer's `checksum=` line
hashes the bits of every static matrix, and a headless session before and after the move printed the same checksum.

**Why the four are defined in `UEFrameConversion.cpp`, not in the header.** Their bodies use glm templates (`glm::translate`, `glm::mat4_cast`, `glm::scale`,
`glm::quat_cast`, `glm::length`). Defined in the header between the pragma pair, they made every includer instantiate
those templates inside `OGSIM_OPTIMIZE_OFF`, and MSVC then stopped inlining them in the includer's optimized code. In
the brawler's two host translation units, which instantiate the whole simulation, a folded
`glm::translate(glm::mat4(1.f), t)` became a call to the generic `translate`, which can turn an exact `-0` translation
component into `+0` (G-05 has the measurements). In `UEFrameConversion.cpp` only that file instantiates them under the
pragma. OGSimulationUnreal is built without unity files (`bUseUnity = false`), so the `.cpp` is its own translation
unit in every target and shares its pragma region with no other file. The callers in other modules call the exported
functions through the DLL import table in the Editor build; the call is the only change in their code.

## 6. The test

`Source/OGSimulationTests/UEFrameConversionTest.cpp` (tag `[SimulationFrame][UEFrameConversion]`, so `[@og]` runs
it through og-simulation-tests' existing `[SimulationFrame]` alias entry) instantiates
`BasicUeFrame<BasicSimFrame<UeZUpLeftHanded, 100>>` and `BasicUeFrame<BasicSimFrame<YUpRightHanded, 1>>` and checks:
* at Unreal's own frame, every vector, quaternion and length function is the component copy, bit for bit, `-0` and
  the edge floats included; `transformToSim` equals a reference translate-rotate-scale composition and is bit-equal to
  `ueNative::toGLMMat4`;
* the Y-up metre table on hand-picked values (the design's map og = 0.01·(ue.y, ue.z, −ue.x));
* the round trips of §3, bit for bit, at both;
* the quaternion rule within 1e-6, and the transform rule (a point transformed in Unreal, then mapped, equals the
  mapped point transformed by `transformToSim`), at both;
* the winding rule: the og triangle the importer's index rule (`reversesWinding`, `UEStaticGeometryImporter.cpp`)
  builds from an Unreal triangle has the normal M·n·s² (−M·n·s² for a mirrored scale), at both.

`static_assert`s at the top of the test pin `kReflects` for the four knob combinations.

The test target does not link the OGSimulationUnreal module (§1), so the test defines `OGSIMULATIONUNREAL_API` empty
before it includes the header, and includes `UEFrameConversion.cpp` after it: the four helpers are compiled into the
test's own translation unit, from the same source and between the same pragma pair as in the module, and are still
called out of line (`FORCENOINLINE`).

## 7. Provenance

Added by og-sim-metres-yup task 7, after design §4 of that initiative. It replaced the frame-free `uglm` conversions,
which classified nothing: every crossing now names its kind. The OGSimulationUnreal
crossings (the render-target table, the static-geometry importer, the tick-driver host's gravity) and the four Chaos
adapter files were routed in the same task. Task 7b of the same initiative moved the four `ueFrameDetail` definitions
out of the header into `UEFrameConversion.cpp` (§5). Task 9 deleted the `uglm` conversions (their header and
`.cpp`), once tasks 7 and 8 had moved every caller to `ueFrame` or `ueNative`.
