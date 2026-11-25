// SPDX-License-Identifier: MPL-2.0
// og-sim-session/OGSimSession/docs/OGSimSession-rationale.md
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class OGSimulationSession : ModuleRules
{
	public OGSimulationSession(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		FPSemantics = FPSemanticsMode.Precise;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDependencyModuleNames.AddRange(new string[] { "OGSimulation", "OGTickDriver", "OGSimulationNet" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });

		string LibraryRoot = Path.Combine(ModuleDirectory, "og-sim-session");
		PublicIncludePaths.Add(LibraryRoot);
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(LibraryRoot, "OGSimSession")));
	}
}
