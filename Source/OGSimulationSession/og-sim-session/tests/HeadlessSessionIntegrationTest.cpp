// SPDX-License-Identifier: MPL-2.0
#if WITH_LOW_LEVEL_TESTS && __has_include("OGSimulationJolt/JoltTickWorldHost.h")
#include "catch_amalgamated.hpp"

#include "HeadlessSessionHost.h"
#include "Jolt/JoltWorldTestRig.h"
#include "SessionTestKit.h"
#include "glm/gtc/matrix_transform.hpp"
#include "OGSimulation/PhysicsBodyState.h"
#include "OGSimulation/PhysicsDeclaration.h"
#include "OGSimulation/RenderSnapshot.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulationJolt/JoltDeterminismFingerprint.h"
#include "OGSimulationJolt/JoltPhysicsBodyAdapter.h"
#include "OGSimulationJolt/JoltPhysicsBodyReaderAdapter.h"
#include "OGSimulationJolt/JoltPhysicsFactory.h"
#include "OGSimulationJolt/JoltSpatialQueryAdapter.h"
#include "OGSimulationJolt/JoltTickWorldHost.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace headlessSessionTest
{
struct HsInput
{
	std::int32_t value = 0;
};

struct HsRadialPart
{
	PhysicsBodyState bodyState;
};

struct HsCapsulePart
{
	PhysicsBodyState bodyState;
};

struct HsScriptPart
{
	std::int32_t  lastInput = 0;
	std::int32_t  inputSum = 0;
	std::uint32_t integrations = 0u;
};

struct IntegrateRecord
{
	std::uint32_t id = 0u;
	std::uint32_t tick = 0u;
	std::int32_t  input = 0;
};

class IntegrateLog
{
public:
	void add(const IntegrateRecord& record)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_records.push_back(record);
	}

	std::vector<IntegrateRecord> records() const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_records;
	}

private:
	mutable std::mutex           m_mutex;
	std::vector<IntegrateRecord> m_records;
};

struct HsStaticData
{
	glm::vec3     radialOffset = glm::vec3(0.7f, 0.3f, -0.f);
	float         queryRadius  = 0.5f;
	IntegrateLog* log          = nullptr;
};
} // namespace headlessSessionTest

template <>
struct SerializableFields<headlessSessionTest::HsInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(headlessSessionTest::HsInput, value)); }
};

template <>
struct SerializableFields<headlessSessionTest::HsRadialPart>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(headlessSessionTest::HsRadialPart, bodyState)); }
};

template <>
struct SerializableFields<headlessSessionTest::HsCapsulePart>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(headlessSessionTest::HsCapsulePart, bodyState)); }
};

template <>
struct SerializableFields<headlessSessionTest::HsScriptPart>
{
	static constexpr auto get()
	{
		return std::make_tuple(SIM_MEMBER(headlessSessionTest::HsScriptPart, lastInput),
			SIM_MEMBER(headlessSessionTest::HsScriptPart, inputSum), SIM_MEMBER(headlessSessionTest::HsScriptPart, integrations));
	}
};

namespace headlessSessionTest
{
struct HsRadialDeclaration
{
	using StateType = HsRadialPart;
	static constexpr const char* name = "hsRadial";

	static const PhysicalObjectDescriptor& descriptor()
	{
		static const PhysicalObjectDescriptor radial =
			joltTestRig::sphereTemplate(0.3f, joltTestRig::bit(joltTestRig::kBody), 0u, 0).descriptor;
		return radial;
	}

	static const HsStaticData& staticDataOf(const HsStaticData& staticData) { return staticData; }

	static std::vector<QueryVolumeDescriptor> queryVolumes(const HsStaticData& staticData)
	{
		return { QueryVolumeDescriptor{ SphereGeometry{ staticData.queryRadius },
			CollisionCategories{ joltTestRig::bit(joltTestRig::kCharacter) }, glm::mat4(1.f), joltTestRig::kQueryRouting } };
	}

