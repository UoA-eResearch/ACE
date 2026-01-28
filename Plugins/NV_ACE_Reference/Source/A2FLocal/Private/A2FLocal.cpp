/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "A2FLocal.h"

// engine includes
#include "Containers/StringConv.h"

// plugin includes
#include "A2FLocalModule.h"
#include "A2FLocalPrivate.h"
#include "AIMA2FContext.h"
#include "AIMModule.h"
#include "AnimDataConsumerRegistry.h"
#include "nvaim.h"
#include "nvaim_a2e.h"
#include "nvaim_a2f.h"
#include "nvaim_a2x.h"
#include "nvaim_cuda.h"


//////
// FA2FLocal implementation

FA2FLocal::FA2FLocal(FString InModelDir, FString InModelGUID, FName InProviderName, const TMap<FString, float>& InFaceParameterDefaults)
	: ModelDir(InModelDir), ModelGUID(InModelGUID), ProviderName(InProviderName), FaceParameterDefaults(InFaceParameterDefaults)
{
	static bool bIsRegistered = false;
	if (!bIsRegistered)
	{
		FAIMModule::Get().RegisterAIMFeature(nvaim::plugin::a2x::pipeline::kId, {}, {});
		FAIMModule::Get().RegisterAIMFeature(nvaim::plugin::a2e::trt::cuda::kId, {}, {});
		FAIMModule::Get().RegisterAIMFeature(nvaim::plugin::a2f::trt::cuda::kId, {}, {});
		bIsRegistered = true;
	}

	bIsFeatureAvailable = FAIMModule::Get().IsAIMFeatureAvailable(nvaim::plugin::a2x::pipeline::kId);
	bIsFeatureAvailable = bIsFeatureAvailable && FAIMModule::Get().IsAIMFeatureAvailable(nvaim::plugin::a2e::trt::cuda::kId);
	bIsFeatureAvailable = bIsFeatureAvailable && FAIMModule::Get().IsAIMFeatureAvailable(nvaim::plugin::a2f::trt::cuda::kId);
	if (!bIsFeatureAvailable)
	{
		UE_LOG(LogACEA2FLocal, Log, TEXT("Unable to load AIM Audio2Face-3D local execution feature, %s provider won't be available"), *ProviderName.ToString());
		Interface = nullptr;
	}
}

FA2FLocal::~FA2FLocal()
{
	if (Interface != nullptr)
	{
		// end sessions so that it's safe to destroy the instance
		FAIMA2FStreamContextProvider* ContextProvider = FAIMA2FStreamContextProvider::Get();
		if (ContextProvider != nullptr)
		{
			ContextProvider->KillAllActiveContexts(ProviderName);
		}

		// destroy instance
		FreeResources();

		// unload feature interface
		FAIMModule::Get().UnloadAIMFeature(nvaim::plugin::a2x::pipeline::kId, Interface);
	}
}

IA2FProvider::IA2FStream* FA2FLocal::CreateA2FStream(IACEAnimDataConsumer* CallbackObject)
{
	bool bInstanceAvailable = IsA2FInstanceAvailable();
	FAIMA2FStreamContextProvider* ContextProvider = FAIMA2FStreamContextProvider::Get();
	if ((ContextProvider != nullptr) && bInstanceAvailable && Instance.IsValid())
	{
		return ContextProvider->CreateA2FContext(ProviderName, CallbackObject, *Instance, FaceParameterDefaults);
	}

	return nullptr;
}

bool FA2FLocal::SendAudioSamples(
	IA2FStream* Stream,
	TArrayView<const int16> SamplesInt16,
	TOptional<FAudio2FaceEmotion> EmotionParameters,
	UAudio2FaceParameters* Audio2FaceParameters)
{
	if (!IsA2FInstanceAvailable())
	{
		return false;
	}
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, ProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	return A2FStream->SendAudioChunk(SamplesInt16, EmotionParameters, Audio2FaceParameters);
}

