// Copyright 2025 JOBUTSU LTD. All rights reserved.
using UnrealBuildTool;
using System.IO;

public class ZenBlinkEditor : ModuleRules
{
    public ZenBlinkEditor(ReadOnlyTargetRules Target) : base(Target)
    {


    PublicDependencyModuleNames.AddRange(new string[] {
        "Core",
        "CoreUObject",
        "Engine",
        "ZenBlink"
        });

    PrivateDependencyModuleNames.AddRange(new string[] {
        "PropertyEditor",
        "Slate",
        "SlateCore",
        "UnrealEd"
        });


    }
}