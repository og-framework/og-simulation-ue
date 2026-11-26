#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionLifecycle-rationale.md · docs/SessionLifecycle-guards.md

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "OGSimulation/BodySlotOccupancy.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"

namespace ogsession
{
namespace sessionLifecycleLines
{
inline constexpr char kQueuedRegistrationDidNotHold[] =
	"[Error]tryRegister: id=%u: the og-sim-net client's queued registration did not hold (%s); the character stays "
	"unsimulated";
inline constexpr char kRosterRemovedIt[]        = "the roster removed it";
inline constexpr char kLifecycleLineNamesWhy[]  = "its [SimNet.Lifecycle] line names why";
inline constexpr char kRegistered[]             = "tryRegister: registered simulatable id=%u isAuthority=%d";
inline constexpr char kRunnerRegisterRefused[]  =
	"[Error]tryRegister: id=%u refused by the og-sim-net server (its [Roster.Refused] line names why); the character stays "
	"unsimulated";
inline constexpr char kRosterVerdictMismatch[]  =
	"[Error]tryRegister: id=%u registers %s an input provider, and its roster entry says the other";
inline constexpr char kWith[]                   = "with";
inline constexpr char kWithout[]                = "without";
inline constexpr std::size_t kLineBytes         = 256u;

static_assert(sizeof(kQueuedRegistrationDidNotHold) + sizeof(kLifecycleLineNamesWhy) + 10u <= kLineBytes
		&& sizeof(kRegistered) + 10u + 11u <= kLineBytes && sizeof(kRunnerRegisterRefused) + 10u <= kLineBytes
		&& sizeof(kRosterVerdictMismatch) + sizeof(kWithout) + 10u <= kLineBytes,
	"sessionLifecycleLines::kLineBytes holds each lifecycle line with its longest reason and a 10-digit id");
} // namespace sessionLifecycleLines

enum class UnregisterRelease : uint8_t
{
	Now,
	AfterApplied,
};

template <typename B>
concept SessionLifecycleBindings = requires(B& bindings, uint8_t simId, bool engineSaysLocal)
{
	{ bindings.markRegisteredIfReady(simId, engineSaysLocal) } -> std::same_as<ogsimnet::BindVerdict>;
	{ bindings.markUnregistered(simId) }                       -> std::same_as<bool>;
};

template <typename C, typename SimulatableT, typename InputProviderT, typename ChannelT>
concept SessionLifecycleClient = requires(C& client, uint8_t simId, SimulatableT&& simulatable, InputProviderT&& provider,
	void (*applied)(ChannelT*))
{
	client.registerCharacter(simId, std::move(simulatable), std::move(provider), applied);
	client.unregisterCharacter(simId, applied);
};

template <typename P>
concept SessionLifecyclePort = SessionLifecycleBindings<typename P::Bindings>
	&& SessionLifecycleClient<typename P::Client, typename P::Simulatable, typename P::InputProvider, typename P::Channel>
	&& requires(P& p, const P& cp, const RegisterRequest& request, typename P::InputProvider&& provider, uint32_t id,
		typename P::Channel& channel, const char* line)
{
	{ p.client() }                                     -> std::same_as<typename P::Client*>;
	{ p.bindings() }                                   -> std::same_as<typename P::Bindings*>;
	{ p.wrapInputProvider(request, std::move(provider)) } -> std::same_as<typename P::InputProvider>;
	{ p.notifyRegistered(id) }                         -> std::same_as<void>;
	{ p.notifyUnregistered(id) }                       -> std::same_as<void>;
	{ cp.inStorage(id) }                               -> std::same_as<bool>;
	{ p.installInputStamps(id, channel) }              -> std::same_as<void>;
	{ p.clearInputStamps(channel) }                    -> std::same_as<void>;
	{ p.stopSteps() }                                  -> std::same_as<void>;
	{ p.drainWhileStopped() }                          -> std::same_as<void>;
	{ p.logRouted(line) }                              -> std::same_as<void>;
};

namespace sessionLifecycleSelfCheck
{
struct Channel
{
};

struct Simulatable
{
};

struct InputProvider
{
	explicit operator bool() const;
};

struct ConformingBindings
{
	ogsimnet::BindVerdict markRegisteredIfReady(uint8_t, bool);
	bool                  markUnregistered(uint8_t);
};

struct BindingsWithoutMark
{
	bool markUnregistered(uint8_t);
};

struct BindingsWithoutUnmark
{
	ogsimnet::BindVerdict markRegisteredIfReady(uint8_t, bool);
};

struct BindingsUnmarkReturnsVoid
{
	ogsimnet::BindVerdict markRegisteredIfReady(uint8_t, bool);
	void                  markUnregistered(uint8_t);
};

struct ConformingClient
{
	template <typename AppliedT>
	void registerCharacter(uint8_t, Simulatable&&, InputProvider&&, AppliedT);
	template <typename AppliedT>
	void unregisterCharacter(uint8_t, AppliedT);
};

struct ClientWithoutRegister
{
	template <typename AppliedT>
	void unregisterCharacter(uint8_t, AppliedT);
};

struct ClientWithoutUnregister
{
	template <typename AppliedT>
	void registerCharacter(uint8_t, Simulatable&&, InputProvider&&, AppliedT);
};

struct ConformingPort
{
	using Bindings      = ConformingBindings;
	using Client        = ConformingClient;
	using Channel       = sessionLifecycleSelfCheck::Channel;
	using Simulatable   = sessionLifecycleSelfCheck::Simulatable;
	using InputProvider = sessionLifecycleSelfCheck::InputProvider;

