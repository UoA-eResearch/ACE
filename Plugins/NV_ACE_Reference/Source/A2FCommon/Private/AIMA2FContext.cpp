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

#include "AIMA2FContext.h"

// engine includes
#if ALLOW_DUMPING_A2F
#include "HAL/IConsoleManager.h"
#include "Sound/SampleBufferIO.h"
#endif

// plugin includes
#include "A2FCommonModule.h"
#include "ACETypes.h"
#include "AIMModule.h"
#include "AnimDataConsumerRegistry.h"
#include "Audio2FaceParameters.h"
#include "nvaim.h"
#include "nvaim_a2e.h"
#include "nvaim_a2f.h"
#include "nvaim_ai.h"


#if ALLOW_DUMPING_A2F
static TAutoConsoleVariable<bool> CVarDumpA2F(
	TEXT("a2f3d.dump"),
	false,
	TEXT("Enable dumping Audio2Face-3D inputs and outputs to Saved/A2F3DDumps folder. (default: false)"),
	ECVF_Default);
#endif

////////////////////////////
// FAIMInferenceInstanceRef

void FAIMInferenceInstanceRef::Reset()
{
	if (bOwned)
	{
		if (ensure(RawInstance != nullptr))
		{
			RawInstance->Guard.Unlock();
		}
		bOwned = false;
	}
	RawInstance = nullptr;
}

FAIMInferenceInstanceRef& FAIMInferenceInstanceRef::operator=(FAIMInferenceInstanceRef&& Other)
{
	Reset();
	RawInstance = MoveTemp(Other.RawInstance);
	bOwned = MoveTemp(Other.bOwned);
	return *this;
}

FAIMInferenceInstanceRef::operator nvaim::InferenceInstance* ()
{
	if (!IsValid())
	{
		return nullptr;
	}

	if (!bOwned)
	{
		RawInstance->Guard.Lock();
		bOwned = true;
	}

	// if the instance has been destroyed we need to recreate it now
	if ((RawInstance->Instance == nullptr) && ensure(RawInstance->CreateFn != nullptr))
	{
		RawInstance->Instance = RawInstance->CreateFn();
	}

	return RawInstance->Instance;
}

void FAIMInferenceInstanceRef::DestroyInstance(nvaim::InferenceInterface* Interface)
{
	// probably shouldn't manually destroy a thing that you don't have a way to recreate, that's asking for trouble later
	if (IsValid() && ensure(RawInstance->CreateFn != nullptr))
	{
		if (!bOwned)
		{
			RawInstance->Guard.Lock();
			bOwned = true;
		}
		if (RawInstance->Instance != nullptr)
		{
			Interface->destroyInstance(RawInstance->Instance);
			RawInstance->Instance = nullptr;
		}
	}
}


///////////////////////
// CastToAIMA2FContext

FAIMA2FStreamContext* CastToAIMA2FContext(IA2FProvider::IA2FStream* Stream, FName ProviderName)
{
	if (Stream != nullptr)
	{
		if (Stream->GetProviderName() == ProviderName)
		{
			return static_cast<FAIMA2FStreamContext*>(Stream);
		}
		UE_LOG(LogACEA2FCommon, Warning, TEXT("Expected %s, received %s"), *ProviderName.ToString(), *Stream->GetProviderName().ToString());
	}

	return nullptr;
}


//////////////////////////
// FAIMA2FStreamContextProvider

FAIMA2FStreamContextProvider* FAIMA2FStreamContextProvider::Get()
{
	// It was once observed that this could be called from another module during shutdown, when the A2FCommon module was no longer loaded.
	// That's unexpected, but we can protect against it by making sure our module is actually loaded. If our module is unloaded, the context
	// is no longer around so returning nullptr is the right thing to do anyway.
	FA2FCommonModule* Module = FModuleManager::GetModulePtr<FA2FCommonModule>(FName("A2FCommon"));
	if (Module != nullptr)
	{
		return Module->GetA2FStreamContextProvider();
	}
	return nullptr;
}

