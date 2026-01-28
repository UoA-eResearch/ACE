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
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


namespace nvaim
{
	using Result = uint32_t;
	struct CudaParameters;
	struct InferenceInterface;
	struct alignas(8) PluginID;
}


class AIMWRAPPER_API FAIMModule : public IModuleInterface
{
public:

	static FAIMModule& Get()
	{
		return FModuleManager::GetModuleChecked<FAIMModule>(FName("AIMWrapper"));
	}

	/** IModuleInterface implementation */

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** AIM-specific things */

	// Call RegisterAIMFeature before loading any AIM feature.
	// Note that current versions of AIM may not support AIMBinaryDirectories. It's reserved for future use.
	void RegisterAIMFeature(nvaim::PluginID Feature, const TArray<FString>& AIMBinaryDirectories, const TArray<nvaim::PluginID>& IncompatibleFeatures);

	// will check whether an AIM feature is available, without logging any errors or warnings if it isn't available
	bool IsAIMFeatureAvailable(nvaim::PluginID Feature);
	// you may use bShushAIMLog to downgrade AIM log errors and warnings to normal log messages during loading
	nvaim::Result LoadAIMFeature(nvaim::PluginID Feature, nvaim::InferenceInterface** Interface, bool bShushAIMLog = false);
	nvaim::Result UnloadAIMFeature(nvaim::PluginID Feature, nvaim::InferenceInterface* Interface);

	const FString GetModelDirectory() const { return AIMModelDirectory; }
	nvaim::CudaParameters* GetCIGCudaParameters();

private:
	FString AIMModelDirectory{};
	TUniquePtr<class FAIMFeatureRegistry> AIMFeatures;
};

// Convert nvaim::Result to a readable message
AIMWRAPPER_API FString GetAIMStatusString(nvaim::Result Result);

