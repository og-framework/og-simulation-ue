// SPDX-License-Identifier: MPL-2.0

using UnrealBuildTool;

public class OGSimulationUnreal : ModuleRules
{
	public OGSimulationUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Chaos", "Core", "PhysicsCore", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "OGSimulation" });
		SetupIrisSupport(Target);
	}
}
