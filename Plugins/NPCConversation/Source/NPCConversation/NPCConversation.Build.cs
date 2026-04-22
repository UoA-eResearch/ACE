// Copyright UoA eResearch. MIT License.

using System.IO;
using UnrealBuildTool;

public class NPCConversation : ModuleRules
{
	public NPCConversation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Standalone C++ core (no UE deps) — WAV encoding utilities.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "..", "Core"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
			"DeveloperSettings",
			"AudioCapture",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AudioMixerCore",
		});
	}
}