	Client*       client();
	Bindings*     bindings();
	InputProvider wrapInputProvider(const RegisterRequest&, InputProvider&&);
	void          notifyRegistered(uint32_t);
	void          notifyUnregistered(uint32_t);
	bool          inStorage(uint32_t) const;
	void          installInputStamps(uint32_t, Channel&);
	void          clearInputStamps(Channel&);
	void          stopSteps();
	void          drainWhileStopped();
	void          logRouted(const char*);
};

struct PortWithoutBindingsMark : ConformingPort
{
	using Bindings = BindingsWithoutMark;
	Bindings* bindings();
};

struct PortWithoutBindingsUnmark : ConformingPort
{
	using Bindings = BindingsWithoutUnmark;
	Bindings* bindings();
};

struct PortWithAVoidUnmark : ConformingPort
{
	using Bindings = BindingsUnmarkReturnsVoid;
	Bindings* bindings();
};

struct PortWithoutClientRegister : ConformingPort
{
	using Client = ClientWithoutRegister;
	Client* client();
};

struct PortWithoutClientUnregister : ConformingPort
{
	using Client = ClientWithoutUnregister;
	Client* client();
};

struct PortClientByReference : ConformingPort
{
	Client& client();
};

struct PortBindingsByReference : ConformingPort
{
	Bindings& bindings();
};

struct PortWrapReturnsVoid : ConformingPort
{
	void wrapInputProvider(const RegisterRequest&, InputProvider&&);
};

struct PortNotifyRegisteredReturnsBool : ConformingPort
{
	bool notifyRegistered(uint32_t);
};

struct PortNotifyUnregisteredReturnsBool : ConformingPort
{
	bool notifyUnregistered(uint32_t);
};

struct PortStorageOnlyWhenMutable : ConformingPort
{
	bool inStorage(uint32_t);
};

struct PortStampsWithoutChannel : ConformingPort
{
	void installInputStamps(uint32_t);
};

struct PortClearReturnsBool : ConformingPort
{
	bool clearInputStamps(Channel&);
};

struct PortStopReturnsBool : ConformingPort
{
	bool stopSteps();
};

struct PortStoppedDrainReturnsCount : ConformingPort
{
	int drainWhileStopped();
};

struct PortLogReturnsBool : ConformingPort
{
	bool logRouted(const char*);
};

static_assert(SessionLifecyclePort<ConformingPort>, "SessionLifecyclePort self-check: the reference shape must satisfy the concept");
static_assert(!SessionLifecyclePort<PortWithoutBindingsMark>,
	"SessionLifecyclePort self-check: the bindings ask and mark the roster verdict in one call, markRegisteredIfReady");
static_assert(!SessionLifecyclePort<PortWithoutBindingsUnmark>,
	"SessionLifecyclePort self-check: the bindings unmark a registration that did not hold, markUnregistered");
static_assert(!SessionLifecyclePort<PortWithAVoidUnmark>,
	"SessionLifecyclePort self-check: markUnregistered returns whether the id was marked, as og-sim-net's bindings do");
static_assert(!SessionLifecyclePort<PortWithoutClientRegister>,
	"SessionLifecyclePort self-check: the client queues a registration, registerCharacter(simId, simulatable, provider, "
	"applied)");
static_assert(!SessionLifecyclePort<PortWithoutClientUnregister>,
	"SessionLifecyclePort self-check: the client queues an unregistration, unregisterCharacter(simId, applied)");
static_assert(!SessionLifecyclePort<PortClientByReference>,
	"SessionLifecyclePort self-check: client() returns a pointer, null while the world has no og-sim-net client");
static_assert(!SessionLifecyclePort<PortBindingsByReference>,
	"SessionLifecyclePort self-check: bindings() returns a pointer, null while the world has no link");
static_assert(!SessionLifecyclePort<PortWrapReturnsVoid>,
	"SessionLifecyclePort self-check: wrapInputProvider returns the provider the client queues");
static_assert(!SessionLifecyclePort<PortNotifyRegisteredReturnsBool>,
	"SessionLifecyclePort self-check: notifyRegistered(id) returns void");
static_assert(!SessionLifecyclePort<PortNotifyUnregisteredReturnsBool>,
	"SessionLifecyclePort self-check: notifyUnregistered(id) returns void");
static_assert(!SessionLifecyclePort<PortStorageOnlyWhenMutable>,
	"SessionLifecyclePort self-check: inStorage(id) must be callable on a const port");
static_assert(!SessionLifecyclePort<PortStampsWithoutChannel>,
	"SessionLifecyclePort self-check: installInputStamps takes the channel og-sim-net registered the character on");
static_assert(!SessionLifecyclePort<PortClearReturnsBool>,
	"SessionLifecyclePort self-check: clearInputStamps(channel) returns void");
static_assert(!SessionLifecyclePort<PortStopReturnsBool>, "SessionLifecyclePort self-check: stopSteps() returns void");
static_assert(!SessionLifecyclePort<PortStoppedDrainReturnsCount>,
	"SessionLifecyclePort self-check: drainWhileStopped() returns void");
static_assert(!SessionLifecyclePort<PortLogReturnsBool>, "SessionLifecyclePort self-check: logRouted(line) returns void");
} // namespace sessionLifecycleSelfCheck

template <typename HandleT>
class SessionHeldForUnregister
{
public:
	void hold(uint32_t simId, HandleT handle) { m_held[simId].push_back(std::move(handle)); }

	template <typename ReleaseF>
	bool releaseOldest(uint32_t simId, ReleaseF&& release)
	{
		const auto found = m_held.find(simId);
		if (found == m_held.end())
			return false;
		HandleT handle = std::move(found->second.front());
		found->second.pop_front();
		if (found->second.empty())
			m_held.erase(found);
		release(simId, handle);
		return true;
	}

	template <typename ReleaseF>
	void releaseAll(ReleaseF&& release)
	{
		for (auto& entry : m_held)
		{
			for (HandleT& handle : entry.second)
				release(entry.first, handle);
		}
		m_held.clear();
	}

	void clear() { m_held.clear(); }
	size_t heldCount(uint32_t simId) const
	{
		const auto found = m_held.find(simId);
		return found != m_held.end() ? found->second.size() : 0u;
	}

private:
	std::unordered_map<uint32_t, std::deque<HandleT>> m_held;
};

template <SessionLifecyclePort PortT>
class SessionLifecycle
{
public:
	using Client        = typename PortT::Client;
	using Channel       = typename PortT::Channel;
	using Simulatable   = typename PortT::Simulatable;
	using InputProvider = typename PortT::InputProvider;
	using Bindings      = typename PortT::Bindings;

	SessionLifecycle(const SessionThreadChecks& threads, PortT port)
		: m_threads(threads)
		, m_port(port)
	{
		OG_CHECK(threads.role() == SessionRole::Prediction,
			"SessionLifecycle: this is the prediction role's path; an authority registers on its runner (og-session-runtime "
			"task 30)");
	}

	SessionLifecycle(const SessionLifecycle&) = delete;
	SessionLifecycle& operator=(const SessionLifecycle&) = delete;

	RegisterStatus tryRegister(const RegisterRequest& request, Simulatable&& simulatable, InputProvider&& inputProvider)
	{
		m_threads.checkOp(SessionOp::TryRegister);
		const uint32_t id = request.simId;
		const auto queued = m_queued.find(id);
		if (queued != m_queued.end())
			return finishQueued(queued);

		Client* const client = m_port.client();
		if (client == nullptr || m_stepsStopped || id == 0u || id > 0xFFu)
			return RegisterStatus::Pending;
		const uint8_t simId = static_cast<uint8_t>(id);
		const bool hasProvider = static_cast<bool>(inputProvider);
		const ogsimnet::BindVerdict verdict = m_port.bindings()->markRegisteredIfReady(simId, hasProvider);
		if (verdict.step == ogsimnet::BindStep::Wait)
			return RegisterStatus::Pending;
		if (verdict.step != ogsimnet::BindStep::Ready)
		{
			char line[sessionLifecycleLines::kLineBytes];
			std::snprintf(line, sizeof(line), sessionLifecycleLines::kRosterVerdictMismatch, static_cast<unsigned>(id),
				hasProvider ? sessionLifecycleLines::kWith : sessionLifecycleLines::kWithout);
			m_port.logRouted(line);
		}
		OG_CHECK(verdict.step == ogsimnet::BindStep::Ready,
			"SessionLifecycle::tryRegister: the character registers with or without an input provider and its roster entry "
			"says the other: the provider is built for the engine's locally controlled entity, which the adapter's bind step "
			"checked against the roster. Was the engine role alone (isLocallyPredicted)");
		if (verdict.step != ogsimnet::BindStep::Ready)
			return RegisterStatus::Pending;
		client->registerCharacter(simId, std::move(simulatable), m_port.wrapInputProvider(request, std::move(inputProvider)),
			[this, id, hasProvider](Channel* channel) { onRegisterApplied(id, channel, hasProvider); });
		m_queued.emplace(id, QueuedRegistration{});
		return RegisterStatus::Pending;
	}

	bool isQueued(uint32_t simId) const
	{
		m_threads.checkOp(SessionOp::TryRegister);
		return m_queued.find(simId) != m_queued.end();
	}

	UnregisterRelease requestUnregister(uint32_t simId, [[maybe_unused]] EntityKey entityKey)
	{
		m_threads.checkOp(SessionOp::RequestUnregister);
		const bool queued = m_queued.erase(simId) != 0u;
		const bool registered = m_registered.erase(simId) != 0u;
		if (m_stepsStopped || (!queued && !registered))
			return UnregisterRelease::Now;
		{
			const std::scoped_lock lock(m_mutex);
			m_requests.push_back(simId);
		}
		m_unregisterPending.insert(simId);
		// ⛔G-03  docs/SessionLifecycle-guards.md
		return UnregisterRelease::AfterApplied;
	}

	void applyRequests(Client& client)
	{
		m_threads.checkOp(SessionOp::ApplyLifecycleRequests);
		applyRequestsUnchecked(client);
	}

