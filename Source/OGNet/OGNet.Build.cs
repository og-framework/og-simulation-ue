// SPDX-License-Identifier: MPL-2.0
// og-net/OGNet/docs/OGNet-rationale.md
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class OGNet : ModuleRules
{
	public OGNet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		OptimizationLevel = OptimizationMode.Speed;
		bEnableExceptions = false;
		bUseRTTI = false;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });

		string LibraryRoot = Path.Combine(ModuleDirectory, "og-net");
		PublicIncludePaths.Add(LibraryRoot);
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(LibraryRoot, "OGNet")));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[] { "ws2_32.lib", "bcrypt.lib" });
		}
	}
}
