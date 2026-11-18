// SPDX-License-Identifier: MPL-2.0

using UnrealBuildTool;

public class OGSimulationUnreal : ModuleRules
{
	public OGSimulationUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		FPSemantics = FPSemanticsMode.Precise;
		PrivatePCHHeaderFile = EngineDirectory + "/Source/Runtime/Engine/Public/EngineSharedPCH.h";
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Chaos", "Core", "PhysicsCore", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "OGSimulation", "OGNet", "OGNetUnreal", "OGSimulationNet" });
		SetupIrisSupport(Target);
	}
}
