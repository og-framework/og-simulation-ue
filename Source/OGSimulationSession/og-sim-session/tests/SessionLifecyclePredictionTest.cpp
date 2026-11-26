// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimSession/SessionLifecycle.h"
#include "OGSimSession/SessionNetHooks.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "OGTickDriver/TickDriverThreadContext.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sessionLifecyclePredictionTest
{
using Events = std::vector<std::string>;

class Recorder
{
public:
	void add(std::string event)
	{
		const std::scoped_lock lock(m_mutex);
		m_events.push_back(std::move(event));
	}

	Events take()
	{
		const std::scoped_lock lock(m_mutex);
		return std::exchange(m_events, Events{});
	}

	size_t count(const std::string& prefix) const
	{
		const std::scoped_lock lock(m_mutex);
		return static_cast<size_t>(std::count_if(m_events.begin(), m_events.end(),
			[&prefix](const std::string& event) { return event.rfind(prefix, 0) == 0; }));
	}

private:
	mutable std::mutex m_mutex;
	Events             m_events;
};

std::string eventOf(const char* what, uint32_t id)
{
	return std::string(what) + " " + std::to_string(id);
}

struct Simulatable
{
	uint32_t tag = 0u;
};

using InputProvider = std::function<int()>;

struct Channel
{
	uint8_t simId = 0u;
};

// og-sim-net's bindings behind one lock, as SimNetLink.h's LockedSimNetBindings (OGSimulation 6d424ec, :117-175) holds them.
class LockedBindings
{
public:
	void onRosterAdded(const ogsimnet::RosterEntry& entry)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.onRosterAdded(entry);
	}

	bool onRosterRemoved(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.onRosterRemoved(simId);
	}

	ogsimnet::BindVerdict markRegisteredIfReady(uint8_t simId, bool engineSaysLocal)
	{
		const std::scoped_lock lock(m_mutex);
		const ogsimnet::BindVerdict verdict = m_bindings.verdict(simId, engineSaysLocal);
		if (verdict.step == ogsimnet::BindStep::Ready)
			m_bindings.markRegistered(simId);
		return verdict;
	}

	bool markUnregistered(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.markUnregistered(simId);
	}

	bool isRegistered(uint8_t simId) const
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.isRegistered(simId);
	}

	void seedFrom(const ogsimnet::RosterMirror& mirror)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.seedFrom(mirror);
	}

	std::vector<uint8_t> registeredIds() const
	{
		const std::scoped_lock lock(m_mutex);
		std::vector<uint8_t> ids;
		for (uint32_t id = 1u; id <= 0xFFu; ++id)
		{
			if (m_bindings.isRegistered(static_cast<uint8_t>(id)))
				ids.push_back(static_cast<uint8_t>(id));
		}
		return ids;
	}

private:
	mutable std::mutex      m_mutex;
	ogsimnet::SimNetBindings m_bindings;
};

// The session's two og-sim-net host callbacks while it is attached: the unregistering hook (SimNetLinkClientHost::
// onLifecycleUnregistering, SimNetLink.h :272-276 at 6d424ec) and the registered-removed handler (onRosterRemoved, :288-301).
struct HostCallbacks
{
	std::function<void(uint8_t)> unregistering;
	std::function<void(uint8_t)> registeredRemoved;
};

// og-sim-net's SimNetClient lifecycle queue, transcribed (SimNetClient.h at OGSimulation 6d424ec: registerCharacter :247-257,
// unregisterCharacter :259-263, onRosterRemoved :500-511, applyRegister :680-700, applyUnregister :702-716,
// unregisterLifecycle :725-735), with the facade replaced by a recorder. One drain runs the queued jobs, then the inbox (the
// roster messages), as SimNetClient::drain does (design §3.4).
class FakeClient
{
public:
	using Applied = std::function<void(Channel*)>;

	FakeClient(Recorder& recorder, HostCallbacks& host, LockedBindings& bindings)
		: m_recorder(recorder)
		, m_host(host)
		, m_bindings(bindings)
	{
	}

	template <typename AppliedT>
	void registerCharacter(uint8_t simId, Simulatable&& simulatable, InputProvider provider, AppliedT applied)
	{
		m_recorder.add(eventOf("client.register", simId));
		const std::scoped_lock lock(m_mutex);
		m_jobs.push_back(Job{ true, simId, Applied(std::move(applied)) });
		(void)simulatable;
		(void)provider;
	}

	template <typename AppliedT>
	void unregisterCharacter(uint8_t simId, AppliedT applied)
	{
		m_recorder.add(eventOf("client.unregister", simId));
		const std::scoped_lock lock(m_mutex);
		m_jobs.push_back(Job{ false, simId, Applied(std::move(applied)) });
	}

	void addToRoster(uint8_t simId, bool isLocal)
	{
		const std::scoped_lock lock(m_mutex);
		m_inbox.push_back(Message{ true, simId, isLocal });
	}

	void removeFromRoster(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		m_inbox.push_back(Message{ false, simId, false });
	}

	void preRegisterInFacade(uint8_t simId)
	{
		m_registered[simId] = true;
		m_inStorage[simId]  = true;
	}

	void drain()
	{
		std::vector<Job> jobs;
		{
			const std::scoped_lock lock(m_mutex);
			jobs.swap(m_jobs);
		}
		for (Job& job : jobs)
		{
			if (job.isRegister)
				applyRegister(job.simId, job.applied);
			else
				applyUnregister(job.simId, job.applied);
		}
		if (betweenQueueAndInbox)
			betweenQueueAndInbox();
		std::vector<Message> inbox;
		{
			const std::scoped_lock lock(m_mutex);
			inbox.swap(m_inbox);
		}
		for (const Message& message : inbox)
		{
			if (message.isAdd)
				onRosterAdded(message.simId, message.isLocal);
			else
				onRosterRemoved(message.simId);
		}
	}

	bool inStorage(uint32_t id) const { return id <= 0xFFu && m_inStorage[id]; }

	bool anyRegistered() const
	{
		return std::find(m_registered.begin(), m_registered.end(), true) != m_registered.end()
			|| std::find(m_inStorage.begin(), m_inStorage.end(), true) != m_inStorage.end();
	}

	bool idle() const
	{
		const std::scoped_lock lock(m_mutex);
		return m_jobs.empty() && m_inbox.empty();
	}

	std::function<void()> betweenQueueAndInbox;

private:
	struct Job
	{
		bool    isRegister = false;
		uint8_t simId      = 0u;
		Applied applied;
	};

	struct Message
	{
		bool    isAdd   = false;
		uint8_t simId   = 0u;
		bool    isLocal = false;
	};

	void applyRegister(uint8_t simId, Applied& applied)
	{
		const auto channel = m_channels.find(simId);
		if (channel == m_channels.end() || m_registered[simId])
		{
			m_recorder.add(eventOf(channel == m_channels.end() ? "apply.register.refused.noChannel" : "apply.register.refused.registered",
				simId));
			applied(nullptr);
			return;
		}
		m_registered[simId] = true;
		m_inStorage[simId]  = true;
		m_recorder.add(eventOf("facade.register", simId));
		applied(&channel->second);
	}

	void applyUnregister(uint8_t simId, Applied& applied)
	{
		const auto channel = m_channels.find(simId);
		Channel* const target = channel != m_channels.end() ? &channel->second : nullptr;
		if (!unregisterLifecycle(simId))
		{
			m_recorder.add(eventOf("apply.unregister.ignored", simId));
			applied(nullptr);
			return;
		}
		applied(target);
	}

	bool unregisterLifecycle(uint8_t simId)
	{
		if (!m_registered[simId])
			return false;
		m_registered[simId] = false;
		m_host.unregistering(simId);
		m_inStorage[simId] = false;
		m_recorder.add(eventOf("facade.unregister", simId));
		return true;
	}

