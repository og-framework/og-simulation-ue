// SPDX-License-Identifier: MPL-2.0
#if WITH_LOW_LEVEL_TESTS && __has_include("OGSimulationJolt/JoltTickWorldHost.h")
#include "catch_amalgamated.hpp"

#include "Jolt/JoltWorldTestRig.h"
#include "glm/gtc/matrix_transform.hpp"
#include "OGSimulation/PhysicsBodyState.h"
#include "OGSimulation/PhysicsDeclaration.h"
#include "OGSimulation/RenderSnapshot.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulationJolt/JoltPhysicsBodyAdapter.h"
#include "OGSimulationJolt/JoltPhysicsFactory.h"
#include "OGSimulationJolt/JoltSpatialQueryAdapter.h"
#include "OGSimulationJolt/JoltDeterminismFingerprint.h"
#include "OGSimulationJolt/JoltPhysicsBodyReaderAdapter.h"
#include "OGSimulationJolt/JoltTickWorldHost.h"
#include "OGSimSession/SessionRuntime.h"
#include "SessionTestKit.h"
#include "TickDriver/TickDriverFakes.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace sessionRuntimeStartStopTest
{
class Recorder
{
public:
	void add(std::string event)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (event == "clockEngine.lines" && !m_events.empty() && m_events.back() == event)
			return;
		m_events.push_back(std::move(event));
		m_threads.push_back(std::this_thread::get_id());
	}

	std::vector<std::string> events() const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_events;
	}

	std::vector<std::thread::id> threads() const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_threads;
	}

	void clear()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_events.clear();
		m_threads.clear();
	}

private:
	mutable std::mutex           m_mutex;
	std::vector<std::string>     m_events;
	std::vector<std::thread::id> m_threads;
};

inline Recorder g_recorder;
inline std::atomic<bool> g_runnerJoined{ false };
inline std::atomic<bool> g_serverDestroyedBeforeJoin{ false };

std::string tagOf(const char* line)
{
	std::string_view text(line);
	if (text.rfind("[Warning]", 0) == 0)
		text.remove_prefix(9);
	if (text.rfind("[Error]", 0) == 0)
		text.remove_prefix(7);
	const std::size_t end = text.find(']');
	return text.rfind("[", 0) == 0 && end != std::string_view::npos ? std::string(text.substr(0, end + 1)) : std::string(text);
}

struct RtInput
{
	std::int32_t value = 0;
};

struct RtStaticData
{
	glm::vec3 radialOffset = glm::vec3(0.7f, 0.3f, -0.f);
	float     queryRadius  = 0.5f;
};

struct RtRadialPart
{
	PhysicsBodyState bodyState;
};

struct RtCapsulePart
{
	PhysicsBodyState bodyState;
};
} // namespace sessionRuntimeStartStopTest

template <>
struct SerializableFields<sessionRuntimeStartStopTest::RtInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(sessionRuntimeStartStopTest::RtInput, value)); }
};

template <>
struct SerializableFields<sessionRuntimeStartStopTest::RtRadialPart>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(sessionRuntimeStartStopTest::RtRadialPart, bodyState)); }
};

template <>
struct SerializableFields<sessionRuntimeStartStopTest::RtCapsulePart>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(sessionRuntimeStartStopTest::RtCapsulePart, bodyState)); }
};

namespace sessionRuntimeStartStopTest
{
struct RtRadialDeclaration
{
	using StateType = RtRadialPart;
	static constexpr const char* name = "rtRadial";

	static const PhysicalObjectDescriptor& descriptor()
	{
		static const PhysicalObjectDescriptor radial =
			joltTestRig::sphereTemplate(0.3f, joltTestRig::bit(joltTestRig::kBody), 0u, 0).descriptor;
		return radial;
	}

	static const RtStaticData& staticDataOf(const RtStaticData& staticData) { return staticData; }