	template <typename OutcomeFn>
	void pollOutcomes(OutcomeFn&& onOutcome)
	{
		m_threads.checkOp(SessionOp::PollOutcomes);
		std::vector<LifecycleOutcome> outcomes;
		{
			const std::scoped_lock lock(m_mutex);
			outcomes.swap(m_outcomes);
		}
		for (const LifecycleOutcome& outcome : outcomes)
		{
			if (const auto* registered = std::get_if<lifecycleOutcome::Registered>(&outcome))
			{
				if (const auto found = m_queued.find(registered->simId); found != m_queued.end())
					found->second.applied = true;
			}
			else if (const auto* refused = std::get_if<lifecycleOutcome::Refused>(&outcome))
			{
				if (const auto found = m_queued.find(refused->simId); found != m_queued.end())
					found->second.refused = true;
			}
			else if (const auto* unregistered = std::get_if<lifecycleOutcome::Unregistered>(&outcome))
			{
				if (m_unregisterPending.erase(unregistered->simId) != 0u)
					onOutcome(outcome);
			}
			else if (const auto* removed = std::get_if<lifecycleOutcome::RosterRemoved>(&outcome))
			{
				if (m_unregisterPending.find(removed->simId) != m_unregisterPending.end())
					continue;
				if (const auto found = m_queued.find(removed->simId); found != m_queued.end())
				{
					found->second.rosterRemoved = true;
					continue;
				}
				if (m_registered.erase(removed->simId) != 0u)
					onOutcome(outcome);
			}
		}
	}

	void onRegisteredRemoved(uint8_t simId)
	{
		checkDrainThread();
		const uint32_t id = simId;
		m_systemsNotified.erase(id);
		post(lifecycleOutcome::RosterRemoved{ id });
	}

	void onUnregistering(uint8_t simId)
	{
		checkDrainThread();
		const uint32_t id = simId;
		// ⛔G-05  docs/SessionLifecycle-guards.md
		if (m_systemsNotified.erase(id) != 0u && m_port.inStorage(id))
			m_port.notifyUnregistered(id);
	}

	template <typename OutcomeFn>
	bool teardown(OutcomeFn&& onOutcome)
	{
		m_threads.checkOp(SessionOp::DrainStopped);
		if (m_stepsStopped)
			return false;
		// ⛔G-04  docs/SessionLifecycle-guards.md
		m_port.stopSteps();
		m_stepsStopped = true;

		if (Client* const client = m_port.client())
		{
			const StoppedDrainScope stoppedDrain(m_inStoppedDrain, m_threads);
			applyRequestsUnchecked(*client);
			std::vector<uint32_t> live;
			for (const auto& entry : m_queued)
				live.push_back(entry.first);
			for (const uint32_t id : m_registered)
				live.push_back(id);
			for (const uint32_t id : live)
				enqueueUnregister(*client, id);
			m_port.drainWhileStopped();
		}
		pollOutcomes(onOutcome);
		m_unregisterPending.clear();
		m_registered.clear();
		unmarkQueued(m_port.bindings());
		return true;
	}

	void detach(Bindings& bindings)
	{
		m_threads.checkOp(SessionOp::RequestUnregister);
		unmarkQueued(&bindings);
	}

	bool stepsStopped() const { return m_stepsStopped; }

private:
	struct QueuedRegistration
	{
		bool applied       = false;
		bool refused       = false;
		bool rosterRemoved = false;
	};

	using QueuedMap = std::unordered_map<uint32_t, QueuedRegistration>;

	RegisterStatus finishQueued(typename QueuedMap::iterator queued)
	{
		const uint32_t id = queued->first;
		const QueuedRegistration record = queued->second;
		if (record.refused || record.rosterRemoved)
		{
			if (Bindings* const bindings = m_port.bindings(); bindings != nullptr && id <= 0xFFu)
				bindings->markUnregistered(static_cast<uint8_t>(id));
			char line[sessionLifecycleLines::kLineBytes];
			std::snprintf(line, sizeof(line), sessionLifecycleLines::kQueuedRegistrationDidNotHold, static_cast<unsigned>(id),
				record.rosterRemoved ? sessionLifecycleLines::kRosterRemovedIt : sessionLifecycleLines::kLifecycleLineNamesWhy);
			m_port.logRouted(line);
			m_queued.erase(queued);
			return RegisterStatus::Refused;
		}
		if (!record.applied)
			return RegisterStatus::Pending;

		m_queued.erase(queued);
		m_registered.insert(id);
		char line[sessionLifecycleLines::kLineBytes];
		std::snprintf(line, sizeof(line), sessionLifecycleLines::kRegistered, static_cast<unsigned>(id), 0);
		m_port.logRouted(line);
		return RegisterStatus::Ready;
	}

	void applyRequestsUnchecked(Client& client)
	{
		std::vector<uint32_t> requests;
		{
			const std::scoped_lock lock(m_mutex);
			requests.swap(m_requests);
		}
		for (const uint32_t id : requests)
			enqueueUnregister(client, id);
	}

	void enqueueUnregister(Client& client, uint32_t id)
	{
		if (id == 0u || id > 0xFFu)
			return;
		m_unregisterRequested.insert(id);
		// ⛔G-01  docs/SessionLifecycle-guards.md
		if (m_systemsNotified.erase(id) != 0u && m_port.inStorage(id))
			m_port.notifyUnregistered(id);
		client.unregisterCharacter(static_cast<uint8_t>(id), [this, id](Channel* channel)
		{
			checkDrainThread();
			if (channel != nullptr)
				m_port.clearInputStamps(*channel);
			m_unregisterRequested.erase(id);
			post(lifecycleOutcome::Unregistered{ id });
		});
	}

	void onRegisterApplied(uint32_t id, Channel* channel, bool hasProvider)
	{
		checkDrainThread();
		if (channel == nullptr)
		{
			post(lifecycleOutcome::Refused{ id, RegisterRefusal::SimNetRefused });
			return;
		}
		if (hasProvider)
			m_port.installInputStamps(id, *channel);
		// ⛔G-02  docs/SessionLifecycle-guards.md
		if (m_unregisterRequested.find(id) == m_unregisterRequested.end())
		{
			m_port.notifyRegistered(id);
			m_systemsNotified.insert(id);
		}
		post(lifecycleOutcome::Registered{ id, 0u });
	}

	void unmarkQueued(Bindings* bindings)
	{
		for (const auto& entry : m_queued)
		{
			if (bindings != nullptr && entry.first <= 0xFFu)
				bindings->markUnregistered(static_cast<uint8_t>(entry.first));
		}
		m_queued.clear();
	}

	void post(const LifecycleOutcome& outcome)
	{
		const std::scoped_lock lock(m_mutex);
		m_outcomes.push_back(outcome);
	}

	void checkDrainThread() const
	{
		m_threads.checkOp(m_inStoppedDrain.load(std::memory_order_relaxed) ? SessionOp::DrainStopped : SessionOp::DrainInbox);
	}

