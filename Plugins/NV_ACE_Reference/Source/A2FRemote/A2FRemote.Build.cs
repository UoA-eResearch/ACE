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

using UnrealBuildTool;

public class A2FRemote : ModuleRules
{
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

	public A2FRemote(ReadOnlyTargetRules Target) : base(Target)
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
				"HTTP",

				// local modules
				"A2FCommon",
				"AIMWrapper",
				"Nvaim",
			}
		);

		if (IsSupportedPlatform(Target))
		{
			Nvaim.AddModelDependencies(this, "nvaim.plugin.a2x.cloud");
			if (IsSupportedWindowsPlatform(Target))
			{
				string[] Deps = { "nvaim.plugin.a2x.cloud.grpc.dll", "abseil_dll.dll", "cares.dll", "libcrypto-3-x64.dll", "libprotobuf.dll", "libssl-3-x64.dll", "re2.dll", "zlib1.dll" };
				Nvaim.AddBinaryDependencies(this, Target, Deps);
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				string[] Deps = {"nvaim.plugin.a2x.cloud.grpc.so", "libz.so"};
				Nvaim.AddBinaryDependencies(this, Target, Deps);
			}
			else
			{
				// need to point to location of any supported platform's binaries
				throw new System.NotImplementedException();
			}
		}
	}
}

