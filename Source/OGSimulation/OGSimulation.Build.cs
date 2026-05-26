// SPDX-License-Identifier: MPL-2.0
using System.IO;
using UnrealBuildTool;

public class OGSimulation : ModuleRules
{
	public OGSimulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "glm" });

		// og-simulation sits directly under this module directory (no extern/ or Source/ wrapper).
		string PureLib = Path.Combine(ModuleDirectory, "og-simulation");
		PublicIncludePaths.Add(Path.Combine(PureLib, "OGSimulation", ".."));
	}
}
