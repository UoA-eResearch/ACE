// Copyright UoA eResearch. MIT License.

using UnrealBuildTool;

public class NPCConversationTests : ModuleRules
{
	public NPCConversationTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NPCConversation",
			"HTTP",
			"Json",
			"JsonUtilities",
		});
	}
}