	static std::vector<QueryVolumeDescriptor> queryVolumes(const RtStaticData& staticData)
	{
		return { QueryVolumeDescriptor{ SphereGeometry{ staticData.queryRadius },
			CollisionCategories{ joltTestRig::bit(joltTestRig::kCharacter) }, glm::mat4(1.f), joltTestRig::kQueryRouting } };
	}

	static glm::vec3 attachmentOffset(const RtStaticData& staticData) { return staticData.radialOffset; }

	static PhysicsBodyState& bodyStateOf(StateType& state) { return state.bodyState; }
	static const PhysicsBodyState& bodyStateOf(const StateType& state) { return state.bodyState; }

	PhysicsRuntimeBindings bindings;
};

struct RtCapsuleDeclaration
{
	using StateType = RtCapsulePart;
	static constexpr const char* name = "rtCapsule";

	static const PhysicalObjectDescriptor& descriptor()
	{
		static const PhysicalObjectDescriptor capsule = joltTestRig::capsuleTemplate(joltTestRig::bit(joltTestRig::kCharacter),
			joltTestRig::bit(joltTestRig::kWorld) | joltTestRig::bit(joltTestRig::kCharacter), 1).descriptor;
		return capsule;
	}

	static const RtStaticData& staticDataOf(const RtStaticData& staticData) { return staticData; }
	static std::vector<QueryVolumeDescriptor> queryVolumes(const RtStaticData&) { return {}; }
	static glm::vec3 attachmentOffset(const RtStaticData&) { return glm::vec3(0.f); }

	static PhysicsBodyState& bodyStateOf(StateType& state) { return state.bodyState; }
	static const PhysicsBodyState& bodyStateOf(const StateType& state) { return state.bodyState; }

	PhysicsRuntimeBindings bindings;
};

static_assert(PhysicsDeclaration<RtRadialDeclaration, RtStaticData> && PhysicsDeclaration<RtCapsuleDeclaration, RtStaticData>);

using RtComposite = SimulationComposite<RtRadialDeclaration, RtCapsuleDeclaration>;
using RtState     = SimulationComposite<RtRadialPart, RtCapsulePart>;

class RtAllState
{
public:
	const RtState& getState() const { return m_state; }
	RtState& editState() { return m_state; }

private:
	RtState m_state;
};

class RtSim
{
public:
	using StateType = RtState;
	using InputType = RtInput;

	const RtAllState& getAllState() const { return m_all; }
	RtAllState& editAllState() { return m_all; }
	void updateVizState() { m_viz = m_all; }
	const RtAllState& getVizState() const { return m_viz; }

	const RtComposite& getPhysicsComposite() const { return m_composite; }
	RtComposite& editPhysicsComposite() { return m_composite; }

	void firstResimStep(JoltPhysicsBodyAdapter&, int32_t) {}
	void integrate(const SimulationTimeStep&, const RtInput&, JoltPhysicsBodyAdapter&, JoltSpatialQueryAdapter&, const RtStaticData&) {}

private:
	RtAllState  m_all;
	RtAllState  m_viz;
	RtComposite m_composite;
};

StaticWorldDescription rtArena()
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

struct RtChannelTransport
{
	void sendInputBundle(std::span<const std::byte>) {}
	void endInputSends() {}
};

using RtChannel = ogsimnet::SimNetChannel<RtSim, RtChannelTransport>;
} // namespace sessionRuntimeStartStopTest

template <>
struct SimulatableOwnerTraits<sessionRuntimeStartStopTest::RtSim>
{
	using PredictionOwnerType = sessionRuntimeStartStopTest::RtChannel;
	using AuthorityOwnerType  = sessionRuntimeStartStopTest::RtChannel;
};

namespace sessionRuntimeStartStopTest
{

class RtRunner
{
public:
	using Host = tickDriverFakes::FakeRunnerHost;

	template <typename F>
	RtRunner(tickDriverFakes::FakeEngine& engine, F&& body, bool highResolutionWait)
		: m_inner(engine,
			[this, run = std::function<void(RtRunner&)>(std::forward<F>(body))](ogsession::sessionTestKit::FakeSessionRunner&)
			{
				g_recorder.add("runner.body");
				run(*this);
			},
			highResolutionWait)
	{
	}