FAIMA2FStreamContext* FAIMA2FStreamContextProvider::CreateA2FContext(FName ProviderName, IACEAnimDataConsumer* CallbackObject,
	FAIMInferenceInstance& InInstance, TOptional<TMap<FString, float>> InDefaultFaceParams)
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry == nullptr)
	{
		return nullptr;
	}

	UE::TScopeLock Lock(ContextArrayGuard);

	// first look for an available entry to reuse
	for (FAIMA2FStreamContext &Context : Contexts)
	{
		if (Context.TryAllocate(ProviderName, CallbackObject, InInstance, InDefaultFaceParams))
		{
			return &Context;
		}
	}

	// no suitable entry, so create a new one
	int32 NewIdx = Contexts.Add();
	FAIMA2FStreamContext &Context = Contexts[NewIdx];
	if (ensure(Context.TryAllocate(ProviderName, CallbackObject, InInstance, InDefaultFaceParams)))
	{
		return &Context;
	}

	return nullptr;
}

void FAIMA2FStreamContextProvider::KillAllActiveContexts(FName ProviderName)
{
	UE::TScopeLock Lock(ContextArrayGuard);
	for (FAIMA2FStreamContext &Context : Contexts)
	{
		Context.KillProvider(ProviderName);
	}
}


//////////////////
// FAIMA2FStreamContext

int32 FAIMA2FStreamContext::GetID() const
{
	FScopeLock Lock(&CS);
	return StreamID;
}

FName FAIMA2FStreamContext::GetProviderName() const
{
	FScopeLock Lock(&CS);
	return Name;
}

void FAIMA2FStreamContext::SetOriginalAudioSampleConversion(int32 InNumerator, int32 InDenominator, int32 InSampleQuantum)
{
	FScopeLock Lock(&CS);
	Numerator = InNumerator;
	Denominator = InDenominator;
	OriginalSampleQuantum = InSampleQuantum;
}

void FAIMA2FStreamContext::EnqueueOriginalSamples(TArrayView<const uint8> InOriginalSamples)
{
	FScopeLock Lock(&CS);
	if (Denominator != 0)
	{
		OriginalSamples.Append(InOriginalSamples);
	}
}

static void SetFaceParam(float& Out, const TMap<FString, float>& A2FParams, FString Name)
{
	const float* Val = A2FParams.Find(Name);
	if (Val != nullptr)
	{
		Out = *Val;
	}
}

template<typename T>
void SetEmotionParams(T& OutEmotionParams, const FAudio2FaceEmotion& InEmotionParams)
{
	OutEmotionParams.emotionContrast = InEmotionParams.DetectedEmotionContrast;
	OutEmotionParams.emotionStrength = InEmotionParams.OverallEmotionStrength;
	OutEmotionParams.liveBlendCoef = InEmotionParams.DetectedEmotionSmoothing;
	OutEmotionParams.maxEmotions = InEmotionParams.MaxDetectedEmotions;
	OutEmotionParams.enablePreferredEmotion = InEmotionParams.bEnableEmotionOverride;
	OutEmotionParams.preferredEmotionStrength = InEmotionParams.EmotionOverrideStrength;
}

