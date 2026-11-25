// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimNet/SimNetRoster.h"
#include "OGSimSession/SessionLifecycle.h"
#include "OGSimSession/SessionNetHooks.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "OGTickDriver/TickDriverThreadContext.h"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sessionLifecycleAuthorityTest
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

std::string eventOf(const char* what, uint64_t value)
{
	return std::string(what) + " " + std::to_string(value);
}

std::string eventOf(const char* what, uint64_t first, uint64_t second)
{
	return std::string(what) + " " + std::to_string(first) + " " + std::to_string(second);
}

// The engine's entity: the Unreal component (whose address is the session's entity key) and its pawn (whose address is the
// bodies' key, TickDriverBackendUImpl::slotKeyOf). The owner connection and local index are the pawn's net facts.
struct Pawn
{
	uint64_t        key             = 0u;
	SimConnectionId ownerConnection = kInvalidSimConnection;
	uint8_t         localIndex      = 0u;
	uint32_t        unresolvableChecks = 0u;
};

struct Simulatable
{
	uint32_t tag = 0u;
	Pawn*    pawn = nullptr;
};

struct Channel
{
	uint8_t simId = 0u;
};

// The bodies: a slot table keyed by the bodies' key, as TickDriverBackendUImpl over JoltTickWorldHost (OGSimulation 221b701:
// createBodies_Runner, TickDriverBackendUImpl.h :366-377, acquireAndBind; a key that already holds a slot gets that slot
// again, JoltTickWorldHost.h :363-373). Runner-only state, read by the test after the runner has gone idle.
class FakeBodies
{
public:
	struct Request
	{
		uint64_t key   = 0u;
		uint32_t simId = 0u;
	};

	using Simulatable = sessionLifecycleAuthorityTest::Simulatable;

	explicit FakeBodies(Recorder& recorder)
		: m_recorder(recorder)
	{
	}

	Request makeRequest(const ogsession::RegisterRequest& request)
	{
		const Pawn* const pawn = reinterpret_cast<const Pawn*>(request.entityKey.value);
		m_recorder.add(eventOf("bodies.makeRequest", request.simId, pawn->key));
		return Request{ pawn->key, request.simId };
	}

	bool create(const Request& request, Simulatable&, uint32_t id)
	{
		if (m_slots.find(request.key) != m_slots.end())
		{
			m_recorder.add(eventOf("bodies.taken", id, request.key));
			return true;
		}
		for (uint32_t slot = 0u; slot < kMaxSimulatableSlots; ++slot)
		{
			if (!isTaken(slot))
			{
				m_slots[request.key] = slot;
				m_recorder.add(eventOf("bodies.create", id, slot));
				return true;
			}
		}
		m_recorder.add(eventOf("bodies.full", id));
		return false;
	}

	std::optional<uint32_t> slotOf(uint64_t key)
	{
		const auto found = m_slots.find(key);
		return found != m_slots.end() ? std::optional<uint32_t>(found->second) : std::nullopt;
	}

	void onRegistered(uint64_t, uint32_t id) { m_recorder.add(eventOf("bodies.onRegistered", id)); }

	void release(uint64_t key, uint32_t id)
	{
		m_recorder.add(eventOf("bodies.release", id));
		m_slots.erase(key);
	}

	size_t held() const { return m_slots.size(); }
	bool   holds(uint64_t key) const { return m_slots.find(key) != m_slots.end(); }

private:
	bool isTaken(uint32_t slot) const
	{
		return std::any_of(m_slots.begin(), m_slots.end(), [slot](const auto& entry) { return entry.second == slot; });
	}

	Recorder&                    m_recorder;
	std::map<uint64_t, uint32_t> m_slots;
};

static_assert(ogsession::SessionBodies<FakeBodies>, "The test's bodies model SessionBodies, as TickDriverBackendUImpl does");

// og-sim-net's server, its lifecycle half: registerCharacter and unregisterCharacter queue a job, drain() applies the jobs in
// order (SimNetServer.h at OGSimulation 221b701: :430-447 the two enqueues, :565-575 unregisterLifecycle (the host's
// unregistering hook before the facade), :577-596 applyRegister, :598-611 applyUnregister, :360-397 addCharacter's refusals,
// :797-801 takeRemove). The roster is og-sim-net's real RosterAuthority, so its capacity and duplicate-owner-slot refusals
// (og-input-latency P5-23) are the product's.
class FakeServer
{
public:
	using Applied = std::function<void(Channel*)>;

	FakeServer(Recorder& recorder, std::function<void(uint8_t)> unregistering)
		: m_recorder(recorder)
		, m_unregistering(std::move(unregistering))
	{
	}

	template <typename SimT, typename AppliedT>
	void registerCharacter(uint8_t simId, uint8_t physicsSlot, SimConnectionId ownerConn, uint8_t localIndex, std::string,
		SimT&&, AppliedT applied)
	{
		m_recorder.add(eventOf("server.registerCharacter", simId, physicsSlot));
		m_jobs.push_back([this, simId, physicsSlot, ownerConn, localIndex, applied = Applied(std::move(applied))]()
		{
			applyRegister(simId, physicsSlot, ownerConn, localIndex, applied);
		});
	}

	template <typename SimT, typename AppliedT>
	void unregisterCharacter(uint8_t simId, ogsimnet::RosterRemoveReason reason, AppliedT applied)
	{
		m_recorder.add(eventOf("server.unregisterCharacter", simId));
		m_jobs.push_back([this, simId, reason, applied = Applied(std::move(applied))]() { applyUnregister(simId, reason, applied); });
	}

	void drain()
	{
		m_recorder.add("server.drain");
		std::vector<std::function<void()>> jobs;
		jobs.swap(m_jobs);
		for (std::function<void()>& job : jobs)
			job();
	}