	Host& host() { return m_inner.host(); }
	const std::atomic<bool>& stopRequested() const { return m_inner.stopRequested(); }

	void join()
	{
		m_inner.join();
		g_runnerJoined.store(true);
		g_recorder.add("runner.join");
	}

	void waitForStart()
	{
		m_inner.waitForStart();
		g_recorder.add("runner.waitForStart");
	}

	void releaseStart()
	{
		g_recorder.add("runner.releaseStart");
		m_inner.releaseStart();
	}

private:
	ogsession::sessionTestKit::FakeSessionRunner m_inner;
};

class RtEngine : public tickDriverFakes::FakeEngine
{
public:
	using Runner = RtRunner;

	void logRouted(const char* line) { g_recorder.add("routed " + tagOf(line)); }
	bool wants(SimulationLogRoute, HostLogLevel) const { return true; }

	template <typename F>
	std::unique_ptr<Runner> startRunner(F&& body, bool highResolutionWait)
	{
		g_recorder.add("engine.startRunner");
		return std::make_unique<Runner>(*this, std::forward<F>(body), highResolutionWait);
	}
};

static_assert(ogsession::SessionEngine<RtEngine>, "RtEngine models SessionEngine");

struct RtClockEngine
{
	void logRouted(const char*) { g_recorder.add("clockEngine.lines"); }
	double nowSeconds() const { return 1.0; }
};

struct RtEngineTraits
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
	using WorldHost = JoltTickWorldHost<SimulatableT, std::mutex, RtEngine>;

	static std::optional<uint64_t> computeDeterminismFingerprint(void (*)(const char*)) { return std::nullopt; }

	static Statics splitStatics(const StaticWorldDescription& description, const std::vector<uint8_t>& physicsOnly)
	{
		return joltTickWorldHost::splitStatics(description, physicsOnly);
	}

	static void logEngineMessage(const char*) {}
};

struct RtComposition
{
	using Simulatables    = SimulatableList<RtSim>;
	using Storage         = apply_t<SimulationObjectStorage, Simulatables>;
	using Reconciliation  = apply_t<SimulationReconciliation, Simulatables>;
	using InputResolution = apply_t<SimulationInputResolution, Simulatables>;
	using NetSync         = apply_t<SimulationNetSync, Simulatables>;
	using RenderSnapshot  = RenderSnapshotT<kMaxSimulatableSlots * 2>;
	using Systems         = SystemList<>;
	using SystemsExec     = SimulationSystemsExecutor<Simulatables, RtStaticData>;
	using IntegrationExec = SimulationIntegrationExecutor<RtStaticData, JoltPhysicsBodyAdapter, JoltSpatialQueryAdapter, RtSim>;
	using Manager         = SimulationManager<IntegrationExec, NetSync, InputResolution, Reconciliation, SystemsExec, Storage,
		RtStaticData>;
};

struct RtGame
{
	using AuthorityPresentationSlice = int;

	static RtStaticData makeStaticData() { return RtStaticData{}; }

	static void emplaceSystems(std::optional<RtComposition::SystemsExec>& systems, JoltPhysicsBodyAdapter&, JoltSpatialQueryAdapter&)
	{
		g_recorder.add("game.emplaceSystems");
		systems.emplace();
	}

	static RtInput neutralInput() { return RtInput{}; }
	static void installGameLoggers(bool isAuthority) { g_recorder.add(isAuthority ? "game.installGameLoggers authority" : "game.installGameLoggers client"); }
	static std::string_view backendFingerprint() { return "fp"; }
	static BodyId rootBodyId(const RtSim&) { return BodyId{}; }
	static int authorityPresentationSlice_Step(const RtComposition::SystemsExec&, unsigned int) { return 0; }
};

struct RtNetConfig
{
	static constexpr int tickFrequencyHz = 60;
};