	SessionThreadChecks           m_threads;
	PortT                         m_port;
	std::mutex                    m_mutex;
	std::vector<uint32_t>         m_requests;
	std::vector<LifecycleOutcome> m_outcomes;
	QueuedMap                     m_queued;
	std::unordered_set<uint32_t>  m_registered;
	std::unordered_set<uint32_t>  m_unregisterPending;
	bool                          m_stepsStopped = false;
	std::atomic<bool>             m_inStoppedDrain{ false };
	std::unordered_set<uint32_t>  m_systemsNotified;
	std::unordered_set<uint32_t>  m_unregisterRequested;
};

template <typename B>
concept SessionBodies = requires(B& bodies, const RegisterRequest& request, const typename B::Request& bodyRequest,
	typename B::Simulatable& simulatable, uint32_t id, uint64_t key)
{
	typename B::Request;
	typename B::Simulatable;
	{ bodyRequest.key }                             -> std::convertible_to<uint64_t>;
	{ bodies.makeRequest(request) }                 -> std::same_as<typename B::Request>;
	{ bodies.create(bodyRequest, simulatable, id) } -> std::same_as<bool>;
	{ bodies.slotOf(key) }                          -> std::same_as<std::optional<uint32_t>>;
	{ bodies.onRegistered(key, id) }                -> std::same_as<void>;
	{ bodies.release(key, id) }                     -> std::same_as<void>;
};

namespace sessionBodiesSelfCheck
{
struct Simulatable
{
};

struct Request
{
	uint64_t key = 0u;
};

struct RequestWithoutKey
{
};

struct ConformingBodies
{
	using Request     = sessionBodiesSelfCheck::Request;
	using Simulatable = sessionBodiesSelfCheck::Simulatable;

	Request                 makeRequest(const RegisterRequest&);
	bool                    create(const Request&, Simulatable&, uint32_t);
	std::optional<uint32_t> slotOf(uint64_t);
	void                    onRegistered(uint64_t, uint32_t);
	void                    release(uint64_t, uint32_t);
};

struct BodiesWithoutRequestType
{
	using Simulatable = sessionBodiesSelfCheck::Simulatable;

	Request                 makeRequest(const RegisterRequest&);
	bool                    create(const Request&, Simulatable&, uint32_t);
	std::optional<uint32_t> slotOf(uint64_t);
	void                    onRegistered(uint64_t, uint32_t);
	void                    release(uint64_t, uint32_t);
};

struct BodiesWithoutSimulatableType
{
	using Request = sessionBodiesSelfCheck::Request;

	Request                 makeRequest(const RegisterRequest&);
	bool                    create(const Request&, Simulatable&, uint32_t);
	std::optional<uint32_t> slotOf(uint64_t);
	void                    onRegistered(uint64_t, uint32_t);
	void                    release(uint64_t, uint32_t);
};

struct BodiesRequestWithoutKey : ConformingBodies
{
	using Request = RequestWithoutKey;

	Request makeRequest(const RegisterRequest&);
	bool    create(const Request&, Simulatable&, uint32_t);
};

struct BodiesWithoutMakeRequest
{
	using Request     = sessionBodiesSelfCheck::Request;
	using Simulatable = sessionBodiesSelfCheck::Simulatable;

	bool                    create(const Request&, Simulatable&, uint32_t);
	std::optional<uint32_t> slotOf(uint64_t);
	void                    onRegistered(uint64_t, uint32_t);
	void                    release(uint64_t, uint32_t);
};

struct BodiesWithoutCreate
{
	using Request     = sessionBodiesSelfCheck::Request;
	using Simulatable = sessionBodiesSelfCheck::Simulatable;

	Request                 makeRequest(const RegisterRequest&);
	std::optional<uint32_t> slotOf(uint64_t);
	void                    onRegistered(uint64_t, uint32_t);
	void                    release(uint64_t, uint32_t);
};

struct BodiesWithoutSlotOf
{
	using Request     = sessionBodiesSelfCheck::Request;
	using Simulatable = sessionBodiesSelfCheck::Simulatable;

	Request makeRequest(const RegisterRequest&);
	bool    create(const Request&, Simulatable&, uint32_t);
	void    onRegistered(uint64_t, uint32_t);
	void    release(uint64_t, uint32_t);
};

struct BodiesWithoutOnRegistered
{
	using Request     = sessionBodiesSelfCheck::Request;
	using Simulatable = sessionBodiesSelfCheck::Simulatable;

	Request                 makeRequest(const RegisterRequest&);
	bool                    create(const Request&, Simulatable&, uint32_t);
	std::optional<uint32_t> slotOf(uint64_t);
	void                    release(uint64_t, uint32_t);
};

struct BodiesWithoutRelease
{
	using Request     = sessionBodiesSelfCheck::Request;
	using Simulatable = sessionBodiesSelfCheck::Simulatable;

	Request                 makeRequest(const RegisterRequest&);
	bool                    create(const Request&, Simulatable&, uint32_t);
	std::optional<uint32_t> slotOf(uint64_t);
	void                    onRegistered(uint64_t, uint32_t);
};

struct BodiesCreateReturnsSlot : ConformingBodies
{
	std::optional<uint32_t> create(const Request&, Simulatable&, uint32_t);
};

struct BodiesSlotOfReturnsBool : ConformingBodies
{
	bool slotOf(uint64_t);
};

static_assert(SessionBodies<ConformingBodies>, "SessionBodies self-check: the reference shape must satisfy the concept");
static_assert(!SessionBodies<BodiesWithoutRequestType>,
	"SessionBodies self-check: the provider names its request type, Request, made on the frame thread and carried to the "
	"runner as data");
static_assert(!SessionBodies<BodiesWithoutSimulatableType>,
	"SessionBodies self-check: the provider names the Simulatable whose bodies it creates");
static_assert(!SessionBodies<BodiesRequestWithoutKey>,
	"SessionBodies self-check: a request carries its bodies' key, the key create, slotOf, onRegistered and release take");
static_assert(!SessionBodies<BodiesWithoutMakeRequest>,
	"SessionBodies self-check: makeRequest(registerRequest) makes the request on the frame thread");
static_assert(!SessionBodies<BodiesWithoutCreate>,
	"SessionBodies self-check: create(request, simulatable, id) creates and binds the bodies on the runner");
static_assert(!SessionBodies<BodiesWithoutSlotOf>, "SessionBodies self-check: slotOf(key) reads the slot the bodies were bound to");
static_assert(!SessionBodies<BodiesWithoutOnRegistered>,
	"SessionBodies self-check: onRegistered(key, id) tells the bodies their character is registered");
static_assert(!SessionBodies<BodiesWithoutRelease>, "SessionBodies self-check: release(key, id) frees the bodies and their slot");
static_assert(!SessionBodies<BodiesCreateReturnsSlot>,
	"SessionBodies self-check: create returns whether the bodies were bound; the slot is slotOf's");
static_assert(!SessionBodies<BodiesSlotOfReturnsBool>,
	"SessionBodies self-check: slotOf returns the slot, std::nullopt when the key holds none");
} // namespace sessionBodiesSelfCheck

namespace authorityRequest
{
template <typename BodyRequestT, typename SimulatableT>
struct CreateBodies
{
	uint32_t                      simId = 0u;
	BodyRequestT                  bodies;
	std::shared_ptr<SimulatableT> simulatable;
};

template <typename SimulatableT>
struct Register
{
	uint32_t                      simId           = 0u;
	uint64_t                      key             = 0u;
	uint8_t                       physicsSlot     = 0u;
	SimConnectionId               ownerConnection = kInvalidSimConnection;
	uint8_t                       localIndex      = 0u;
	std::string                   debugName;
	std::shared_ptr<SimulatableT> simulatable;
};

struct Unregister
{
	uint32_t simId          = 0u;
	uint64_t key            = 0u;
	bool     registerPosted = false;
};

struct NamesCallOperator
{
	void operator()() const;
};

template <typename T>
struct CallOperatorProbe : T, NamesCallOperator
{
};

template <typename T>
concept NamesNoCallOperator = requires { &CallOperatorProbe<T>::operator(); };

template <typename T>
consteval bool isCallable()
{
	if constexpr (std::is_function_v<T> || std::is_function_v<std::remove_pointer_t<T>> || std::is_member_pointer_v<T>)
		return true;
	else if constexpr (std::is_union_v<T> || (std::is_class_v<T> && std::is_final_v<T>))
		return true;
	else if constexpr (std::is_class_v<T>)
		return std::is_invocable_v<T> || !NamesNoCallOperator<T>;
	else
		return false;
}

template <typename T>
inline constexpr bool kIsCallable = isCallable<std::remove_cvref_t<T>>();

struct AnyField
{
	template <typename U>
	operator U() const;
};

template <typename T, typename IndicesT>
struct BraceInitializableWith;

template <typename T, std::size_t... IndexVs>
struct BraceInitializableWith<T, std::index_sequence<IndexVs...>>
	: std::bool_constant<requires { T{ (static_cast<void>(IndexVs), AnyField{})... }; }>
{
};

inline constexpr std::size_t kMaxFields = 8u;

template <typename T, std::size_t CountV = kMaxFields>
consteval std::size_t fieldCount()
{
	if constexpr (CountV == 0u || BraceInitializableWith<T, std::make_index_sequence<CountV>>::value)
		return CountV;
	else
		return fieldCount<T, CountV - 1u>();
}

template <typename T>
consteval bool holdsNoCallable();

template <typename... FieldTs>
consteval bool noFieldIsCallable()
{
	return (holdsNoCallable<std::remove_cvref_t<FieldTs>>() && ...);
}

template <std::size_t CountV, typename T>
auto fieldsHoldNoCallable(const T& request)
{
	if constexpr (CountV == 0u)
	{
		static_assert(std::is_empty_v<T>,
			"authorityRequest: a request's fields could not be counted, so they cannot be checked: a field does not take "
			"AnyField (clang: a reference to a function)");
		return std::true_type{};
	}
	else if constexpr (CountV == 1u)
	{
		const auto& [f0] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0)>()>{};
	}
	else if constexpr (CountV == 2u)
	{
		const auto& [f0, f1] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0), decltype(f1)>()>{};
	}
	else if constexpr (CountV == 3u)
	{
		const auto& [f0, f1, f2] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0), decltype(f1), decltype(f2)>()>{};
	}
	else if constexpr (CountV == 4u)
	{
		const auto& [f0, f1, f2, f3] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0), decltype(f1), decltype(f2), decltype(f3)>()>{};
	}
	else if constexpr (CountV == 5u)
	{
		const auto& [f0, f1, f2, f3, f4] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0), decltype(f1), decltype(f2), decltype(f3),
			decltype(f4)>()>{};
	}
	else if constexpr (CountV == 6u)
	{
		const auto& [f0, f1, f2, f3, f4, f5] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0), decltype(f1), decltype(f2), decltype(f3), decltype(f4),
			decltype(f5)>()>{};
	}
	else if constexpr (CountV == 7u)
	{
		const auto& [f0, f1, f2, f3, f4, f5, f6] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0), decltype(f1), decltype(f2), decltype(f3), decltype(f4),
			decltype(f5), decltype(f6)>()>{};
	}
	else
	{
		static_assert(CountV == kMaxFields, "authorityRequest: fieldsHoldNoCallable destructures up to kMaxFields fields");
		const auto& [f0, f1, f2, f3, f4, f5, f6, f7] = request;
		return std::bool_constant<noFieldIsCallable<decltype(f0), decltype(f1), decltype(f2), decltype(f3), decltype(f4),
			decltype(f5), decltype(f6), decltype(f7)>()>{};
	}
}

