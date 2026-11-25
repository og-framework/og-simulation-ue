#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionLifecycle-rationale.md · docs/SessionLifecycle-guards.md

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "OGSimulation/OGAssert.h"
#include "OGSimNet/SimNetBindings.h"
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
inline constexpr std::size_t kLineBytes         = 256u;

static_assert(sizeof(kQueuedRegistrationDidNotHold) + sizeof(kLifecycleLineNamesWhy) + 10u <= kLineBytes
		&& sizeof(kRegistered) + 10u + 11u <= kLineBytes,
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
			m_inStoppedDrain.store(true, std::memory_order_relaxed);
			applyRequestsUnchecked(*client);
			std::vector<uint32_t> live;
			for (const auto& entry : m_queued)
				live.push_back(entry.first);
			for (const uint32_t id : m_registered)
				live.push_back(id);
			for (const uint32_t id : live)
				enqueueUnregister(*client, id);
			m_port.drainWhileStopped();
			m_inStoppedDrain.store(false, std::memory_order_relaxed);
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
} // namespace ogsession