struct RtProvider
{
};

struct RtDiagnostics
{
	using InputProvider = RtProvider;

	void stampAfterStep_Internal(double) { g_recorder.add("diag.stampAfterStep"); }
	void stampStepEnd_Internal() { g_recorder.add("diag.stampStepEnd"); }
	void stampCorrectionHanded_Internal(unsigned int, uint32_t, double) {}
	void noteWireSample_Internal(double) {}
	void stampHandedAndLeft_Internal(uint32_t, uint32_t) {}
};

struct RtEndpoint
{
	ognet::EndpointConfig endpointConfig;
	const ognet::EndpointConfig& config() const { return endpointConfig; }
};

struct RtTransport
{
};

struct RtClockServer
{
	void publishServerTick(uint32_t, double) {}
};

struct RtClockClient
{
	ogsimnet::SimNetClockStats stats() const { return {}; }
};

struct RtBindings
{
	ogsimnet::BindVerdict markRegisteredIfReady(uint8_t, bool) { return {}; }
	bool markUnregistered(uint8_t) { return false; }
	ogsimnet::BindVerdict verdict(uint8_t, bool) const { return {}; }
};

struct RtClient
{
	using Channel = RtChannel;

	template <typename... ArgTs>
	void bindLifecycle(ArgTs&...) { g_recorder.add("client.bindLifecycle"); }

	template <typename SimT, typename ProviderT, typename AppliedT>
	void registerCharacter(uint8_t, SimT&&, ProviderT&&, AppliedT) {}

	template <typename AppliedT>
	void unregisterCharacter(uint8_t, AppliedT) {}

	template <typename NetSyncT, typename ConfigT>
	void afterCaptureTick_Step(NetSyncT&, uint32_t, const ConfigT&) {}
};

struct RtServerHost
{
	template <typename RuntimeT>
	RtServerHost(RtEndpoint&, RuntimeT*) { g_recorder.add("serverHost.ctor"); }
	~RtServerHost() { g_recorder.add("serverHost.dtor"); }
};

struct RtServer
{
	template <typename SimT>
	using Channel = RtChannel;

	RtServer(RtServerHost&, RtTransport&, const TimeConfig&, std::size_t) { g_recorder.add("server.ctor"); }

	~RtServer()
	{
		if (!g_runnerJoined.load())
			g_serverDestroyedBeforeJoin.store(true);
		g_recorder.add("server.dtor");
	}

	template <typename... ArgTs>
	void bindLifecycle(ArgTs&...) { g_recorder.add("server.bindLifecycle"); }

	void publishFloor(uint8_t) { g_recorder.add("server.publishFloor"); }

	template <typename SimT, typename AppliedT>
	void registerCharacter(uint8_t, uint8_t, SimConnectionId, uint8_t, std::string, SimT&&, AppliedT) {}

	template <typename SimT, typename AppliedT>
	void unregisterCharacter(uint8_t, ogsimnet::RosterRemoveReason, AppliedT) {}

	size_t drain() { return 0u; }
	void releaseForStep(int32_t, int32_t, std::optional<double>) {}
	const std::vector<SimConnectionId>& attachedConnections() const { return m_attached; }
	void publishClock(uint32_t) {}
	void afterInlineBatch(int32_t) {}

	template <typename SimT>
	RtChannel* channel(uint8_t) { return nullptr; }

	std::vector<SimConnectionId> m_attached;
};

struct RtIdle
{
	explicit RtIdle(RtEndpoint&) { g_recorder.add("idle.ctor"); }
	~RtIdle() { g_recorder.add("idle.dtor"); }

	IdleWake idleUntil(double deadlineSeconds)
	{
		engine->now = std::max(engine->now, deadlineSeconds);
		return IdleWake::Deadline;
	}

	inline static RtEngine* engine = nullptr;
};

struct RtLink
{
	using Transport  = RtTransport;
	using ServerHost = RtServerHost;
	using Client     = RtClient;