bool FA2FLocal::EndOutgoingStream(IA2FStream* Stream)
{
	if (!IsA2FInstanceAvailable())
	{
		return false;
	}
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, ProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	A2FStream->EndStream();
	return true;
}

FName FA2FLocal::GetName() const
{
	return ProviderName;
}

void FA2FLocal::AllocateResources()
{
	UE_LOG(LogACEA2FLocal, Log, TEXT("Allocation of instance of %s requested"), *GetName().ToString());
	bool bSuccess = IsA2FInstanceAvailable();

	if (bSuccess)
	{
		UE_LOG(LogACEA2FLocal, Log, TEXT("Allocation of instance of %s complete"), *GetName().ToString());
	}
	else
	{
		UE_LOG(LogACEA2FLocal, Log, TEXT("Allocation of instance of %s failed"), *GetName().ToString());
	}
}

void FA2FLocal::FreeResources()
{
	if (Instance.IsValid())
	{
		UE_LOG(LogACEA2FLocal, Log, TEXT("Removal of instance of %s requested"), *GetName().ToString());
		// safely remove local execution instance
		FAIMInferenceInstanceRef InstanceRef(Instance.Get());
		InstanceRef.DestroyInstance(Interface);
		UE_LOG(LogACEA2FLocal, Log, TEXT("Instance of %s removed"), *GetName().ToString());
	}

}

#define AIM_RETURNS_GARBAGE_AUDIO 1

void FA2FLocal::SetOriginalAudioParams(IA2FStream* Stream, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize)
{
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, ProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

#if AIM_RETURNS_GARBAGE_AUDIO
	// AIM sends us garbage audio anyway so don't pay any mind to what it gives us back, just save off and reuse the original audio even if it's in the correct format
	const bool bSaveApplicationAudio = true;
#else
	const bool bSaveApplicationAudio = (SampleRate != DEFAULT_SAMPLE_RATE) || (NumChannels != DEFAULT_NUM_CHANNELS) || (SampleByteSize != DEFAULT_SAMPLE_BYTE_SIZE);
#endif
	if (bSaveApplicationAudio)
	{
		FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
		if (ensure(Registry != nullptr))
		{
			Registry->SetAudioParams_AnyThread(Stream->GetID(), SampleRate, NumChannels, SampleByteSize);

			// AIM doesn't give us the animation timestamps that the A2F-3D service outputs, so we have to work out indirectly how many original samples we'll need.
			// We figure out a ratio here that will be used in the callback from AIM, and apply it to the number of samples coming out of the service.
			int32 Numerator = static_cast<int32>(SampleRate) * NumChannels * SampleByteSize;
			int32 Denominator = static_cast<int32>(DEFAULT_SAMPLE_RATE) * DEFAULT_NUM_CHANNELS * DEFAULT_SAMPLE_BYTE_SIZE;
			int32 GCD = FMath::GreatestCommonDivisor(Numerator, Denominator);
			A2FStream->SetOriginalAudioSampleConversion(Numerator / GCD, Denominator / GCD, SampleByteSize * NumChannels);
		}
	}
	else
	{
		A2FStream->SetOriginalAudioSampleConversion(0, 0, 0);
	}
}

void FA2FLocal::EnqueueOriginalSamples(IA2FStream* Stream, TArrayView<const uint8> OriginalSamples)
{
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, ProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	A2FStream->EnqueueOriginalSamples(OriginalSamples);
}

struct FAIMVRAMBudget
{
	size_t A2F3D = 0;
	size_t A2E = 0;
};

