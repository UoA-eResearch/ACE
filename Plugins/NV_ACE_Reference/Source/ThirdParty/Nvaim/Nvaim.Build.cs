/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;

public class Nvaim : ModuleRules
{
	/// Adds a single model directory to the runtime dependencies.
	/// Called by modules within this plugin, could also be called by external plugins.
	public static void AddModelDependencies(ModuleRules Module, string AIMFeature)
	{
		// assumes that plugin stores its models under a specific path
		string AIMModelPath = Path.Combine(Module.PluginDirectory, "ThirdParty", "Nvigi", "Models", AIMFeature);
		IEnumerable<string> AIMModels = Directory.EnumerateFiles(AIMModelPath, "*.*", SearchOption.AllDirectories);
		foreach (string AIMModel in AIMModels)
		{
			Module.RuntimeDependencies.Add(AIMModel);
		}
	}

	/// Adds all of a plugin's model files to the runtime dependencies.
	/// Helper function to be called by external plugins.
	public static void AddAllPluginModelDependencies(ModuleRules Module)
	{
		// assumes that plugin stores its models under a specific path
		string AIMModelPath = Path.Combine(Module.PluginDirectory, "ThirdParty", "Nvigi", "Models");
		IEnumerable<string> AIMModels = Directory.EnumerateFiles(AIMModelPath, "*.*", SearchOption.AllDirectories);
		foreach (string AIMModel in AIMModels)
		{
			Module.RuntimeDependencies.Add(AIMModel);
		}
	}

	public static void AddBinaryDependencies(ModuleRules Module, ReadOnlyTargetRules Target, IEnumerable<string> Libs)
	{
		string BinDir = GetBinDir(Target);
		foreach (string Lib in Libs)
		{
			string LibPath = Path.Combine(BinDir, Lib);
			Module.RuntimeDependencies.Add(LibPath);
		}
	}

	public static string GetBinDir(ReadOnlyTargetRules Target)
	{
		PluginInfo BasePlugin = UnrealBuildTool.Plugins.GetPlugin("NV_ACE_Reference");

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			return Path.Combine(BasePlugin.Directory.FullName, "ThirdParty", "Nvigi", "Binaries", "Linux");
		}
		else if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			return Path.Combine(BasePlugin.Directory.FullName, "ThirdParty", "Nvigi", "Binaries", "Win64");
		}
		else
		{
			// unreachable code. if we ever reach this point we've messed up something with the supported platform logic
			throw new System.NotImplementedException();
		}
	}

	protected virtual bool IsSupportedTarget(ReadOnlyTargetRules Target)
	{
#if (UE_5_5_OR_LATER && !UE_5_7_OR_LATER)
		return IsSupportedPlatform(Target);
#else
		return false;
#endif
	}
	protected virtual bool IsSupportedWindowsPlatform(ReadOnlyTargetRules Target)
	{
		return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows);
	}
	protected virtual bool IsSupportedPlatform(ReadOnlyTargetRules Target)
	{
		return IsSupportedWindowsPlatform(Target) || Target.Platform == UnrealTargetPlatform.Linux;
	}

	public Nvaim(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (IsSupportedTarget(Target))
		{
#if UE_5_3_OR_LATER
			// UE 5.3 added the ability to override bWarningsAsErrors.
			// Treat all warnings as errors only on supported targets/engine versions >= 5.3.
			// Anyone else is in uncharted territory and should use their own judgment :-)
			bWarningsAsErrors = true;
#endif
		}

		if (IsSupportedPlatform(Target))
		{
			// headers
			PublicIncludePaths.Add(Path.Combine(PluginDirectory, "ThirdParty", "Nvigi", "Public"));

			// platform-specific binary locations
			if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PublicDefinitions.Add("AIM_CORE_BINARY_NAME=TEXT(\"nvaim.core.framework.so\")");
				// Add lib folder to RPATH. Probably not necessary with our current workarounds but it can't hurt.
				// Note that on Windows this currently only works from plugins installed to the engine. Needs more exploration
				PublicRuntimeLibraryPaths.Add(GetBinDir(Target));
				string[] Deps = {"nvaim.core.framework.so"};
				AddBinaryDependencies(this, Target, Deps);
			}
			else if (IsSupportedWindowsPlatform(Target))
			{
				PublicDefinitions.Add("AIM_CORE_BINARY_NAME=TEXT(\"nvaim.core.framework.dll\")");
				string[] Deps = {"nvaim.core.framework.dll"};
				AddBinaryDependencies(this, Target, Deps);
			}
			else
			{
				// unreachable code. if we ever reach this point we've messed up something with the supported platform logic
				throw new System.NotImplementedException();
			}
		}
	}
}
