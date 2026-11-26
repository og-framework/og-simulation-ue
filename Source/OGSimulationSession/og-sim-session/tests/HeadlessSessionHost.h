#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../OGSimSession/docs/HeadlessSessionHost-rationale.md

#include <cstdint>
#include <map>
#include <set>
#include <string>

#include "HeadlessPlatform.h"
#include "HeadlessSessionLink.h"
#include "OGSimNet/SimNetIdle.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimSession/SessionRuntime.h"

namespace headlessSession
{
template <typename InputT>
struct Diagnostics
{
	using InputProvider = std::function<InputT(const SimulationTimeStep&, const LocalInputCache<InputT>&)>;
	void stampAfterStep_Internal(double) {}
	void stampStepEnd_Internal() {}
	void stampCorrectionHanded_Internal(unsigned int, uint32_t, double) {}
	void noteWireSample_Internal(double) {}
	void stampHandedAndLeft_Internal(uint32_t, uint32_t) {}
};

template <typename GameTraitsT>
struct RuntimeTraits : GameTraitsT
{
	using Host        = Peer<GameTraitsT>;
	using Engine      = headlessSession::Engine;
	using ClockEngine = headlessSession::ClockEngine;
	using Link        = Lanes<GameTraitsT>;
	using Diagnostics = headlessSession::Diagnostics<typename GameTraitsT::PlayerInput>;
	using Server      = ogsimnet::SimNetServer<typename GameTraitsT::Composition, typename Link::ServerHost, typename Link::Transport>;
	using Idle        = ogsimnet::SimNetIdle<Endpoint>;
};

template <typename GameTraitsT>
class Peer
{
public:
	using RuntimeType   = Runtime<GameTraitsT>;
	using Simulatable   = typename GameTraitsT::Simulatable;
	using InputProvider = typename Diagnostics<typename GameTraitsT::PlayerInput>::InputProvider;

	struct Entity
	{
		uint32_t        simId = 0u;
		SimConnectionId owner = kInvalidSimConnection;
		bool            local = false;
		Simulatable     prototype;
		InputProvider   provider;
	};

	Peer(ognet::test::VirtualNetwork& network, uint8_t hostIndex, const ognet::EndpointConfig& config)
		: m_lanes(config.role == ognet::Role::Server)
		, m_endpoint(m_netThreads, network.addHost(hostIndex), config)
	{
		static_assert(__builtin_offsetof(Peer, m_lanes) < __builtin_offsetof(Peer, m_endpoint)
				&& __builtin_offsetof(Peer, m_endpoint) < __builtin_offsetof(Peer, m_runtime),
			"Peer members destruct in REVERSE declaration order. og-net's endpoint calls the link's transport until it is "
			"destroyed and the runtime holds the link, so the runtime goes first, then the endpoint, then the lanes. Keep "
			"m_lanes < m_endpoint < m_runtime. Was SimNetLink's 'an engine destroys the link only after og-net's endpoint'");
	}

	RuntimeType&    runtime() { return m_runtime; }
	Endpoint&       endpoint() { return m_endpoint; }
	Lanes<GameTraitsT>& lanes() { return m_lanes; }
	auto*           driver() { return m_driver; }
	bool            isAuthority() const { return m_endpoint.config().role == ognet::Role::Server; }
	std::thread::id netIoThread() const { return m_netThreads.started(); }
	bool            registered(uint32_t simId) const { return m_registered.count(simId) != 0u; }
	bool            known(uint32_t simId) const { return registered(simId) || m_pending.count(simId) != 0u || m_gone.count(simId) != 0u; }
	uint32_t        refusals() const { return m_refusals; }
	void            addEntity(Entity entity) { m_pending.insert_or_assign(entity.simId, std::move(entity)); }
	bool            stopSession() { return m_runtime.stop(); }

	void open(std::optional<ognet::Address> server)
	{
		m_lanes.beforeStart(m_endpoint);
		OG_CHECK(m_endpoint.net().start(m_endpoint.config()), "headlessSession::Peer::open: og-net's endpoint did not start");
		if (server.has_value())
			m_endpoint.net().startOwnThread();
		m_lanes.markStarted();
		if (server.has_value())
			m_endpoint.net().connect(*server);
	}