	bool inStorage(uint32_t id) const { return m_storage.find(id) != m_storage.end(); }

	void occupyRoster(uint8_t simId, std::optional<SimConnectionId> owner, uint8_t localIndex)
	{
		ogsimnet::RosterRecord record;
		record.simId       = simId;
		record.physicsSlot = ogsimnet::kRosterNoSlot;
		record.owner       = owner;
		record.localIndex  = owner.has_value() ? localIndex : ogsimnet::kRosterNotLocal;
		REQUIRE(m_roster.add(record) == ogsimnet::RosterAddRefusal::None);
		m_characters.insert(simId);
	}

	void vacateRoster(uint8_t simId)
	{
		m_roster.remove(simId, ogsimnet::RosterRemoveReason::Left);
		m_roster.removeFor(simId);
		m_characters.erase(simId);
	}

	size_t storageSize() const { return m_storage.size(); }

private:
	void applyRegister(uint8_t simId, uint8_t physicsSlot, SimConnectionId ownerConn, uint8_t localIndex, const Applied& applied)
	{
		if (m_characters.find(simId) != m_characters.end())
		{
			m_recorder.add(eventOf("server.refused.simIdLive", simId));
			applied(nullptr);
			return;
		}
		ogsimnet::RosterRecord record;
		record.simId       = simId;
		record.physicsSlot = physicsSlot;
		record.owner       = ownerConn == kInvalidSimConnection ? std::nullopt : std::optional<SimConnectionId>(ownerConn);
		record.localIndex  = localIndex;
		const ogsimnet::RosterAddRefusal refusal = m_roster.add(record);
		if (refusal != ogsimnet::RosterAddRefusal::None)
		{
			m_recorder.add(eventOf(refusal == ogsimnet::RosterAddRefusal::Full ? "server.refused.full" : "server.refused.duplicateOwnerSlot",
				simId));
			applied(nullptr);
			return;
		}
		m_characters.insert(simId);
		m_storage.insert(simId);
		m_lifecycleRegistered.insert(simId);
		m_channels[simId] = Channel{ simId };
		m_recorder.add(eventOf("server.registered", simId));
		applied(&m_channels[simId]);
	}

	void applyUnregister(uint8_t simId, ogsimnet::RosterRemoveReason reason, const Applied& applied)
	{
		if (m_characters.find(simId) == m_characters.end() || m_lifecycleRegistered.erase(simId) == 0u)
		{
			m_recorder.add(eventOf("server.unregister.ignored", simId));
			applied(nullptr);
			return;
		}
		m_unregistering(simId);
		m_storage.erase(simId);
		m_recorder.add(eventOf("facade.unregister", simId));
		applied(&m_channels[simId]);
		m_roster.remove(simId, reason);
		m_roster.removeFor(simId);
		m_characters.erase(simId);
	}

	Recorder&                            m_recorder;
	std::function<void(uint8_t)>         m_unregistering;
	std::vector<std::function<void()>>   m_jobs;
	ogsimnet::RosterAuthority            m_roster;
	std::set<uint32_t>                   m_characters;
	std::set<uint32_t>                   m_storage;
	std::set<uint32_t>                   m_lifecycleRegistered;
	std::map<uint32_t, Channel>          m_channels;
};

// One world: the recorder, the bodies, the server (whose host hook the session sets), the runner-stopped flag the adapter
// owns (UESimulationSession::m_runnerStopped).
struct World
{
	Recorder                     recorder;
	FakeBodies                   bodies{ recorder };
	std::function<void(uint8_t)> unregistering;
	FakeServer                   server{ recorder, [this](uint8_t simId) { unregistering(simId); } };
	bool                         runnerStopped = false;

	bool bodiesResolvable(const Simulatable& simulatable)
	{
		Pawn* const pawn = simulatable.pawn;
		if (pawn != nullptr && pawn->unresolvableChecks > 0u)
		{
			--pawn->unresolvableChecks;
			recorder.add(eventOf("bodies.unresolvable", simulatable.tag));
			return false;
		}
		return true;
	}
};

enum class Status : uint8_t
{
	Pending,
	Ready,
	Refused,
};

std::string levelLine(const char* line)
{
	const std::string text(line);
	if (text.rfind("[Error]", 0) == 0)
		return "log Error " + text.substr(7);
	return "log Log " + text;
}

// The authority's lifecycle as the Unreal session ran it at the (a) commit (OGSimulation 221b701), transcribed with each
// Unreal call replaced by a recorder. SL = SessionLifecycle.h's tail at 221b701, UE = UESimulationSession.h at 221b701.
class OldSession
{
public:
	explicit OldSession(World& world)
		: m_world(world)
	{
		world.unregistering = [this](uint8_t simId) { onSimNetLifecycleUnregistering(simId); };
	}

	// UE :1605-1664, the authority's part of tryRegister: the record, then tryRegisterOnRunner.
	Status tryRegister(unsigned int id, Pawn& owner, uint32_t tag)
	{
		auto it = m_pendingRegistrations.find(id);
		if (it == m_pendingRegistrations.end())
		{
			PendingRegistration record;
			record.simulatable.emplace(Simulatable{ tag, &owner });
			record.isAuthority = true;
			it = m_pendingRegistrations.emplace(id, std::move(record)).first;
		}
		if (it->second.isAuthority)
			return tryRegisterOnRunner(id, owner);
		return Status::Pending;
	}

	// UE :1741 (unregisterFromNewFramework's authority call) → SL :832-870.
	void unregister(unsigned int id, Pawn* owner) { requestRunnerUnregister_GameThread(id, owner); }

	// UE :1584, the post-physics collect.
	void collect() { collectLifecycleOutcomes_GameThread(); }

	// SessionNetHooks.h :268-274 at 221b701 (beforePump): the runner requests, then og-sim-net's drain.
	void wake()
	{
		runRunnerRequests_Owner();
		m_world.server.drain();
	}