	void onRosterAdded(uint8_t simId, bool isLocal)
	{
		m_channels[simId] = Channel{ simId };
		m_bindings.onRosterAdded(ogsimnet::RosterEntry{ simId, simId, isLocal ? uint8_t{ 0u } : ogsimnet::kRosterNotLocal });
	}

	void onRosterRemoved(uint8_t simId)
	{
		m_recorder.add(eventOf("roster.removed", simId));
		unregisterLifecycle(simId);
		const bool wasRegistered = m_bindings.onRosterRemoved(simId);
		if (wasRegistered)
			m_host.registeredRemoved(simId);
		m_channels.erase(simId);
	}

	Recorder&                  m_recorder;
	HostCallbacks&             m_host;
	LockedBindings&            m_bindings;
	mutable std::mutex         m_mutex;
	std::vector<Job>           m_jobs;
	std::vector<Message>       m_inbox;
	std::map<uint8_t, Channel> m_channels;
	std::array<bool, 256>      m_registered{};
	std::array<bool, 256>      m_inStorage{};
};

// What both sessions share: the recorder, the og-sim-net client and its bindings.
struct World
{
	Recorder       recorder;
	LockedBindings bindings;
	HostCallbacks  host;
	FakeClient     client{ recorder, host, bindings };
	bool           hasLink = true;
	std::function<void()> stopSteps = [] {};
};

std::string levelLineOf(const char* line)
{
	const std::string text(line);
	if (text.rfind("[Error]", 0) == 0)
		return "log Error " + text.substr(7);
	return "log Log " + text;
}

enum class Status : uint8_t
{
	Pending,
	Ready,
	Refused,
};

const char* nameOf(Status status)
{
	return status == Status::Ready ? "Ready" : (status == Status::Refused ? "Refused" : "Pending");
}

// ---------------------------------------------------------------------------------------------------------------------------
// The OLD code: a transcription of the Unreal session's prediction path at og-session-runtime task 29 (a) (OGSimulation
// 6d424ec), each Unreal call replaced by a recorder. File:line are of that commit: SL = OGSimSession/SessionLifecycle.h (the
// verbatim copy), UE = OGSimulationUnreal/UESimulationSession.h. The frame door (createBodies_GameThread, the resolvable poll,
// UE :1611-1651) is the adapter's and unchanged; the transcription starts where the queue path starts.
// ---------------------------------------------------------------------------------------------------------------------------
class OldSession
{
public:
	explicit OldSession(World& world)
		: m_world(world)
	{
		world.host.unregistering     = [this](uint8_t simId) { onSimNetLifecycleUnregistering(simId); };
		world.host.registeredRemoved = [this](uint8_t simId) { onSimNetRegisteredRemoved(simId); };
	}

	// UE :1595-1609 (the record; a queued record goes to the finish) and :1653 (the queue, after the frame door)
	Status tryRegister(unsigned int id, Simulatable&& simulatable, InputProvider&& inputProvider)
	{
		auto it = m_pendingRegistrations.find(id);
		if (it == m_pendingRegistrations.end())
		{
			PendingRegistration record;
			record.simulatable.emplace(std::move(simulatable));
			record.inputProvider = std::move(inputProvider);
			it = m_pendingRegistrations.emplace(id, std::move(record)).first;
		}
		if (it->second.queued)
			return finishQueuedRegistration(id);
		return tryRegisterQueued(id);
	}

	// UE :1656-1680, the client's branch
	void unregisterFromNewFramework(unsigned int id)
	{
		if (m_simUnregisteredIds.erase(id) != 0u)
		{
			m_world.recorder.add(eventOf("unregister.ignored.G30", id));
			return;
		}
		requestQueuedUnregister_GameThread(id);
	}

	// SessionNetHooks::receiveForStep (task 26): the requests, then the drain
	void step()
	{
		applyUnregisterRequests_Step(m_world.client);
		m_world.client.drain();
	}

	// UE :1698-1769, the prediction arms (RegisterApplied/Refused for a queued record, UnregisterApplied, RosterRemoved)
	void collectLifecycleOutcomes_GameThread()
	{
		std::vector<LifecycleOutcome> outcomes;
		{
			const std::scoped_lock lock(m_lifecycleBridgeMutex);
			outcomes.swap(m_lifecycleOutcomes);
		}
		for (const LifecycleOutcome& outcome : outcomes)
		{
			switch (outcome.kind)
			{
			case Kind::RegisterApplied:
			case Kind::RegisterRefused:
			{
				const auto pending = m_pendingRegistrations.find(outcome.id);
				if (pending == m_pendingRegistrations.end() || !pending->second.queued)
					break;
				if (outcome.kind == Kind::RegisterApplied)
					pending->second.applied = true;
				else
					pending->second.refused = true;
				break;
			}
			case Kind::UnregisterApplied:
			{
				const auto found = m_pendingUnregisters.find(outcome.id);
				if (found == m_pendingUnregisters.end())
					break;
				m_world.recorder.add(eventOf("backend.release", outcome.id));
				m_pendingUnregisters.erase(found);
				break;
			}
			case Kind::RosterRemoved:
			{
				if (m_pendingUnregisters.find(outcome.id) != m_pendingUnregisters.end())
					break;
				const auto pending = m_pendingRegistrations.find(outcome.id);
				if (pending != m_pendingRegistrations.end() && pending->second.queued)
				{
					pending->second.rosterRemoved = true;
					break;
				}
				const auto found = m_registeredComponentsById.find(outcome.id);
				if (found == m_registeredComponentsById.end())
					break;
				unregisterCharacterHostOnly(outcome.id, /*releaseBodiesNow=*/true);
				m_simUnregisteredIds.insert(outcome.id);
				break;
			}
			}
		}
	}

	// SL :155-213
	void teardownImmediateClient_GameThread()
	{
		if (m_immediateStepsStopped)
			return;
		m_world.recorder.add("stopSteps");
		m_world.stopSteps();
		m_immediateStepsStopped = true;

		FakeClient* const client = m_world.hasLink ? &m_world.client : nullptr;
		if (client != nullptr)
		{
			applyUnregisterRequests_Step(*client);
			std::vector<unsigned int> live;
			for (const auto& entry : m_pendingRegistrations)
			{
				if (entry.second.queued)
					live.push_back(entry.first);
			}
			for (const auto& entry : m_registeredComponentsById)
				live.push_back(entry.first);
			for (const unsigned int id : live)
				enqueueQueuedUnregister_Step(*client, id);
			m_world.recorder.add("stoppedDrain.begin");
			client->drain();
			m_world.recorder.add("stoppedDrain.end");
		}
		collectLifecycleOutcomes_GameThread();

		for (auto& entry : m_pendingUnregisters)
			m_world.recorder.add(eventOf("backend.release", entry.first));
		m_pendingUnregisters.clear();

		std::vector<unsigned int> registered;
		for (const auto& entry : m_registeredComponentsById)
			registered.push_back(entry.first);
		for (const unsigned int id : registered)
		{
			unregisterCharacterHostOnly(id, /*releaseBodiesNow=*/true);
			m_simUnregisteredIds.insert(id);
		}

		for (auto it = m_pendingRegistrations.begin(); it != m_pendingRegistrations.end();)
		{
			if (!it->second.queued)
			{
				++it;
				continue;
			}
			if (m_world.hasLink && it->first <= 0xFFu)
				m_world.bindings.markUnregistered(static_cast<uint8_t>(it->first));
			it = m_pendingRegistrations.erase(it);
		}
	}

	// UE :1181-1184, the client's arm of detachFromSimNetLanes
	void detachFromSimNetLanes()
	{
		for (const auto& entry : m_registeredComponentsById)
			m_world.bindings.markUnregistered(static_cast<uint8_t>(entry.first));
	}

	size_t registeredCount() const { return m_registeredComponentsById.size(); }
	size_t pendingUnregisterCount() const { return m_pendingUnregisters.size(); }
	size_t queuedCount() const
	{
		return static_cast<size_t>(std::count_if(m_pendingRegistrations.begin(), m_pendingRegistrations.end(),
			[](const auto& entry) { return entry.second.queued; }));
	}

private:
	enum class Kind : uint8_t
	{
		RegisterApplied,
		RegisterRefused,
		UnregisterApplied,
		RosterRemoved,
	};