	template <typename ConfigT>
	void startSession(const ConfigT& config, StaticWorldDescription staticWorld)
	{
		ogsession::SessionStart start;
		start.role = isAuthority() ? ogsession::SessionRole::Authority : ogsession::SessionRole::Prediction;
		start.stepSeconds = static_cast<double>(1.f / static_cast<float>(GameTraitsT::NetConfig::tickFrequencyHz));
		start.requestedMaxCatchUpSteps = 60;
		start.authorityIdleSlackSeconds = isAuthority() ? 0.0005 : 0.0;
		start.highResolutionWait = isAuthority() && m_endpoint.hasHighResolutionWait();
		start.physicsOnlyShapes.assign(staticWorld.shapes.size(), 0u);
		start.staticWorld = std::move(staticWorld);
		start.netIoThread = isAuthority() ? std::thread::id{} : netIoThread();
		OG_CHECK(isAuthority() || start.netIoThread != std::thread::id{},
			"headlessSession::Peer::startSession: a client's og-net I/O thread runs before its session starts, so the "
			"session's thread table holds its id (review_SR5_31 lead item 2)");
		m_runtime.start(std::move(start), config, &m_lanes);
	}

	void frame(double frameDeltaSeconds)
	{
		m_engine.advanceFrame();
		m_lanes.afterDispatch();
		pumpRegistrations();
		if (typename RuntimeType::Driver* const driver = m_driver)
		{
			driver->startFrame(frameDeltaSeconds);
			driver->endFrame(nullptr, 0);
		}
		m_lanes.flushFrame();
	}

	void close()
	{
		m_lanes.beforeStop();
		m_endpoint.net().stop();
	}

	void removeEntity(uint32_t simId)
	{
		OG_CHECK(isAuthority(), "headlessSession::Peer::removeEntity: a client's characters leave by the roster or the teardown");
		m_pending.erase(simId);
		if (m_runtime.getManager().has_value() && m_gone.erase(simId) == 0u)
			requestRunnerUnregister(simId);
	}

	void onConfigure() {}
	void bindDiagnosticGates() {}
	void afterPredictionConfigured() {}
	void onSessionConfigured() {}
	void sampleInput() {}
	void endFrames() { m_driver = nullptr; }
	void logHost(const char* line) { logLine(line); }
	void forgetComponents() { m_registered.clear(), m_gone.clear(); }
	uint64_t bodyKeyOf(ogsession::EntityKey key) const { return key.value; }
	InputProvider wrapInputProvider(const ogsession::RegisterRequest&, InputProvider&& provider) { return std::move(provider); }

	template <typename FunctionT>
	void forEachRegisteredId(FunctionT&& function) const
	{
		for (const uint32_t simId : m_registered)
			function(simId);
	}

	void beginFrames(typename RuntimeType::Driver& driver)
	{
		OG_CHECK(isAuthority() || m_lanes.clientSessionBound(),
			"headlessSession::Peer::beginFrames: a client's link is attached before its first frame dispatches a step: the "
			"roster seed and the bound flag reach the steps by that order alone (SimNetLink-rationale section 5; task 32)");
		m_driver = &driver;
	}

	void onPostPhysics()
	{
		m_runtime.present(PresentControls{}, m_renderSink);
		collectOutcomes();
		if (isAuthority())
			m_runtime.presentAuthority([](unsigned int, const auto&) {});
		else
			m_runtime.refreshDisplayView();
	}

	ogsession::SessionBodyRequest<typename GameTraitsT::StaticData> makeBodyRequest(const ogsession::RegisterRequest& request)
	{
		const uint64_t key = bodyKeyOf(request.entityKey);
		return { key, request.debugName, isAuthority(), m_runtime.physicsSlotFor(key, request.simId), &m_runtime.editStaticData() };
	}

	void teardownComponents()
	{
		auto* const lifecycle = m_runtime.lifecycle();
		if (isAuthority() || lifecycle == nullptr || !lifecycle->teardown([this](const auto& outcome) { onOutcome(outcome); }))
			return;
		for (const uint32_t simId : std::set<uint32_t>(m_registered))
			unregisterHostOnly(simId);
		m_runtime.forgetQueuedRegistrations();
	}