template <typename T>
consteval bool holdsNoCallable()
{
	if constexpr (kIsCallable<T>)
		return false;
	else if constexpr (std::is_class_v<T> && std::is_aggregate_v<T>)
	{
		static_assert(!BraceInitializableWith<T, std::make_index_sequence<kMaxFields + 1u>>::value,
			"authorityRequest: a request holds at most kMaxFields fields; add a binding to fieldsHoldNoCallable and raise it");
		return decltype(fieldsHoldNoCallable<fieldCount<T>()>(std::declval<const T&>()))::value;
	}
	else
		return true;
}

template <typename RequestT>
inline constexpr bool kHoldsNoCallable = holdsNoCallable<std::remove_cvref_t<RequestT>>();

template <typename VariantT>
struct AlternativesHoldNoCallable;

template <typename... AlternativeTs>
struct AlternativesHoldNoCallable<std::variant<AlternativeTs...>>
	: std::bool_constant<((std::is_aggregate_v<AlternativeTs> && kHoldsNoCallable<AlternativeTs>) && ...)>
{
};

template <typename VariantT>
inline constexpr bool kAlternativesHoldNoCallable = AlternativesHoldNoCallable<VariantT>::value;
} // namespace authorityRequest

template <typename BodyRequestT, typename SimulatableT>
using AuthorityRequest = std::variant<authorityRequest::CreateBodies<BodyRequestT, SimulatableT>,
	authorityRequest::Register<SimulatableT>, authorityRequest::Unregister>;

namespace authorityRequestSelfCheck
{
using authorityRequest::kAlternativesHoldNoCallable;
using authorityRequest::kHoldsNoCallable;
using authorityRequest::kIsCallable;
using Simulatable = sessionBodiesSelfCheck::Simulatable;

struct UnaryCall
{
	void operator()(Simulatable*) const;
};

struct GenericCall
{
	template <typename T>
	void operator()(T) const;
};

class PrivateCall
{
	void operator()(uint32_t) const;
};

struct InheritedCall : UnaryCall
{
};

struct FinalData final
{
	uint64_t key = 0u;
};

union UnionData
{
	uint64_t key;
	uint32_t simId;
};

struct DataRequest
{
	uint64_t                     key = 0u;
	std::string                  name;
	bool                         isAuthority = false;
	std::optional<uint32_t>      replicatedSlot;
	const Simulatable*           staticData = nullptr;
	std::shared_ptr<Simulatable> simulatable;
};

struct RequestWithCallField
{
	uint64_t  key = 0u;
	UnaryCall applied;
};

struct RequestWithNestedCallField
{
	uint64_t             key = 0u;
	RequestWithCallField inner;
};

struct RequestWithFunctionPointer
{
	uint64_t key = 0u;
	void (*applied)(Simulatable*) = nullptr;
};

struct RegisterWithCallField
{
	uint32_t                     simId           = 0u;
	uint64_t                     key             = 0u;
	uint8_t                      physicsSlot     = 0u;
	SimConnectionId              ownerConnection = kInvalidSimConnection;
	uint8_t                      localIndex      = 0u;
	std::string                  debugName;
	std::shared_ptr<Simulatable> simulatable;
	UnaryCall                    applied;
};

struct RequestNotAggregate
{
	RequestNotAggregate() {}
	uint64_t key = 0u;
};

static_assert(!kIsCallable<uint64_t> && !kIsCallable<std::string> && !kIsCallable<std::optional<uint32_t>>
		&& !kIsCallable<std::shared_ptr<Simulatable>> && !kIsCallable<const Simulatable*> && !kIsCallable<DataRequest>,
	"authorityRequest self-check: data is not callable");
static_assert(kIsCallable<UnaryCall> && kIsCallable<GenericCall> && kIsCallable<PrivateCall> && kIsCallable<InheritedCall>,
	"authorityRequest self-check: a class that names an operator() is callable, whatever its arity, template, access or base");
static_assert(kIsCallable<void (*)(Simulatable*)> && kIsCallable<void (&)(Simulatable*)> && kIsCallable<void(Simulatable*)>,
	"authorityRequest self-check: a function, a pointer to one and a reference to one are callable");
static_assert(kIsCallable<void (UnaryCall::*)(Simulatable*) const> && kIsCallable<uint64_t DataRequest::*>,
	"authorityRequest self-check: a member pointer is callable");
static_assert(kIsCallable<FinalData> && kIsCallable<UnionData>,
	"authorityRequest self-check: a final class or a union cannot be probed for an operator(), so it is refused");
static_assert(kHoldsNoCallable<DataRequest>, "authorityRequest self-check: a request of data holds no callable");
static_assert(!kHoldsNoCallable<RequestWithCallField> && !kHoldsNoCallable<RequestWithNestedCallField>
		&& !kHoldsNoCallable<RequestWithFunctionPointer>,
	"authorityRequest self-check: a callable field, also inside an aggregate field, is found");
static_assert(kAlternativesHoldNoCallable<AuthorityRequest<DataRequest, Simulatable>>,
	"authorityRequest self-check: the requests hold no callable with a bodies' request of data");
static_assert(!kAlternativesHoldNoCallable<AuthorityRequest<RequestWithCallField, Simulatable>>,
	"authorityRequest self-check: a callable field in the bodies' request is found through CreateBodies");
static_assert(!kAlternativesHoldNoCallable<std::variant<authorityRequest::Unregister, RegisterWithCallField>>,
	"authorityRequest self-check: a callable field added to a request is found");
static_assert(!kAlternativesHoldNoCallable<std::variant<authorityRequest::Unregister, RequestNotAggregate>>,
	"authorityRequest self-check: a request that is not an aggregate cannot be walked field by field, so it is refused");
} // namespace authorityRequestSelfCheck

