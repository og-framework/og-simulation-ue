#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetHost-rationale.md

#include "OGSimulation/Network/SimTransport.h"

#include <concepts>
#include <cstdint>

struct SimClockRttSample;

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

template <class H>
concept SimNetLifecycleUnregisteringHook = requires(H& host, uint8_t simId)
{
	{ host.onLifecycleUnregistering(simId) } -> std::same_as<void>;
};

namespace simNetHostMemberNames
{
struct UnregisteringNames
{
	int onLifecycleUnregistering;
};

template <class H>
struct UnregisteringProbe : H, UnregisteringNames
{
};
} // namespace simNetHostMemberNames

template <class H>
concept SimNetHostHasNoUnregisteringMember = requires { &simNetHostMemberNames::UnregisteringProbe<H>::onLifecycleUnregistering; };

template <class H>
concept SimNetLifecycleUnregisteringHookWellFormed = SimNetLifecycleUnregisteringHook<H> || SimNetHostHasNoUnregisteringMember<H>;

template <class H>
concept SimNetClockRttSampleHook = requires(H& host, const SimClockRttSample& sample)
{
	{ host.onClockSample(sample) } -> std::same_as<void>;
};

namespace simNetHostMemberNames
{
struct ClockSampleNames
{
	int onClockSample;
};

template <class H>
struct ClockSampleProbe : H, ClockSampleNames
{
};
} // namespace simNetHostMemberNames

template <class H>
concept SimNetHostHasNoClockSampleMember = requires { &simNetHostMemberNames::ClockSampleProbe<H>::onClockSample; };

template <class H>
concept SimNetClockRttSampleHookWellFormed = SimNetClockRttSampleHook<H> || SimNetHostHasNoClockSampleMember<H>;

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

struct ReferenceUnregisteringServerHost : ReferenceServerHost
{
	void onLifecycleUnregistering(uint8_t simId);
};

struct ReferenceUnregisteringClientHost : ReferenceClientHost
{
	void onLifecycleUnregistering(uint8_t simId);
};

struct HostWithAnUnregisteringHookThatReturnsBool : ReferenceServerHost
{
	bool onLifecycleUnregistering(uint8_t simId);
};

struct HostWithAPrivateUnregisteringHook : ReferenceClientHost
{
private:
	void onLifecycleUnregistering(uint8_t simId);
};

struct ReferenceClockSampleClientHost : ReferenceClientHost
{
	void onClockSample(const SimClockRttSample& sample);
};

struct HostWithATwoArgumentClockSampleHook : ReferenceClientHost
{
	void onClockSample(const SimClockRttSample& sample, double nowSeconds);
};

struct HostWithAPrivateClockSampleHook : ReferenceClientHost
{
private:
	void onClockSample(const SimClockRttSample& sample);
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
static_assert(SimNetServerHost<ReferenceUnregisteringServerHost> && SimNetLifecycleUnregisteringHook<ReferenceUnregisteringServerHost>
		&& SimNetClientHost<ReferenceUnregisteringClientHost> && SimNetLifecycleUnregisteringHook<ReferenceUnregisteringClientHost>,
	"SimNetLifecycleUnregisteringHook self-check: a host with onLifecycleUnregistering(uint8_t) is still its role's host, and "
	"has the hook");
static_assert(!SimNetLifecycleUnregisteringHook<ReferenceServerHost> && SimNetLifecycleUnregisteringHookWellFormed<ReferenceServerHost>
		&& SimNetLifecycleUnregisteringHookWellFormed<ReferenceClientHost>
		&& SimNetLifecycleUnregisteringHookWellFormed<ReferenceUnregisteringServerHost>,
	"SimNetLifecycleUnregisteringHook: the hook is optional; a host without an onLifecycleUnregistering member is well formed "
	"and is never called");
static_assert(!SimNetLifecycleUnregisteringHookWellFormed<HostWithAnUnregisteringHookThatReturnsBool>
		&& !SimNetLifecycleUnregisteringHookWellFormed<HostWithAPrivateUnregisteringHook>,
	"SimNetLifecycleUnregisteringHookWellFormed: a host member named onLifecycleUnregistering that the sessions cannot call as "
	"onLifecycleUnregistering(uint8_t) -> void (another return type, or private) is refused, because the sessions would "
	"silently never call it and a queue-registered character would leave the game's systems pointing at freed storage");
static_assert(SimNetClientHost<ReferenceClockSampleClientHost> && SimNetClockRttSampleHook<ReferenceClockSampleClientHost>
		&& SimNetClockRttSampleHookWellFormed<ReferenceClockSampleClientHost> && !SimNetHostHasNoClockSampleMember<ReferenceClockSampleClientHost>,
	"SimNetClockRttSampleHook self-check: a client host with onClockSample(const SimClockRttSample&) is still a client host, has the "
	"hook, and the name probe sees it");
static_assert(!SimNetClockRttSampleHook<ReferenceClientHost> && SimNetClockRttSampleHookWellFormed<ReferenceClientHost>,
	"SimNetClockRttSampleHook: the hook is optional; a client host without an onClockSample member is well formed, and the session "
	"drops the SimClock samples it drains (counted, never decoded as a message)");
static_assert(!SimNetClockRttSampleHookWellFormed<HostWithATwoArgumentClockSampleHook>
		&& !SimNetClockRttSampleHookWellFormed<HostWithAPrivateClockSampleHook>,
	"SimNetClockRttSampleHookWellFormed: a host member named onClockSample that the client session cannot call as "
	"onClockSample(const SimClockRttSample&) -> void (another arity, or private) is refused, because the session would "
	"silently never call it and the clock would stay on the engine ping");
} // namespace simNetHostSelfCheck
} // namespace ogsimnet
