// SPDX-License-Identifier: MPL-2.0
// docs/OGNetUnreal-rationale.md
using UnrealBuildTool;

public class OGNetUnreal : ModuleRules
{
	public OGNetUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "NetCore", "Sockets", "PacketHandler", "OGNet" });
	}
}
