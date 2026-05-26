// SPDX-License-Identifier: MPL-2.0
using System.IO;
using UnrealBuildTool;

public class glm : ModuleRules
{
	public glm(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		// glm headers live in OGSimulation's og-simulation subtree (no extern/ or Source/ wrapper)
		string GlmDir = Path.Combine(ModuleDirectory, "..", "OGSimulation", "og-simulation", "glm");
		PublicIncludePaths.Add(Path.Combine(GlmDir, ".."));
		bUseUnity = false;
	}
}
