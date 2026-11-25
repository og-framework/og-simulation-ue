#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionTypes-rationale.md

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/StaticGeometry.h"

namespace ogsession
{
enum class SessionRole : uint8_t
{
	Authority,
	Prediction,
};

struct EntityKey
{
	std::uintptr_t value = 0u;

	friend bool operator==(EntityKey, EntityKey) = default;
};

struct SessionStart
{
	SessionRole            role                      = SessionRole::Prediction;
	double                 stepSeconds               = 0.0;
	int32_t                requestedMaxCatchUpSteps  = 0;
	double                 authorityIdleSlackSeconds = 0.0;
	bool                   highResolutionWait        = false;
	StaticWorldDescription staticWorld;
	std::vector<uint8_t>   physicsOnlyShapes;
};

struct RegisterRequest
{
	uint32_t        simId           = 0u;
	EntityKey       entityKey;
	std::string     debugName;
	SimConnectionId ownerConnection = kInvalidSimConnection;
	uint8_t         localIndex      = 0u;
	bool            engineSaysLocal = false;
};

enum class RegisterStatus : uint8_t
{
	Pending,
	Ready,
	Refused,
};

enum class RegisterRefusal : uint8_t
{
	SimNetRefused,
};

namespace lifecycleOutcome
{
struct Registered
{
	uint32_t simId = 0u;
	uint32_t slot  = 0u;
};

struct Refused
{
	uint32_t        simId  = 0u;
	RegisterRefusal reason = RegisterRefusal::SimNetRefused;
};

struct Unregistered
{
	uint32_t simId = 0u;
};

struct RosterRemoved
{
	uint32_t simId = 0u;
};

struct BodiesReleasable
{
	EntityKey entityKey;
};
} // namespace lifecycleOutcome

using LifecycleOutcome = std::variant<lifecycleOutcome::Registered, lifecycleOutcome::Refused, lifecycleOutcome::Unregistered,
	lifecycleOutcome::RosterRemoved, lifecycleOutcome::BodiesReleasable>;

static_assert(std::variant_size_v<LifecycleOutcome> == 5u,
	"LifecycleOutcome: one alternative per outcome kind the frame thread collects (Registered, Refused, Unregistered, "
	"RosterRemoved, BodiesReleasable)");
static_assert(std::is_trivially_copyable_v<LifecycleOutcome>,
	"LifecycleOutcome: an outcome is plain data, posted by the owner thread and copied out by the frame thread");
} // namespace ogsession