	std::optional<uint32_t> slotFor(const Pawn& pawn) const
	{
		const auto found = m_authoritativeSlotsByActor.find(pawn.key);
		return found != m_authoritativeSlotsByActor.end() ? std::optional<uint32_t>(found->second) : std::nullopt;
	}

	bool isRegistered(unsigned int id) const { return m_registeredComponentsById.find(id) != m_registeredComponentsById.end(); }
	size_t heldComponents() const { return m_componentsHeldForRunner.size(); }
	size_t pendingRecords() const { return m_pendingRegistrations.size(); }

private:
	enum class Kind : uint8_t { RegisterApplied, RegisterRefused, BodiesBound, BodiesPending, BodiesUnresolvable };

	struct LifecycleOutcome
	{
		unsigned int id   = 0u;
		Kind         kind = Kind::RegisterApplied;
		uint32_t     slot = 0u;
	};

	struct PendingRegistration
	{
		std::optional<Simulatable>   simulatable;
		bool                         bodiesCreated  = false;
		bool                         isAuthority    = false;
		bool                         queued         = false;
		bool                         applied        = false;
		bool                         refused        = false;
		bool                         onRunner       = false;
		bool                         runnerBusy     = false;
		bool                         bodiesBound    = false;
		bool                         registerPosted = false;
		uint32_t                     boundSlot      = 0u;
		uint64_t                     bodyKey        = 0u;
		std::shared_ptr<Simulatable> runnerSimulatable;
	};

	// UE :765-769 (postLifecycleOutcome).
	void postLifecycleOutcome(unsigned int id, Kind kind, uint32_t slot = 0u)
	{
		const std::scoped_lock lock(m_lifecycleBridgeMutex);
		m_lifecycleOutcomes.push_back(LifecycleOutcome{ id, kind, slot });
	}

	// SL :576-580.
	void postRunnerRequest(std::function<void()> request)
	{
		const std::scoped_lock lock(m_lifecycleBridgeMutex);
		m_runnerRequests.push_back(std::move(request));
	}

	// SL :583-592.
	void runRunnerRequests_Owner()
	{
		std::vector<std::function<void()>> requests;
		{
			const std::scoped_lock lock(m_lifecycleBridgeMutex);
			requests.swap(m_runnerRequests);
		}
		for (std::function<void()>& request : requests)
			request();
	}

	// SL :595-601.
	void onSimNetLifecycleUnregistering(uint8_t simId)
	{
		const unsigned int id = simId;
		if (m_systemsNotified_Step.erase(id) != 0u && m_world.server.inStorage(id))
			m_world.recorder.add(eventOf("manager.notifyUnregistered", id));
	}

	// SL :628-642.
	void recordAuthoritativeSlot(Pawn& owner, uint32_t slot)
	{
		m_authoritativeSlotsByActor[owner.key] = slot;
	}