static FAIMVRAMBudget GetVramBudgetMB(nvaim::InferenceInterface* Interface, const char* ModelDirUTF8, const char* ModelGUID)
{
	// get caps
	nvaim::CommonCreationParameters CCP{};
	CCP.utf8PathToModels = ModelDirUTF8;
	nvaim::Audio2FaceCreationParameters A2FCP{};
	A2FCP.common = &CCP;

	nvaim::Audio2EmotionCreationParameters A2ECP{};
	A2ECP.common = &CCP;

	nvaim::A2XCreationParameters A2XCP{};
	const nvaim::PluginID A2FID = nvaim::plugin::a2f::trt::cuda::kId;
	const nvaim::PluginID A2EID = nvaim::plugin::a2e::trt::cuda::kId;
	A2XCP.a2f = &A2FID;
	A2XCP.a2e = &A2EID;
	A2XCP.a2fCreationParameters = A2FCP;
	A2XCP.a2eCreationParameters = A2ECP;

	nvaim::Audio2FaceCapabilitiesAndRequirements* A2XCaps = nullptr;
	nvaim::Result Result = nvaim::getCapsAndRequirements(Interface, A2XCP, &A2XCaps);
	const nvaim::Audio2FaceCapabilitiesAndRequirements* A2FCaps = nvaim::findStruct<nvaim::Audio2FaceCapabilitiesAndRequirements>(*A2XCaps);
	const nvaim::Audio2EmotionCapabilitiesAndRequirements* A2ECaps = nvaim::findStruct<nvaim::Audio2EmotionCapabilitiesAndRequirements>(*A2XCaps);
	// The audioBufferOffset, audioBufferSize "requirements" are not really requirements, they're internal implementation details. 🙄
	// They would only be useful if we were running A2F-3D or a2e directly instead of in a pipeline, but there's no reason to do that.
	FAIMVRAMBudget Budget;
	if (ensure((A2FCaps != nullptr) && (A2FCaps->common != nullptr) && (A2ECaps != nullptr) && (A2ECaps->common != nullptr)))
	{
		TArrayView<const char*> GUIDs(A2FCaps->common->supportedModelGUIDs, A2FCaps->common->numSupportedModels);
		int32 Idx = GUIDs.IndexOfByPredicate([ModelGUID](const char* GUID) { return FCStringAnsi::Strcmp(GUID, ModelGUID) == 0; });
		if (ensure(Idx != INDEX_NONE))
		{
			Budget.A2F3D = A2FCaps->common->modelMemoryBudgetMB[Idx];
			if (ensure(A2ECaps->common->numSupportedModels == 1))
			{
				Budget.A2E = A2ECaps->common->modelMemoryBudgetMB[0];
			}
		}
	}
	if (Budget.A2E == 0)
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to determine memory budget needed for Audio2Emotion local execution, falling back to 1 GiB (%s)"), *GetAIMStatusString(Result));
		// 1024 copied from AIM documentation
		Budget.A2E = 1024;
	}
	if (Budget.A2F3D == 0)
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to determine memory budget needed for Audio2Face-3D local execution, falling back to 1 GiB (%s)"), *GetAIMStatusString(Result));
		// 1024 copied from AIM documentation
		Budget.A2F3D = 1024;
	}

	return Budget;
}