template<typename T>
void SetEmotionOverrides(nvaim::Audio2FaceRuntimeParameters& OutRuntimeParams, T& OutEmotionOverrides, const FAudio2FaceEmotion& InEmotionParams)
{
	check(InEmotionParams.IsEmotionOverrideActive());
	OutRuntimeParams.chain(OutEmotionOverrides);

	const FAudio2FaceEmotionOverride& EmotionOverrides = InEmotionParams.EmotionOverrides;
	OutEmotionOverrides.amazement = EmotionOverrides.bOverrideAmazement ? EmotionOverrides.Amazement : nvaim::kUnassignedF;
	OutEmotionOverrides.anger = EmotionOverrides.bOverrideAnger ? EmotionOverrides.Anger : nvaim::kUnassignedF;
	OutEmotionOverrides.cheekiness = EmotionOverrides.bOverrideCheekiness ? EmotionOverrides.Cheekiness : nvaim::kUnassignedF;
	OutEmotionOverrides.disgust = EmotionOverrides.bOverrideDisgust ? EmotionOverrides.Disgust : nvaim::kUnassignedF;
	OutEmotionOverrides.fear = EmotionOverrides.bOverrideFear ? EmotionOverrides.Fear : nvaim::kUnassignedF;
	OutEmotionOverrides.grief = EmotionOverrides.bOverrideGrief ? EmotionOverrides.Grief : nvaim::kUnassignedF;
	OutEmotionOverrides.joy = EmotionOverrides.bOverrideJoy ? EmotionOverrides.Joy : nvaim::kUnassignedF;
	OutEmotionOverrides.outofbreath = EmotionOverrides.bOverrideOutOfBreath ? EmotionOverrides.OutOfBreath : nvaim::kUnassignedF;
	OutEmotionOverrides.pain = EmotionOverrides.bOverridePain ? EmotionOverrides.Pain : nvaim::kUnassignedF;
	OutEmotionOverrides.sadness = EmotionOverrides.bOverrideSadness ? EmotionOverrides.Sadness : nvaim::kUnassignedF;
}

struct FAggregateAIMRuntimeParams: FNoncopyable
{
public:
	nvaim::Audio2FaceRuntimeParameters RuntimeParams;

public:
	FAggregateAIMRuntimeParams(TOptional<FAudio2FaceEmotion> InEmotionParams,
		const UAudio2FaceParameters* Audio2FaceParams,
		TOptional<TMap<FString, float>> MaybeDefaultAudio2FaceParams,
		FName ProviderName, int32 OutgoingAudioCurrentSampleCount = 0)
	{
		auto SetFaceParams = [this](const TMap<FString, float>& A2FParams)
		{
			// documented face parameters
			SetFaceParam(RuntimeParams.lowerFaceSmoothing, A2FParams, "lowerFaceSmoothing");
			SetFaceParam(RuntimeParams.upperFaceSmoothing, A2FParams, "upperFaceSmoothing");
			SetFaceParam(RuntimeParams.lowerFaceStrength, A2FParams, "lowerFaceStrength");
			SetFaceParam(RuntimeParams.upperFaceStrength, A2FParams, "upperFaceStrength");
			SetFaceParam(RuntimeParams.faceMaskLevel, A2FParams, "faceMaskLevel");
			SetFaceParam(RuntimeParams.faceMaskSoftness, A2FParams, "faceMaskSoftness");
			SetFaceParam(RuntimeParams.skinStrength, A2FParams, "skinStrength");
			SetFaceParam(RuntimeParams.blinkStrength, A2FParams, "blinkStrength");
			SetFaceParam(RuntimeParams.eyelidOpenOffset, A2FParams, "eyelidOpenOffset");
			SetFaceParam(RuntimeParams.lipOpenOffset, A2FParams, "lipOpenOffset");
			SetFaceParam(RuntimeParams.tongueStrength, A2FParams, "tongueStrength");
			SetFaceParam(RuntimeParams.tongueHeightOffset, A2FParams, "tongueHeightOffset");
			SetFaceParam(RuntimeParams.tongueDepthOffset, A2FParams, "tongueDepthOffset");

			// undocumented but AIM supports them so we do too
			SetFaceParam(RuntimeParams.inputStrength, A2FParams, "inputStrength");
			SetFaceParam(RuntimeParams.blinkOffset, A2FParams, "blinkOffset");
		};

		if (MaybeDefaultAudio2FaceParams.IsSet())
		{
			// set defaults if provided
			// this is to work around AIM bug where local execution face parameters are "sticky"
			SetFaceParams(MaybeDefaultAudio2FaceParams.GetValue());
		}

		if (Audio2FaceParams != nullptr)
		{
			SetFaceParams(Audio2FaceParams->Audio2FaceParameterMap);
		}

		if (InEmotionParams.IsSet())
		{
			// for local execution to work, we chain in parameters from a separate structure
			RuntimeParams.chain(LocalEmotionParams);
			SetEmotionParams(LocalEmotionParams, InEmotionParams.GetValue());

			// for remote execution to work, we set the parameters directly in the nvaim::Audio2FaceRuntimeParameters
			SetEmotionParams(RuntimeParams, InEmotionParams.GetValue());

			if (InEmotionParams->IsEmotionOverrideActive())
			{
				// required for local execution to work, harmless for remote execution
				SetEmotionOverrides(RuntimeParams, LocalEmotionOverrideParams, InEmotionParams.GetValue());

				// work around AIM bug where setting the parameters required for remote execution will break things with local execution
				if (bHackWorkaroundAIMEmotionOverrideBug)
				{
					IA2FProvider* Provider = IA2FProvider::FindProvider(ProviderName);
					if (ensure(Provider != nullptr))
					{
						if (Provider->GetRemoteProvider() != nullptr)
						{
							RemoteEmotionOverrideParams.timeCode = static_cast<float>(OutgoingAudioCurrentSampleCount) / static_cast<float>(DefaultSampleRate);
							SetEmotionOverrides(RuntimeParams, RemoteEmotionOverrideParams, InEmotionParams.GetValue());
						}
					}
				}
			}
		}
	}