	// SL :656-704, the authority's arms (a client has no runner records).
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
				if (pending == m_pendingRegistrations.end() || !(pending->second.queued || pending->second.onRunner))
					break;
				pending->second.runnerBusy = false;
				if (outcome.kind == Kind::RegisterApplied)
					pending->second.applied = true;
				else
					pending->second.refused = true;
				break;
			}
			case Kind::BodiesBound:
			case Kind::BodiesPending:
			case Kind::BodiesUnresolvable:
			{
				if (outcome.kind != Kind::BodiesUnresolvable && m_componentsHeldForRunner.erase(outcome.id) != 0u)
					m_world.recorder.add(eventOf("held.release", outcome.id));
				const auto pending = m_pendingRegistrations.find(outcome.id);
				if (pending == m_pendingRegistrations.end() || !pending->second.onRunner)
					break;
				pending->second.runnerBusy = false;
				if (outcome.kind == Kind::BodiesBound)
				{
					pending->second.bodiesBound = true;
					pending->second.boundSlot = outcome.slot;
				}
				break;
			}
			}
		}
	}

	// SL :707-799.
	Status tryRegisterOnRunner(unsigned int id, Pawn& owner)
	{
		collectLifecycleOutcomes_GameThread();
		const auto it = m_pendingRegistrations.find(id);
		PendingRegistration& record = it->second;
		record.onRunner = true;
		if (record.refused)
		{
			char line[256];
			std::snprintf(line, sizeof(line), "tryRegister: id=%u refused by the og-sim-net server (its [Roster.Refused] line "
				"names why); the character stays unsimulated", id);
			m_world.recorder.add(std::string("log Error ") + line);
			m_pendingRegistrations.erase(it);
			return Status::Refused;
		}
		if (record.applied)
		{
			m_registeredComponentsById[id] = &owner;
			m_runnerBodyKeysById[id] = record.bodyKey;
			char line[256];
			std::snprintf(line, sizeof(line), "tryRegister: registered simulatable id=%u isAuthority=%d", id, 1);
			m_world.recorder.add(std::string("log Log ") + line);
			m_world.recorder.add(eventOf("game.afterRegistered", id));
			m_world.recorder.add(eventOf("host.onRegisteredHost", id));
			m_pendingRegistrations.erase(it);
			return Status::Ready;
		}
		if (record.runnerBusy || m_world.runnerStopped)
			return Status::Pending;

		if (!record.runnerSimulatable)
		{
			record.runnerSimulatable = std::make_shared<Simulatable>(std::move(*record.simulatable));
			record.simulatable.reset();
		}

		if (!record.bodiesCreated)
		{
			if (!record.bodiesBound)
			{
				ogsession::RegisterRequest bodyRequestFor;
				bodyRequestFor.simId     = id;
				bodyRequestFor.entityKey = ogsession::EntityKey{ reinterpret_cast<std::uintptr_t>(&owner) };
				FakeBodies::Request request = m_world.bodies.makeRequest(bodyRequestFor);
				record.bodyKey = request.key;
				record.runnerBusy = true;
				m_componentsHeldForRunner.insert(id);
				m_world.recorder.add(eventOf("held.hold", id));
				postRunnerRequest([this, id, request, simulatable = record.runnerSimulatable]()
				{
					const std::optional<uint32_t> slot = m_world.bodies.create(request, *simulatable, id)
						? m_world.bodies.slotOf(request.key)
						: std::nullopt;
					postLifecycleOutcome(id, slot.has_value() ? Kind::BodiesBound : Kind::BodiesPending, slot.value_or(0u));
				});
				return Status::Pending;
			}
			recordAuthoritativeSlot(owner, record.boundSlot);
			m_world.recorder.add(eventOf("game.afterBodiesCreated", id));
			record.bodiesCreated = true;
			return Status::Pending;
		}

		const uint8_t localIndex = owner.localIndex;
		const SimConnectionId ownerConn = owner.ownerConnection;
		record.runnerBusy = true;
		record.registerPosted = true;
		postRunnerRequest([this, id, key = record.bodyKey, physicsSlot = static_cast<uint8_t>(record.boundSlot), ownerConn,
			localIndex, simulatable = record.runnerSimulatable]() mutable
		{
			if (!m_world.bodiesResolvable(*simulatable))
			{
				postLifecycleOutcome(id, Kind::BodiesUnresolvable);
				return;
			}
			m_world.server.template registerCharacter<Simulatable>(static_cast<uint8_t>(id), physicsSlot, ownerConn, localIndex,
				std::string("pawn"), std::move(*simulatable),
				[this, id, key](Channel* channel) { onRunnerRegisterApplied_Runner(id, key, channel); });
		});
		return Status::Pending;
	}

	// SL :816-829.
	void onRunnerRegisterApplied_Runner(unsigned int id, uint64_t key, Channel* channel)
	{
		if (channel == nullptr)
		{
			m_world.bodies.release(key, id);
			postLifecycleOutcome(id, Kind::RegisterRefused);
			return;
		}
		m_world.recorder.add(eventOf("manager.notifyRegistered", id));
		m_systemsNotified_Step.insert(id);
		m_world.bodies.onRegistered(key, id);
		postLifecycleOutcome(id, Kind::RegisterApplied);
	}

	// SL :832-870.
	void requestRunnerUnregister_GameThread(unsigned int id, Pawn* owner)
	{
		const bool registered = m_registeredComponentsById.find(id) != m_registeredComponentsById.end();
		const auto pending = m_pendingRegistrations.find(id);
		const bool pendingOnRunner = pending != m_pendingRegistrations.end() && pending->second.onRunner;
		const bool registerPosted = registered || (pendingOnRunner && pending->second.registerPosted);
		const bool bodiesHeld = registered || (pendingOnRunner && (pending->second.bodiesBound || pending->second.runnerBusy));
		const auto knownKey = m_runnerBodyKeysById.find(id);
		const uint64_t key = knownKey != m_runnerBodyKeysById.end() ? knownKey->second
			: pendingOnRunner ? pending->second.bodyKey : (owner != nullptr ? owner->key : 0u);
		if (pending != m_pendingRegistrations.end())
			m_pendingRegistrations.erase(pending);
		if (knownKey != m_runnerBodyKeysById.end())
			m_runnerBodyKeysById.erase(knownKey);

		if (!m_world.runnerStopped && bodiesHeld)
		{
			postRunnerRequest([this, id, key, registerPosted]()
			{
				if (!registerPosted)
				{
					releaseOnRunner_Runner(id, key);
					return;
				}
				m_world.server.template unregisterCharacter<Simulatable>(static_cast<uint8_t>(id),
					ogsimnet::RosterRemoveReason::Left, [this, id, key](Channel*) { releaseOnRunner_Runner(id, key); });
			});
		}

		m_registeredComponentsById.erase(id);
		m_authoritativeSlotsByActor.erase(owner != nullptr ? owner->key : 0u);
		m_world.recorder.add(eventOf("game.onUnregistered", id));
		m_world.recorder.add(eventOf("latency.forget", id));
		if (registered)
			m_world.recorder.add(eventOf("host.onUnregisteredHost", id));
		char line[96];
		std::snprintf(line, sizeof(line), "NewFramework: unregistered simulatable id=%u", id);
		m_world.recorder.add(std::string("log Log ") + line);
	}

	// SL :873-877.
	void releaseOnRunner_Runner(unsigned int id, uint64_t key)
	{
		m_systemsNotified_Step.erase(id);
		m_world.bodies.release(key, id);
	}

	World&                                             m_world;
	std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;
	std::unordered_map<unsigned int, uint64_t>         m_runnerBodyKeysById;
	std::unordered_map<uint64_t, uint32_t>             m_authoritativeSlotsByActor;
	std::unordered_map<unsigned int, Pawn*>            m_registeredComponentsById;
	std::unordered_set<unsigned int>                   m_componentsHeldForRunner;
	std::mutex                                         m_lifecycleBridgeMutex;
	std::vector<LifecycleOutcome>                      m_lifecycleOutcomes;
	std::vector<std::function<void()>>                 m_runnerRequests;
	std::unordered_set<unsigned int>                   m_systemsNotified_Step;
};

// The port the Unreal session's AuthorityLifecyclePort is (UESimulationSession.h), over the fakes.
struct AuthorityPort
{
	using Bodies      = FakeBodies;
	using Server      = FakeServer;
	using Channel     = sessionLifecycleAuthorityTest::Channel;
	using Simulatable = sessionLifecycleAuthorityTest::Simulatable;

	World* world = nullptr;

