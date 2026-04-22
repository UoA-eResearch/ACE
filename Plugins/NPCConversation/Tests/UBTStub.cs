// Minimal UnrealBuildTool stubs for syntax-checking Build.cs files without
// a full UE installation.  Used by the CI build-cs-check job.
// This file is NOT part of the shipping plugin.
using System.Collections.Generic;

namespace UnrealBuildTool
{
    public enum PCHUsageMode
    {
        UseExplicitOrSharedPCHs
    }

    public class ReadOnlyTargetRules { }

    public abstract class ModuleRules
    {
        public PCHUsageMode    PCHUsage;
        public List<string>    PublicDependencyModuleNames  = new List<string>();
        public List<string>    PrivateDependencyModuleNames = new List<string>();
        public List<string>    PublicIncludePaths           = new List<string>();
        public List<string>    PrivateIncludePaths          = new List<string>();
        public bool            bEnableExceptions;
        protected string       ModuleDirectory              = string.Empty;

        protected ModuleRules(ReadOnlyTargetRules Target) { }
    }
}