static nvaim::InferenceInstance* CreateA2FInstanceInternal(nvaim::InferenceInterface* Interface, const FString& ModelDir, const FString& ModelGUID)
{
	if (Interface != nullptr)
	{
		// get model dir and GUID in UTF8
		auto ModelDirUTF8 = StringCast<UTF8CHAR>(*ModelDir);
		auto ModelGUIDUTF8 = StringCast<UTF8CHAR>(*ModelGUID);

		// Get VRAM budget
		FAIMVRAMBudget Budget = GetVramBudgetMB(Interface, reinterpret_cast<const char*>(ModelDirUTF8.Get()), reinterpret_cast<const char*>(ModelGUIDUTF8.Get()));

		// audio2face parameters
		nvaim::Audio2FaceCreationParameters A2FCreationParams;
		nvaim::CommonCreationParameters A2FCommonCreationParams;
		A2FCommonCreationParams.numThreads = 1;		// local inference seems to prefer a single thread
		A2FCommonCreationParams.vramBudgetMB = Budget.A2F3D;
		A2FCommonCreationParams.utf8PathToModels = reinterpret_cast<const char*>(ModelDirUTF8.Get());
		A2FCommonCreationParams.modelGUID = reinterpret_cast<const char*>(ModelGUIDUTF8.Get());
		A2FCreationParams.common = &A2FCommonCreationParams;

		// audio2emotion parameters
		nvaim::Audio2EmotionCreationParameters A2ECreationParams;
		nvaim::CommonCreationParameters A2ECommonCreationParams{ A2FCommonCreationParams };
		A2FCommonCreationParams.numThreads = 1;		// local inference seems to prefer a single thread
		A2FCommonCreationParams.vramBudgetMB = Budget.A2E;
		A2FCommonCreationParams.utf8PathToModels = reinterpret_cast<const char*>(ModelDirUTF8.Get());
		// only only model available for A2E
		A2ECommonCreationParams.modelGUID = "{E5E4043F-5BC9-4175-B510-A563A5BFB035}";
		A2ECreationParams.common = &A2ECommonCreationParams;

		// pipeline parameters
		nvaim::A2XCreationParameters PipelineCreationParams{};
		const nvaim::PluginID A2FID = nvaim::plugin::a2f::trt::cuda::kId;
		const nvaim::PluginID A2EID = nvaim::plugin::a2e::trt::cuda::kId;
		PipelineCreationParams.a2f = &A2FID;
		PipelineCreationParams.a2e = &A2EID;
		PipelineCreationParams.a2fCreationParameters = A2FCreationParams;
		PipelineCreationParams.a2eCreationParameters = A2ECreationParams;

		nvaim::CudaParameters* CIG = FAIMModule::Get().GetCIGCudaParameters();
		if (CIG != nullptr)
		{
			// Optimal performance with Compute in Graphics
			PipelineCreationParams.chain(*CIG);
		}

		// enable streaming
		nvaim::A2XStreamingParameters StreamingParams{};
		StreamingParams.streaming = true;
		PipelineCreationParams.chain(StreamingParams);

		nvaim::InferenceInstance* NewInstance = nullptr;
		nvaim::Result Result = Interface->createInstance(PipelineCreationParams, &NewInstance);
		if (Result == nvaim::ResultOk)
		{
			return NewInstance;
		}
		else
		{
			UE_LOG(LogACEA2FLocal, Warning, TEXT("Failed to create Audio2Face-3D local inference instance: %s"), *GetAIMStatusString(Result));
		}
	}

	return nullptr;
}

bool FA2FLocal::IsA2FInstanceAvailable()
{
	if (!bIsFeatureAvailable)
	{
		return false;
	}

	FScopeLock Lock(&InstanceCreationCS);

	if (Interface == nullptr)
	{
		// load the feature interface if it's not already loaded
		nvaim::Result Result = FAIMModule::Get().LoadAIMFeature(nvaim::plugin::a2x::pipeline::kId, &Interface);
		if (Result != nvaim::ResultOk)
		{
			UE_LOG(LogACEA2FLocal, Error, TEXT("Unable to load Audio2Face-3D local execution feature: %s"), *GetAIMStatusString(Result));
			Interface = nullptr;
			return false;
		}
	}

	if (Interface != nullptr)
	{
		if (!Instance.IsValid())
		{
			nvaim::InferenceInstance* RawInstance = CreateA2FInstanceInternal(Interface, ModelDir, ModelGUID);
			if (RawInstance != nullptr)
			{
				Instance = MakePimpl<FAIMInferenceInstance>(RawInstance, [this]() {
					FScopeLock Lock(&InstanceCreationCS);
					return CreateA2FInstanceInternal(Interface, ModelDir, ModelGUID);
				});
			}
			else
			{
				Instance.Reset();
			}
		}
	}

	return Instance.IsValid();
}