	bool                 server = false;
	RtEndpoint           laneEndpoint;
	RtTransport          laneTransport;
	RtClockServer        clockServerObject;
	RtClockClient        clockClientObject;
	RtBindings           laneBindings;
	RtClient             laneClient;
	bool                 bound = false;
	std::function<void()> stopHandler;

	bool isServer() const { return server; }
	bool hostServiced() const { return server; }
	RtEndpoint& endpoint() { return laneEndpoint; }
	RtTransport& transport() { return laneTransport; }
	RtClockServer* clockServer() { return server ? &clockServerObject : nullptr; }
	const RtClockClient* clockClient() const { return server ? nullptr : &clockClientObject; }
	double ackSrttSeconds() const { return -1.0; }
	RtClient* client() { return server ? nullptr : &laneClient; }
	RtBindings& editBindings() { return laneBindings; }
	const RtBindings& bindings() const { return laneBindings; }
	bool clientSessionBound() const { return bound; }
	void drainClient_Step() {}
	void drainClientWhileStopped_GameThread() { g_recorder.add("link.drainWhileStopped"); }

	template <typename JoinF>
	void bindAuthorityRunner(JoinF&&) { g_recorder.add("link.bindAuthorityRunner"); }

	template <typename F>
	void setStopHandler(F&& handler)
	{
		stopHandler = std::forward<F>(handler);
		g_recorder.add(stopHandler ? "link.setStopHandler" : "link.clearStopHandler");
	}

	void unbindServerSession() { g_recorder.add("link.unbindServerSession"); }

	template <typename F>
	void setOnRegisteredRemoved(F&&) { g_recorder.add("link.setOnRegisteredRemoved"); }

	template <typename RuntimeT>
	void bindClientSession(RuntimeT&)
	{
		bound = true;
		g_recorder.add("link.bindClientSession");
	}

	void unbindClientSession()
	{
		bound = false;
		g_recorder.add("link.unbindClientSession");
	}
};

struct RtHost
{
	void* driverAddress = nullptr;

	void onConfigure() { g_recorder.add("host.onConfigure"); }
	void bindDiagnosticGates() { g_recorder.add("host.bindDiagnosticGates"); }
	void afterPredictionConfigured() { g_recorder.add("host.afterPredictionConfigured"); }
	void onSessionConfigured() { g_recorder.add("host.onSessionConfigured"); }

	template <typename DriverT>
	void beginFrames(DriverT& driver)
	{
		driverAddress = &driver;
		g_recorder.add("host.beginFrames");
	}

	void endFrames() { g_recorder.add("host.endFrames"); }
	void sampleInput() {}
	void onPostPhysics() {}
	void logHost(const char* line) { g_recorder.add("logHost " + tagOf(line)); }
	ogsession::SessionBodyRequest<RtStaticData> makeBodyRequest(const ogsession::RegisterRequest&) { return {}; }
	uint64_t bodyKeyOf(ogsession::EntityKey key) const { return key.value; }
	RtProvider wrapInputProvider(const ogsession::RegisterRequest&, RtProvider&& provider) { return provider; }
	void teardownComponents() { g_recorder.add("host.teardownComponents"); }
	void forgetComponents() { g_recorder.add("host.forgetComponents"); }
	void onLinkStopping(bool) {}

	template <typename F>
	void forEachRegisteredId(F&&) const {}
};

struct RtTraits
{
	using Host         = RtHost;
	using Game         = RtGame;
	using Composition  = RtComposition;
	using EngineTraits = RtEngineTraits;
	using StaticData   = RtStaticData;
	using Simulatable  = RtSim;
	using PlayerInput  = RtInput;
	using NetConfig    = RtNetConfig;
	using Engine       = RtEngine;
	using ClockEngine  = RtClockEngine;
	using Link         = RtLink;
	using Diagnostics  = RtDiagnostics;
	using Server       = RtServer;
	using Idle         = RtIdle;

