<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: SessionGameHooks -- retired by og-session-runtime task 33b (no template was constrained by it and no engine type modelled it; SessionGame-rationale.md section 3); kept in the history -->
<!-- lint-external-ref: sessionGameHooksSelfCheck -- retired by og-session-runtime task 33b (no template was constrained by it and no engine type modelled it; SessionGame-rationale.md section 3); kept in the history -->
# `SessionGame.h` — rationale

`SessionGame.h` holds the game's port into the session runtime (og-session-runtime design §3.5): the engine-free traits
(`SessionGame`), a concept with a reference shape and one negative per requirement, checked by `static_assert`s in the
header, as in `SessionEngine.h` (`SessionEngine-rationale.md` §1). The engine-side hooks' concept, `SessionGameHooks`, was
retired by og-session-runtime task 33b (§3). It is
the docs pointer target of `SessionGame.h`; it states no prohibition outside those checks, so there is no guards file.

**If this file and `SessionGame.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Two halves

A game supplies the runtime one traits type with its engine-free facts, and, on the engine side, an object with the hooks
that touch the engine (design §3.5). Today both halves are `BrawlerSessionGameUImpl`'s members, beside
`BrawlerSessionTraitsUImpl` and `UEBrawlerNetConfig`; the traits move to og-brawler as the design's Q13 says, the hooks stay
in the game's Unreal module. og-session-runtime task 22 declared both ports; the engine-side one never gained a model or a
constrained user and was retired by task 33b (§3).

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

## §3 The engine-side hooks: what an adapter provides (`SessionGameHooks` retired, og-session-runtime task 33b)

Task 22 declared `SessionGameHooks<Hooks, Game>`: the six engine-side hooks of design §3.3's last row (`sampleInput`,
`afterBodiesCreated(Simulatable&, EntityKey, id, isAuthority)`, `afterRegistered`, `onUnregistered`, `presentAuthoritySlice`,
`afterVisualization`), each returning `void`, with a reference shape and one negative each. Nothing was constrained by it: no
template took it as a constraint and no `static_assert` named a model, only its own self-check and one `STATIC_REQUIRE` in
`SessionSkeletonTest.cpp`. No engine type modelled it either: the Unreal game has no `sampleInput`, its `afterBodiesCreated`
takes the engine's character and the net mode, and its `presentAuthoritySlice` and `afterVisualization` are statics that take
the session. Task 33b deleted the concept, `sessionGameHooksSelfCheck` and that `STATIC_REQUIRE`. What an engine adapter
provides instead:

- **The runtime calls no engine-side game hook.** It calls the game's statics through its traits (`SessionGame`, §2, and
  `emplaceSystems`, which `SessionRuntime::start` checks) and the adapter's own hooks through `SessionRuntimeHost`
  (`SessionRuntime.h`), `sampleInput()` among them; the Unreal adapter forwards that one to the brawler's
  `BrawlerSessionHooksUImpl::sampleInput`.
- **The adapter calls the game's engine-side hooks itself**, in its engine's types. On Unreal: `beforeWorldBuild` and
  `afterSessionConfigured` at start, `afterBodiesCreated(simulatable, character, id, isAuthority, netMode)` and
  `afterRegistered(id, isAuthority)` in `tryRegister`, `onUnregistered(id)` in `unregisterFromNewFramework`
  (`ueSimulationSessionHookNames::GameInstanceHooks` checks these and `rootBodyId`), and the statics
  `presentAuthoritySlice(session, id, slice)` and `afterVisualization(session)` in its visualization pass.

So a new engine models `SessionRuntimeHost` and calls its own game's engine-side hooks in whatever shape its engine needs;
og-sim-session names none of them (`Porting.md` item 8).