	static glm::vec3 attachmentOffset(const HsStaticData& staticData) { return staticData.radialOffset; }
	static PhysicsBodyState& bodyStateOf(StateType& state) { return state.bodyState; }
	static const PhysicsBodyState& bodyStateOf(const StateType& state) { return state.bodyState; }

	PhysicsRuntimeBindings bindings;
};

struct HsCapsuleDeclaration
{
	using StateType = HsCapsulePart;
	static constexpr const char* name = "hsCapsule";

	static const PhysicalObjectDescriptor& descriptor()
	{
		static const PhysicalObjectDescriptor capsule = joltTestRig::capsuleTemplate(joltTestRig::bit(joltTestRig::kCharacter),
			joltTestRig::bit(joltTestRig::kWorld), 1).descriptor;
		return capsule;
	}

	static const HsStaticData& staticDataOf(const HsStaticData& staticData) { return staticData; }
	static std::vector<QueryVolumeDescriptor> queryVolumes(const HsStaticData&) { return {}; }
	static glm::vec3 attachmentOffset(const HsStaticData&) { return glm::vec3(0.f); }
	static PhysicsBodyState& bodyStateOf(StateType& state) { return state.bodyState; }
	static const PhysicsBodyState& bodyStateOf(const StateType& state) { return state.bodyState; }

	PhysicsRuntimeBindings bindings;
};

static_assert(PhysicsDeclaration<HsRadialDeclaration, HsStaticData> && PhysicsDeclaration<HsCapsuleDeclaration, HsStaticData>);

using HsComposite = SimulationComposite<HsRadialDeclaration, HsCapsuleDeclaration>;
using HsState     = SimulationComposite<HsRadialPart, HsCapsulePart, HsScriptPart>;

inline HsInput scriptedInput(std::uint32_t id, std::uint32_t tick)
{
	return HsInput{ 1 + static_cast<std::int32_t>((tick * 31u + id * 7u) % 17u) };
}

class HsAllState
{
public:
	const HsState& getState() const { return m_state; }
	HsState& editState() { return m_state; }

private:
	HsState m_state;
};

class HsSim
{
public:
	using StateType = HsState;
	using InputType = HsInput;

	std::uint32_t id = 0u;

	const HsAllState& getAllState() const { return m_all; }
	HsAllState& editAllState() { return m_all; }
	void updateVizState() { m_viz = m_all; }
	const HsAllState& getVizState() const { return m_viz; }
	const HsComposite& getPhysicsComposite() const { return m_composite; }
	HsComposite& editPhysicsComposite() { return m_composite; }

	void firstResimStep(JoltPhysicsBodyAdapter&, int32_t) {}

	void integrate(const SimulationTimeStep& step, const HsInput& input, JoltPhysicsBodyAdapter& physics, JoltSpatialQueryAdapter&,
		const HsStaticData& staticData)
	{
		if (staticData.log != nullptr)
			staticData.log->add(IntegrateRecord{ id, step.getTick(), input.value });
		HsScriptPart& script = m_all.editState().edit<HsScriptPart>();
		script.lastInput = input.value;
		script.inputSum += input.value;
		++script.integrations;
		physics.setBodyLinearVelocity(m_composite.get<HsCapsuleDeclaration>().bindings.ownBodyId,
			glm::vec3(0.01f * static_cast<float>(input.value % 5 - 2), 0.f, 0.f));
	}

private:
	HsAllState  m_all;
	HsAllState  m_viz;
	HsComposite m_composite;
};
} // namespace headlessSessionTest

template <>
struct SimulatableOwnerTraits<headlessSessionTest::HsSim>
{
	using PredictionOwnerType = headlessSession::Channel<headlessSessionTest::HsSim>;
	using AuthorityOwnerType  = headlessSession::Channel<headlessSessionTest::HsSim>;
};