	struct LifecycleOutcome
	{
		unsigned int id   = 0u;
		Kind         kind = Kind::RegisterApplied;
	};

	struct PendingRegistration
	{
		std::optional<Simulatable> simulatable;
		InputProvider              inputProvider;
		bool                       queued        = false;
		bool                       applied       = false;
		bool                       refused       = false;
		bool                       rosterRemoved = false;
	};

	void postLifecycleOutcome(unsigned int id, Kind kind)
	{
		const std::scoped_lock lock(m_lifecycleBridgeMutex);
		m_lifecycleOutcomes.push_back(LifecycleOutcome{ id, kind });
	}

	// SL :11-17
	void onSimNetRegisteredRemoved(uint8_t simId)
	{
		const unsigned int id = simId;
		m_systemsNotified_Step.erase(id);
		postLifecycleOutcome(id, Kind::RosterRemoved);
	}

	// UE :1114-1121
	void onSimNetLifecycleUnregistering(uint8_t simId)
	{
		const unsigned int id = simId;
		if (m_systemsNotified_Step.erase(id) != 0u && m_world.client.inStorage(id))
			m_world.recorder.add(eventOf("notifyUnregistered", id));
	}

	// SL :19-29
	void applyUnregisterRequests_Step(FakeClient& client)
	{
		std::vector<unsigned int> requests;
		{
			const std::scoped_lock lock(m_lifecycleBridgeMutex);
			requests.swap(m_unregisterRequests);
		}
		for (const unsigned int id : requests)
			enqueueQueuedUnregister_Step(client, id);
	}

	// SL :31-47
	void enqueueQueuedUnregister_Step(FakeClient& client, unsigned int id)
	{
		if (id == 0u || id > 0xFFu)
			return;
		m_unregisterRequested_Step.insert(id);
		if (m_systemsNotified_Step.erase(id) != 0u && m_world.client.inStorage(id))
			m_world.recorder.add(eventOf("notifyUnregistered", id));
		client.unregisterCharacter(static_cast<uint8_t>(id), [this, id](Channel* channel)
		{
			if (channel != nullptr)
				m_world.recorder.add(eventOf("stamps.clear", id));
			m_unregisterRequested_Step.erase(id);
			postLifecycleOutcome(id, Kind::UnregisterApplied);
		});
	}

	// SL :49-74
	void onQueuedRegisterApplied_Step(unsigned int id, Channel* channel, bool hasProvider)
	{
		if (channel == nullptr)
		{
			postLifecycleOutcome(id, Kind::RegisterRefused);
			return;
		}
		if (hasProvider)
			m_world.recorder.add(eventOf("stamps.install", id));
		if (m_unregisterRequested_Step.find(id) == m_unregisterRequested_Step.end())
		{
			m_world.recorder.add(eventOf("notifyRegistered", id));
			m_systemsNotified_Step.insert(id);
		}
		postLifecycleOutcome(id, Kind::RegisterApplied);
	}

	// SL :76-101
	Status tryRegisterQueued(unsigned int id)
	{
		PendingRegistration& record = m_pendingRegistrations.at(id);
		FakeClient* const client = m_world.hasLink ? &m_world.client : nullptr;
		if (client == nullptr || m_immediateStepsStopped || id == 0u || id > 0xFFu)
			return Status::Pending;
		const uint8_t simId = static_cast<uint8_t>(id);
		const bool hasProvider = static_cast<bool>(record.inputProvider);
		const ogsimnet::BindVerdict verdict = m_world.bindings.markRegisteredIfReady(simId, hasProvider);
		if (verdict.step == ogsimnet::BindStep::Wait)
			return Status::Pending;
		REQUIRE(verdict.step == ogsimnet::BindStep::Ready);
		m_world.recorder.add(eventOf("wrap", id));
		client->registerCharacter(simId, std::move(*record.simulatable), std::move(record.inputProvider),
			[this, id, hasProvider](Channel* channel) { onQueuedRegisterApplied_Step(id, channel, hasProvider); });
		record.simulatable.reset();
		record.queued = true;
		return Status::Pending;
	}

	// SL :103-131
	Status finishQueuedRegistration(unsigned int id)
	{
		collectLifecycleOutcomes_GameThread();
		const auto it = m_pendingRegistrations.find(id);
		if (it == m_pendingRegistrations.end())
			return Status::Pending;
		PendingRegistration& record = it->second;
		if (record.refused || record.rosterRemoved)
		{
			if (m_world.hasLink && id <= 0xFFu)
				m_world.bindings.markUnregistered(static_cast<uint8_t>(id));
			char line[256];
			std::snprintf(line, sizeof(line),
				"tryRegister: id=%u: the og-sim-net client's queued registration did not hold (%s); the character stays "
				"unsimulated",
				id, record.rosterRemoved ? "the roster removed it" : "its [SimNet.Lifecycle] line names why");
			m_world.recorder.add(std::string("log Error ") + line);
			m_pendingRegistrations.erase(it);
			return Status::Refused;
		}
		if (!record.applied)
			return Status::Pending;

		m_registeredComponentsById[id] = 1;
		char line[256];
		std::snprintf(line, sizeof(line), "tryRegister: registered simulatable id=%u isAuthority=%d", id, 0);
		m_world.recorder.add(std::string("log Log ") + line);
		m_world.recorder.add(eventOf("afterRegistered", id));
		m_world.recorder.add(eventOf("onRegistered", id));
		m_pendingRegistrations.erase(it);
		return Status::Ready;
	}

	// SL :133-153
	void requestQueuedUnregister_GameThread(unsigned int id)
	{
		const auto pending = m_pendingRegistrations.find(id);
		const bool queued = pending != m_pendingRegistrations.end() && pending->second.queued;
		const bool registered = m_registeredComponentsById.find(id) != m_registeredComponentsById.end();
		if (pending != m_pendingRegistrations.end())
			m_pendingRegistrations.erase(pending);
		if (m_immediateStepsStopped || (!queued && !registered))
		{
			unregisterCharacterHostOnly(id, /*releaseBodiesNow=*/true);
			return;
		}
		{
			const std::scoped_lock lock(m_lifecycleBridgeMutex);
			m_unregisterRequests.push_back(id);
		}
		if (m_pendingUnregisters.find(id) != m_pendingUnregisters.end())
			m_world.recorder.add(eventOf("heldComponentReplaced", id));
		m_pendingUnregisters[id] = 1;
		unregisterCharacterHostOnly(id, /*releaseBodiesNow=*/false);
	}

	// UE :1686-1696, with forgetSimNetCharacter (UE :1298-1307)
	void unregisterCharacterHostOnly(unsigned int id, bool releaseBodiesNow)
	{
		m_registeredComponentsById.erase(id);
		if (id != 0u && id <= 0xFFu && m_world.hasLink)
			m_world.bindings.markUnregistered(static_cast<uint8_t>(id));
		m_world.recorder.add(std::string("hostOnly ") + std::to_string(id) + (releaseBodiesNow ? " release" : " keep"));
	}

	World&                                                  m_world;
	std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;
	std::unordered_map<unsigned int, int>                  m_registeredComponentsById;
	std::unordered_map<unsigned int, int>                  m_pendingUnregisters;
	std::unordered_set<unsigned int>                       m_simUnregisteredIds;
	bool                                                   m_immediateStepsStopped = false;
	std::mutex                                             m_lifecycleBridgeMutex;
	std::vector<unsigned int>                              m_unregisterRequests;
	std::vector<LifecycleOutcome>                          m_lifecycleOutcomes;
	std::unordered_set<unsigned int>                       m_systemsNotified_Step;
	std::unordered_set<unsigned int>                       m_unregisterRequested_Step;
};

