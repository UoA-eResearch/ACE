/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class A2FLocal : ModuleRules
{
	/// <summary>
	/// Adds Audio2Face-3D local execution binaries and a single model directory to the runtime dependencies.
	/// Called by modules that depend on this module, and implement an actual Audio2Face-3D local provider.
	/// </summary>
	/// <param name="Module">pass in "this"</param>
	/// <param name="Target">pass in "Target"</param>
	public static void AddAIMDependencies(ModuleRules Module, ReadOnlyTargetRules Target)
	{
		// add binaries
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			string[] Deps = {"nvaim.plugin.a2e.trt.cuda.dll", "nvaim.plugin.a2f.trt.cuda.dll", "nvaim.plugin.a2x.pipeline.dll", "audio2x.dll",
				"cublas64_12.dll", "cublasLt64_12.dll", "nvinfer_10.dll", "nvinfer_plugin_10.dll", "nvaim.plugin.hwi.cuda.dll", "zlib1.dll",
				"zlibwapi.dll"};
			Nvaim.AddBinaryDependencies(Module, Target, Deps);
		}
		else
		{
			// this module doesn't support that target platform
			throw new System.NotImplementedException();
		}

		// add model files, assumes that plugin stores its models under a specific path
		string AIMModelPath = Path.Combine(Module.PluginDirectory, "ThirdParty", "Nvigi", "Models");
		IEnumerable<string> AIMModels = Directory.EnumerateFiles(AIMModelPath, "*.*", SearchOption.AllDirectories);
		foreach (string AIMModel in AIMModels)
		{
			Module.RuntimeDependencies.Add(AIMModel);
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
		return IsSupportedWindowsPlatform(Target);
	}

	public A2FLocal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (IsSupportedTarget(Target))
		{
#if UE_5_3_OR_LATER
			// UE 5.3 added the ability to override bWarningsAsErrors.
			// Treat all warnings as errors only on supported targets/engine versions >= 5.3.
			// Anyone else is in uncharted territory and should use their own judgment :-)
			bWarningsAsErrors = true;
#endif
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ACECore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// engine modules
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"Projects",

				// local modules
				"A2FCommon",
				"AIMWrapper",
				"Nvaim",
			}
		);

	}
}