	Server*  server() { return &world->server; }
	Bodies&  bodies() { return world->bodies; }
	bool     bodiesResolvable(const Simulatable& simulatable) const { return world->bodiesResolvable(simulatable); }
	uint64_t bodyKeyOf(ogsession::EntityKey key) const
	{
		const Pawn* const pawn = reinterpret_cast<const Pawn*>(key.value);
		return pawn != nullptr ? pawn->key : 0u;
	}
	bool runnerStopped() const { return world->runnerStopped; }
	void notifyRegistered(uint32_t id) { world->recorder.add(eventOf("manager.notifyRegistered", id)); }
	void notifyUnregistered(uint32_t id) { world->recorder.add(eventOf("manager.notifyUnregistered", id)); }
	bool inStorage(uint32_t id) const { return world->server.inStorage(id); }
	void logRouted(const char* line) { world->recorder.add(levelLine(line)); }
};

static_assert(ogsession::SessionAuthorityLifecyclePort<AuthorityPort>,
	"The test's port models SessionAuthorityLifecyclePort, as the Unreal session's AuthorityLifecyclePort does");

struct NoLanes
{
	struct Client
	{
	};

	Client* client() { return nullptr; }
	bool    clientSessionBound() const { return false; }
	void    drainClient_Step() {}
};

using Lifecycle = ogsession::SessionAuthorityLifecycle<AuthorityPort>;

struct NetPort
{
	struct Idle
	{
	};

	using Lanes       = NoLanes;
	using Server      = FakeServer;
	using Simulatable = sessionLifecycleAuthorityTest::Simulatable;

	World*     world     = nullptr;
	Lifecycle* lifecycle = nullptr;

	Lanes*  lanes() const { return nullptr; }
	Server* server() { return &world->server; }
	Idle*   idle() { return nullptr; }
	void    runRunnerRequests() { lifecycle->applyRequests(); }
	void    applyUnregisterRequests(NoLanes::Client&) {}
	void    publishPresentation() {}
	double  nowSeconds() const { return 0.0; }
	void    logRouted(const char*) {}
};

struct AuthorityManager
{
	bool runsPrediction() const { return false; }
};

struct NoStamps
{
	void stampCorrectionHanded_Internal(unsigned int, uint32_t, double) {}
};

// The Unreal session's authority half after task 30 (UESimulationSession.h: tryRegister's authority branch,
// tryRegisterOnRunner, collectLifecycleOutcomes_GameThread, onAuthorityOutcome_GameThread, requestRunnerUnregister_GameThread,
// onSimNetLifecycleUnregistering), over the real SessionAuthorityLifecycle, its runner wake through the real
// SessionNetHooks::beforePump.
class NewSession
{
public:
	using NetHooks = ogsession::SessionNetHooks<NetPort, AuthorityManager, int, int, NoStamps>;

	NewSession(World& world, const ogsession::SessionThreadChecks& threads)
		: m_world(world)
		, m_lifecycle(threads, AuthorityPort{ &world })
		, m_netHooks(threads, NetPort{ &world, &m_lifecycle }, m_manager, m_netSync, m_storage, m_stamps)
	{
		world.unregistering = [this](uint8_t simId) { m_lifecycle.onUnregistering(simId); };
	}

	Status tryRegister(unsigned int id, Pawn& owner, uint32_t tag)
	{
		collect();
		struct Hooks
		{
			NewSession*  session;
			unsigned int id;

			void bodiesRequested()
			{
				session->m_componentsHeldForRunner.insert(id);
				session->m_world.recorder.add(eventOf("held.hold", id));
			}

			void bodiesCreated(Simulatable&) { session->m_world.recorder.add(eventOf("game.afterBodiesCreated", id)); }
		};
		Hooks hooks{ this, id };
		ogsession::RegisterRequest request;
		request.simId           = id;
		request.entityKey       = ogsession::EntityKey{ reinterpret_cast<std::uintptr_t>(&owner) };
		request.debugName       = "pawn";
		request.ownerConnection = owner.ownerConnection;
		request.localIndex      = owner.localIndex;
		const ogsession::RegisterStatus status = m_lifecycle.tryRegister(request, Simulatable{ tag, &owner }, hooks);
		if (status == ogsession::RegisterStatus::Pending)
			return Status::Pending;
		if (status == ogsession::RegisterStatus::Refused)
			return Status::Refused;
		m_registeredComponentsById[id] = &owner;
		m_world.recorder.add(eventOf("game.afterRegistered", id));
		m_world.recorder.add(eventOf("host.onRegisteredHost", id));
		return Status::Ready;
	}

	void unregister(unsigned int id, Pawn* owner)
	{
		const bool registered = m_lifecycle.requestUnregister(id,
			owner != nullptr ? ogsession::EntityKey{ reinterpret_cast<std::uintptr_t>(owner) } : ogsession::EntityKey{});
		m_registeredComponentsById.erase(id);
		m_world.recorder.add(eventOf("game.onUnregistered", id));
		m_world.recorder.add(eventOf("latency.forget", id));
		if (registered)
			m_world.recorder.add(eventOf("host.onUnregisteredHost", id));
		char line[96];
		std::snprintf(line, sizeof(line), "NewFramework: unregistered simulatable id=%u", id);
		m_world.recorder.add(std::string("log Log ") + line);
	}

	void collect()
	{
		m_lifecycle.pollOutcomes([this](const ogsession::LifecycleOutcome& outcome)
		{
			uint32_t id = 0u;
			if (const auto* bound = std::get_if<ogsession::lifecycleOutcome::BodiesBound>(&outcome))
				id = bound->simId;
			else if (const auto* pending = std::get_if<ogsession::lifecycleOutcome::BodiesPending>(&outcome))
				id = pending->simId;
			else
				return;
			if (m_componentsHeldForRunner.erase(id) != 0u)
				m_world.recorder.add(eventOf("held.release", id));
		});
	}

	void wake() { m_netHooks.beforePump(); }

