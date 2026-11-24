#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionGame-rationale.md

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "OGSimulation/BodyId.h"
#include "OGSimulation/Network/SimWireToken.h"
#include "OGSimSession/SessionTypes.h"

namespace ogsession
{
template <typename G>
concept SessionGame = requires(const typename G::Simulatable& simulatable, const typename G::SystemsExec& systems, uint32_t id,
	bool isAuthority)
{
	typename G::StaticData;
	{ G::neutralInput() }                             -> std::same_as<typename G::PlayerInput>;
	{ G::rootBodyId(simulatable) }                     -> std::same_as<BodyId>;
	{ G::backendFingerprint() }                        -> std::same_as<std::string_view>;
	{ G::installGameLoggers(isAuthority) }             -> std::same_as<void>;
	{ G::authorityPresentationSlice_Step(systems, id) } -> std::same_as<typename G::AuthorityPresentationSlice>;
	{ G::kTickFrequencyHz }                            -> std::convertible_to<int32_t>;
	typename std::integral_constant<int32_t, static_cast<int32_t>(G::kTickFrequencyHz)>;
	{ G::kWireTokenFragment }                          -> std::convertible_to<simWireToken::Token>;
	typename std::integral_constant<std::size_t, G::kWireTokenFragment.length>;
};

namespace sessionGameSelfCheck
{
struct Simulatable {};
struct PlayerInput {};
struct StaticData {};
struct SystemsExec {};
struct Slice {};

struct Conforming
{
	using Simulatable                = sessionGameSelfCheck::Simulatable;
	using PlayerInput                = sessionGameSelfCheck::PlayerInput;
	using StaticData                 = sessionGameSelfCheck::StaticData;
	using SystemsExec                = sessionGameSelfCheck::SystemsExec;
	using AuthorityPresentationSlice = sessionGameSelfCheck::Slice;

	static constexpr int32_t kTickFrequencyHz = 60;
	static constexpr simWireToken::Token kWireTokenFragment = simWireToken::tokenOf({ { "g", 1u } });

	static PlayerInput neutralInput();
	static BodyId rootBodyId(const Simulatable&);
	static std::string_view backendFingerprint();
	static void installGameLoggers(bool);
	static Slice authorityPresentationSlice_Step(const SystemsExec&, uint32_t);
};

struct StaticDataIsAValue : Conforming
{
	static constexpr int StaticData = 0;
};

struct NeutralInputByReference : Conforming
{
	static const PlayerInput& neutralInput();
};

struct RootBodyIdAsInteger : Conforming
{
	static uint32_t rootBodyId(const Simulatable&);
};

struct FingerprintAsPointer : Conforming
{
	static const char* backendFingerprint();
};

struct MissingInstallGameLoggers
{
	using Simulatable                = sessionGameSelfCheck::Simulatable;
	using PlayerInput                = sessionGameSelfCheck::PlayerInput;
	using StaticData                 = sessionGameSelfCheck::StaticData;
	using SystemsExec                = sessionGameSelfCheck::SystemsExec;
	using AuthorityPresentationSlice = sessionGameSelfCheck::Slice;

	static constexpr int32_t kTickFrequencyHz = 60;
	static constexpr simWireToken::Token kWireTokenFragment = simWireToken::tokenOf({ { "g", 1u } });

