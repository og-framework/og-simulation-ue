#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimNetHost-rationale.md · ../../../OGSimulationUnreal/docs/UESimNetHost-guards.md

#include "OGSimulation/Network/SimTransport.h"

#include <cstdint>

namespace ogsimnet
{
template <typename TraitsT>
class SimNetLinkServerHost : public UESimNetLanesServerHost<TraitsT>
{
	using Base = UESimNetLanesServerHost<TraitsT>;
	using Base::m_endpoint;

public:
	using Base::Base;

	void log(const char* line) { ueSimNetHost::routeLine(line); }

	void noteDelivered(SimLane lane, double recvSeconds) { ueSimNetHost::noteDelivered(m_endpoint, lane, recvSeconds); }

	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds)
	{
		if (this->session() == nullptr)
			return;
		this->session()->editLatencyBudgetDiagnostics().stampServerReceived_Internal(simId, captureTick, recvSeconds);
	}

	void stampHanded(uint8_t simId, uint32_t captureTick)
	{
		if (this->session() != nullptr)
			this->session()->editLatencyBudgetDiagnostics().stampHandedAndLeft_Internal(simId, captureTick);
	}

	void onLifecycleUnregistering(uint8_t simId)
	{
		if (this->session() != nullptr)
			this->session()->onSimNetLifecycleUnregistering(simId);
	}
};
} // namespace ogsimnet