// ---------------------------------------------------------------------------------------------------------------------------
// The NEW code: og-sim-session's SessionLifecycle with the Unreal adapter's half (UESimulationSession.h after task 29 (b):
// tryRegister's queued finish and queue, unregisterFromNewFramework's client branch, onPredictionOutcome_GameThread,
// teardownImmediateClient_GameThread, detachFromSimNetLanes) and the step through SessionNetHooks::receiveForStep.
// ---------------------------------------------------------------------------------------------------------------------------
struct LifecyclePort
{
	using Client        = FakeClient;
	using Channel       = sessionLifecyclePredictionTest::Channel;
	using Simulatable   = sessionLifecyclePredictionTest::Simulatable;
	using InputProvider = sessionLifecyclePredictionTest::InputProvider;
	using Bindings      = LockedBindings;

	World* world = nullptr;

	Client*   client() { return world->hasLink ? &world->client : nullptr; }
	Bindings* bindings() { return world->hasLink ? &world->bindings : nullptr; }

	InputProvider wrapInputProvider(const ogsession::RegisterRequest& request, InputProvider&& provider)
	{
		world->recorder.add(eventOf("wrap", request.simId));
		return std::move(provider);
	}

	void notifyRegistered(uint32_t id) { world->recorder.add(eventOf("notifyRegistered", id)); }
	void notifyUnregistered(uint32_t id) { world->recorder.add(eventOf("notifyUnregistered", id)); }
	bool inStorage(uint32_t id) const { return world->client.inStorage(id); }
	void installInputStamps(uint32_t id, Channel&) { world->recorder.add(eventOf("stamps.install", id)); }
	void clearInputStamps(Channel& channel) { world->recorder.add(eventOf("stamps.clear", channel.simId)); }

	void stopSteps()
	{
		world->recorder.add("stopSteps");
		world->stopSteps();
	}

	void drainWhileStopped()
	{
		world->recorder.add("stoppedDrain.begin");
		world->client.drain();
		world->recorder.add("stoppedDrain.end");
	}

	void logRouted(const char* line) { world->recorder.add(levelLineOf(line)); }
};

static_assert(ogsession::SessionLifecyclePort<LifecyclePort>,
	"The test's port models SessionLifecyclePort, as the Unreal session's LifecyclePort does");

struct FakeLanes
{
	using Client = FakeClient;

	World* world = nullptr;

	Client* client() { return world->hasLink ? &world->client : nullptr; }
	bool    clientSessionBound() const { return true; }
	void    drainClient_Step() { world->client.drain(); }
};

struct NetPort
{
	struct Server
	{
	};

	struct Idle
	{
	};

	using Lanes       = FakeLanes;
	using Simulatable = sessionLifecyclePredictionTest::Simulatable;

	FakeLanes*                                   lanesPtr  = nullptr;
	ogsession::SessionLifecycle<LifecyclePort>* lifecycle = nullptr;

	Lanes*  lanes() const { return lanesPtr; }
	Server* server() { return nullptr; }
	Idle*   idle() { return nullptr; }
	void    runRunnerRequests() {}
	void    applyUnregisterRequests(FakeClient& client) { lifecycle->applyRequests(client); }
	void    publishPresentation() {}
	double  nowSeconds() const { return 0.0; }
	void    logRouted(const char*) {}
};

struct PredictingManager
{
	bool runsPrediction() const { return true; }
};

struct NoStamps
{
	void stampCorrectionHanded_Internal(unsigned int, uint32_t, double) {}
};

class NewSession
{
public:
	using Lifecycle = ogsession::SessionLifecycle<LifecyclePort>;
	using NetHooks  = ogsession::SessionNetHooks<NetPort, PredictingManager, int, int, NoStamps>;

	NewSession(World& world, const ogsession::SessionThreadChecks& threads)
		: m_world(world)
		, m_lanes{ &world }
		, m_lifecycle(threads, LifecyclePort{ &world })
		, m_netHooks(threads, NetPort{ &m_lanes, &m_lifecycle }, m_manager, m_netSync, m_storage, m_stamps)
	{
		world.host.unregistering     = [this](uint8_t simId) { m_lifecycle.onUnregistering(simId); };
		world.host.registeredRemoved = [this](uint8_t simId) { m_lifecycle.onRegisteredRemoved(simId); };
	}

	Status tryRegister(unsigned int id, Simulatable&& simulatable, InputProvider&& inputProvider)
	{
		auto it = m_pendingRegistrations.find(id);
		if (it == m_pendingRegistrations.end())
		{
			PendingRegistration record;
			record.simulatable.emplace(std::move(simulatable));
			record.inputProvider = std::move(inputProvider);
			it = m_pendingRegistrations.emplace(id, std::move(record)).first;
		}
		PendingRegistration& record = it->second;
		if (record.queued)
		{
			collectLifecycleOutcomes_GameThread();
			const ogsession::RegisterStatus status =
				m_lifecycle.tryRegister(requestFor(id), std::move(simulatable), std::move(inputProvider));
			if (status == ogsession::RegisterStatus::Pending)
				return Status::Pending;
			if (status == ogsession::RegisterStatus::Refused)
			{
				m_pendingRegistrations.erase(id);
				return Status::Refused;
			}
			m_registeredComponentsById[id] = 1;
			m_world.recorder.add(eventOf("afterRegistered", id));
			m_world.recorder.add(eventOf("onRegistered", id));
			m_pendingRegistrations.erase(id);
			return Status::Ready;
		}
		m_lifecycle.tryRegister(requestFor(id), std::move(*record.simulatable), std::move(record.inputProvider));
		if (m_lifecycle.isQueued(id))
		{
			record.simulatable.reset();
			record.queued = true;
		}
		return Status::Pending;
	}

	void unregisterFromNewFramework(unsigned int id)
	{
		if (m_simUnregisteredIds.erase(id) != 0u)
		{
			m_world.recorder.add(eventOf("unregister.ignored.G30", id));
			return;
		}
		const ogsession::UnregisterRelease release = m_lifecycle.requestUnregister(id, ogsession::EntityKey{ id });
		m_pendingRegistrations.erase(id);
		if (release == ogsession::UnregisterRelease::AfterApplied)
		{
			if (m_pendingUnregisters.find(id) != m_pendingUnregisters.end())
				m_world.recorder.add(eventOf("heldComponentReplaced", id));
			m_pendingUnregisters[id] = 1;
		}
		unregisterCharacterHostOnly(id, release == ogsession::UnregisterRelease::Now);
	}

	void step() { m_netHooks.receiveForStep(); }

	void collectLifecycleOutcomes_GameThread()
	{
		m_lifecycle.pollOutcomes([this](const ogsession::LifecycleOutcome& outcome) { onPredictionOutcome_GameThread(outcome); });
	}

	void teardownImmediateClient_GameThread()
	{
		if (!m_lifecycle.teardown([this](const ogsession::LifecycleOutcome& outcome) { onPredictionOutcome_GameThread(outcome); }))
			return;
		for (auto& entry : m_pendingUnregisters)
			m_world.recorder.add(eventOf("backend.release", entry.first));
		m_pendingUnregisters.clear();
		std::vector<unsigned int> registered;
		for (const auto& entry : m_registeredComponentsById)
			registered.push_back(entry.first);
		for (const unsigned int id : registered)
		{
			unregisterCharacterHostOnly(id, /*releaseBodiesNow=*/true);
			m_simUnregisteredIds.insert(id);
		}
		for (auto it = m_pendingRegistrations.begin(); it != m_pendingRegistrations.end();)
		{
			if (it->second.queued)
				it = m_pendingRegistrations.erase(it);
			else
				++it;
		}
	}

	void detachFromSimNetLanes()
	{
		for (const auto& entry : m_registeredComponentsById)
			m_world.bindings.markUnregistered(static_cast<uint8_t>(entry.first));
		m_lifecycle.detach(m_world.bindings);
	}

