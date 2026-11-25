<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionGame.h` — rationale

`SessionGame.h` holds the game's two ports into the session runtime (og-session-runtime design §3.5): the engine-free traits
(`SessionGame`) and the engine-side hooks (`SessionGameHooks`). Each is a concept with a reference shape and one negative
per requirement, checked by `static_assert`s in the header, as in `SessionEngine.h` (`SessionEngine-rationale.md` §1). It is
the docs pointer target of `SessionGame.h`; it states no prohibition outside those checks, so there is no guards file.

**If this file and `SessionGame.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Two halves

A game supplies the runtime one traits type with its engine-free facts, and, on the engine side, an object with the hooks
that touch the engine (design §3.5). Today both halves are `BrawlerSessionGameUImpl`'s members, beside
`BrawlerSessionTraitsUImpl` and `UEBrawlerNetConfig`; the traits move to og-brawler as the design's Q13 says, the hooks stay
in the game's Unreal module. og-session-runtime task 22 declares the ports only; nothing models them outside the self-checks.

## §2 `SessionGame`: the engine-free traits

| requirement | today | negative in `sessionGameSelfCheck` |
|---|---|---|
| `StaticData`, the type the composition is built over | `simulatableBrawler::StaticData` | `StaticDataIsAValue` |
| `neutralInput()` returns `PlayerInput` by value | `BrawlerSessionGameUImpl::neutralInput` | `NeutralInputByReference` |
| `rootBodyId(const Simulatable&)` returns og-simulation's `BodyId` | `BrawlerSessionGameUImpl::rootBodyId` | `RootBodyIdAsInteger` |
| `backendFingerprint()` returns `std::string_view` | `BrawlerSessionGameUImpl::backendFingerprint` | `FingerprintAsPointer` |
| `installGameLoggers(bool isAuthority)` | `BrawlerSessionGameUImpl::installGameLoggers` | `MissingInstallGameLoggers` |
| `authorityPresentationSlice_Step(const SystemsExec&, uint32_t)` returns `AuthorityPresentationSlice`, read on the step from const systems | `BrawlerSessionGameUImpl::authorityPresentationSlice_Step` | `SliceReadMutatesSystems` |
| `kTickFrequencyHz` converts implicitly to `int32_t` | `UEBrawlerNetConfig::tickFrequencyHz` (an `int`) | `TickFrequencyAsScopedEnum` |
| `kTickFrequencyHz` is a constant expression | `UEBrawlerNetConfig::tickFrequencyHz` | `RuntimeTickFrequency` |
| `kWireTokenFragment` converts to og-simulation's `simWireToken::Token` | `UEBrawlerNetConfig::kSimNetWireTokenFragment` | `WireTokenFragmentOfAnotherType` |
| `kWireTokenFragment` is a constant expression | `UEBrawlerNetConfig::kSimNetWireTokenFragment` | `RuntimeWireTokenFragment` |

- `Simulatable`, `PlayerInput`, `SystemsExec` and `AuthorityPresentationSlice` are required through the signatures that name
  them, so they carry no negative of their own.
- **Each constant has two requirements, its type and its being a constant expression, and a negative for each.** A
  constant of the wrong type passes the constant-expression line (`static_cast<int32_t>` accepts a scoped
  enumeration; a constant of any literal type whose `length` converts to `std::size_t` passes the fragment's), so
  only the type line rejects it.
- **The constant-expression lines work the way `TickDriverEngine` checks `kHostDiagnostics`**: as a template argument, so a
  value only known at run time fails the concept instead of the build. The fragment is a constant because a game joins its
  wire token with og-simulation's `simWireToken::join`, which is `consteval`.
- **Not yet in the concept:** `emplaceSystems` and `gravity` (design §3.5). Both take or give the engine traits' types (the
  body and query adapters, the world host's options), so they join with the composition (task 31), where those types are
  known.

## §3 `SessionGameHooks`: the engine-side hooks

`SessionGameHooks<Hooks, Game>` requires that `Game` is a `SessionGame` and that the hooks object offers the six hooks of
design §3.3's last row, each returning `void`:

| hook | today | negative in `sessionGameHooksSelfCheck` |
|---|---|---|
| `Game` is a `SessionGame` | — | `HooksOfNoGame` (over `MissingInstallGameLoggers`) |
| `sampleInput()` | `BrawlerSessionHooksUImpl::sampleInput` (a static over the session since og-session-runtime task 26) | `SampleReturnsBool` |
| `afterBodiesCreated(Simulatable&, EntityKey, uint32_t id, bool isAuthority)` | `BrawlerSessionGameUImpl::afterBodiesCreated`, which takes the engine's character; the runtime passes the opaque key | `BodiesCreatedWithoutEntityKey` |
| `afterRegistered(uint32_t, bool)` | `BrawlerSessionGameUImpl::afterRegistered` | `MissingAfterRegistered` |
| `onUnregistered(uint32_t)` | `BrawlerSessionGameUImpl::onUnregistered` | `MissingOnUnregistered` |
| `presentAuthoritySlice(uint32_t, const AuthorityPresentationSlice&)` | `BrawlerSessionGameUImpl::presentAuthoritySlice` | `PresentTakesMutableSlice` |
| `afterVisualization()` | `BrawlerSessionGameUImpl::afterVisualization`, which reads the session; the runtime calls it after the visualization pass | `AfterVisualizationTakesAnArgument` |

Sim ids are `uint32_t` here; today's hooks take `unsigned int`, the same type on every target this project builds.
