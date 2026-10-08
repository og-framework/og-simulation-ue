// SPDX-License-Identifier: MPL-2.0
// og-simulation-jolt/OGSimulationJolt/docs/OGSimulationJolt-rationale.md
using System.IO;
using UnrealBuildTool;

public class OGSimulationJolt : ModuleRules
{
	public OGSimulationJolt(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		IWYUSupport = IWYUSupport.None;
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "glm", "OGSimulation" });

		string LibraryRoot = Path.Combine(ModuleDirectory, "og-simulation-jolt");
		PublicIncludePaths.Add(LibraryRoot);
		PublicIncludePaths.Add(Path.Combine(LibraryRoot, "ThirdParty", "JoltPhysics"));

		PublicDefinitions.Add("JPH_OBJECT_LAYER_BITS=16");

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("JPH_USE_SSE4_1=1");
			PublicDefinitions.Add("JPH_USE_SSE4_2=1");
		}
	}
}
