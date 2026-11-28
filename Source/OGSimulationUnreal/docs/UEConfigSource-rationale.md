<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: FCString::Atod -- Unreal Engine symbol, outside every scan root -->
<!-- lint-external-ref: ConfigCacheIni.cpp -- Unreal Engine file (Runtime/Core/Private/Misc), outside every scan root -->
# `UEConfigSource.h` — rationale

`UEConfigSource` is the Unreal adapter's model of og-sim-session's `ConfigSource` port (`SessionEngine-rationale.md` §4): it reads
one key of the `[OGNetcode]` section through `GConfig`, as `readInt`, `readDouble` or `readString`, each `std::optional`, absent
when no ini sets the key. The Unreal session passes one to og-sim-session's `readSessionKnobs` in `beginPlay`
(`UESimulationSession-rationale.md` §9.8). It is the docs pointer target of `UEConfigSource.h`; it has no prohibition (the
header's one rule, that it models the port, is a `static_assert`), so there is no guards file.

**If this file and `UEConfigSource.h` disagree, the header is authoritative and this file is stale.**

---

## §1 The reads are the knob intake's, unchanged

Until og-session-runtime task 23 the Unreal session's `readSessionKnobs` read each key itself; task 23 moved the intake to
og-sim-session and the reads here. Each read is the call the intake made, for one key:

- **The game ini, then the engine ini.** `GetInt` (or `GetDouble`, `GetString`) on `GGameIni`, and only if that finds no key, on
  `GEngineIni`. The game ini is the conventional place for a gameplay-tuning value; the engine ini is where the brawler keeps its
  netcode settings (its `Config/DefaultEngine.ini` `[OGNetcode]` section). A key in both takes the game ini's value.
- **Present means "the key is there".** `GConfig`'s `GetInt` and `GetDouble` read the key as a string and, when it is there,
  answer `true` with `FCString::Atoi` or `FCString::Atod` of its text (`ConfigCacheIni.cpp`), whether or not the text is a
  number: a word reads 0. So a mistyped number is a present value, as it was; og-sim-session's intake then reports what it does
  with it.
- **Words are trimmed at both ends** (`FString::TrimStartAndEnd`), on the `FString`, before the UTF-8 conversion, as the intake's
  `iniPolicy.TrimStartAndEnd()` was. A blank value reads as the empty word, which no knob accepts.
- **No `GConfig`, no value.** Every read answers absent when `GConfig` is null, as the intake's `GConfig != nullptr` blocks did.
- **The key** arrives as og-sim-session's `const char*` and is widened with `UTF8_TO_TCHAR`; every key is ASCII.

## §2 What it does not do

It holds no state and caches nothing: each read goes to `GConfig`, once, at the session's start. `beginPlay` calls
`readSessionKnobs` on the game thread on both roles, before the configuration it then runs (on the runner for an authority),
as it called the member. It reads one section only, `OGNetcode`; another section would be another source.

## §3 How it is checked

The `static_assert` beside the class checks it models `ConfigSource`. Its reads are the same `GConfig` calls the transcription
test's `FakeUEConfigSource` models (`SessionKnobs-rationale.md` §8), and the smoke's start-up lines (the `[OGNetcode]` keys the
brawler's ini sets) read byte for byte as before task 23.