template <typename S, typename SimulatableT, typename ChannelT>
concept SessionAuthorityServer = requires(S& server, uint8_t simId, uint8_t physicsSlot, SimConnectionId ownerConnection,
	uint8_t localIndex, std::string debugName, SimulatableT&& simulatable, void (*applied)(ChannelT*))
{
	server.template registerCharacter<SimulatableT>(simId, physicsSlot, ownerConnection, localIndex, std::move(debugName),
		std::move(simulatable), applied);
	server.template unregisterCharacter<SimulatableT>(simId, ogsimnet::RosterRemoveReason::Left, applied);
};

template <typename P>
concept SessionAuthorityLifecyclePort = SessionBodies<typename P::Bodies>
	&& std::same_as<typename P::Simulatable, typename P::Bodies::Simulatable>
	&& SessionAuthorityServer<typename P::Server, typename P::Simulatable, typename P::Channel>
	&& requires(P& p, const P& cp, const typename P::Simulatable& simulatable, uint32_t id, EntityKey entityKey,
		const char* line)
{
	{ p.server() }                         -> std::same_as<typename P::Server*>;
	{ p.bodies() }                         -> std::same_as<typename P::Bodies&>;
	{ cp.bodiesResolvable(simulatable) }   -> std::same_as<bool>;
	{ cp.bodyKeyOf(entityKey) }            -> std::same_as<uint64_t>;
	{ cp.runnerStopped() }                 -> std::same_as<bool>;
	{ p.notifyRegistered(id) }             -> std::same_as<void>;
	{ p.notifyUnregistered(id) }           -> std::same_as<void>;
	{ cp.inStorage(id) }                   -> std::same_as<bool>;
	{ p.logRouted(line) }                  -> std::same_as<void>;
};

template <typename H, typename SimulatableT>
concept SessionAuthorityRegisterHooks = requires(H& hooks, SimulatableT& simulatable)
{
	{ hooks.bodiesRequested() }            -> std::same_as<void>;
	{ hooks.bodiesCreated(simulatable) }   -> std::same_as<void>;
};

namespace sessionAuthorityLifecycleSelfCheck
{
using Bodies      = sessionBodiesSelfCheck::ConformingBodies;
using Simulatable = sessionBodiesSelfCheck::Simulatable;

struct Channel
{
};

struct ConformingServer
{
	template <typename SimT, typename AppliedT>
	void registerCharacter(uint8_t, uint8_t, SimConnectionId, uint8_t, std::string, SimT&&, AppliedT);
	template <typename SimT, typename AppliedT>
	void unregisterCharacter(uint8_t, ogsimnet::RosterRemoveReason, AppliedT);
};

struct ServerWithoutRegister
{
	template <typename SimT, typename AppliedT>
	void unregisterCharacter(uint8_t, ogsimnet::RosterRemoveReason, AppliedT);
};

struct ServerWithoutUnregister
{
	template <typename SimT, typename AppliedT>
	void registerCharacter(uint8_t, uint8_t, SimConnectionId, uint8_t, std::string, SimT&&, AppliedT);
};

struct ConformingPort
{
	using Bodies      = sessionAuthorityLifecycleSelfCheck::Bodies;
	using Server      = ConformingServer;
	using Channel     = sessionAuthorityLifecycleSelfCheck::Channel;
	using Simulatable = sessionAuthorityLifecycleSelfCheck::Simulatable;

	Server*  server();
	Bodies&  bodies();
	bool     bodiesResolvable(const Simulatable&) const;
	uint64_t bodyKeyOf(EntityKey) const;
	bool     runnerStopped() const;
	void     notifyRegistered(uint32_t);
	void     notifyUnregistered(uint32_t);
	bool     inStorage(uint32_t) const;
	void     logRouted(const char*);
};

struct PortWithoutBodies : ConformingPort
{
	using Bodies = sessionBodiesSelfCheck::BodiesWithoutRelease;
	Bodies& bodies();
};

struct PortWithAnotherSimulatable : ConformingPort
{
	struct Simulatable
	{
	};
	bool bodiesResolvable(const Simulatable&) const;
};

struct PortWithoutServerRegister : ConformingPort
{
	using Server = ServerWithoutRegister;
	Server* server();
};

struct PortWithoutServerUnregister : ConformingPort
{
	using Server = ServerWithoutUnregister;
	Server* server();
};

struct PortServerByReference : ConformingPort
{
	Server& server();
};

struct PortBodiesByValue : ConformingPort
{
	Bodies bodies();
};

struct PortResolvableOnlyWhenMutable : ConformingPort
{
	bool bodiesResolvable(const Simulatable&);
};

struct PortKeyOfReturnsEntityKey : ConformingPort
{
	EntityKey bodyKeyOf(EntityKey) const;
};

struct PortStoppedOnlyWhenMutable : ConformingPort
{
	bool runnerStopped();
};

struct PortNotifyRegisteredReturnsBool : ConformingPort
{
	bool notifyRegistered(uint32_t);
};

struct PortNotifyUnregisteredReturnsBool : ConformingPort
{
	bool notifyUnregistered(uint32_t);
};

struct PortStorageOnlyWhenMutable : ConformingPort
{
	bool inStorage(uint32_t);
};

struct PortLogReturnsBool : ConformingPort
{
	bool logRouted(const char*);
};

struct ConformingHooks
{
	void bodiesRequested();
	void bodiesCreated(Simulatable&);
};

struct HooksWithoutRequested
{
	void bodiesCreated(Simulatable&);
};

struct HooksWithoutCreated
{
	void bodiesRequested();
};

static_assert(SessionAuthorityLifecyclePort<ConformingPort>,
	"SessionAuthorityLifecyclePort self-check: the reference shape must satisfy the concept");
static_assert(!SessionAuthorityLifecyclePort<PortWithoutBodies>,
	"SessionAuthorityLifecyclePort self-check: the port's bodies model SessionBodies");
static_assert(!SessionAuthorityLifecyclePort<PortWithAnotherSimulatable>,
	"SessionAuthorityLifecyclePort self-check: the port's Simulatable is the one its bodies are created for");
static_assert(!SessionAuthorityLifecyclePort<PortWithoutServerRegister>,
	"SessionAuthorityLifecyclePort self-check: the server queues a registration, registerCharacter<Simulatable>(simId, "
	"physicsSlot, ownerConnection, localIndex, debugName, simulatable, applied)");
static_assert(!SessionAuthorityLifecyclePort<PortWithoutServerUnregister>,
	"SessionAuthorityLifecyclePort self-check: the server queues an unregistration, unregisterCharacter<Simulatable>(simId, "
	"reason, applied)");
static_assert(!SessionAuthorityLifecyclePort<PortServerByReference>,
	"SessionAuthorityLifecyclePort self-check: server() returns a pointer, null while the world has no og-sim-net server");
static_assert(!SessionAuthorityLifecyclePort<PortBodiesByValue>,
	"SessionAuthorityLifecyclePort self-check: bodies() returns the one provider by reference");
static_assert(!SessionAuthorityLifecyclePort<PortResolvableOnlyWhenMutable>,
	"SessionAuthorityLifecyclePort self-check: bodiesResolvable(simulatable) must be callable on a const port");
static_assert(!SessionAuthorityLifecyclePort<PortKeyOfReturnsEntityKey>,
	"SessionAuthorityLifecyclePort self-check: bodyKeyOf(entityKey) returns the bodies' key, a uint64_t");
static_assert(!SessionAuthorityLifecyclePort<PortStoppedOnlyWhenMutable>,
	"SessionAuthorityLifecyclePort self-check: runnerStopped() must be callable on a const port");
static_assert(!SessionAuthorityLifecyclePort<PortNotifyRegisteredReturnsBool>,
	"SessionAuthorityLifecyclePort self-check: notifyRegistered(id) returns void");
static_assert(!SessionAuthorityLifecyclePort<PortNotifyUnregisteredReturnsBool>,
	"SessionAuthorityLifecyclePort self-check: notifyUnregistered(id) returns void");
static_assert(!SessionAuthorityLifecyclePort<PortStorageOnlyWhenMutable>,
	"SessionAuthorityLifecyclePort self-check: inStorage(id) must be callable on a const port");
static_assert(!SessionAuthorityLifecyclePort<PortLogReturnsBool>,
	"SessionAuthorityLifecyclePort self-check: logRouted(line) returns void");
static_assert(SessionAuthorityRegisterHooks<ConformingHooks, Simulatable>,
	"SessionAuthorityRegisterHooks self-check: the reference shape must satisfy the concept");
static_assert(!SessionAuthorityRegisterHooks<HooksWithoutRequested, Simulatable>,
	"SessionAuthorityRegisterHooks self-check: bodiesRequested() runs when the body request is posted");
static_assert(!SessionAuthorityRegisterHooks<HooksWithoutCreated, Simulatable>,
	"SessionAuthorityRegisterHooks self-check: bodiesCreated(simulatable) runs once the bodies are bound");
} // namespace sessionAuthorityLifecycleSelfCheck

