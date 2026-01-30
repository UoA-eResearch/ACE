// Copyright 2025 JOBUTSU LTD. All rights reserved.

using UnrealBuildTool;
using System.IO;

public class ZenBlink : ModuleRules
{
    public ZenBlink(ReadOnlyTargetRules Target) : base(Target)
    {

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

    
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",

            }
        );
    }
}