	FAggregateAIMRuntimeParams() = delete;

private:
	nvaim::Audio2EmotionRuntimeParameters LocalEmotionParams;
	nvaim::Audio2EmotionPreferredEmotion LocalEmotionOverrideParams;
	nvaim::Audio2FaceEmotions RemoteEmotionOverrideParams;
	static const bool bHackWorkaroundAIMEmotionOverrideBug = true;
	static const int32 DefaultSampleRate = 16'000;
};

bool FAIMA2FStreamContext::SendAudioChunk(
	TArrayView<const int16> SamplesInt16,
	TOptional<FAudio2FaceEmotion> EmotionParameters,
	UAudio2FaceParameters* Audio2FaceParameters)
{
	FScopeLock Lock(&CS);

	if (State == EState::SessionEnded)
	{
		// This could happen if the consumer was destroyed already and the callback requested AIM to cancel the stream.
		// It's unclear from the docs if AIM would handle attempts to send more audio now, so we play it safe here.
		return false;
	}
	if (!((State == EState::Allocated) || (State == EState::SessionStarted)))
	{
		// This could happen if KillProvider was called at just the right moment
		return false;
	}

	if (!ensure(AIMContext.IsValid() && AIMInstance.IsValid()))
	{
		return false;
	}

	if (State == EState::Allocated)
	{
		// stream hasn't started yet, so we need to initialize the AIM execution context
		AIMContext->instance = AIMInstance;
		AIMContext->callback = &FAIMA2FStreamContext::AIMCallback;
		AIMContext->callbackUserData = this;
	}

	// convert incoming audio to something AIM likes
	nvaim::CpuData AIMAudioWrapper(SamplesInt16.Num() * sizeof(int16), static_cast<const void*>(SamplesInt16.GetData()));
	nvaim::InferenceDataAudio AIMAudioWrapperWrapper(AIMAudioWrapper);
	nvaim::InferenceDataSlot AIMAudioWrapperWrapperWrapper(nvaim::kAudio2FaceDataSlotAudio, &AIMAudioWrapperWrapper);
	nvaim::InferenceDataSlotArray AIMAudioWrapperWrapperWrapperWrapper(1, &AIMAudioWrapperWrapperWrapper);

	// set other inputs
	AIMContext->inputs = &AIMAudioWrapperWrapperWrapperWrapper;
	FAggregateAIMRuntimeParams AggregateParams(EmotionParameters, Audio2FaceParameters, MaybeDefaultFaceParams, Name, OutgoingAudioSampleCount);
	AIMContext->runtimeParameters = AggregateParams.RuntimeParams;

	// send audio to A2F-3D service
	State = EState::SessionStarted;
	nvaim::Result Result = AIMInstance->evaluate(AIMContext.Get());

	if (Result != nvaim::ResultOk)
	{
		UE_LOG(LogACEA2FCommon, Warning, TEXT("[ACE SID %d] Failed to send audio samples to A2F-3D: %s"), StreamID, *GetAIMStatusString(Result));
		return false;
	}

	OutgoingAudioSampleCount += SamplesInt16.Num();

#if ALLOW_DUMPING_A2F
	if (CVarDumpA2F.GetValueOnAnyThread())
	{
		OutgoingAudio.Append(SamplesInt16);
	}
#endif

	return true;
}