	static JoltTickWorldHostConfig worldConfig(bool isAuthority, const StaticData&)
	{
		JoltTickWorldHostConfig config;
		config.isAuthority = isAuthority;
		config.mappedCategories = joltTestRig::kAllMappedCategories;
		return config;
	}
};

using Runtime = ogsession::SessionRuntime<RtTraits>;

struct Rig
{
	explicit Rig(bool authority)
	{
		g_recorder.clear();
		g_runnerJoined.store(false);
		g_serverDestroyedBeforeJoin.store(false);
		RtIdle::engine = &frameEngine;
		link.server = authority;
		runtime.emplace(host, diagnostics, frameEngine);
	}

	ogsession::SessionStart startOf(bool authority) const
	{
		ogsession::SessionStart start;
		start.role = authority ? ogsession::SessionRole::Authority : ogsession::SessionRole::Prediction;
		start.stepSeconds = static_cast<double>(joltTestRig::kDt);
		start.requestedMaxCatchUpSteps = 60;
		start.staticWorld = rtArena();
		start.physicsOnlyShapes.assign(start.staticWorld.shapes.size(), 0u);
		return start;
	}

	RtEngine                                 frameEngine;
	RtHost                                   host;
	RtDiagnostics                            diagnostics;
	RtLink                                   link;
	ogsession::sessionTestKit::MapConfigSource config;
	std::optional<Runtime>                   runtime;
};

struct Step
{
	const char* event;
	const char* source;
};

void checkOrder(const std::vector<std::string>& recorded, const std::vector<Step>& transcription)
{
	std::string expected;
	for (const Step& step : transcription)
		expected += std::string(step.event) + "  <- " + step.source + "\n";
	std::string got;
	for (const std::string& event : recorded)
		got += event + "\n";
	INFO("transcription (the Unreal session at the step (a) commit, OGSimulation 5224ad5):\n" << expected);
	INFO("recorded:\n" << got);
	CHECK(recorded.size() == transcription.size());
	for (std::size_t index = 0u; index < recorded.size() && index < transcription.size(); ++index)
		CHECK(recorded[index] == transcription[index].event);
}

const std::vector<Step> kAuthorityStartTranscription = {
	{ "engine.startRunner", "TickDriverBackendUImpl.h:212 (5224ad5), runAuthorityLoop_GameThread's MakeUnique<FUEAuthorityRunner>" },
	{ "runner.body", "TickDriverBackendUImpl.h:213, the runner's body" },
	{ "host.onConfigure", "SessionRuntimeStartStop.h:60, registerInstance()" },
	{ "game.emplaceSystems", "SessionRuntimeStartStop.h:63" },
	{ "serverHost.ctor", "UESimulationSession.h:993, beginServerSimNet (from SessionRuntimeStartStop.h:78)" },
	{ "server.ctor", "UESimulationSession.h:994" },
	{ "server.bindLifecycle", "UESimulationSession.h:996" },
	{ "idle.ctor", "UESimulationSession.h:998" },
	{ "link.bindAuthorityRunner", "UESimulationSession.h:999" },
	{ "link.setStopHandler", "UESimulationSession.h:1001" },
	{ "logHost [SimNet.Host]", "UESimulationSession.h:1002, logSimNetHostLine(true)" },
	{ "clockEngine.lines", "SessionRuntimeStartStop.h:80, applySessionKnobs's lines" },
	{ "server.publishFloor", "SessionRuntimeStartStop.h:80, applySessionKnobs's floor" },
	{ "clockEngine.lines", "SessionRuntimeStartStop.h:81-84, the floor advisory and the tier consumer's first delay line" },
	{ "game.installGameLoggers authority", "SessionRuntimeStartStop.h:87" },
	{ "host.bindDiagnosticGates", "SessionRuntimeStartStop.h:88, bindCorrectionFieldDiffGate()" },
	{ "clockEngine.lines", "SessionRuntimeStartStop.h:126-128, the resim policy's and the latency knobs' lines" },
	{ "host.onSessionConfigured", "SessionRuntimeStartStop.h:131, m_game.afterSessionConfigured" },
	{ "routed [SimHost.Backend]", "SessionRuntimeStartStop.h:135" },
	{ "runner.releaseStart", "TickDriverBackendUImpl.h:223, the driver started on the runner" },
	{ "runner.waitForStart", "TickDriverBackendUImpl.h:229" },
	{ "host.beginFrames", "TickDriverBackendUImpl.h:233, m_frameHost.begin" },
	{ "routed SimulationManager: adapters, integration layer, and manager initialized", "SessionRuntimeStartStop.h:146" },
};