	Lifecycle& lifecycle() { return m_lifecycle; }
	size_t registeredCount() const { return m_registeredComponentsById.size(); }
	size_t pendingUnregisterCount() const { return m_pendingUnregisters.size(); }
	size_t queuedCount() const
	{
		return static_cast<size_t>(std::count_if(m_pendingRegistrations.begin(), m_pendingRegistrations.end(),
			[](const auto& entry) { return entry.second.queued; }));
	}

private:
	struct PendingRegistration
	{
		std::optional<Simulatable> simulatable;
		InputProvider              inputProvider;
		bool                       queued = false;
	};

	static ogsession::RegisterRequest requestFor(unsigned int id)
	{
		ogsession::RegisterRequest request;
		request.simId     = id;
		request.entityKey = ogsession::EntityKey{ id };
		return request;
	}

	void onPredictionOutcome_GameThread(const ogsession::LifecycleOutcome& outcome)
	{
		if (const auto* const unregistered = std::get_if<ogsession::lifecycleOutcome::Unregistered>(&outcome))
		{
			const auto found = m_pendingUnregisters.find(unregistered->simId);
			if (found == m_pendingUnregisters.end())
				return;
			m_world.recorder.add(eventOf("backend.release", unregistered->simId));
			m_pendingUnregisters.erase(found);
			return;
		}
		if (const auto* const removed = std::get_if<ogsession::lifecycleOutcome::RosterRemoved>(&outcome))
		{
			const auto found = m_registeredComponentsById.find(removed->simId);
			if (found == m_registeredComponentsById.end())
				return;
			unregisterCharacterHostOnly(removed->simId, /*releaseBodiesNow=*/true);
			m_simUnregisteredIds.insert(removed->simId);
		}
	}

	void unregisterCharacterHostOnly(unsigned int id, bool releaseBodiesNow)
	{
		m_registeredComponentsById.erase(id);
		if (id != 0u && id <= 0xFFu && m_world.hasLink)
			m_world.bindings.markUnregistered(static_cast<uint8_t>(id));
		m_world.recorder.add(std::string("hostOnly ") + std::to_string(id) + (releaseBodiesNow ? " release" : " keep"));
	}

	World&                                                  m_world;
	FakeLanes                                               m_lanes;
	PredictingManager                                       m_manager;
	int                                                     m_netSync = 0;
	int                                                     m_storage = 0;
	NoStamps                                                m_stamps;
	Lifecycle                                               m_lifecycle;
	NetHooks                                                m_netHooks;
	std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;
	std::unordered_map<unsigned int, int>                  m_registeredComponentsById;
	std::unordered_map<unsigned int, int>                  m_pendingUnregisters;
	std::unordered_set<unsigned int>                       m_simUnregisteredIds;
};

// One thread that runs the steps (inside a StepScope of the client's context, as the tick driver's worker task does), one job
// at a time; start() hands it a step without waiting, so the frame thread can act while it runs.
class StepThread
{
public:
	explicit StepThread(const TickDriverThreadContext& context)
		: m_context(context)
		, m_thread([this] { loop(); })
	{
	}

	~StepThread()
	{
		{
			const std::scoped_lock lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_all();
		m_thread.join();
	}

	StepThread(const StepThread&) = delete;
	StepThread& operator=(const StepThread&) = delete;

	void start(std::function<void()> job)
	{
		const std::scoped_lock lock(m_mutex);
		m_job  = std::move(job);
		m_busy = true;
		m_wake.notify_all();
	}

	void wait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_wake.wait(lock, [this] { return !m_busy; });
	}

	void run(std::function<void()> job)
	{
		start(std::move(job));
		wait();
	}

	std::thread::id id() const { return m_thread.get_id(); }

private:
	void loop()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (true)
		{
			m_wake.wait(lock, [this] { return m_busy || m_stop; });
			if (!m_busy)
				return;
			std::function<void()> job = std::move(m_job);
			lock.unlock();
			{
				const StepScope scope(m_context);
				job();
			}
			lock.lock();
			m_busy = false;
			m_wake.notify_all();
		}
	}

	const TickDriverThreadContext& m_context;
	std::mutex                     m_mutex;
	std::condition_variable        m_wake;
	std::function<void()>          m_job;
	bool                           m_busy = false;
	bool                           m_stop = false;
	std::thread                    m_thread;
};

struct ClientContext
{
	ClientContext() { context.configure<true>(tickDriverThread::kClientRoleBit, StepThreading::FrameBounded); }

	TickDriverThreadContext context;
};

// Runs the same script against the old transcription and the new code, each on its own world and its own step thread.
template <typename SessionT>
struct Rig
{
	ClientContext                  client;
	World                          world;
	ogsession::SessionThreadChecks threads{ ogsession::SessionRole::Prediction, &client.context, std::this_thread::get_id() };
	SessionT                       session = makeSession();
	StepThread                     stepThread{ client.context };

	SessionT makeSession();

	void step()
	{
		stepThread.run([this] { session.step(); });
	}

	Status tryRegister(unsigned int id, bool isLocal)
	{
		const Status status = session.tryRegister(id, Simulatable{ id }, isLocal ? InputProvider([] { return 1; }) : InputProvider());
		world.recorder.add(std::string("tryRegister ") + std::to_string(id) + " -> " + nameOf(status));
		return status;
	}
};

template <>
OldSession Rig<OldSession>::makeSession()
{
	return OldSession(world);
}

template <>
NewSession Rig<NewSession>::makeSession()
{
	return NewSession(world, threads);
}

// A teardown's own order follows each side's id containers; the events from "stopSteps" on are compared as a multiset, and
// "stopSteps" must open the section on both sides.
Events normalized(Events events)
{
	const auto teardown = std::find(events.begin(), events.end(), std::string("stopSteps"));
	if (teardown != events.end())
		std::sort(teardown + 1, events.end());
	return events;
}

template <typename ScriptT>
std::pair<Events, Events> runBoth(ScriptT&& script)
{
	Rig<OldSession> oldRig;
	script(oldRig);
	Rig<NewSession> newRig;
	script(newRig);
	return { normalized(oldRig.world.recorder.take()), normalized(newRig.world.recorder.take()) };
}

void checkEqual(const Events& oldEvents, const Events& newEvents)
{
	const size_t common = std::min(oldEvents.size(), newEvents.size());
	for (size_t index = 0u; index < common; ++index)
	{
		INFO("event " << index);
		REQUIRE(newEvents[index] == oldEvents[index]);
	}
	REQUIRE(newEvents.size() == oldEvents.size());
}

size_t countOf(const Events& events, const std::string& prefix)
{
	return static_cast<size_t>(std::count_if(events.begin(), events.end(),
		[&prefix](const std::string& event) { return event.rfind(prefix, 0) == 0; }));
}

bool precedes(const Events& events, const std::string& first, const std::string& second)
{
	const auto a = std::find(events.begin(), events.end(), first);
	const auto b = std::find(events.begin(), events.end(), second);
	return a != events.end() && b != events.end() && a < b;
}

// ---------------------------------------------------------------------------------------------------------------------------
// The randomized scripts: pawns spawn, poll their registration each frame and end play; the server adds and removes roster
// entries; steps run; the frame collects; the run ends with the teardown.
// ---------------------------------------------------------------------------------------------------------------------------
constexpr unsigned int kIds = 6u;

bool isLocalId(unsigned int id)
{
	return id == 1u;
}

struct PawnModel
{
	enum class State : uint8_t
	{
		Absent,
		Registering,
		Ready,
		Abandoned,
	};

	std::array<State, kIds + 1u> pawns{};
	std::array<bool, kIds + 1u>  inRoster{};
};