template <SessionAuthorityLifecyclePort PortT>
class SessionAuthorityLifecycle
{
public:
	using Bodies      = typename PortT::Bodies;
	using BodyRequest = typename Bodies::Request;
	using Server      = typename PortT::Server;
	using Channel     = typename PortT::Channel;
	using Simulatable = typename PortT::Simulatable;
	using Request     = AuthorityRequest<BodyRequest, Simulatable>;

	static_assert(std::is_aggregate_v<BodyRequest> && authorityRequest::kAlternativesHoldNoCallable<Request>,
		"SessionAuthorityLifecycle: a request is data. Every alternative of AuthorityRequest and the bodies' request are "
		"aggregates; every field of each, and of each aggregate field within, is found by structured binding and is not a "
		"function, a pointer or reference to one, a member pointer, a union, a final class or a class that names an "
		"operator() (any arity, a template, private or inherited). Was TUniqueFunction<void()> closures capturing the "
		"session, a shared simulatable and a static-data pointer (og-session-runtime task 30)");

	SessionAuthorityLifecycle(const SessionThreadChecks& threads, PortT port)
		: m_threads(threads)
		, m_port(port)
	{
		OG_CHECK(threads.role() == SessionRole::Authority,
			"SessionAuthorityLifecycle: this is the authority's path; a client registers through SessionLifecycle "
			"(og-session-runtime task 29)");
	}

	SessionAuthorityLifecycle(const SessionAuthorityLifecycle&) = delete;
	SessionAuthorityLifecycle& operator=(const SessionAuthorityLifecycle&) = delete;

	template <SessionAuthorityRegisterHooks<Simulatable> HooksT>
	RegisterStatus tryRegister(const RegisterRequest& request, Simulatable&& simulatable, HooksT& hooks)
	{
		m_threads.checkOp(SessionOp::TryRegister);
		const uint32_t id = request.simId;
		auto it = m_pending.find(id);
		if (it == m_pending.end())
			it = m_pending.emplace(id, Registration{ std::make_shared<Simulatable>(std::move(simulatable)) }).first;
		Registration& record = it->second;
		if (record.refused)
		{
			char line[sessionLifecycleLines::kLineBytes];
			std::snprintf(line, sizeof(line), sessionLifecycleLines::kRunnerRegisterRefused, static_cast<unsigned>(id));
			m_port.logRouted(line);
			m_pending.erase(it);
			return RegisterStatus::Refused;
		}
		if (record.applied)
		{
			m_bodyKeysById[id] = record.bodyKey;
			char line[sessionLifecycleLines::kLineBytes];
			std::snprintf(line, sizeof(line), sessionLifecycleLines::kRegistered, static_cast<unsigned>(id), 1);
			m_port.logRouted(line);
			m_pending.erase(it);
			return RegisterStatus::Ready;
		}
		if (record.runnerBusy || m_port.runnerStopped())
			return RegisterStatus::Pending;

		if (!record.bodiesCreated)
		{
			if (!record.bodiesBound)
			{
				BodyRequest bodyRequest = m_port.bodies().makeRequest(request);
				record.bodyKey = bodyRequest.key;
				record.runnerBusy = true;
				hooks.bodiesRequested();
				push(authorityRequest::CreateBodies<BodyRequest, Simulatable>{ id, std::move(bodyRequest), record.simulatable });
				return RegisterStatus::Pending;
			}
			recordSlot(record.bodyKey, record.boundSlot);
			hooks.bodiesCreated(*record.simulatable);
			record.bodiesCreated = true;
			return RegisterStatus::Pending;
		}

		OG_CHECK(id != 0u && id <= 0xFFu,
			"SessionAuthorityLifecycle::tryRegister: og-sim-net's roster keys a character by its sim id (1..255)");
		record.runnerBusy = true;
		record.registerPosted = true;
		push(authorityRequest::Register<Simulatable>{ id, record.bodyKey, static_cast<uint8_t>(record.boundSlot),
			request.ownerConnection, request.localIndex, request.debugName, record.simulatable });
		return RegisterStatus::Pending;
	}

	bool requestUnregister(uint32_t simId, EntityKey entityKey)
	{
		m_threads.checkOp(SessionOp::RequestUnregister);
		const auto knownKey = m_bodyKeysById.find(simId);
		const bool registered = knownKey != m_bodyKeysById.end();
		const auto pending = m_pending.find(simId);
		const bool pendingOnRunner = pending != m_pending.end();
		const bool registerPosted = registered || (pendingOnRunner && pending->second.registerPosted);
		const bool bodiesHeld = registered || (pendingOnRunner && (pending->second.bodiesBound || pending->second.runnerBusy));
		const uint64_t entityBodyKey = m_port.bodyKeyOf(entityKey);
		const uint64_t key = registered ? knownKey->second : pendingOnRunner ? pending->second.bodyKey : entityBodyKey;
		if (pendingOnRunner)
			m_pending.erase(pending);
		if (registered)
			m_bodyKeysById.erase(knownKey);

		if (!m_port.runnerStopped() && bodiesHeld)
			push(authorityRequest::Unregister{ simId, key, registerPosted });

		m_slotsByKey.erase(entityBodyKey);
		return registered;
	}

	void applyRequests()
	{
		m_threads.checkOp(SessionOp::ApplyLifecycleRequests);
		std::vector<Request> requests;
		{
			const std::scoped_lock lock(m_mutex);
			requests.swap(m_requests);
		}
		for (Request& request : requests)
			std::visit([this](auto& alternative) { run(alternative); }, request);
	}

