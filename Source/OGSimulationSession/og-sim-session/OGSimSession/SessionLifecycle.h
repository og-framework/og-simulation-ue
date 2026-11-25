#ifndef OGSIM_SESSION_LIFECYCLE_H
// SPDX-License-Identifier: MPL-2.0
// docs/SessionLifecycle-rationale.md · docs/SessionLifecycle-guards.md
#define OGSIM_SESSION_LIFECYCLE_H

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
#endif

#if defined(OGSIM_SESSION_LIFECYCLE_UE_AUTHORITY_DEFINITIONS)

template <typename TraitsT>
void UESimulationSession<TraitsT>::postRunnerRequest(TUniqueFunction<void()> request)
{
	const std::scoped_lock lock(m_lifecycleBridgeMutex);
	m_runnerRequests.push_back(MoveTemp(request));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::runRunnerRequests_Owner()
{
	std::vector<TUniqueFunction<void()>> requests;
	{
		const std::scoped_lock lock(m_lifecycleBridgeMutex);
		requests.swap(m_runnerRequests);
	}
	for (TUniqueFunction<void()>& request : requests)
		request();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetLifecycleUnregistering(uint8_t simId)
{
	const unsigned int id = simId;
	// ⛔G-41  docs/UESimulationSession-guards.md
	if (m_systemsNotified_Step.erase(id) != 0u && m_storage.template has<Simulatable>(id))
		m_manager->notifyCharacterUnregistered(id);
}

template <typename TraitsT>
std::optional<uint32> UESimulationSession<TraitsT>::physicsSlotFor(AActor& owner, [[maybe_unused]] uint64 key,
    unsigned int storageKey) const
{
    checkf(m_manager.has_value(), TEXT("UESimulationSession::physicsSlotFor before beginPlay built the manager"));
    if (!m_manager->runsPrediction())
    {
        const auto found = m_authoritativeSlotsByActor.find(&owner);
        return found != m_authoritativeSlotsByActor.end() ? std::optional<uint32>(found->second) : std::nullopt;
    }
    if (storageKey == 0u)
        return std::nullopt;
    checkf(storageKey <= 0xFFu,
        TEXT("UESimulationSession::physicsSlotFor: a client's slot comes from the roster, which keys characters by their sim "
             "id (1..255); storage key %u is not one"), storageKey);
    SimNetLanes* const lanes = m_simNetLanes;
    if (lanes == nullptr || lanes->client() == nullptr)
        return std::nullopt;
    const ogsimnet::BindVerdict entry = lanes->bindings().verdict(static_cast<uint8_t>(storageKey), /*engineSaysLocal=*/false);
    if (entry.step == ogsimnet::BindStep::Wait || entry.physicsSlot == ogsimnet::kRosterNoSlot)
        return std::nullopt;
    return static_cast<uint32>(entry.physicsSlot);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::recordAuthoritativeSlot(AActor& owner, [[maybe_unused]] uint64 key, uint32 slot)
{
    checkf(m_simNetServer.has_value(),
        TEXT("UESimulationSession::recordAuthoritativeSlot on a peer without the og-sim-net server: only the authority "
             "chooses a slot, and a client binds the slot its roster entry carries. Was SetAuthoritativePhysicsSlot's "
             "authority checkf"));
    checkf(slot < kMaxSimulatableSlots,
        TEXT("UESimulationSession::recordAuthoritativeSlot: slot %u is not a slot of the physics world (kMaxSimulatableSlots "
             "= %u). Was SetAuthoritativePhysicsSlot's range checkf"), slot, static_cast<uint32>(kMaxSimulatableSlots));
    checkf(m_authoritativeSlotsByActor.find(&owner) == m_authoritativeSlotsByActor.end(),
        TEXT("UESimulationSession::recordAuthoritativeSlot: '%s' already holds slot %u and was handed slot %u; a character "
             "takes one slot for its lifetime. Was SetAuthoritativePhysicsSlot's once-only checkf"),
        *owner.GetName(), m_authoritativeSlotsByActor.count(&owner) != 0u ? m_authoritativeSlotsByActor.at(&owner) : 0u, slot);
    m_authoritativeSlotsByActor[&owner] = slot;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::forgetSimNetCharacter(unsigned int id, const AActor* pawn)
{
    m_registeredComponentsById.erase(id);
    m_authoritativeSlotsByActor.erase(pawn);
    if (id == 0u || id > 0xFFu)
        return;
    if (m_simNetLanes != nullptr)
        m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(id));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::collectLifecycleOutcomes_GameThread()
{
    if (m_lifecycle.has_value())
    {
        m_lifecycle->pollOutcomes([this](const ogsession::LifecycleOutcome& outcome) { onPredictionOutcome_GameThread(outcome); });
        return;
    }
    std::vector<LifecycleOutcome> outcomes;
    {
        const std::scoped_lock lock(m_lifecycleBridgeMutex);
        outcomes.swap(m_lifecycleOutcomes);
    }
    for (const LifecycleOutcome& outcome : outcomes)
    {
        switch (outcome.kind)
        {
        case LifecycleOutcomeKind::RegisterApplied:
        case LifecycleOutcomeKind::RegisterRefused:
        {
            const auto pending = m_pendingRegistrations.find(outcome.id);
            if (pending == m_pendingRegistrations.end() || !(pending->second.queued || pending->second.onRunner))
                break;
            pending->second.runnerBusy = false;
            if (outcome.kind == LifecycleOutcomeKind::RegisterApplied)
                pending->second.applied = true;
            else
                pending->second.refused = true;
            break;
        }
        case LifecycleOutcomeKind::BodiesBound:
        case LifecycleOutcomeKind::BodiesPending:
        case LifecycleOutcomeKind::BodiesUnresolvable:
        {
            if (outcome.kind != LifecycleOutcomeKind::BodiesUnresolvable)
                m_componentsHeldForRunner.erase(outcome.id);
            const auto pending = m_pendingRegistrations.find(outcome.id);
            if (pending == m_pendingRegistrations.end() || !pending->second.onRunner)
                break;
            pending->second.runnerBusy = false;
            if (outcome.kind == LifecycleOutcomeKind::BodiesBound)
            {
                pending->second.bodiesBound = true;
                pending->second.boundSlot = outcome.slot;
            }
            break;
        }
        }
    }
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegisterOnRunner(unsigned int id, typename TraitsT::Component& owner)
{
    collectLifecycleOutcomes_GameThread();
    const auto it = m_pendingRegistrations.find(id);
    checkf(it != m_pendingRegistrations.end(), TEXT("UESimulationSession::tryRegisterOnRunner: tryRegister made the record"));
    PendingRegistration& record = it->second;
    record.onRunner = true;
    if (record.refused)
    {
        UE_LOG(LogOGMgmt, Error,
            TEXT("tryRegister: id=%u refused by the og-sim-net server (its [Roster.Refused] line names why); the character "
                 "stays unsimulated"), id);
        m_pendingRegistrations.erase(it);
        return TryRegisterStatus::Refused;
    }
    if (record.applied)
    {
        m_registeredComponentsById[id] = &owner;
        m_runnerBodyKeysById[id] = record.bodyKey;
        UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, 1);
        m_game.afterRegistered(id, /*isAuthority=*/true);
        m_physicsBackend.onRegisteredHost_GameThread(owner, id);
        m_pendingRegistrations.erase(it);
        return TryRegisterStatus::Ready;
    }
    if (record.runnerBusy || m_runnerStopped)
        return TryRegisterStatus::Pending;

    typename TraitsT::Character* const character = Cast<typename TraitsT::Character>(owner.GetOwner());
    checkf(character != nullptr,
           TEXT("UESimulationSession::tryRegister: the component must be attached to a TraitsT::Character — the ")
           TEXT("first-call body pass reads that class's own root capsule"));
    if (!record.runnerSimulatable)
    {
        record.runnerSimulatable = std::make_shared<Simulatable>(std::move(*record.simulatable));
        record.simulatable.reset();
    }

    if (!record.bodiesCreated)
    {
        if (!record.bodiesBound)
        {
            typename PhysicsBackend::BodyRequest request = m_physicsBackend.makeBodyRequest_GameThread(owner, *character, id);
            record.bodyKey = request.key;
            record.runnerBusy = true;
            m_componentsHeldForRunner[id] = TStrongObjectPtr<typename TraitsT::Component>(&owner);
            postRunnerRequest([this, id, request = MoveTemp(request), simulatable = record.runnerSimulatable,
                staticData = &owner.getStaticData()]()
            {
                const std::optional<uint32_t> slot = m_physicsBackend.createBodies_Runner(request, *simulatable, *staticData, id);
                postLifecycleOutcome(id, slot.has_value() ? LifecycleOutcomeKind::BodiesBound : LifecycleOutcomeKind::BodiesPending,
                    slot.value_or(0u));
            });
            return TryRegisterStatus::Pending;
        }
        recordAuthoritativeSlot(*owner.GetOwner(), record.bodyKey, record.boundSlot);
#if DO_CHECK
        record.runnerSimulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            checkf(decl.bindings.ownBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), id);
            checkf(decl.bindings.parentBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), id);
        });
#endif
        m_game.afterBodiesCreated(*record.runnerSimulatable, *character, id, /*isAuthority=*/true, m_owner.GetNetMode());
        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

    checkf(id != 0u && id <= 0xFFu,
        TEXT("UESimulationSession::tryRegister: og-sim-net's roster keys a character by its sim id (1..255); id %u is not one"), id);
    AActor* const pawn = owner.GetOwner();
    const UOGNetConnection* const ownerConnection = Cast<UOGNetConnection>(GetRootNetConnection(pawn));
    const SimConnectionId ownerConn = ownerConnection != nullptr ? ownerConnection->OGNetId : kInvalidSimConnection;
    const uint8_t localIndex = GetPlayerSlotForActor(pawn);
    const FTCHARToUTF8 debugName(pawn != nullptr ? *pawn->GetName() : TEXT("none"));
    record.runnerBusy = true;
    record.registerPosted = true;
    postRunnerRequest([this, id, key = record.bodyKey, physicsSlot = static_cast<uint8_t>(record.boundSlot), ownerConn, localIndex,
        name = std::string(debugName.Get()), simulatable = record.runnerSimulatable]() mutable
    {
        if (!bodiesResolvable_Runner(*simulatable))
        {
            postLifecycleOutcome(id, LifecycleOutcomeKind::BodiesUnresolvable);
            return;
        }
        m_simNetServer->template registerCharacter<Simulatable>(static_cast<uint8_t>(id), physicsSlot, ownerConn, localIndex,
            std::move(name), std::move(*simulatable),
            [this, id, key](SimChannel* channel) { onRunnerRegisterApplied_Runner(id, key, channel); });
    });
    return TryRegisterStatus::Pending;
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::bodiesResolvable_Runner(const Simulatable& simulatable) const
{
    if (!m_physAdapter->isBodyResolvable(Game::rootBodyId(simulatable)))
        return false;
    bool allResolvable = true;
    simulatable.getPhysicsComposite().forEach([&](const auto& decl)
    {
        if (!m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
            allResolvable = false;
    });
    return allResolvable;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onRunnerRegisterApplied_Runner(unsigned int id, uint64 key, SimChannel* channel)
{
    if (channel == nullptr)
    {
        m_physicsBackend.releaseBodies_Runner(key, id);
        postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterRefused);
        return;
    }
    // ⛔G-36  docs/UESimulationSession-guards.md
    m_manager->notifyCharacterRegistered(id);
    m_systemsNotified_Step.insert(id);
    m_physicsBackend.onRegistered_Runner(key, id);
    postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterApplied);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::requestRunnerUnregister_GameThread(unsigned int id, typename TraitsT::Component* owner)
{
    AActor* const pawn = owner != nullptr ? owner->GetOwner() : nullptr;
    const bool registered = m_registeredComponentsById.find(id) != m_registeredComponentsById.end();
    const auto pending = m_pendingRegistrations.find(id);
    const bool pendingOnRunner = pending != m_pendingRegistrations.end() && pending->second.onRunner;
    const bool registerPosted = registered || (pendingOnRunner && pending->second.registerPosted);
    const bool bodiesHeld = registered || (pendingOnRunner && (pending->second.bodiesBound || pending->second.runnerBusy));
    const auto knownKey = m_runnerBodyKeysById.find(id);
    const uint64 key = knownKey != m_runnerBodyKeysById.end() ? knownKey->second
        : pendingOnRunner ? pending->second.bodyKey : PhysicsBackend::slotKeyFor(pawn);
    if (pending != m_pendingRegistrations.end())
        m_pendingRegistrations.erase(pending);
    if (knownKey != m_runnerBodyKeysById.end())
        m_runnerBodyKeysById.erase(knownKey);

    if (!m_runnerStopped && bodiesHeld)
    {
        postRunnerRequest([this, id, key, registerPosted]()
        {
            // ⛔G-37  docs/UESimulationSession-guards.md
            if (!registerPosted)
            {
                releaseOnRunner_Runner(id, key);
                return;
            }
            m_simNetServer->template unregisterCharacter<Simulatable>(static_cast<uint8_t>(id), ogsimnet::RosterRemoveReason::Left,
                [this, id, key](SimChannel*) { releaseOnRunner_Runner(id, key); });
        });
    }

    m_registeredComponentsById.erase(id);
    m_authoritativeSlotsByActor.erase(pawn);
    m_game.onUnregistered(id);
    m_latencyBudgetDiagnostics.forgetCharacter(id);
    if (registered)
        m_physicsBackend.onUnregisteredHost_GameThread(id);
    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::releaseOnRunner_Runner(unsigned int id, uint64 key)
{
    m_systemsNotified_Step.erase(id);
    m_physicsBackend.releaseBodies_Runner(key, id);
}

#endif