void FAIMA2FStreamContext::EndStream()
{
	// We do some sketchy things in this function. We have to leave the data critical section CS unlocked while we call
	// evaluate. This is because evaluate blocks on callbacks that need to to lock CS, potentially from a different
	// thread.
	//
	// But that sketchiness is a problem during provider shutdown because it might need to safely tear down all its
	// contexts. See KillProvider(). So we have a second critical section EndStream protecting this function
	// specifically, so that KillProvider won't mess with anything during EndStream.
	FScopeLock EndStreamLock(&EndStreamCS);

	nvaim::InferenceExecutionContext* Context = nullptr;
	nvaim::InferenceInstance* Instance = nullptr;
	{
		// only safe to read these while locked, so make a copy here
		FScopeLock Lock(&CS);
		Context = AIMContext.Get();
		if (Context != nullptr)
		{
			Instance = AIMInstance;
		}
		if ((Context == nullptr) || (Instance == nullptr))
		{
			// this could happen if KillProvider got called
			return;
		}
	}

	// Tell AIM we're done providing audio.
	// We don't lock here because there may still be callbacks that need to get called, and this
	// evaluate call will block until callbacks are done unlike other streaming instance evaluates
	Context->inputs = nullptr;
	Context->runtimeParameters = nullptr;
	nvaim::Result Result = Instance->evaluate(Context);

	FScopeLock Lock(&CS);
	if (Result != nvaim::ResultOk)
	{
		UE_LOG(LogACEA2FCommon, Warning, TEXT("[ACE SID %d] Failed ending audio stream: %s"), StreamID, *GetAIMStatusString(Result));
	}
#if ALLOW_DUMPING_A2F
	else
	{
		bSentCompleteAudio = true;
	}
#endif
	// relinquish ownership of the underlying AIM Instance
	AIMInstance.Reset();
	// release this context to be reused again
	Reset(EState::Available);
}

// called by FAIMA2FStreamContextProvider::CreateA2FContext
// will return false if in any state other than FAIMA2FStreamContext::EState::Available
// if it returns true, it's now in the Allocated state
bool FAIMA2FStreamContext::TryAllocate(FName ProviderName, IACEAnimDataConsumer* CallbackObject,
	FAIMInferenceInstance& InInstance, TOptional<TMap<FString, float>> InMaybeDefaultFaceParams)
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry == nullptr)
	{
		return false;
	}

	FScopeLock Lock(&CS);
	if (State == EState::Available)
	{
		StreamID = Registry->CreateStream_AnyThread();
		Name = ProviderName;
		MaybeDefaultFaceParams = InMaybeDefaultFaceParams;
		AIMContext = MakePimpl<nvaim::InferenceExecutionContext>();
		AIMInstance = FAIMInferenceInstanceRef(&InInstance);
		Reset(EState::Allocated);
		Registry->AttachConsumerToStream_AnyThread(StreamID, CallbackObject);
		return true;
	}

	return false;
}

void FAIMA2FStreamContext::KillProvider(FName ProviderName)
{
	// ensure that we don't mess with anything while EndStream is running
	FScopeLock EndStreamLock(&EndStreamCS);
	FScopeLock Lock(&CS);
	if (ProviderName == Name)
	{
		// Make this context available for reuse
		Reset(EState::Available);
	}
}