const std::vector<Step> kPredictionStartTranscription = {
	{ "host.onConfigure", "SessionRuntimeStartStop.h:92, registerInstance()" },
	{ "game.emplaceSystems", "SessionRuntimeStartStop.h:95" },
	{ "clockEngine.lines", "SessionRuntimeStartStop.h:109, the tier consumer's first delay line" },
	{ "link.setOnRegisteredRemoved", "UESimulationSession.h:1136, beginClientSimNet (from SessionRuntimeStartStop.h:111)" },
	{ "link.setStopHandler", "UESimulationSession.h:1137" },
	{ "client.bindLifecycle", "UESimulationSession.h:1138" },
	{ "link.bindClientSession", "UESimulationSession.h:1139" },
	{ "logHost [SimNet.Host]", "UESimulationSession.h:1140, logSimNetHostLine(false)" },
	{ "game.installGameLoggers client", "SessionRuntimeStartStop.h:113" },
	{ "host.bindDiagnosticGates", "SessionRuntimeStartStop.h:114" },
	{ "host.afterPredictionConfigured", "SessionRuntimeStartStop.h:118, the [ResimProbe.Session] line" },
	{ "clockEngine.lines", "SessionRuntimeStartStop.h:126-128" },
	{ "host.onSessionConfigured", "SessionRuntimeStartStop.h:131" },
	{ "routed [SimHost.Backend]", "SessionRuntimeStartStop.h:135" },
	{ "host.beginFrames", "TickDriverBackendUImpl.h:438, startDriverStepping's m_frameHost.begin (from SessionRuntimeStartStop.h:154)" },
	{ "routed SimulationManager: adapters, integration layer, and manager initialized", "SessionRuntimeStartStop.h:155" },
};

const std::vector<Step> kAuthorityStopTranscription = {
	{ "host.endFrames", "TickDriverBackendUImpl.h:255, onEndPlayBegin (from SessionRuntimeStartStop.h:162)" },
	{ "runner.join", "TickDriverBackendUImpl.h:256, stopAuthorityLoop_GameThread" },
	{ "host.teardownComponents", "SessionRuntimeStartStop.h:170, teardownImmediateClient_GameThread" },
	{ "link.clearStopHandler", "UESimulationSession.h:1176, detachFromSimNetLanes (from SessionRuntimeStartStop.h:172)" },
	{ "link.unbindServerSession", "UESimulationSession.h:1179" },
	{ "idle.dtor", "SessionRuntimeStartStop.h:175" },
	{ "server.dtor", "SessionRuntimeStartStop.h:177" },
	{ "serverHost.dtor", "SessionRuntimeStartStop.h:178" },
	{ "host.forgetComponents", "SessionRuntimeStartStop.h:179, m_registeredComponentsById.clear() and the maps after it" },
};

const std::vector<Step> kPredictionStopTranscription = {
	{ "host.endFrames", "TickDriverBackendUImpl.h:255 (from SessionRuntimeStartStop.h:162)" },
	{ "diag.stampAfterStep", "TickDriverBackendUImpl.h:258, m_tickBackend.stop() waits for the outstanding step" },
	{ "diag.stampStepEnd", "TickDriverBackendUImpl.h:258" },
	{ "host.teardownComponents", "SessionRuntimeStartStop.h:170" },
	{ "link.clearStopHandler", "UESimulationSession.h:1176 (from SessionRuntimeStartStop.h:172)" },
	{ "link.setOnRegisteredRemoved", "UESimulationSession.h:1186, the cleared callback" },
	{ "link.unbindClientSession", "UESimulationSession.h:1187" },
	{ "host.forgetComponents", "SessionRuntimeStartStop.h:179" },
};