namespace headlessSessionTest
{
struct HsEngineTraits
{
	using BodyAdapter   = JoltPhysicsBodyAdapter;
	using ReaderAdapter = JoltPhysicsBodyReaderAdapter;
	using QueryAdapter  = JoltSpatialQueryAdapter;
	using Factory       = JoltPhysicsFactory;
	using VizQuery      = JoltSpatialQueryAdapter;
	using VizReader     = JoltPhysicsBodyReaderAdapter;
	using CategoryTable = int;
	using Statics       = JoltTickStatics;

	static constexpr char kName[] = "jolt";

	template <typename SimulatableT>
	using WorldHost = JoltTickWorldHost<SimulatableT, std::mutex, headlessSession::Engine>;

	static std::optional<std::uint64_t> computeDeterminismFingerprint(void (*logger)(const char*))
	{
		JoltRuntime& runtime = JoltRuntime::acquire(logger);
		const std::uint64_t value = determinismFingerprint(runtime).value();
		JoltRuntime::release();
		return value;
	}

	static Statics splitStatics(const StaticWorldDescription& description, const std::vector<std::uint8_t>& physicsOnly)
	{
		return joltTickWorldHost::splitStatics(description, physicsOnly);
	}

	static void logEngineMessage(const char* message) { headlessSession::logLine(message); }
};

struct HsComposition
{
	using Simulatables    = SimulatableList<HsSim>;
	using Storage         = apply_t<SimulationObjectStorage, Simulatables>;
	using Reconciliation  = apply_t<SimulationReconciliation, Simulatables>;
	using InputResolution = apply_t<SimulationInputResolution, Simulatables>;
	using NetSync         = apply_t<SimulationNetSync, Simulatables>;
	using RenderSnapshot  = RenderSnapshotT<kMaxSimulatableSlots * 2>;
	using Systems         = SystemList<>;
	using SystemsExec     = SimulationSystemsExecutor<Simulatables, HsStaticData>;
	using IntegrationExec = SimulationIntegrationExecutor<HsStaticData, JoltPhysicsBodyAdapter, JoltSpatialQueryAdapter, HsSim>;
	using Manager         = SimulationManager<IntegrationExec, NetSync, InputResolution, Reconciliation, SystemsExec, Storage,
		HsStaticData>;
};

struct HsGame
{
	using AuthorityPresentationSlice = int;

	static HsStaticData makeStaticData() { return HsStaticData{}; }

	static void emplaceSystems(std::optional<HsComposition::SystemsExec>& systems, JoltPhysicsBodyAdapter&, JoltSpatialQueryAdapter&)
	{
		systems.emplace();
	}

	static HsInput neutralInput() { return HsInput{}; }
	static void installGameLoggers(bool) {}
	static BodyId rootBodyId(const HsSim& simulatable) { return simulatable.getPhysicsComposite().get<HsCapsuleDeclaration>().bindings.ownBodyId; }
	static int authorityPresentationSlice_Step(const HsComposition::SystemsExec&, unsigned int) { return 0; }

	static std::string_view backendFingerprint()
	{
		static const std::string fingerprint = []
		{
			const std::optional<std::uint64_t> value = HsEngineTraits::computeDeterminismFingerprint(&HsEngineTraits::logEngineMessage);
			char hex[17] = "none";
			if (value.has_value())
				std::snprintf(hex, sizeof(hex), "%016" PRIx64, *value);
			return std::string(hex);
		}();
		return fingerprint;
	}
};

struct HsNetConfig
{
	static constexpr int tickFrequencyHz = 60;
};

struct HsGameTraits
{
	using Game         = HsGame;
	using Composition  = HsComposition;
	using EngineTraits = HsEngineTraits;
	using StaticData   = HsStaticData;
	using Simulatable  = HsSim;
	using PlayerInput  = HsInput;
	using NetConfig    = HsNetConfig;