template <typename RigT>
void frameOp(RigT& rig, PawnModel& model, std::mt19937& random)
{
	const unsigned int id = 1u + static_cast<unsigned int>(random() % kIds);
	switch (random() % 6u)
	{
	case 0u:
		if (model.pawns[id] == PawnModel::State::Absent)
			model.pawns[id] = PawnModel::State::Registering;
		break;
	case 1u:
		if (model.pawns[id] != PawnModel::State::Absent)
		{
			rig.session.unregisterFromNewFramework(id);
			model.pawns[id] = PawnModel::State::Absent;
		}
		break;
	case 2u:
		if (!model.inRoster[id])
		{
			rig.world.client.addToRoster(static_cast<uint8_t>(id), isLocalId(id));
			model.inRoster[id] = true;
		}
		break;
	case 3u:
		if (model.inRoster[id] && random() % 3u == 0u)
		{
			rig.world.client.removeFromRoster(static_cast<uint8_t>(id));
			model.inRoster[id] = false;
		}
		break;
	default:
		break;
	}
	for (unsigned int pawn = 1u; pawn <= kIds; ++pawn)
	{
		if (model.pawns[pawn] != PawnModel::State::Registering)
			continue;
		const Status status = rig.tryRegister(pawn, isLocalId(pawn));
		if (status == Status::Ready)
			model.pawns[pawn] = PawnModel::State::Ready;
		else if (status == Status::Refused)
			model.pawns[pawn] = PawnModel::State::Abandoned;
	}
	rig.session.collectLifecycleOutcomes_GameThread();
}

template <typename RigT>
void serialScript(RigT& rig, uint32_t seed, int frames)
{
	std::mt19937 random(seed);
	PawnModel model;
	for (int frame = 0; frame < frames; ++frame)
	{
		frameOp(rig, model, random);
		if (random() % 3u != 0u)
			rig.step();
	}
	rig.session.teardownImmediateClient_GameThread();
}

struct Totals
{
	size_t registered   = 0u;
	size_t unregistered = 0u;
	size_t ready        = 0u;
	size_t refused      = 0u;
	size_t released     = 0u;
	size_t keeps        = 0u;
	size_t replaced     = 0u;
};

Totals totalsOf(const Events& events)
{
	Totals totals;
	totals.registered   = countOf(events, "notifyRegistered ");
	totals.unregistered = countOf(events, "notifyUnregistered ");
	totals.released     = countOf(events, "backend.release ");
	totals.replaced     = countOf(events, "heldComponentReplaced ");
	totals.keeps        = static_cast<size_t>(std::count_if(events.begin(), events.end(), [](const std::string& event)
		{ return event.rfind("hostOnly ", 0) == 0 && event.find(" keep") != std::string::npos; }));
	for (const std::string& event : events)
	{
		if (event.rfind("tryRegister ", 0) != 0)
			continue;
		if (event.find("-> Ready") != std::string::npos)
			++totals.ready;
		else if (event.find("-> Refused") != std::string::npos)
			++totals.refused;
	}
	return totals;
}

// A free-running step thread for the concurrent interleavings: it steps until the lifecycle's teardown stops it (the port's
// stopSteps joins it, as the backend's onEndPlayBegin waits for the step tasks).
class FreeRunningSteps
{
public:
	FreeRunningSteps(const TickDriverThreadContext& context, std::function<void()> step)
		: m_thread([this, &context, step = std::move(step)]
		{
			while (!m_stop.load(std::memory_order_acquire))
			{
				{
					const StepScope scope(context);
					step();
				}
				m_steps.fetch_add(1u, std::memory_order_relaxed);
				std::this_thread::yield();
			}
		})
	{
	}

	~FreeRunningSteps() { stop(); }

	void stop()
	{
		m_stop.store(true, std::memory_order_release);
		if (m_thread.joinable())
			m_thread.join();
	}

	uint64_t steps() const { return m_steps.load(std::memory_order_relaxed); }

private:
	std::atomic<bool>     m_stop{ false };
	std::atomic<uint64_t> m_steps{ 0u };
	std::thread           m_thread;
};

struct ConcurrentResult
{
	Totals   totals;
	uint64_t steps                  = 0u;
	size_t   afterApplied           = 0u;
	bool     clientClear            = false;
	bool     bindingsClear          = false;
	bool     adapterClear           = false;
	bool     outcomesLeft           = false;
};

ConcurrentResult runConcurrent(uint32_t seed, int frames)
{
	ClientContext client;
	World world;
	const ogsession::SessionThreadChecks threads(ogsession::SessionRole::Prediction, &client.context, std::this_thread::get_id());
	NewSession session(world, threads);
	FreeRunningSteps steps(client.context, [&session] { session.step(); });
	world.stopSteps = [&steps] { steps.stop(); };

	struct ConcurrentRig
	{
		NewSession& session;
		World&      world;

		Status tryRegister(unsigned int id, bool isLocal)
		{
			const Status status =
				session.tryRegister(id, Simulatable{ id }, isLocal ? InputProvider([] { return 1; }) : InputProvider());
			world.recorder.add(std::string("tryRegister ") + std::to_string(id) + " -> " + nameOf(status));
			return status;
		}
	} rig{ session, world };

	std::mt19937 random(seed);
	PawnModel model;
	for (int frame = 0; frame < frames; ++frame)
	{
		const uint64_t stepsBefore = steps.steps();
		frameOp(rig, model, random);
		if (random() % 3u != 0u)
		{
			while (steps.steps() == stepsBefore)
				std::this_thread::yield();
		}
	}
	session.teardownImmediateClient_GameThread();

	ConcurrentResult result;
	const Events events = world.recorder.take();
	result.totals        = totalsOf(events);
	result.steps         = steps.steps();
	result.afterApplied  = result.totals.keeps;
	result.clientClear   = !world.client.anyRegistered() && world.client.idle();
	result.bindingsClear = world.bindings.registeredIds().empty();
	result.adapterClear  = session.registeredCount() == 0u && session.pendingUnregisterCount() == 0u && session.queuedCount() == 0u;
	session.lifecycle().pollOutcomes([&result](const ogsession::LifecycleOutcome&) { result.outcomesLeft = true; });
	return result;
}
} // namespace sessionLifecyclePredictionTest

using namespace sessionLifecyclePredictionTest;

TEST_CASE("SessionLifecycle.RegisterToReadyOnTwoThreads", "[Session][SessionLifecycle]")
{
	const auto [oldEvents, newEvents] = runBoth([](auto& rig)
	{
		rig.world.client.addToRoster(1u, /*isLocal=*/true);
		rig.step();
		CHECK(rig.tryRegister(1u, true) == Status::Pending);
		CHECK(rig.tryRegister(1u, true) == Status::Pending);
		rig.step();
		CHECK(rig.tryRegister(1u, true) == Status::Ready);
	});
	checkEqual(oldEvents, newEvents);
	const Events expected{ "wrap 1", "client.register 1", "tryRegister 1 -> Pending", "tryRegister 1 -> Pending",
		"facade.register 1", "stamps.install 1", "notifyRegistered 1", "log Log tryRegister: registered simulatable id=1 isAuthority=0",
		"afterRegistered 1", "onRegistered 1", "tryRegister 1 -> Ready" };
	CHECK(newEvents == expected);
}

TEST_CASE("SessionLifecycle.RefusedByTheRosterVerdictAndByTheDrain", "[Session][SessionLifecycle]")
{
	const auto [oldEvents, newEvents] = runBoth([](auto& rig)
	{
		CHECK(rig.tryRegister(2u, false) == Status::Pending);
		rig.step();
		CHECK(rig.tryRegister(2u, false) == Status::Pending);
		CHECK_FALSE(rig.world.bindings.isRegistered(2u));

		rig.world.client.addToRoster(3u, /*isLocal=*/false);
		rig.step();
		rig.world.client.preRegisterInFacade(3u);
		CHECK(rig.tryRegister(3u, false) == Status::Pending);
		CHECK(rig.world.bindings.isRegistered(3u));
		rig.step();
		CHECK(rig.tryRegister(3u, false) == Status::Refused);
		CHECK_FALSE(rig.world.bindings.isRegistered(3u));
	});
	checkEqual(oldEvents, newEvents);
	CHECK(countOf(newEvents, "client.register 2") == 0u);
	CHECK(countOf(newEvents, "apply.register.refused.registered 3") == 1u);
	CHECK(countOf(newEvents, "log Error tryRegister: id=3: the og-sim-net client's queued registration did not hold (its "
		"[SimNet.Lifecycle] line names why); the character stays unsimulated") == 1u);
	CHECK(countOf(newEvents, "notifyRegistered") == 0u);
}

