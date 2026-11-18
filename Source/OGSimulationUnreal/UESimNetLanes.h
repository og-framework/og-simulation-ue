// SPDX-License-Identifier: MPL-2.0
// docs/UESimNetLanes-rationale.md · docs/UESimNetLanes-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGNetUnreal/OGNetSimLanes.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

inline constexpr size_t kUESimNetInboxSlots = 2048u;

using UESimNetTransport = ogsimnet::SimNetTransport<ognetunreal::SimLaneEndpoint, kUESimNetInboxSlots>;

template <typename SimT>
using UESimNetChannel = ogsimnet::SessionChannel<SimT, typename UESimNetTransport::ChannelTransport>;

template <typename TraitsT>
class UESimNetLanes final : public IOGNetSimLanes
{
public:
	using Transport        = UESimNetTransport;
	using ChannelTransport = typename Transport::ChannelTransport;
	using ServerHost       = UESimNetServerHost<TraitsT>;
	using ClientHost       = UESimNetClientHost<TraitsT>;
	using Client           = ogsimnet::SimNetClient<typename TraitsT::Composition, ClientHost, Transport>;
	using Session          = UESimulationSession<TraitsT>;
	using FFrameFn         = TFunction<void()>;

	UESimNetLanes(UOGNetDriver* ognetDriver, bool bIsServer) : m_driver(ognetDriver), m_isServer(bIsServer) {}

	UESimNetLanes(const UESimNetLanes&) = delete;
	UESimNetLanes& operator=(const UESimNetLanes&) = delete;

	static TUniquePtr<UESimNetLanes> makeDetached(ognet::Role role, const ognet::EndpointConfig& config)
	{
		ognet::EndpointConfig detachedConfig = config;
		detachedConfig.role = role;
		TUniquePtr<UESimNetLanes> lanes = MakeUnique<UESimNetLanes>(nullptr, role == ognet::Role::Server);
		lanes->m_detachedEndpoint.reset(
			new ognetunreal::SimLaneEndpoint(ognetunreal::SimLaneEndpoint::detached(detachedConfig)));
		lanes->beforeStart(*lanes->m_detachedEndpoint);
		lanes->afterStart();
		return lanes;
	}

	virtual void beforeStart(ognetunreal::SimLaneEndpoint& simEndpoint) override
	{
		checkf(m_endpoint == nullptr, TEXT("UESimNetLanes::beforeStart: a lanes object attaches to one endpoint, once"));
		checkf((simEndpoint.config().role == ognet::Role::Server) == m_isServer,
			TEXT("UESimNetLanes::beforeStart: the lanes were made for the %s role and the endpoint is the other role's"),
			m_isServer ? TEXT("server") : TEXT("client"));
		m_endpoint = &simEndpoint;
		m_transport.emplace(ogsimnet::simNetConfigFor(simEndpoint.config(), /*immediate*/ false));
		m_transport->installHandlers(simEndpoint);
		simEndpoint.setConnectionHandler(m_transport->connectionHandler());
		if (m_isServer)
			return;
		m_clientHost.emplace(m_driver, simEndpoint, m_bindings);
		// ⛔G-01  docs/UESimNetLanes-guards.md
		m_client.emplace(*m_clientHost, *m_transport);
		m_clientHost->setRoster(m_client->roster());
	}

	virtual void afterStart() override
	{
		checkf(m_transport.has_value(), TEXT("UESimNetLanes::afterStart before beforeStart: the driver attaches the lanes first"));
		m_transport->markEndpointStarted();
	}

	virtual void afterDispatch() override
	{
		if (m_isServer)
		{
			if (m_serverDispatch)
				m_serverDispatch();
			return;
		}
		checkf(m_client.has_value(), TEXT("UESimNetLanes::afterDispatch on a client before beforeStart"));
		m_client->dispatchFrame();
		const std::optional<ogsimnet::SimNetDisconnectRequest> request = m_client->takeDisconnectRequest();
		if (!request)
			return;
		UE_LOG(LogOGNet, Error,
			TEXT("[SimNet.Host] role=client disconnect conn=%u reason=%s: the server's roster was refused, so this client leaves the server"),
			static_cast<unsigned>(request->conn), UTF8_TO_TCHAR(ogsimnet::rosterRefusalName(request->reason)));
		if (request->conn != kInvalidSimConnection)
			m_endpoint->disconnect(request->conn);
	}