	static JoltTickWorldHostConfig worldConfig(bool isAuthority, const StaticData&)
	{
		JoltTickWorldHostConfig config;
		config.isAuthority = isAuthority;
		config.runInline = false;
		config.gravity = glm::vec3(0.f);
		config.mappedCategories = joltTestRig::kAllMappedCategories;
		config.log = &HsEngineTraits::logEngineMessage;
		return config;
	}
};

using Peer    = headlessSession::Peer<HsGameTraits>;
using Entity  = Peer::Entity;
using Runtime = Peer::RuntimeType;

constexpr std::uint8_t  kServerHostIndex = 1u;
constexpr std::uint16_t kServerPort = 7777u;
constexpr std::uint32_t kScriptTicks = 600u;
constexpr std::uint32_t kLeaveTick = 240u;
constexpr std::uint32_t kRejoinTick = 300u;
constexpr std::uint32_t kMaxSimId = 8u;
constexpr double        kFrameSeconds = 0.01;
constexpr double        kTickSeconds = 1.0 / 60.0;
constexpr double        kGiveUpSeconds = 20.0;
constexpr double        kAuthorityWarmupSeconds = 2.0;
constexpr double        kWindowSpanSeconds = 2.5;
constexpr std::uint32_t kMaxInputDelayTicks = 32u;
constexpr std::uint32_t kRelayDelayFloorTicks = 6u;

const std::vector<std::string> kStartupTags = { "[SimHost.Mode]", "[SimNet.Host]", "[LatencyKnobs]", "[ClockSource.Config]",
	"[SimHost.Backend]", "[SimHost.Frame]" };

ognet::EndpointConfig endpointConfig(ognet::Role role)
{
	ognet::EndpointConfig config;
	config.role = role;
	if (role == ognet::Role::Server)
		config.bind.port = kServerPort;
	config.buildIdentity.fill(0x5A);
	return config;
}

ogsession::sessionTestKit::MapConfigSource shippedNetcodeConfig()
{
	return ogsession::sessionTestKit::MapConfigSource({ { "RelayDelayFloorTicks", std::to_string(kRelayDelayFloorTicks) },
		{ "ResimTriggerPolicy", "OnDisagreement" }, { "RedundancyFrames", "3" }, { "RelayResendEntries", "2" },
		{ "ClockDilation", "On" }, { "ClockDilationSetpoint", "Common" } });
}

StaticWorldDescription arena()
{
	StaticShapeDescriptor floor;
	floor.shape = StaticBox{ glm::vec3(15.f, 0.5f, 15.f) };
	floor.localToWorld = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -0.5f, 0.f));
	floor.categories = CollisionCategories{ joltTestRig::bit(joltTestRig::kWorld) };
	floor.blockingCategories = CollisionCategories{ joltTestRig::kAllMappedCategories };
	floor.stableKey = 1u;
	StaticWorldDescription description;
	description.shapes.push_back(floor);
	return description;
}

HsSim prototypeOf(std::uint32_t simId)
{
	HsSim simulatable;
	simulatable.id = simId;
	return simulatable;
}

Peer::InputProvider scriptedProvider(std::uint32_t simId)
{
	return [simId](const SimulationTimeStep& step, const LocalInputCache<HsInput>&) { return scriptedInput(simId, step.getTick()); };
}

std::string tagOf(std::string_view text)
{
	for (const std::string_view level : { std::string_view("[Warning]"), std::string_view("[Error]") })
	{
		if (text.rfind(level, 0) == 0)
			text.remove_prefix(level.size());
	}
	const std::size_t end = text.find(']');
	return text.rfind("[", 0) == 0 && end != std::string_view::npos ? std::string(text.substr(0, end + 1)) : std::string();
}

struct Line
{
	double      seconds = 0.0;
	std::string text;
};

class PeerLog
{
public:
	headlessSession::PeerContext context{ [this](const char* line) { add(line); } };

