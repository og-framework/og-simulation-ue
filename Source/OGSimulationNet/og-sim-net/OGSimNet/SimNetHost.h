#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetHost-rationale.md

#include "OGSimulation/Network/SimTransport.h"

#include <concepts>
#include <cstdint>

namespace ogsimnet
{
enum class SimNetStream : uint8_t { Input, State };

template <class H>
concept SimNetServerHost = requires(H& host, const char* line, SimConnectionId conn, uint8_t simId, uint32_t tick, double seconds)
{
	{ host.nowSeconds() } -> std::same_as<double>;
	{ host.rttMs(conn) } -> std::same_as<double>;
	{ host.receptionTick() } -> std::same_as<int32_t>;
	{ host.log(line) } -> std::same_as<void>;
	{ host.stampServerReceived(simId, tick, seconds) } -> std::same_as<void>;
	{ host.stampHanded(simId, tick) } -> std::same_as<void>;
};

template <class H>
concept SimNetClientHost = requires(H& host, const char* line, SimNetStream stream, uint8_t simId, uint8_t value, uint32_t tick,
	double seconds, bool replayed)
{
	{ host.nowSeconds() } -> std::same_as<double>;
	{ host.serverRttSeconds() } -> std::same_as<double>;
	{ host.onTimingInfo(tick, seconds) } -> std::same_as<void>;
	{ host.onRelayFloor(value, replayed) } -> std::same_as<void>;
	{ host.onConnectionTier(value, value, replayed) } -> std::same_as<void>;
	{ host.onRosterAdded(simId) } -> std::same_as<void>;
	{ host.onRosterRemoved(simId) } -> std::same_as<void>;
	{ host.onWireMismatch(value, value) } -> std::same_as<void>;
	{ host.log(line) } -> std::same_as<void>;
	{ host.stampClientReceived(stream, simId, tick, seconds) } -> std::same_as<void>;
};

template <class H>
concept SimNetDeliveryHook = requires(H& host, SimLane lane, double recvSeconds)
{
	{ host.noteDelivered(lane, recvSeconds) } -> std::same_as<void>;
};

namespace simNetHostMemberNames
{
struct Names
{
	int noteDelivered;
};

template <class H>
struct Probe : H, Names
{
};
} // namespace simNetHostMemberNames

template <class H>
concept SimNetHostHasNoDeliveryMember = requires { &simNetHostMemberNames::Probe<H>::noteDelivered; };

template <class H>
concept SimNetDeliveryHookWellFormed = SimNetDeliveryHook<H> || SimNetHostHasNoDeliveryMember<H>;

namespace simNetHostSelfCheck
{
struct ReferenceServerHost
{
	double nowSeconds();
	double rttMs(SimConnectionId conn);
	int32_t receptionTick();
	void log(const char* line);
	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds);
	void stampHanded(uint8_t simId, uint32_t captureTick);
};

struct ReferenceClientHost
{
	double nowSeconds();
	double serverRttSeconds();
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onRelayFloor(uint8_t floorTicks, bool replayed);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier, bool replayed);
	void onRosterAdded(uint8_t simId);
	void onRosterRemoved(uint8_t simId);
	void onWireMismatch(uint8_t received, uint8_t expected);
	void log(const char* line);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct ServerHostWithAFloatRtt : ReferenceServerHost
{
	float rttMs(SimConnectionId conn);
};

struct ReferenceDeliveryServerHost : ReferenceServerHost
{
	void noteDelivered(SimLane lane, double recvSeconds);
};

struct ReferenceDeliveryClientHost : ReferenceClientHost
{
	void noteDelivered(SimLane lane, double recvSeconds);
};

struct HostWithTheEndpointsThreeArgumentNoteDelivered : ReferenceServerHost
{
	void noteDelivered(SimLane lane, double recvSeconds, double nowSeconds);
};

struct HostWithAPrivateNoteDelivered : ReferenceClientHost
{
private:
	void noteDelivered(SimLane lane, double recvSeconds);
};

static_assert(SimNetServerHost<ReferenceServerHost>, "SimNetServerHost self-check: the reference server host must satisfy the concept");
static_assert(SimNetClientHost<ReferenceClientHost>, "SimNetClientHost self-check: the reference client host must satisfy the concept");
static_assert(!SimNetServerHost<ReferenceClientHost> && !SimNetClientHost<ReferenceServerHost>,
	"SimNetHost: a server host and a client host are different services; neither reference shape satisfies the other concept");
static_assert(!SimNetServerHost<ServerHostWithAFloatRtt>,
	"SimNetServerHost: rttMs returns double. The tier table smooths a double, and a narrowed reading would change tier "
	"transitions silently (the coordinator's 'rttMs stays double' fence)");
static_assert(SimNetServerHost<ReferenceDeliveryServerHost> && SimNetDeliveryHook<ReferenceDeliveryServerHost>
		&& SimNetClientHost<ReferenceDeliveryClientHost> && SimNetDeliveryHook<ReferenceDeliveryClientHost>,
	"SimNetDeliveryHook self-check: a host with noteDelivered(SimLane, double) is still its role's host, and has the hook");
static_assert(!SimNetDeliveryHook<ReferenceServerHost> && SimNetDeliveryHookWellFormed<ReferenceServerHost>
		&& SimNetDeliveryHookWellFormed<ReferenceClientHost>,
	"SimNetDeliveryHook: the hook is optional; a host without a noteDelivered member is well formed and is never called");
static_assert(SimNetDeliveryHookWellFormed<ReferenceDeliveryServerHost> && !SimNetHostHasNoDeliveryMember<ReferenceDeliveryServerHost>,
	"SimNetHostHasNoDeliveryMember self-check: the name probe sees a public noteDelivered");
static_assert(!SimNetDeliveryHookWellFormed<HostWithTheEndpointsThreeArgumentNoteDelivered>
		&& !SimNetDeliveryHookWellFormed<HostWithAPrivateNoteDelivered>,
	"SimNetDeliveryHookWellFormed: a host member named noteDelivered that the sessions cannot call as noteDelivered(SimLane, "
	"double) -> void (another arity, such as the endpoint's (lane, recvSeconds, now), or private) is refused, because the "
	"sessions would silently never call it");
} // namespace simNetHostSelfCheck
} // namespace ogsimnet