	virtual void onTickFlush() override
	{
		if (m_isServer)
		{
			if (m_serverFlush)
				m_serverFlush();
			return;
		}
		checkf(m_client.has_value(), TEXT("UESimNetLanes::onTickFlush on a client before beforeStart"));
		m_client->flushFrame();
	}

	virtual void beforeStop() override
	{
		FFrameFn onStop = MoveTemp(m_onStop);
		m_onStop = nullptr;
		if (onStop)
			onStop();
		if (m_isServer)
			unbindServerSession();
		else
			unbindClientSession();
	}

	void bindClientSession(Session& worldSession)
	{
		checkf(!m_isServer && m_client.has_value(),
			TEXT("UESimNetLanes::bindClientSession: only a started client's lanes hold a client session"));
		m_clientHost->bindSession(worldSession);
		m_client->bindListener();
		m_bindings.seedFrom(m_client->roster());
	}

	void unbindClientSession()
	{
		if (!m_client.has_value())
			return;
		m_client->unbindListener();
		m_clientHost->unbindSession();
	}

	void setOnRegisteredRemoved(typename ClientHost::FRegisteredRemoved onRegisteredRemoved)
	{
		checkf(m_clientHost.has_value(), TEXT("UESimNetLanes::setOnRegisteredRemoved: only a started client's lanes have a client host"));
		m_clientHost->setOnRegisteredRemoved(MoveTemp(onRegisteredRemoved));
	}

	void setStopHandler(FFrameFn onStop) { m_onStop = MoveTemp(onStop); }

	void bindServerSession(FFrameFn dispatchFn, FFrameFn flushFn)
	{
		checkf(m_isServer, TEXT("UESimNetLanes::bindServerSession on a client's lanes"));
		checkf(static_cast<bool>(dispatchFn) && static_cast<bool>(flushFn),
			TEXT("UESimNetLanes::bindServerSession: a server session binds both frame points"));
		m_serverDispatch = MoveTemp(dispatchFn);
		m_serverFlush = MoveTemp(flushFn);
	}

	void unbindServerSession()
	{
		m_serverDispatch = nullptr;
		m_serverFlush = nullptr;
	}

	bool isServer() const { return m_isServer; }
	bool isDetached() const { return m_detachedEndpoint != nullptr; }
	bool serverSessionBound() const { return static_cast<bool>(m_serverFlush); }
	UOGNetDriver* driver() const { return m_driver; }

	ognetunreal::SimLaneEndpoint& endpoint()
	{
		checkf(m_endpoint != nullptr, TEXT("UESimNetLanes::endpoint before beforeStart"));
		return *m_endpoint;
	}

	Transport& transport()
	{
		checkf(m_transport.has_value(), TEXT("UESimNetLanes::transport before beforeStart"));
		return *m_transport;
	}

	Client* client() { return m_client.has_value() ? &*m_client : nullptr; }
	ClientHost* clientHost() { return m_clientHost.has_value() ? &*m_clientHost : nullptr; }
	const ogsimnet::SimNetBindings& bindings() const { return m_bindings; }
	ogsimnet::SimNetBindings& editBindings() { return m_bindings; }

private:
	UOGNetDriver* m_driver;
	bool m_isServer;
	std::unique_ptr<ognetunreal::SimLaneEndpoint> m_detachedEndpoint;
	ognetunreal::SimLaneEndpoint* m_endpoint = nullptr;
	std::optional<Transport> m_transport;
	ogsimnet::SimNetBindings m_bindings;
	std::optional<ClientHost> m_clientHost;
	std::optional<Client> m_client;
	FFrameFn m_serverDispatch;
	FFrameFn m_serverFlush;
	FFrameFn m_onStop;
};