	static PlayerInput neutralInput();
	static BodyId rootBodyId(const Simulatable&);
	static std::string_view backendFingerprint();
	static Slice authorityPresentationSlice_Step(const SystemsExec&, uint32_t);
};

struct SliceReadMutatesSystems : Conforming
{
	static Slice authorityPresentationSlice_Step(SystemsExec&, uint32_t);
};

enum class TickFrequency : int32_t
{
	Hz60 = 60,
};

struct TickFrequencyAsScopedEnum : Conforming
{
	static constexpr TickFrequency kTickFrequencyHz = TickFrequency::Hz60;
};

struct RuntimeTickFrequency : Conforming
{
	static inline int32_t kTickFrequencyHz = 60;
};

struct NotAToken
{
	std::size_t length = 1u;
};

struct WireTokenFragmentOfAnotherType : Conforming
{
	static constexpr NotAToken kWireTokenFragment{};
};

struct RuntimeWireTokenFragment : Conforming
{
	static inline simWireToken::Token kWireTokenFragment = {};
};

static_assert(SessionGame<Conforming>,
	"SessionGame self-check: the reference shape must satisfy the concept");
static_assert(!SessionGame<StaticDataIsAValue>,
	"SessionGame self-check: StaticData names the game's static data type, which the composition is built over");
static_assert(!SessionGame<NeutralInputByReference>,
	"SessionGame self-check: neutralInput returns the game's input by value, the input of a character with none");
static_assert(!SessionGame<RootBodyIdAsInteger>,
	"SessionGame self-check: rootBodyId returns og-simulation's BodyId, never a bare integer");
static_assert(!SessionGame<FingerprintAsPointer>,
	"SessionGame self-check: backendFingerprint returns a std::string_view");
static_assert(!SessionGame<MissingInstallGameLoggers>,
	"SessionGame self-check: installGameLoggers (the game's loggers, per role) is required");
static_assert(!SessionGame<SliceReadMutatesSystems>,
	"SessionGame self-check: the presentation slice is read from const systems on the step; reading changes nothing");
static_assert(!SessionGame<TickFrequencyAsScopedEnum>,
	"SessionGame self-check: kTickFrequencyHz converts implicitly to int32_t; a scoped enumeration, even one over "
	"int32_t, does not");
static_assert(!SessionGame<RuntimeTickFrequency>,
	"SessionGame self-check: kTickFrequencyHz must be a constant expression, because the session's budgets are sized with it");
static_assert(!SessionGame<WireTokenFragmentOfAnotherType>,
	"SessionGame self-check: kWireTokenFragment converts to og-simulation's simWireToken::Token, the type "
	"simWireToken::join takes; another type with a length does not");
static_assert(!SessionGame<RuntimeWireTokenFragment>,
	"SessionGame self-check: kWireTokenFragment must be a constant expression, because a game joins its wire token at "
	"compile time");
} // namespace sessionGameSelfCheck

template <typename H, typename G>
concept SessionGameHooks = SessionGame<G>
	&& requires(H& h, typename G::Simulatable& simulatable, EntityKey entityKey, uint32_t id, bool isAuthority,
		const typename G::AuthorityPresentationSlice& slice)
{
	{ h.sampleInput() }                                           -> std::same_as<void>;
	{ h.afterBodiesCreated(simulatable, entityKey, id, isAuthority) } -> std::same_as<void>;
	{ h.afterRegistered(id, isAuthority) }                        -> std::same_as<void>;
	{ h.onUnregistered(id) }                                      -> std::same_as<void>;
	{ h.presentAuthoritySlice(id, slice) }                        -> std::same_as<void>;
	{ h.afterVisualization() }                                    -> std::same_as<void>;
};

namespace sessionGameHooksSelfCheck
{
using Game = sessionGameSelfCheck::Conforming;

struct Conforming
{
	void sampleInput();
	void afterBodiesCreated(Game::Simulatable&, EntityKey, uint32_t, bool);
	void afterRegistered(uint32_t, bool);
	void onUnregistered(uint32_t);
	void presentAuthoritySlice(uint32_t, const Game::AuthorityPresentationSlice&);
	void afterVisualization();
};

struct SampleReturnsBool : Conforming
{
	bool sampleInput();
};

struct BodiesCreatedWithoutEntityKey : Conforming
{
	void afterBodiesCreated(Game::Simulatable&, uint32_t, bool);
};

struct MissingAfterRegistered
{
	void sampleInput();
	void afterBodiesCreated(Game::Simulatable&, EntityKey, uint32_t, bool);
	void onUnregistered(uint32_t);
	void presentAuthoritySlice(uint32_t, const Game::AuthorityPresentationSlice&);
	void afterVisualization();
};

struct MissingOnUnregistered
{
	void sampleInput();
	void afterBodiesCreated(Game::Simulatable&, EntityKey, uint32_t, bool);
	void afterRegistered(uint32_t, bool);
	void presentAuthoritySlice(uint32_t, const Game::AuthorityPresentationSlice&);
	void afterVisualization();
};

struct PresentTakesMutableSlice : Conforming
{
	void presentAuthoritySlice(uint32_t, Game::AuthorityPresentationSlice&);
};

struct AfterVisualizationTakesAnArgument : Conforming
{
	void afterVisualization(double);
};

struct HooksOfNoGame : Conforming
{
};

static_assert(SessionGameHooks<Conforming, Game>,
	"SessionGameHooks self-check: the reference shape must satisfy the concept");
static_assert(!SessionGameHooks<SampleReturnsBool, Game>,
	"SessionGameHooks self-check: sampleInput returns void, so it cannot veto or skip the frame");
static_assert(!SessionGameHooks<BodiesCreatedWithoutEntityKey, Game>,
	"SessionGameHooks self-check: afterBodiesCreated takes the entity key, the engine's handle of the entity whose bodies "
	"were created");
static_assert(!SessionGameHooks<MissingAfterRegistered, Game>,
	"SessionGameHooks self-check: afterRegistered is required");
static_assert(!SessionGameHooks<MissingOnUnregistered, Game>,
	"SessionGameHooks self-check: onUnregistered is required");
static_assert(!SessionGameHooks<PresentTakesMutableSlice, Game>,
	"SessionGameHooks self-check: presentAuthoritySlice reads the slice and cannot rewrite it");
static_assert(!SessionGameHooks<AfterVisualizationTakesAnArgument, Game>,
	"SessionGameHooks self-check: afterVisualization takes nothing");
static_assert(!SessionGameHooks<HooksOfNoGame, sessionGameSelfCheck::MissingInstallGameLoggers>,
	"SessionGameHooks self-check: the hooks are another game's half, so their game must be a SessionGame");
} // namespace sessionGameHooksSelfCheck
} // namespace ogsession