TEST_CASE("SessionLifecycle.ARosterRemovalBeforeAndAfterTheApply", "[Session][SessionLifecycle]")
{
	SECTION("after the apply, before the finish: the registration does not hold")
	{
		const auto [oldEvents, newEvents] = runBoth([](auto& rig)
		{
			rig.world.client.addToRoster(4u, /*isLocal=*/false);
			rig.step();
			CHECK(rig.tryRegister(4u, false) == Status::Pending);
			rig.world.client.removeFromRoster(4u);
			rig.step();
			CHECK(rig.tryRegister(4u, false) == Status::Refused);
		});
		checkEqual(oldEvents, newEvents);
		CHECK(precedes(newEvents, "facade.register 4", "roster.removed 4"));
		CHECK(precedes(newEvents, "notifyUnregistered 4", "facade.unregister 4"));
		CHECK(countOf(newEvents, "notifyRegistered 4") == countOf(newEvents, "notifyUnregistered 4"));
		CHECK(countOf(newEvents, "log Error tryRegister: id=4: the og-sim-net client's queued registration did not hold (the "
			"roster removed it); the character stays unsimulated") == 1u);
	}
	SECTION("before the apply: the step thread drains the removal while the frame thread queues the registration")
	{
		const auto [oldEvents, newEvents] = runBoth([](auto& rig)
		{
			rig.world.client.addToRoster(5u, /*isLocal=*/false);
			rig.step();
			rig.world.client.removeFromRoster(5u);
			std::atomic<bool> paused{ false };
			std::atomic<bool> resume{ false };
			rig.world.client.betweenQueueAndInbox = [&paused, &resume]
			{
				paused.store(true);
				while (!resume.load())
					std::this_thread::yield();
			};
			rig.stepThread.start([&rig] { rig.session.step(); });
			while (!paused.load())
				std::this_thread::yield();
			CHECK(rig.tryRegister(5u, false) == Status::Pending);
			resume.store(true);
			rig.stepThread.wait();
			rig.world.client.betweenQueueAndInbox = nullptr;
			rig.step();
			CHECK(rig.tryRegister(5u, false) == Status::Refused);
		});
		checkEqual(oldEvents, newEvents);
		CHECK(precedes(newEvents, "roster.removed 5", "apply.register.refused.noChannel 5"));
		CHECK(countOf(newEvents, "notifyRegistered") == 0u);
	}
	SECTION("a registered character: the roster removal unregisters it once, and its pawn's end of play is ignored (G-30)")
	{
		const auto [oldEvents, newEvents] = runBoth([](auto& rig)
		{
			rig.world.client.addToRoster(6u, /*isLocal=*/false);
			rig.step();
			rig.tryRegister(6u, false);
			rig.step();
			CHECK(rig.tryRegister(6u, false) == Status::Ready);
			rig.world.client.removeFromRoster(6u);
			rig.step();
			rig.session.collectLifecycleOutcomes_GameThread();
			rig.session.unregisterFromNewFramework(6u);
		});
		checkEqual(oldEvents, newEvents);
		CHECK(countOf(newEvents, "hostOnly 6 release") == 1u);
		CHECK(countOf(newEvents, "unregister.ignored.G30 6") == 1u);
		CHECK(countOf(newEvents, "notifyUnregistered 6") == 1u);
	}
}

TEST_CASE("SessionLifecycle.AnUnregisterAppliesBeforeThisStepsDrain", "[Session][SessionLifecycle]")
{
	const auto [oldEvents, newEvents] = runBoth([](auto& rig)
	{
		rig.world.client.addToRoster(1u, /*isLocal=*/true);
		rig.step();
		rig.tryRegister(1u, true);
		rig.step();
		CHECK(rig.tryRegister(1u, true) == Status::Ready);
		rig.session.unregisterFromNewFramework(1u);
		rig.world.recorder.add("--- one step");
		rig.step();
		rig.world.recorder.add("--- collect");
		rig.session.collectLifecycleOutcomes_GameThread();
	});
	checkEqual(oldEvents, newEvents);
	const auto step = std::find(newEvents.begin(), newEvents.end(), std::string("--- one step"));
	const auto collect = std::find(newEvents.begin(), newEvents.end(), std::string("--- collect"));
	REQUIRE(step != newEvents.end());
	REQUIRE(collect != newEvents.end());
	const Events inTheStep(step + 1, collect);
	const Events expectedStep{ "notifyUnregistered 1", "client.unregister 1", "facade.unregister 1", "stamps.clear 1" };
	CHECK(inTheStep == expectedStep);
	CHECK(Events(collect + 1, newEvents.end()) == Events{ "backend.release 1" });
	CHECK(countOf(newEvents, "hostOnly 1 keep") == 1u);
}

TEST_CASE("SessionLifecycle.AnUnregisterWhileARegisterIsQueued", "[Session][SessionLifecycle]")
{
	const auto [oldEvents, newEvents] = runBoth([](auto& rig)
	{
		rig.world.client.addToRoster(2u, /*isLocal=*/false);
		rig.step();
		CHECK(rig.tryRegister(2u, false) == Status::Pending);
		rig.session.unregisterFromNewFramework(2u);
		rig.step();
		rig.session.collectLifecycleOutcomes_GameThread();
		rig.session.teardownImmediateClient_GameThread();
	});
	checkEqual(oldEvents, newEvents);
	CHECK(precedes(newEvents, "facade.register 2", "facade.unregister 2"));
	CHECK(countOf(newEvents, "notifyRegistered 2") == 0u);
	CHECK(countOf(newEvents, "notifyUnregistered 2") == 0u);
	CHECK(countOf(newEvents, "backend.release 2") == 1u);
	CHECK(countOf(newEvents, "hostOnly 2 keep") == 1u);
}

TEST_CASE("SessionLifecycle.AnUnregisterAtTeardownWithTheStepsStopped", "[Session][SessionLifecycle]")
{
	const auto [oldEvents, newEvents] = runBoth([](auto& rig)
	{
		for (uint8_t id = 1u; id <= 3u; ++id)
			rig.world.client.addToRoster(id, isLocalId(id));
		rig.step();
		rig.tryRegister(1u, true);
		rig.tryRegister(2u, false);
		rig.step();
		CHECK(rig.tryRegister(1u, true) == Status::Ready);
		CHECK(rig.tryRegister(2u, false) == Status::Ready);
		rig.session.unregisterFromNewFramework(2u);
		rig.tryRegister(3u, false);
		rig.session.teardownImmediateClient_GameThread();
		rig.session.teardownImmediateClient_GameThread();
		CHECK(rig.tryRegister(4u, false) == Status::Pending);
		CHECK(rig.world.bindings.registeredIds().empty());
		CHECK_FALSE(rig.world.client.anyRegistered());
	});
	checkEqual(oldEvents, newEvents);
	const auto stop = std::find(newEvents.begin(), newEvents.end(), std::string("stopSteps"));
	REQUIRE(stop != newEvents.end());
	CHECK(countOf(newEvents, "stopSteps") == 1u);
	CHECK(countOf(newEvents, "notifyRegistered") == countOf(newEvents, "notifyUnregistered"));
	CHECK(countOf(newEvents, "backend.release 2") == 1u);
	CHECK(countOf(newEvents, "hostOnly 1 release") == 1u);
	CHECK(countOf(newEvents, "facade.unregister 3") == 1u);
}