	std::optional<uint32_t> slotFor(const Pawn& pawn) const { return m_lifecycle.authoritativeSlotFor(pawn.key); }
	bool isRegistered(unsigned int id) const { return m_registeredComponentsById.find(id) != m_registeredComponentsById.end(); }
	size_t heldComponents() const { return m_componentsHeldForRunner.size(); }
	Lifecycle& lifecycle() { return m_lifecycle; }

private:
	World&                                  m_world;
	Lifecycle                               m_lifecycle;
	AuthorityManager                        m_manager;
	int                                     m_netSync = 0;
	int                                     m_storage = 0;
	NoStamps                                m_stamps;
	NetHooks                                m_netHooks;
	std::unordered_map<unsigned int, Pawn*> m_registeredComponentsById;
	std::unordered_set<unsigned int>        m_componentsHeldForRunner;
};

// The authority's runner: a real thread that configures the authority's thread context (the owner thread, free-running, as
// SimulationTickDriver's runAuthorityLoop does on FUEAuthorityRunner's thread) and runs one wake at a time when the frame
// thread asks; wake() returns when the runner is idle again. stop() joins it, as the session's endPlay joins its runner.
class RunnerThread
{
public:
	RunnerThread()
		: m_thread([this] { loop(); })
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this] { return m_configured; });
	}

	~RunnerThread() { stop(); }

	RunnerThread(const RunnerThread&) = delete;
	RunnerThread& operator=(const RunnerThread&) = delete;

	const TickDriverThreadContext& context() const { return m_context; }

	void run(std::function<void()> job)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_job  = std::move(job);
		m_busy = true;
		m_cv.notify_all();
		m_cv.wait(lock, [this] { return !m_busy; });
	}

	void stop()
	{
		{
			const std::scoped_lock lock(m_mutex);
			if (m_stopped)
				return;
			m_stop = true;
		}
		m_cv.notify_all();
		m_thread.join();
		m_stopped = true;
	}

	std::thread::id id() const { return m_runnerId; }

private:
	void loop()
	{
		m_context.configure<true>(tickDriverThread::kAuthorityRoleBit, StepThreading::FreeRunning);
		std::unique_lock<std::mutex> lock(m_mutex);
		m_runnerId   = std::this_thread::get_id();
		m_configured = true;
		m_cv.notify_all();
		while (true)
		{
			m_cv.wait(lock, [this] { return m_busy || m_stop; });
			if (!m_busy)
				return;
			std::function<void()> job = std::move(m_job);
			lock.unlock();
			job();
			lock.lock();
			m_busy = false;
			m_cv.notify_all();
		}
	}

	TickDriverThreadContext m_context;
	std::mutex              m_mutex;
	std::condition_variable m_cv;
	std::function<void()>   m_job;
	bool                    m_busy       = false;
	bool                    m_stop       = false;
	bool                    m_stopped    = false;
	bool                    m_configured = false;
	std::thread::id         m_runnerId;
	std::thread             m_thread;
};

template <typename SessionT>
struct Rig
{
	RunnerThread                   runner;
	World                          world;
	ogsession::SessionThreadChecks threads{ ogsession::SessionRole::Authority, &runner.context(), std::this_thread::get_id() };
	SessionT                       session = makeSession();

	SessionT makeSession();

	void wake()
	{
		if (world.runnerStopped)
			return;
		world.recorder.add("--- wake");
		runner.run([this] { session.wake(); });
	}

	Status tryRegister(unsigned int id, Pawn& pawn) { return session.tryRegister(id, pawn, id); }

	void unregister(unsigned int id, Pawn* pawn)
	{
		world.recorder.add(eventOf("--- unregister", id));
		session.unregister(id, pawn);
	}

	void collect()
	{
		world.recorder.add("--- collect");
		session.collect();
	}