	void add(std::string text)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_lines.push_back(Line{ headlessSession::monotonicSeconds(), std::move(text) });
	}

	std::vector<Line> lines() const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_lines;
	}

	std::size_t size() const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_lines.size();
	}

private:
	mutable std::mutex m_mutex;
	std::vector<Line>  m_lines;
};

struct PeerResult
{
	std::uint32_t ownId = 0u;
	bool          registered = false;
	std::uint32_t refusals = 0u;
	std::size_t   startupBegin = 0u;
	std::size_t   startupEnd = 0u;
	bool          netIoThreadSet = false;
};

struct Scenario
{
	std::atomic<bool>   serverUp{ false };
	std::atomic<bool>   serverStop{ false };
	std::atomic<double> scriptStartSeconds{ 0.0 };
	std::atomic<bool>   ownRegistered[kMaxSimId + 1u]{};

	std::uint32_t tick() const
	{
		const double start = scriptStartSeconds.load();
		return start > 0.0 ? static_cast<std::uint32_t>((headlessSession::monotonicSeconds() - start) / kTickSeconds) : 0u;
	}
};

template <typename DoneF>
bool frameUntil(Peer& peer, DoneF&& done)
{
	const double giveUp = headlessSession::monotonicSeconds() + kGiveUpSeconds;
	double last = headlessSession::monotonicSeconds();
	while (!done())
	{
		if (headlessSession::monotonicSeconds() > giveUp)
			return false;
		std::this_thread::sleep_for(std::chrono::duration<double>(kFrameSeconds));
		const double now = headlessSession::monotonicSeconds();
		peer.frame(now - last);
		last = now;
	}
	return true;
}

void runServer(ognet::test::VirtualNetwork& network, Scenario& scenario, PeerLog& log, IntegrateLog& authorityLog, PeerResult& result)
{
	const headlessSession::PeerScope scope(log.context);
	Peer server(network, kServerHostIndex, endpointConfig(ognet::Role::Server));
	server.runtime().editStaticData().log = &authorityLog;
	server.open(std::nullopt);
	result.startupBegin = log.size();
	server.startSession(shippedNetcodeConfig(), arena());
	result.startupEnd = log.size();
	scenario.serverUp.store(true);

	std::map<ognet::ConnectionId, std::uint32_t> idOfConnection;
	std::uint32_t nextSimId = 1u;
	frameUntil(server, [&]
	{
		for (const ognet::ConnectionEvent& event : server.endpoint().takeConnectionEvents())
		{
			if (event.kind == ognet::ConnectionEventKind::Connected)
			{
				idOfConnection[event.id] = nextSimId;
				server.addEntity(Entity{ nextSimId, event.id, false, prototypeOf(nextSimId), {} });
				++nextSimId;
			}
			else if (event.kind == ognet::ConnectionEventKind::Disconnected && idOfConnection.count(event.id) != 0u)
			{
				log.add("[Test.Event] unregister id=" + std::to_string(idOfConnection[event.id]));
				server.removeEntity(idOfConnection[event.id]);
			}
		}
		return scenario.serverStop.load();
	});
	result.refusals = server.refusals();
	server.close();
	server.stopSession();
}