TEST_CASE("SessionLifecycle.AfterTheTeardownAStepWorkersReceiptIsAStepReceipt", "[Session][SessionLifecycle]")
{
	Rig<NewSession> rig;
	rig.world.client.addToRoster(1u, /*isLocal=*/true);
	rig.step();
	CHECK(rig.tryRegister(1u, true) == Status::Pending);
	rig.step();
	CHECK(rig.tryRegister(1u, true) == Status::Ready);
	rig.session.teardownImmediateClient_GameThread();
	CHECK(rig.world.recorder.count("stoppedDrain.end") == 1u);
	bool received = false;
	rig.stepThread.run([&rig, &received]
	{
		rig.session.lifecycle().onRegisteredRemoved(1u);
		received = true;
	});
	CHECK(received);
}

TEST_CASE("SessionLifecycle.TheDetachWithoutTeardownUnmarksAQueuedRegistration", "[Session][SessionLifecycle]")
{
	auto script = [](auto& rig)
	{
		rig.world.client.addToRoster(1u, /*isLocal=*/true);
		rig.world.client.addToRoster(2u, /*isLocal=*/false);
		rig.step();
		rig.tryRegister(2u, false);
		rig.step();
		CHECK(rig.tryRegister(2u, false) == Status::Ready);
		CHECK(rig.tryRegister(1u, true) == Status::Pending);
		rig.session.detachFromSimNetLanes();
	};
	Rig<OldSession> oldRig;
	script(oldRig);
	Rig<NewSession> newRig;
	script(newRig);
	CHECK(oldRig.world.bindings.registeredIds() == std::vector<uint8_t>{ 1u });
	REQUIRE(newRig.world.bindings.registeredIds().empty());
	newRig.world.bindings.seedFrom(ogsimnet::RosterMirror{});
	CHECK(newRig.world.bindings.registeredIds().empty());
}

TEST_CASE("SessionLifecycle.ThousandSerializedScriptsMatchTheTranscription", "[Session][SessionLifecycle]")
{
	Totals all;
	for (uint32_t seed = 1u; seed <= 1000u; ++seed)
	{
		INFO("seed " << seed);
		const auto [oldEvents, newEvents] = runBoth([seed](auto& rig) { serialScript(rig, seed, 60); });
		checkEqual(oldEvents, newEvents);
		const Totals totals = totalsOf(newEvents);
		REQUIRE(totals.registered == totals.unregistered);
		REQUIRE(totals.released + totals.replaced == totals.keeps);
		all.registered += totals.registered;
		all.ready += totals.ready;
		all.refused += totals.refused;
		all.released += totals.released;
		all.replaced += totals.replaced;
	}
	CHECK(all.registered == 4011u);
	CHECK(all.ready == 3790u);
	CHECK(all.refused == 65u);
	CHECK(all.released == 1735u);
	CHECK(all.replaced == 2u);
}

TEST_CASE("SessionLifecycle.ThousandConcurrentInterleavingsLoseNoOutcome", "[Session][SessionLifecycle]")
{
	Totals   all;
	uint64_t steps = 0u;
	for (uint32_t seed = 1u; seed <= 1000u; ++seed)
	{
		INFO("seed " << seed);
		const ConcurrentResult result = runConcurrent(seed, 60);
		REQUIRE(result.totals.registered == result.totals.unregistered);
		REQUIRE(result.totals.released + result.totals.replaced == result.afterApplied);
		REQUIRE(result.clientClear);
		REQUIRE(result.bindingsClear);
		REQUIRE(result.adapterClear);
		REQUIRE_FALSE(result.outcomesLeft);
		all.registered += result.totals.registered;
		all.ready += result.totals.ready;
		all.released += result.totals.released;
		steps += result.steps;
	}
	CHECK(all.registered > 2000u);
	CHECK(all.ready > 2000u);
	CHECK(all.released > 500u);
	CHECK(steps > 20000u);
}

TEST_CASE("SessionLifecycle.Survey", "[.][SessionLifecycleSurvey]")
{
	Totals serial;
	for (uint32_t seed = 1u; seed <= 1000u; ++seed)
	{
		Rig<NewSession> rig;
		serialScript(rig, seed, 60);
		const Totals totals = totalsOf(rig.world.recorder.take());
		serial.registered += totals.registered;
		serial.unregistered += totals.unregistered;
		serial.ready += totals.ready;
		serial.refused += totals.refused;
		serial.released += totals.released;
		serial.keeps += totals.keeps;
		serial.replaced += totals.replaced;
	}
	std::printf("[SessionLifecycleSurvey] serial seeds=1000 notifyRegistered=%zu notifyUnregistered=%zu ready=%zu refused=%zu "
				"released=%zu keeps=%zu replaced=%zu\n",
		serial.registered, serial.unregistered, serial.ready, serial.refused, serial.released, serial.keeps, serial.replaced);
	Totals concurrent;
	uint64_t steps = 0u;
	for (uint32_t seed = 1u; seed <= 1000u; ++seed)
	{
		const ConcurrentResult result = runConcurrent(seed, 60);
		concurrent.registered += result.totals.registered;
		concurrent.unregistered += result.totals.unregistered;
		concurrent.ready += result.totals.ready;
		concurrent.refused += result.totals.refused;
		concurrent.released += result.totals.released;
		concurrent.keeps += result.afterApplied;
		concurrent.replaced += result.totals.replaced;
		steps += result.steps;
	}
	std::printf("[SessionLifecycleSurvey] concurrent seeds=1000 notifyRegistered=%zu notifyUnregistered=%zu ready=%zu "
				"refused=%zu released=%zu keeps=%zu replaced=%zu steps=%llu\n",
		concurrent.registered, concurrent.unregistered, concurrent.ready, concurrent.refused, concurrent.released,
		concurrent.keeps, concurrent.replaced, static_cast<unsigned long long>(steps));
	SUCCEED();
}

TEST_CASE("SessionLifecycle.ApplyRequestsOnTheFrameThreadTripsTheCheck", "[.][SessionLifecycleDeath]")
{
	Rig<NewSession> rig;
	rig.session.lifecycle().applyRequests(rig.world.client);
	FAIL("SessionLifecycle::applyRequests on the frame thread did NOT trip its ApplyLifecycleRequests check");
}

TEST_CASE("SessionLifecycle.ARosterRemovalHookOutsideADrainTripsTheCheck", "[.][SessionLifecycleDeath]")
{
	Rig<NewSession> rig;
	rig.session.lifecycle().onRegisteredRemoved(1u);
	FAIL("SessionLifecycle::onRegisteredRemoved on the frame thread outside the stopped drain did NOT trip its DrainInbox check");
}

TEST_CASE("SessionLifecycle.EachHeldComponentIsReleasedByItsOwnUnregisterOutcome", "[Session][SessionLifecycle]")
{
	ogsession::SessionHeldForUnregister<int> held;
	std::vector<std::pair<uint32_t, int>> released;
	const auto record = [&released](uint32_t simId, int& handle) { released.emplace_back(simId, handle); };

	held.hold(7u, 1);
	held.hold(7u, 2);
	held.hold(9u, 3);
	CHECK(held.heldCount(7u) == 2u);
	CHECK(held.heldCount(9u) == 1u);

	REQUIRE(held.releaseOldest(7u, record));
	CHECK(released.back() == std::make_pair(7u, 1));
	REQUIRE(held.releaseOldest(7u, record));
	CHECK(released.back() == std::make_pair(7u, 2));
	CHECK_FALSE(held.releaseOldest(7u, record));
	CHECK(held.heldCount(7u) == 0u);

	held.hold(7u, 4);
	held.releaseAll(record);
	CHECK(held.heldCount(7u) == 0u);
	CHECK(held.heldCount(9u) == 0u);
	REQUIRE(released.size() == 4u);
	std::vector<int> handles;
	for (const auto& entry : released)
		handles.push_back(entry.second);
	std::sort(handles.begin(), handles.end());
	CHECK(handles == std::vector<int>{ 1, 2, 3, 4 });
}