	void stopRunner()
	{
		runner.stop();
		world.runnerStopped = true;
		world.recorder.add("--- runner stopped");
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

// Runs one register to its end: poll, wake, poll, ... (the component polls tryRegister each frame and the runner wakes
// between frames), at most maxFrames frames.
template <typename RigT>
Status registerToEnd(RigT& rig, unsigned int id, Pawn& pawn, int maxFrames = 12)
{
	for (int frame = 0; frame < maxFrames; ++frame)
	{
		rig.world.recorder.add(eventOf("--- frame register", id));
		const Status status = rig.tryRegister(id, pawn);
		if (status != Status::Pending)
			return status;
		rig.wake();
	}
	return Status::Pending;
}

template <typename ScriptT>
std::pair<Events, Events> runBoth(ScriptT&& script)
{
	Rig<OldSession> oldRig;
	script(oldRig);
	Rig<NewSession> newRig;
	script(newRig);
	return { oldRig.world.recorder.take(), newRig.world.recorder.take() };
}

void requireEqual(const Events& expected, const Events& actual)
{
	const size_t common = std::min(expected.size(), actual.size());
	for (size_t index = 0u; index < common; ++index)
	{
		INFO("event " << index);
		REQUIRE(expected[index] == actual[index]);
	}
	REQUIRE(expected.size() == actual.size());
}

size_t countOf(const Events& events, const std::string& prefix)
{
	return static_cast<size_t>(std::count_if(events.begin(), events.end(),
		[&prefix](const std::string& event) { return event.rfind(prefix, 0) == 0; }));
}

Pawn pawnAt(uint64_t key, SimConnectionId owner, uint8_t localIndex)
{
	Pawn pawn;
	pawn.key             = key;
	pawn.ownerConnection = owner;
	pawn.localIndex      = localIndex;
	return pawn;
}
} // namespace sessionLifecycleAuthorityTest

using namespace sessionLifecycleAuthorityTest;

TEST_CASE("SessionLifecycleAuthority.TheFullRegisterSequenceOnARealRunner", "[Session][SessionLifecycle]")
{
	Pawn pawn = pawnAt(0x1000u, 7u, 0u);
	Status oldStatus = Status::Pending;
	Status newStatus = Status::Pending;
	std::optional<uint32_t> oldSlot;
	std::optional<uint32_t> newSlot;
	const auto [expected, actual] = runBoth([&](auto& rig)
	{
		const Status status = registerToEnd(rig, 1u, pawn);
		if constexpr (std::is_same_v<std::remove_cvref_t<decltype(rig.session)>, NewSession>)
		{
			newStatus = status;
			newSlot   = rig.session.slotFor(pawn);
			CHECK(rig.session.isRegistered(1u));
			CHECK(rig.session.heldComponents() == 0u);
		}
		else
		{
			oldStatus = status;
			oldSlot   = rig.session.slotFor(pawn);
		}
	});
	CHECK(oldStatus == Status::Ready);
	CHECK(newStatus == Status::Ready);
	CHECK(oldSlot == std::optional<uint32_t>(0u));
	CHECK(newSlot == oldSlot);
	requireEqual(expected, actual);

	// The sequence, in the order of the Description: request (frame) → create and bind (runner) → BodiesBound → the game's
	// afterBodiesCreated (frame) → register posted → resolvable, registerCharacter, applied in the drain: the systems told,
	// onRegistered → Registered → afterRegistered (frame).
	const Events sequence = { "bodies.makeRequest 1 4096", "held.hold 1", "bodies.create 1 0", "held.release 1",
		"game.afterBodiesCreated 1", "server.registerCharacter 1 0", "server.registered 1", "manager.notifyRegistered 1",
		"bodies.onRegistered 1", "log Log tryRegister: registered simulatable id=1 isAuthority=1", "game.afterRegistered 1",
		"host.onRegisteredHost 1" };
	Events observed;
	for (const std::string& event : actual)
	{
		if (std::find(sequence.begin(), sequence.end(), event) != sequence.end())
			observed.push_back(event);
	}
	CHECK(observed == sequence);
}

TEST_CASE("SessionLifecycleAuthority.ARosterRefusalReleasesTheBodies", "[Session][SessionLifecycle]")
{
	SECTION("the roster is full (og-input-latency P5-23's capacity case)")
	{
		Pawn refused = pawnAt(0x2000u, 9u, 0u);
		const auto [expected, actual] = runBoth([&](auto& rig)
		{
			for (uint8_t other = 100u; other < 100u + ogsimnet::kRosterMaxEntries; ++other)
				rig.world.server.occupyRoster(other, std::nullopt, 0u);
			CHECK(registerToEnd(rig, 1u, refused) == Status::Refused);
			CHECK_FALSE(rig.world.bodies.holds(refused.key));

			// The refused pawn ends play (its unregistration forgets its recorded slot); room again, and a pawn at the same
			// address (the same bodies' key) registers under the next id: its key finds no slot taken.
			rig.unregister(1u, &refused);
			rig.world.server.vacateRoster(100u);
			CHECK(registerToEnd(rig, 2u, refused) == Status::Ready);
		});
		CHECK(countOf(actual, "bodies.taken") == 0u);
		CHECK(countOf(actual, "server.refused.full 1") == 1u);
		CHECK(countOf(actual, "bodies.release 1") == 1u);
		CHECK(countOf(actual, "log Error tryRegister: id=1 refused by the og-sim-net server") == 1u);
		requireEqual(expected, actual);
	}

	SECTION("a second character on the same owner slot (og-input-latency P5-23's duplicate case)")
	{
		Pawn first = pawnAt(0x3000u, 5u, 0u);
		Pawn duplicate = pawnAt(0x3100u, 5u, 0u);
		const auto [expected, actual] = runBoth([&](auto& rig)
		{
			CHECK(registerToEnd(rig, 1u, first) == Status::Ready);
			CHECK(registerToEnd(rig, 2u, duplicate) == Status::Refused);
			CHECK_FALSE(rig.world.bodies.holds(duplicate.key));

			// The refused pawn ends play; the first leaves, freeing the owner slot; a pawn at the refused one's address (the same
			// bodies' key) registers under the next id: its key finds no slot taken.
			rig.unregister(2u, &duplicate);
			rig.unregister(1u, &first);
			rig.wake();
			CHECK(registerToEnd(rig, 3u, duplicate) == Status::Ready);
		});
		CHECK(countOf(actual, "bodies.taken") == 0u);
		CHECK(countOf(actual, "server.refused.duplicateOwnerSlot 2") == 1u);
		CHECK(countOf(actual, "bodies.release 2") == 1u);
		requireEqual(expected, actual);
	}
}

TEST_CASE("SessionLifecycleAuthority.AnUnregisterBeforeTheRegisterWasPosted", "[Session][SessionLifecycle]")
{
	Pawn pawn = pawnAt(0x4000u, 3u, 0u);
	const auto [expected, actual] = runBoth([&](auto& rig)
	{
		CHECK(rig.tryRegister(1u, pawn) == Status::Pending);
		rig.unregister(1u, &pawn);
		rig.wake();
		rig.collect();
		CHECK(rig.world.bodies.held() == 0u);
		CHECK(rig.world.server.storageSize() == 0u);
		CHECK(rig.session.heldComponents() == 0u);
		CHECK_FALSE(rig.session.isRegistered(1u));
	});
	requireEqual(expected, actual);
	CHECK(countOf(actual, "server.registerCharacter") == 0u);
	CHECK(countOf(actual, "server.unregisterCharacter") == 0u);
	CHECK(countOf(actual, "bodies.release 1") == 1u);
}

TEST_CASE("SessionLifecycleAuthority.AnUnregisterAfterTheRunnerStopped", "[Session][SessionLifecycle]")
{
	Pawn pawn = pawnAt(0x5000u, 4u, 0u);
	const auto [expected, actual] = runBoth([&](auto& rig)
	{
		CHECK(registerToEnd(rig, 1u, pawn) == Status::Ready);
		rig.stopRunner();
		rig.unregister(1u, &pawn);
		CHECK_FALSE(rig.session.isRegistered(1u));
		CHECK(rig.world.bodies.holds(pawn.key));
		CHECK(rig.world.server.inStorage(1u));
		CHECK_FALSE(rig.session.slotFor(pawn).has_value());
	});
	requireEqual(expected, actual);
	const auto stopped = std::find(actual.begin(), actual.end(), std::string("--- runner stopped"));
	REQUIRE(stopped != actual.end());
	const Events after(stopped + 1, actual.end());
	const Events afterExpected = { "--- unregister 1", "game.onUnregistered 1", "latency.forget 1", "host.onUnregisteredHost 1",
		"log Log NewFramework: unregistered simulatable id=1" };
	CHECK(after == afterExpected);
}

TEST_CASE("SessionLifecycleAuthority.ThreeRegistersInOneWakeKeepTheirOrder", "[Session][SessionLifecycle]")
{
	Pawn pawns[3] = { pawnAt(0x6000u, 1u, 0u), pawnAt(0x6100u, 2u, 0u), pawnAt(0x6200u, 3u, 0u) };
	const auto [expected, actual] = runBoth([&](auto& rig)
	{
		for (int frame = 0; frame < 3; ++frame)
		{
			for (unsigned int index = 0u; index < 3u; ++index)
				CHECK(rig.tryRegister(index + 1u, pawns[index]) == Status::Pending);
			rig.wake();
		}
		for (unsigned int index = 0u; index < 3u; ++index)
			CHECK(rig.tryRegister(index + 1u, pawns[index]) == Status::Ready);
	});
	requireEqual(expected, actual);

	// The third wake runs the three register requests in their order, then og-sim-net's drain applies them in the same wake.
	const auto third = std::find_if(actual.begin(), actual.end(), [count = 0](const std::string& event) mutable
	{
		return event == "--- wake" && ++count == 3;
	});
	REQUIRE(third != actual.end());
	const Events thirdWake(third + 1, third + 9);
	const Events order = { "server.registerCharacter 1 0", "server.registerCharacter 2 1", "server.registerCharacter 3 2",
		"server.drain", "server.registered 1", "manager.notifyRegistered 1", "bodies.onRegistered 1", "server.registered 2" };
	CHECK(thirdWake == order);
}

TEST_CASE("SessionLifecycleAuthority.FiveHundredScriptsMatchTheTranscription", "[Session][SessionLifecycle]")
{
	size_t ready = 0u;
	size_t refused = 0u;
	size_t releases = 0u;
	size_t notifies = 0u;
	for (uint32_t seed = 1u; seed <= 500u; ++seed)
	{
		Pawn pawns[6];
		for (unsigned int index = 0u; index < 6u; ++index)
			pawns[index] = pawnAt(0x10000u + index * 0x100u, 1u + index % 4u, 0u);
		auto script = [&](auto& rig)
		{
			std::mt19937 random(seed);
			unsigned int nextId = 1u;
			unsigned int idOf[6] = {};
			bool readyOf[6] = {};
			for (int action = 0; action < 60; ++action)
			{
				const unsigned int index = random() % 6u;
				const unsigned int roll = random() % 10u;
				if (roll < 4u)
				{
					if (readyOf[index])
						continue;
					if (idOf[index] == 0u)
					{
						if (nextId > 250u)
							continue;
						idOf[index] = nextId++;
						pawns[index].key = 0x10000u + index * 0x100u + idOf[index];
						pawns[index].unresolvableChecks = (random() % 5u == 0u) ? 1u : 0u;
					}
					const Status status = rig.tryRegister(idOf[index], pawns[index]);
					if (status == Status::Refused)
						idOf[index] = 0u;
					readyOf[index] = status == Status::Ready;
				}
				else if (roll < 6u)
				{
					if (idOf[index] != 0u)
					{
						rig.unregister(idOf[index], &pawns[index]);
						idOf[index] = 0u;
						readyOf[index] = false;
					}
				}
				else if (roll < 9u)
				{
					rig.wake();
				}
				else
				{
					rig.collect();
				}
				if (action == 50 && seed % 4u == 0u)
					rig.stopRunner();
			}
		};
		const auto [expected, actual] = runBoth(script);
		INFO("seed " << seed);
		requireEqual(expected, actual);
		ready += countOf(actual, "log Log tryRegister: registered");
		refused += countOf(actual, "log Error tryRegister:");
		releases += countOf(actual, "bodies.release");
		notifies += countOf(actual, "manager.notifyRegistered");
	}
	std::printf("[SessionLifecycleAuthoritySurvey] seeds=500 ready=%zu refused=%zu releases=%zu notifyRegistered=%zu\n", ready, refused,
		releases, notifies);
	CHECK(ready > 500u);
	CHECK(refused > 0u);
	CHECK(releases > 100u);
}

TEST_CASE("SessionLifecycleAuthority.ApplyRequestsOnTheFrameThreadTripsTheCheck", "[.][SessionLifecycleDeath]")
{
	Rig<NewSession> rig;
	rig.session.lifecycle().applyRequests();
	FAIL("SessionAuthorityLifecycle::applyRequests on the frame thread did NOT trip its ApplyLifecycleRequests check");
}

TEST_CASE("SessionLifecycleAuthority.TheUnregisteringHookOnTheFrameThreadTripsTheCheck", "[.][SessionLifecycleDeath]")
{
	Rig<NewSession> rig;
	rig.session.lifecycle().onUnregistering(1u);
	FAIL("SessionAuthorityLifecycle::onUnregistering on the frame thread did NOT trip its DrainInbox check");
}