void runClient(ognet::test::VirtualNetwork& network, Scenario& scenario, std::uint8_t hostIndex, std::optional<std::uint32_t> leaveTick,
	PeerLog& log, PeerResult& result)
{
	const headlessSession::PeerScope scope(log.context);
	Peer client(network, hostIndex, endpointConfig(ognet::Role::Client));
	client.open(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
	const auto ownEntry = [&]() -> const ogsimnet::RosterEntry*
	{
		for (const ogsimnet::RosterEntry& entry : client.lanes().client()->roster().entries())
		{
			if (entry.isLocal())
				return &entry;
		}
		return nullptr;
	};
	if (!frameUntil(client, [&] { return ownEntry() != nullptr && client.lanes().client()->heldClockSample().has_value(); }))
		return;
	result.ownId = ownEntry()->simId;
	result.netIoThreadSet = client.netIoThread() != std::thread::id{};
	result.startupBegin = log.size();
	client.startSession(shippedNetcodeConfig(), arena());
	result.startupEnd = log.size();
	client.addEntity(Entity{ result.ownId, kInvalidSimConnection, true, prototypeOf(result.ownId), scriptedProvider(result.ownId) });

	std::map<std::uint32_t, bool> wasRegistered;
	frameUntil(client, [&]
	{
		for (std::uint32_t simId = 1u; simId <= kMaxSimId; ++simId)
		{
			if (simId != result.ownId && !client.known(simId) && client.runtime().bindStep(simId, false) == ogsimnet::BindStep::Ready)
				client.addEntity(Entity{ simId, kInvalidSimConnection, false, prototypeOf(simId), {} });
			if (client.registered(simId) != wasRegistered[simId])
				log.add("[Test.Event] " + std::string(client.registered(simId) ? "registered" : "unregistered") + " id=" + std::to_string(simId));
			wasRegistered[simId] = client.registered(simId);
		}
		result.registered = result.registered || client.registered(result.ownId);
		scenario.ownRegistered[result.ownId].store(client.registered(result.ownId));
		const std::uint32_t tick = scenario.tick();
		return leaveTick.has_value() ? tick >= *leaveTick : tick >= kScriptTicks;
	});
	result.refusals = client.refusals();
	client.close();
	client.stopSession();
}

bool waitFor(const std::atomic<bool>& flag)
{
	const double giveUp = headlessSession::monotonicSeconds() + kGiveUpSeconds;
	while (!flag.load())
	{
		if (headlessSession::monotonicSeconds() > giveUp)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}

std::vector<std::string> startupTags(const PeerLog& log, const PeerResult& result)
{
	const std::vector<Line> lines = log.lines();
	std::vector<std::string> tags;
	for (std::size_t index = result.startupBegin; index < result.startupEnd && index < lines.size(); ++index)
	{
		const std::string tag = tagOf(lines[index].text);
		if (std::find(kStartupTags.begin(), kStartupTags.end(), tag) != kStartupTags.end())
			tags.push_back(tag);
	}
	return tags;
}

std::size_t countContaining(const PeerLog& log, std::string_view needle)
{
	std::size_t count = 0u;
	for (const Line& line : log.lines())
		count += line.text.find(needle) != std::string::npos ? 1u : 0u;
	return count;
}

struct SteadyResims
{
	std::uint32_t windows = 0u;
	std::uint32_t requested = 0u;
};

SteadyResims steadyStateResims(const PeerLog& log)
{
	const std::vector<Line> lines = log.lines();
	std::vector<double> events;
	for (const Line& line : lines)
	{
		if (line.text.rfind("[Test.Event]", 0) == 0)
			events.push_back(line.seconds);
	}
	SteadyResims result;
	bool joinWindowSeen = false;
	for (const Line& line : lines)
	{
		if (line.text.find("[ResimProbe.Gate]") == std::string::npos)
			continue;
		const bool nearAnEvent = std::any_of(events.begin(), events.end(),
			[&](double seconds) { return seconds <= line.seconds && seconds > line.seconds - kWindowSpanSeconds; });
		const bool steady = joinWindowSeen && !nearAnEvent;
		joinWindowSeen = true;
		unsigned requested = 0u;
		const std::size_t at = line.text.find("requested=");
		if (!steady || at == std::string::npos || std::sscanf(line.text.c_str() + at, "requested=%u", &requested) != 1)
			continue;
		++result.windows;
		result.requested += requested;
	}
	return result;
}

std::uint32_t localDivergencesAfterJoin(const PeerLog& log, std::uint32_t ownId)
{
	const std::string own = "[DivergenceProbe.Correction] id=" + std::to_string(ownId) + " ";
	std::uint32_t divergences = 0u;
	bool joinWindowClosed = false;
	for (const Line& line : log.lines())
	{
		joinWindowClosed = joinWindowClosed || line.text.find("[ResimProbe.Gate]") != std::string::npos;
		divergences += joinWindowClosed && line.text.find(own) != std::string::npos
				&& line.text.find("class=LocallyPredicted correct=0") != std::string::npos ? 1u : 0u;
	}
	return divergences;
}

struct AppliedInputs
{
	std::uint32_t matched = 0u;
	std::uint32_t delayTicks = 0u;
};

AppliedInputs scriptedInputsApplied(const IntegrateLog& authorityLog, std::uint32_t simId)
{
	const std::vector<IntegrateRecord> records = authorityLog.records();
	AppliedInputs best;
	for (std::uint32_t delay = 0u; delay < kMaxInputDelayTicks; ++delay)
	{
		std::uint32_t matched = 0u;
		for (const IntegrateRecord& record : records)
			matched += record.id == simId && record.tick >= delay && record.input == scriptedInput(simId, record.tick - delay).value ? 1u : 0u;
		if (matched > best.matched)
			best = AppliedInputs{ matched, delay };
	}
	return best;
}
} // namespace headlessSessionTest

using namespace headlessSessionTest;

TEST_CASE("HeadlessSession.OneAuthorityAndTwoClientsRunTheScriptedSessionEngineFree", "[Session][HeadlessSession]")
{
	const double startSeconds = headlessSession::monotonicSeconds();
	const std::string fingerprint(HsGame::backendFingerprint());
	ognet::test::VirtualNetwork network(ognet::test::NetworkMode::Steady);
	Scenario scenario;
	IntegrateLog authorityLog;
	PeerLog serverLog, client1Log, client2aLog, client2bLog;
	PeerResult serverResult, client1Result, client2aResult, client2bResult;

	std::thread server([&] { runServer(network, scenario, serverLog, authorityLog, serverResult); });
	CHECK(waitFor(scenario.serverUp));
	std::this_thread::sleep_for(std::chrono::duration<double>(kAuthorityWarmupSeconds));
	std::thread client1([&] { runClient(network, scenario, 10u, std::nullopt, client1Log, client1Result); });
	CHECK(waitFor(scenario.ownRegistered[1]));
	std::thread client2a([&] { runClient(network, scenario, 11u, kLeaveTick, client2aLog, client2aResult); });
	CHECK(waitFor(scenario.ownRegistered[2]));
	scenario.scriptStartSeconds.store(headlessSession::monotonicSeconds());
	client2a.join();
	while (scenario.tick() < kRejoinTick)
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	std::thread client2b([&] { runClient(network, scenario, 12u, std::nullopt, client2bLog, client2bResult); });
	client2b.join();
	client1.join();
	scenario.serverStop.store(true);
	server.join();
	const double wallSeconds = headlessSession::monotonicSeconds() - startSeconds;
	WARN("[HeadlessSession] wall=" << wallSeconds << " s fingerprint=" << fingerprint);
	CHECK(client1Result.ownId == 1u);
	CHECK(client2aResult.ownId == 2u);
	CHECK(client2bResult.ownId == 3u);
	CHECK(client1Result.registered);
	CHECK(client2aResult.registered);
	CHECK(client2bResult.registered);
	CHECK(serverResult.refusals + client1Result.refusals + client2aResult.refusals + client2bResult.refusals == 0u);
	CHECK(client1Result.netIoThreadSet);

	CHECK(startupTags(serverLog, serverResult) == kStartupTags);
	CHECK(startupTags(client1Log, client1Result) == kStartupTags);
	CHECK(startupTags(client2aLog, client2aResult) == kStartupTags);
	CHECK(startupTags(client2bLog, client2bResult) == kStartupTags);
	CHECK(countContaining(serverLog, "[SimHost.Backend] backend=jolt fingerprint=" + fingerprint) == 1u);

	const SteadyResims client1Resims = steadyStateResims(client1Log);
	const SteadyResims client2bResims = steadyStateResims(client2bLog);
	INFO("client1 steady windows=" << client1Resims.windows << " requested=" << client1Resims.requested << "; client2b steady "
		<< "windows=" << client2bResims.windows << " requested=" << client2bResims.requested);
	CHECK(client1Resims.windows >= 2u);
	CHECK(client1Resims.requested == 0u);
	CHECK(client2bResims.requested == 0u);
	CHECK(localDivergencesAfterJoin(client1Log, 1u) == 0u);
	CHECK(localDivergencesAfterJoin(client2aLog, 2u) == 0u);
	CHECK(localDivergencesAfterJoin(client2bLog, 3u) == 0u);

	CHECK(countContaining(client2bLog, "PCTM CPC: hard resync") == 1u);
	CHECK(countContaining(client2bLog, "cause=firstRtt") == 0u);

	const AppliedInputs applied1 = scriptedInputsApplied(authorityLog, 1u);
	const AppliedInputs applied2 = scriptedInputsApplied(authorityLog, 2u);
	const AppliedInputs applied3 = scriptedInputsApplied(authorityLog, 3u);
	INFO("the authority applied the scripted inputs: " << applied1.matched << ", " << applied2.matched << " and " << applied3.matched
		<< " ticks, " << applied1.delayTicks << ", " << applied2.delayTicks << " and " << applied3.delayTicks << " ticks after capture");
	CHECK(applied1.matched >= kScriptTicks * 9u / 10u);
	CHECK(applied2.matched >= kLeaveTick * 9u / 10u);
	CHECK(applied3.matched >= (kScriptTicks - kRejoinTick) * 8u / 10u);
	CHECK(applied1.delayTicks == kRelayDelayFloorTicks);
	CHECK(applied2.delayTicks == kRelayDelayFloorTicks);
	CHECK(applied3.delayTicks == kRelayDelayFloorTicks);
}

TEST_CASE("HeadlessSession.StartFrameFromASecondThreadTripsTheThreadCheck", "[.][HeadlessSessionDeath]")
{
	PeerLog log;
	const headlessSession::PeerScope scope(log.context);
	ognet::test::VirtualNetwork network(ognet::test::NetworkMode::Steady);
	Peer client(network, 10u, endpointConfig(ognet::Role::Client));
	client.open(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
	REQUIRE(frameUntil(client, [&] { return client.netIoThread() != std::thread::id{}; }));
	client.startSession(shippedNetcodeConfig(), arena());
	REQUIRE(client.driver() != nullptr);
	client.frame(kFrameSeconds);
	std::thread second([&] { client.driver()->startFrame(kFrameSeconds); });
	second.join();
	FAIL("startFrame on a second thread did NOT fire the session's thread check - the check is vacuous in this target");
}

TEST_CASE("HeadlessSession.AServiceNetOpOffOgNetsIoThreadTripsTheExactRow", "[.][HeadlessSessionDeath]")
{
	PeerLog log;
	const headlessSession::PeerScope scope(log.context);
	ognet::test::VirtualNetwork network(ognet::test::NetworkMode::Steady);
	Peer client(network, 10u, endpointConfig(ognet::Role::Client));
	client.open(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
	REQUIRE(frameUntil(client, [&] { return client.netIoThread() != std::thread::id{}; }));
	client.startSession(shippedNetcodeConfig(), arena());
	const Runtime::TickHost tickHost{ &client.runtime() };
	CHECK_FALSE(tickHost.threads().isAllowed(ogsession::SessionOp::ServiceNet));
	std::thread stray([&] { tickHost.threads().checkOp(ogsession::SessionOp::ServiceNet); });
	stray.join();
	FAIL("a prediction ServiceNet op on a thread that is not og-net's I/O thread did NOT fire the exact row's check");
}
#endif