	template <typename OutcomeFn>
	void pollOutcomes(OutcomeFn&& onOutcome)
	{
		m_threads.checkOp(SessionOp::PollOutcomes);
		std::vector<LifecycleOutcome> outcomes;
		{
			const std::scoped_lock lock(m_mutex);
			outcomes.swap(m_outcomes);
		}
		for (const LifecycleOutcome& outcome : outcomes)
		{
			onOutcome(outcome);
			if (const auto* registered = std::get_if<lifecycleOutcome::Registered>(&outcome))
			{
				if (Registration* const record = pendingRecord(registered->simId))
				{
					record->runnerBusy = false;
					record->applied = true;
				}
			}
			else if (const auto* refused = std::get_if<lifecycleOutcome::Refused>(&outcome))
			{
				if (Registration* const record = pendingRecord(refused->simId))
				{
					record->runnerBusy = false;
					record->refused = true;
				}
			}
			else if (const auto* bound = std::get_if<lifecycleOutcome::BodiesBound>(&outcome))
			{
				if (Registration* const record = pendingRecord(bound->simId))
				{
					record->runnerBusy = false;
					record->bodiesBound = true;
					record->boundSlot = bound->slot;
				}
			}
			else if (const auto* bodiesPending = std::get_if<lifecycleOutcome::BodiesPending>(&outcome))
			{
				if (Registration* const record = pendingRecord(bodiesPending->simId))
					record->runnerBusy = false;
			}
			else if (const auto* unresolvable = std::get_if<lifecycleOutcome::BodiesUnresolvable>(&outcome))
			{
				if (Registration* const record = pendingRecord(unresolvable->simId))
					record->runnerBusy = false;
			}
		}
	}

	void onUnregistering(uint8_t simId)
	{
		m_threads.checkOp(SessionOp::DrainInbox);
		const uint32_t id = simId;
		// ⛔G-08  docs/SessionLifecycle-guards.md
		if (m_systemsNotified.erase(id) != 0u && m_port.inStorage(id))
			m_port.notifyUnregistered(id);
	}

	std::optional<uint32_t> authoritativeSlotFor(uint64_t bodyKey) const
	{
		m_threads.checkOp(SessionOp::TryRegister);
		const auto found = m_slotsByKey.find(bodyKey);
		return found != m_slotsByKey.end() ? std::optional<uint32_t>(found->second) : std::nullopt;
	}

	void recordAuthoritativeSlot(uint64_t bodyKey, uint32_t slot)
	{
		m_threads.checkOp(SessionOp::TryRegister);
		recordSlot(bodyKey, slot);
	}

	void forgetRegistered()
	{
		m_threads.checkOp(SessionOp::RequestUnregister);
		m_bodyKeysById.clear();
		m_slotsByKey.clear();
	}

private:
	struct Registration
	{
		std::shared_ptr<Simulatable> simulatable;
		bool                         applied        = false;
		bool                         refused        = false;
		bool                         runnerBusy     = false;
		bool                         bodiesBound    = false;
		bool                         bodiesCreated  = false;
		bool                         registerPosted = false;
		uint32_t                     boundSlot      = 0u;
		uint64_t                     bodyKey        = 0u;
	};

	Registration* pendingRecord(uint32_t simId)
	{
		const auto found = m_pending.find(simId);
		return found != m_pending.end() ? &found->second : nullptr;
	}

	void recordSlot(uint64_t bodyKey, uint32_t slot)
	{
		OG_CHECK(m_port.server() != nullptr,
			"SessionAuthorityLifecycle::recordAuthoritativeSlot on a peer without the og-sim-net server: only the authority "
			"chooses a slot, and a client binds the slot its roster entry carries. Was SetAuthoritativePhysicsSlot's "
			"authority checkf");
		OG_CHECK(slot < kMaxSimulatableSlots,
			"SessionAuthorityLifecycle::recordAuthoritativeSlot: the slot is not a slot of the physics world "
			"(kMaxSimulatableSlots). Was SetAuthoritativePhysicsSlot's range checkf");
		OG_CHECK(m_slotsByKey.find(bodyKey) == m_slotsByKey.end(),
			"SessionAuthorityLifecycle::recordAuthoritativeSlot: this character already holds a slot; a character takes one "
			"slot for its lifetime. Was SetAuthoritativePhysicsSlot's once-only checkf");
		m_slotsByKey[bodyKey] = slot;
	}

	void push(Request&& request)
	{
		const std::scoped_lock lock(m_mutex);
		m_requests.push_back(std::move(request));
	}

	void post(const LifecycleOutcome& outcome)
	{
		const std::scoped_lock lock(m_mutex);
		m_outcomes.push_back(outcome);
	}

	Server& server()
	{
		Server* const server = m_port.server();
		OG_CHECK(server != nullptr,
			"SessionAuthorityLifecycle: the runner runs the lifecycle's requests while the og-sim-net server exists; the "
			"server host's stop resets it only after the runner joined");
		return *server;
	}

	void run(authorityRequest::CreateBodies<BodyRequest, Simulatable>& request)
	{
		const uint32_t id = request.simId;
		const std::optional<uint32_t> slot = m_port.bodies().create(request.bodies, *request.simulatable, id)
			? m_port.bodies().slotOf(request.bodies.key)
			: std::nullopt;
		if (slot.has_value())
			post(lifecycleOutcome::BodiesBound{ id, *slot });
		else
			post(lifecycleOutcome::BodiesPending{ id });
	}

	void run(authorityRequest::Register<Simulatable>& request)
	{
		const uint32_t id = request.simId;
		const uint64_t key = request.key;
		if (!m_port.bodiesResolvable(*request.simulatable))
		{
			post(lifecycleOutcome::BodiesUnresolvable{ id });
			return;
		}
		server().template registerCharacter<Simulatable>(static_cast<uint8_t>(id), request.physicsSlot, request.ownerConnection,
			request.localIndex, std::move(request.debugName), std::move(*request.simulatable),
			[this, id, key](Channel* channel) { onRegisterApplied(id, key, channel); });
	}

	void run(authorityRequest::Unregister& request)
	{
		const uint32_t id = request.simId;
		const uint64_t key = request.key;
		// ⛔G-07  docs/SessionLifecycle-guards.md
		if (!request.registerPosted)
		{
			release(id, key);
			return;
		}
		server().template unregisterCharacter<Simulatable>(static_cast<uint8_t>(id), ogsimnet::RosterRemoveReason::Left,
			[this, id, key](Channel*)
			{
				m_threads.checkOp(SessionOp::DrainInbox);
				release(id, key);
			});
	}

	void onRegisterApplied(uint32_t id, uint64_t key, Channel* channel)
	{
		m_threads.checkOp(SessionOp::DrainInbox);
		if (channel == nullptr)
		{
			m_port.bodies().release(key, id);
			post(lifecycleOutcome::Refused{ id, RegisterRefusal::SimNetRefused });
			return;
		}
		// ⛔G-06  docs/SessionLifecycle-guards.md
		m_port.notifyRegistered(id);
		m_systemsNotified.insert(id);
		m_port.bodies().onRegistered(key, id);
		post(lifecycleOutcome::Registered{ id, 0u });
	}

	void release(uint32_t id, uint64_t key)
	{
		m_systemsNotified.erase(id);
		m_port.bodies().release(key, id);
	}

	SessionThreadChecks                          m_threads;
	PortT                                        m_port;
	std::mutex                                   m_mutex;
	std::vector<Request>                         m_requests;
	std::vector<LifecycleOutcome>                m_outcomes;
	std::unordered_map<uint32_t, Registration>   m_pending;
	std::unordered_map<uint32_t, uint64_t>       m_bodyKeysById;
	std::unordered_map<uint64_t, uint32_t>       m_slotsByKey;
	std::unordered_set<uint32_t>                 m_systemsNotified;
};
} // namespace ogsession