void FAIMA2FStreamContext::Reset(EState NewState)
{
	// note: CS must be locked when calling this function
	OriginalSamples.Empty();
	ReceivedAudioSampleCount = 0;
	Numerator = 0;
	Denominator = 0;
	OriginalSampleQuantum = 0;
	if (NewState == EState::Available)
	{
		if (AIMInstance.IsOwned() && ensure(AIMInstance.IsValid()))
		{
			// We've still got a lock on the underlying AIM inference instance, which likely means that the stream was started
			// but not finished. So we issue a non-blocking end of stream to AIM here. This could happen if called from
			// KillProvider before EndStream runs.

			if (!ensure(AIMContext.IsValid()))
			{
				// it's weird that the AIM inference execution context is gone already, but I guess if we create a new one it might
				// still work?
				AIMContext = MakePimpl<nvaim::InferenceExecutionContext>();
				AIMContext->instance = AIMInstance;
				AIMContext->callback = &FAIMA2FStreamContext::AIMCallback;
				AIMContext->callbackUserData = this;
			}

			nvaim::CpuData NullAudio(0, nullptr);
			nvaim::InferenceDataAudio NullAudioWrapper(NullAudio);
			nvaim::InferenceDataSlot NullAudioWrapperWrapper(nvaim::kAudio2FaceDataSlotAudio, &NullAudioWrapper);
			nvaim::InferenceDataSlotArray NullAudioWrapperWrapperWrapper(1, &NullAudioWrapperWrapper);
			AIMContext->inputs = &NullAudioWrapperWrapperWrapper;
			AIMContext->runtimeParameters = nullptr;

			AIMInstance->evaluate(AIMContext.Get());
		}
		// reset the AIM context and AIM instance too
		AIMInstance.Reset();
		AIMContext.Reset();

		Name = NAME_None;
		OutgoingAudioSampleCount = 0;
#if ALLOW_DUMPING_A2F
		if (CVarDumpA2F.GetValueOnAnyThread())
		{
			// write wav files
			FString DirName = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("A2F3DDumps"));
			DirName = FPaths::ConvertRelativePathToFull(DirName);

			FString OutputFilename;
			Audio::FSoundWavePCMWriter WavWriter;
			if (OutgoingAudio.Num() > 0)
			{
				// file name will include stream ID, optionally appended with "_partial" if we stopped AIM input early
				FString FileName = FString::Printf(TEXT("aim_a2f_%d_input"), StreamID);
				if (!bSentCompleteAudio)
				{
					FileName += TEXT("_partial");
				}
				Audio::TSampleBuffer<int16> SampleBuffer(OutgoingAudio.GetData(), OutgoingAudio.Num(), 1, 16'000);
				bool bSuccess = WavWriter.SynchronouslyWriteToWavFile(SampleBuffer, FileName, DirName, &OutputFilename);
				if (bSuccess)
				{
					UE_LOG(LogACEA2FCommon, Log, TEXT("dumped AIM A2F-3D input to %s"), *OutputFilename);
				}
				SampleBuffer.Reset();
			}
			if (ReceivedAudio.Num() > 0)
			{
				// file name will include stream ID, optionally appended with "_partial" if we stopped AIM output early
				FString FileName = FString::Printf(TEXT("aim_a2f_%d_output"), StreamID);
				if (!bReceivedCompleteStream)
				{
					FileName += TEXT("_partial");
				}
				Audio::TSampleBuffer<int16> SampleBuffer(ReceivedAudio.GetData(), ReceivedAudio.Num(), 1, 16'000);
				bool bSuccess = WavWriter.SynchronouslyWriteToWavFile(SampleBuffer, FileName, DirName, &OutputFilename);
				if (bSuccess)
				{
					UE_LOG(LogACEA2FCommon, Log, TEXT("dumped AIM A2F-3D output to %s"), *OutputFilename);
				}
				SampleBuffer.Reset();
			}
		}

		OutgoingAudio.Empty();
		ReceivedAudio.Empty();
		bSentCompleteAudio = false;
		bReceivedCompleteStream = false;
#endif
	}
	State = NewState;
}

template<class T>
static TArrayView<T> GetArrayViewFromAIMParameter(const nvaim::NVAIMParameter* AIMParameter)
{
	const nvaim::CpuData* AIMCpuData{ nvaim::castTo<nvaim::CpuData>(AIMParameter) };
	return TArrayView<T>(static_cast<T*>(AIMCpuData->buffer), AIMCpuData->sizeInBytes / sizeof(T));
}

FACEAnimDataChunk FAIMA2FStreamContext::CreateChunkFromAIMOutputs(nvaim::InferenceExecutionState AIMState)
{
	// Danger! The FACEAnimDataChunk may include a reference to OriginalSamples, which is protected by CS.
	// The critical section for the OriginalSamples array MUST be acquired by the caller for the lifetime of the FACEAnimDataChunk returned by this function.

	const nvaim::InferenceDataSlotArray* AIMOutputs = AIMContext->outputs;
	if (AIMOutputs == nullptr)
	{
		FACEAnimDataChunk Chunk;
		Chunk.Status = EACEAnimDataStatus::ERROR_UNEXPECTED_OUTPUT;
		return Chunk;
	}

	const nvaim::InferenceDataByteArray* BlendShapeWeightSlot = nullptr;
	const nvaim::InferenceDataAudio* AudioSampleSlot = nullptr;
	const nvaim::InferenceDataByteArray* EmotionSlot = nullptr;

	AIMOutputs->findAndValidateSlot(nvaim::kAudio2FaceDataSlotBlendshapes, &BlendShapeWeightSlot);
	AIMOutputs->findAndValidateSlot(nvaim::kAudio2FaceDataSlotAudio, &AudioSampleSlot);
	AIMOutputs->findAndValidateSlot(nvaim::kAudio2EmotionDataSlotEmotions, &EmotionSlot);

	FACEAnimDataChunk Chunk;
	Chunk.Status = (AIMState == nvaim::InferenceExecutionState::eDone) ? EACEAnimDataStatus::OK_NO_MORE_DATA : EACEAnimDataStatus::ERROR_UNEXPECTED_OUTPUT;
	if (BlendShapeWeightSlot != nullptr)
	{
		Chunk.BlendShapeWeights = GetArrayViewFromAIMParameter<const float>(BlendShapeWeightSlot->bytes);
		if (AIMState != nvaim::InferenceExecutionState::eDone)
		{
			Chunk.Status = EACEAnimDataStatus::OK;
		}
	}
	if (AudioSampleSlot != nullptr)
	{
		// we assume PCM16 output at the moment
		check(AudioSampleSlot->bitsPerSample == 16);
		Chunk.AudioBuffer = GetArrayViewFromAIMParameter<const uint8>(AudioSampleSlot->audio);
#if ALLOW_DUMPING_A2F
		if (CVarDumpA2F.GetValueOnAnyThread())
		{
			ReceivedAudio.Append(BitCast<const int16*>(Chunk.AudioBuffer.GetData()), Chunk.AudioBuffer.Num() / sizeof(int16));
		}
#endif
		// AIM doesn't give us timestamps from the service, so we derive timestamps from the output audio data 🙄
		Chunk.Timestamp = static_cast<double>(ReceivedAudioSampleCount) / (16000.0 * sizeof(int16_t));
		int32 OutputSampleCount = Chunk.AudioBuffer.Num();
		int32 NextReceivedAudioSampleCount = ReceivedAudioSampleCount +  OutputSampleCount;

		bool bUsePassthroughAudio = (Denominator != 0);
		if (bUsePassthroughAudio)
		{
			// convert a received sample index to an original sample index
			auto ToOriginalIdx = [this](int32 ReceivedIdx)
			{
				int32 OriginalIdx = static_cast<int32>(static_cast<int64>(Numerator) * ReceivedIdx / Denominator);
				// take into account that the result needs to be a multiple of the original sample size in bytes times the number of channels
				return OriginalIdx - (OriginalIdx % OriginalSampleQuantum);
			};

			// Use the saved original samples and pass them through to the consumer
			int32 FirstOriginalIdx = ToOriginalIdx(ReceivedAudioSampleCount);
			int32 LastOriginalIdx = ToOriginalIdx(NextReceivedAudioSampleCount);

			if (LastOriginalIdx > OriginalSamples.Num())
			{
				// This is normal behavior, since the Audio2Face-3D service can send back more audio than we send out.
				// The service adds silence to the end of the buffer.
				UE_LOG(LogACEA2FCommon, VeryVerbose,
					TEXT("[ACE SID %d] received more audio from Audio2Face-3D service (%d samples) than original audio (%d samples)"),
					StreamID, NextReceivedAudioSampleCount, OriginalSamples.Num());
				LastOriginalIdx = OriginalSamples.Num();
			}
			int32 ByteCount = LastOriginalIdx - FirstOriginalIdx;
			if (ByteCount > 0)
			{
				const uint8* Start = &OriginalSamples[FirstOriginalIdx];
				Chunk.AudioBuffer = TArrayView<const uint8>(Start, ByteCount);
			}
			else
			{
				Chunk.AudioBuffer = TArrayView<const uint8>();
			}
		}

		ReceivedAudioSampleCount = NextReceivedAudioSampleCount;

		if (AIMState != nvaim::InferenceExecutionState::eDone)
		{
			Chunk.Status = EACEAnimDataStatus::OK;
		}
	}
	else
	{
		// AIM doesn't give us timestamps from the service, so put in a negative value. This will notify the animation code to ignore our timestamp and do the best it can
		Chunk.Timestamp = -1.0;
	}
	if (EmotionSlot != nullptr)
	{
		// We don't pass through the emotion outputs yet.
		// The format is an array of 10 floats in the order defined by the nvaim::Audio2FaceEmotions struct. Essentially that struct without timeCode.
		//TArrayView<const float> Emotions = GetArrayViewFromAIMParameter<const float>(EmotionSlot->bytes);
		if (AIMState != nvaim::InferenceExecutionState::eDone)
		{
			Chunk.Status = EACEAnimDataStatus::OK;
		}
	}

	return Chunk;
}

nvaim::InferenceExecutionState FAIMA2FStreamContext::AnimDataFrameCallback(nvaim::InferenceExecutionState AIMState)
{
	FScopeLock Lock(&CS);
	if ((State == EState::SessionStarted) && AIMContext.IsValid())
	{
		int32 NumConsumers = 0;
		FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
		if (Registry != nullptr)
		{
			NumConsumers = Registry->SendAnimData_AnyThread(CreateChunkFromAIMOutputs(AIMState), StreamID);
			if (NumConsumers < 1)
			{
				// no one is listening, so tell AIM to stop sending callbacks
				AIMState = nvaim::InferenceExecutionState::eCancel;
			}
		}
		if ((AIMState == nvaim::InferenceExecutionState::eDone) || (AIMState == nvaim::InferenceExecutionState::eCancel))
		{
#if ALLOW_DUMPING_A2F
			if (AIMState == nvaim::InferenceExecutionState::eDone)
			{
				bReceivedCompleteStream = true;
			}
#endif
			// this session is over
			Reset(EState::SessionEnded);
		}
	}

	return AIMState;
}

nvaim::InferenceExecutionState FAIMA2FStreamContext::AIMCallback(const nvaim::InferenceExecutionContext* AIMContext, nvaim::InferenceExecutionState AIMState, void* InContext)
{
	FAIMA2FStreamContext* A2FContext = reinterpret_cast<FAIMA2FStreamContext*>(InContext);
	if (ensure((A2FContext != nullptr) && (AIMContext == A2FContext->AIMContext.Get())))
	{
		return A2FContext->AnimDataFrameCallback(AIMState);
	}
	return AIMState;
}