	void onLinkStopping(bool isServer)
	{
		if (!isServer)
			teardownComponents();
		for (const uint32_t simId : std::set<uint32_t>(m_registered))
		{
			requestRunnerUnregister(simId);
			m_gone.insert(simId);
		}
	}

private:
	struct RegisterHooks
	{
		Peer*         peer;
		const Entity* entity;
		void bodiesRequested() {}
		void bodiesCreated(Simulatable&) {}
		void collectOutcomes() { peer->collectOutcomes(); }
		bool createBodies(Simulatable& simulatable)
		{
			return peer->m_runtime.acquireAndBind(peer->makeBodyRequest(requestFor(*entity)), [](uint32_t) {}, entity->simId, simulatable);
		}
	};

	static ogsession::EntityKey keyOf(uint32_t simId) { return ogsession::EntityKey{ 0x1000u + simId }; }

	static ogsession::RegisterRequest requestFor(const Entity& entity)
	{
		ogsession::RegisterRequest request;
		request.simId = entity.simId;
		request.entityKey = keyOf(entity.simId);
		request.debugName = "headless" + std::to_string(entity.simId);
		request.ownerConnection = entity.owner;
		request.engineSaysLocal = entity.local;
		return request;
	}

	ogsession::RegisterStatus tryRegister(const Entity& entity)
	{
		RegisterHooks hooks{ this, &entity };
		if (auto* const authority = m_runtime.authorityLifecycle())
			return collectOutcomes(), authority->tryRegister(requestFor(entity), Simulatable(entity.prototype), hooks);
		if (isAuthority() || m_runtime.bindStep(entity.simId, entity.local) != ogsimnet::BindStep::Ready)
			return ogsession::RegisterStatus::Pending;
		return m_runtime.tryRegisterPrediction(requestFor(entity), Simulatable(entity.prototype), InputProvider(entity.provider), hooks);
	}

	void pumpRegistrations()
	{
		for (auto it = m_pending.begin(); m_runtime.getManager().has_value() && it != m_pending.end();)
		{
			const ogsession::RegisterStatus status = tryRegister(it->second);
			if (status == ogsession::RegisterStatus::Pending)
			{
				++it;
				continue;
			}
			if (status == ogsession::RegisterStatus::Refused)
				++m_refusals;
			else if (m_registered.insert(it->first).second && !isAuthority())
				m_runtime.bodiesRegistered(it->first, keyOf(it->first).value);
			it = m_pending.erase(it);
		}
	}

	void collectOutcomes()
	{
		if (auto* const lifecycle = m_runtime.lifecycle())
			lifecycle->pollOutcomes([this](const ogsession::LifecycleOutcome& outcome) { onOutcome(outcome); });
		else if (auto* const authority = m_runtime.authorityLifecycle())
			authority->pollOutcomes([](const ogsession::LifecycleOutcome&) {});
	}

	void onOutcome(const ogsession::LifecycleOutcome& outcome)
	{
		const auto* const removed = std::get_if<ogsession::lifecycleOutcome::RosterRemoved>(&outcome);
		if (removed != nullptr && registered(removed->simId))
			unregisterHostOnly(removed->simId);
	}

	void unregisterHostOnly(uint32_t simId)
	{
		m_registered.erase(simId);
		m_gone.insert(simId);
		m_runtime.markUnregistered(simId);
		m_runtime.forgetBodies(simId, keyOf(simId).value, /*releaseSlot=*/false);
		m_runtime.releaseBodySlot(simId, keyOf(simId).value);
	}

	void requestRunnerUnregister(uint32_t simId)
	{
		if (auto* const authority = m_runtime.authorityLifecycle())
			authority->requestUnregister(simId, keyOf(simId));
		m_registered.erase(simId);
	}

	PeerThreads                                    m_netThreads;
	Lanes<GameTraitsT>                             m_lanes;
	Endpoint                                       m_endpoint;
	Engine                                         m_engine;
	Diagnostics<typename GameTraitsT::PlayerInput> m_diagnostics;
	NullRenderSink                                 m_renderSink;
	RuntimeType                                    m_runtime{ *this, m_diagnostics, m_engine };
	typename RuntimeType::Driver*                  m_driver = nullptr;
	std::map<uint32_t, Entity>                     m_pending;
	std::set<uint32_t>                             m_registered;
	std::set<uint32_t>                             m_gone;
	uint32_t                                       m_refusals = 0u;
};
} // namespace headlessSession
