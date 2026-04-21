// Copyright UoA eResearch. MIT License.

using UnrealBuildTool;

public class NPCConversation : ModuleRules
{
	public NPCConversation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
