// SPDX-License-Identifier: MPL-2.0
// ../OGSimulation/og-simulation/og-tickdriver/OGTickDriver/docs/OGTickDriver-rationale.md
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class OGTickDriver : ModuleRules
{
	public OGTickDriver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		FPSemantics = FPSemanticsMode.Precise;
		PrivatePCHHeaderFile = EngineDirectory + "/Source/Runtime/Core/Public/CoreSharedPCH.h";

		PublicDependencyModuleNames.AddRange(new string[] { "OGSimulation" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });

		string LibraryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "OGSimulation", "og-simulation", "og-tickdriver"));
		PublicIncludePaths.Add(LibraryRoot);
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(LibraryRoot, "OGTickDriver")));
	}
}