std::vector<std::string> withoutSteps(const std::vector<std::string>& events, const std::vector<std::thread::id>& threads)
{
	std::vector<std::string> kept;
	bool runnerReleased = false;
	const std::thread::id frame = std::this_thread::get_id();
	for (size_t index = 0; index < events.size(); ++index)
	{
		const std::string& event = events[index];
		const bool steppingRunner = runnerReleased && threads[index] != frame;
		if (event == "runner.releaseStart")
			runnerReleased = true;
		if (event.rfind("diag.", 0) != 0 && !steppingRunner)
			kept.push_back(event);
	}
	return kept;
}
std::vector<std::string> withoutRunnerLinesBeforeTheJoin(const std::vector<std::string>& events)
{
	std::vector<std::string> kept;
	bool joined = false;
	for (const std::string& event : events)
	{
		if (event == "runner.join")
			joined = true;
		const bool runnerLine = event.rfind("routed ", 0) == 0 || event.rfind("logHost ", 0) == 0 || event == "clockEngine.lines"
			|| event.rfind("diag.", 0) == 0;
		if (joined || !runnerLine)
			kept.push_back(event);
	}
	return kept;
}
std::vector<std::string> stepsCollapsed(const std::vector<std::string>& events)
{
	std::vector<std::string> kept;
	for (const std::string& event : events)
	{
		if (event.rfind("logHost ", 0) == 0)
			continue;
		const bool repeatedStep = kept.size() >= 2u && event == "diag.stampAfterStep" && kept.back() == "diag.stampStepEnd";
		if (repeatedStep)
		{
			kept.pop_back();
			kept.pop_back();
		}
		kept.push_back(event);
	}
	return kept;
}
} // namespace sessionRuntimeStartStopTest

using namespace sessionRuntimeStartStopTest;

TEST_CASE("SessionRuntime.TheAuthoritysStartAndStopRunInTheUnrealSessionsOrder", "[Session][SessionRuntime]")
{
	Rig rig(true);
	rig.runtime->start(rig.startOf(true), rig.config, &rig.link);
	checkOrder(withoutSteps(g_recorder.events(), g_recorder.threads()), kAuthorityStartTranscription);
	REQUIRE(rig.host.driverAddress != nullptr);

	g_recorder.clear();
	CHECK(rig.runtime->stop());
	checkOrder(withoutRunnerLinesBeforeTheJoin(g_recorder.events()), kAuthorityStopTranscription);
	CHECK(g_runnerJoined.load());
	CHECK_FALSE(g_serverDestroyedBeforeJoin.load());
}

TEST_CASE("SessionRuntime.ThePredictionsStartAndStopRunInTheUnrealSessionsOrderAndTheStopWaitsForTheStep", "[Session][SessionRuntime]")
{
	Rig rig(false);
	rig.runtime->start(rig.startOf(false), rig.config, &rig.link);
	checkOrder(withoutSteps(g_recorder.events(), g_recorder.threads()), kPredictionStartTranscription);
	REQUIRE(rig.host.driverAddress != nullptr);

	auto& driver = *static_cast<Runtime::Driver*>(rig.host.driverAddress);
	rig.frameEngine.advance(static_cast<double>(joltTestRig::kDt));
	driver.startFrame(static_cast<double>(joltTestRig::kDt));
	REQUIRE(rig.frameEngine.queuedTasks() == 1u);

	g_recorder.clear();
	CHECK(rig.runtime->stop());
	CHECK(rig.frameEngine.queuedTasks() == 0u);
	checkOrder(stepsCollapsed(g_recorder.events()), kPredictionStopTranscription);
}
#endif
