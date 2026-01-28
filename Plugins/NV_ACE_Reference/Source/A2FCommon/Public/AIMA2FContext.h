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
#pragma once

// engine includes
#include "CoreMinimal.h"
#include "Containers/ChunkedArray.h"
#include "Misc/EngineVersionComparison.h"

// project includes
#include "A2FProvider.h"
#include "AnimDataConsumer.h"


#define ALLOW_DUMPING_A2F (!UE_BUILD_SHIPPING && !UE_VERSION_OLDER_THAN(5,4,0))


struct FAudio2FaceEmotion;
class UAudio2FaceParameters;
namespace nvaim
{
	struct InferenceExecutionContext;
	enum class InferenceExecutionState: uint32_t;
	struct InferenceInstance;
	struct InferenceInterface;
}


// AIM's Audio2Face-3D implementation has an interesting bug/limitation with nvaim::InferenceExecutionContext. Despite
// what the name suggests, you can't actually use it to have more than one execution in flight for a single
// nvaim::InferenceInstance. It's really more of a method to shift the management of AIM's internal
// nvaim::InferenceInstance state from AIM to the application.
//
// So don't use an nvaim::InferenceInstance* directly, at least not for Audio2Face-3D features. Instead, the owner of
// the nvaim::InferenceInstance* should wrap it in a FAIMInferenceInstance, and any context where it needs to be used
// should wrap _that_ in a FAIMInferenceInstanceRef. Generally every nvaim::InferenceExecutionContext will have its
// own associated FAIMInferenceInstanceRef.
//
// The optional CreateFn will be used to recreate a destroyed instance if necessary.
class A2FCOMMON_API FAIMInferenceInstance : FNoncopyable
{
public:
	FAIMInferenceInstance(nvaim::InferenceInstance* InInstance, TFunction<nvaim::InferenceInstance*()> InCreateFn)
		: Instance(InInstance), CreateFn(InCreateFn) {}

private:
	UE::FMutex Guard{};
	nvaim::InferenceInstance* Instance = nullptr;
	const TFunction<nvaim::InferenceInstance* ()> CreateFn;
	friend class FAIMInferenceInstanceRef;
};

// Safe wrapper for an nvaim::InferenceInstance*. Use it like an nvaim::InferenceInstance*
//
// An instance of FAIMInferenceInstanceRef will get exclusive access to the underlying nvaim::InferenceInstance* from
// the time it is first used until the FAIMInferenceInstanceRef is destroyed or Reset() is called. Generally when
// you're done with the associated nvaim::InferenceExecutionContext, you should reset the FAIMInferenceInstanceRef.
class A2FCOMMON_API FAIMInferenceInstanceRef
{
public:
	~FAIMInferenceInstanceRef()
	{
		Reset();
	}

	FAIMInferenceInstanceRef(FAIMInferenceInstance* InInstance = nullptr) : RawInstance(InInstance) {}

	// move assignment is the only assignment allowed
	FAIMInferenceInstanceRef& operator=(FAIMInferenceInstanceRef&& Other);

	bool IsValid() const
	{
		return RawInstance != nullptr;
	}

	bool IsOwned() const
	{
		return bOwned;
	}

	// sets the instance to null, and releases ownership if instance is owned
	void Reset();

	// May block. Will lock access to the underlying instance and mark it as owned before returning the instance pointer
	operator nvaim::InferenceInstance* ();
	nvaim::InferenceInstance* operator->()
	{
		check(IsValid());
		return *this;
	}

	// Calls destroyInstance on the underlying nvaim::InferenceInstance while leaving things in a safe state so references
	// don't lose their minds. May block since it needs to lock the underlying instance.
	void DestroyInstance(nvaim::InferenceInterface* Interface);

private:
	bool bOwned = false;
	FAIMInferenceInstance* RawInstance = nullptr;
};

// Represents a bidirectional audio2face stream to an AIM streaming instance
struct A2FCOMMON_API FAIMA2FStreamContext final : public IA2FProvider::IA2FStream
{
public:
	// begin IA2FStream
	virtual int32 GetID() const override;
	virtual FName GetProviderName() const override;
	// end IA2FStream

	// Set numerator/denominator for converting the number of sample bytes at the receiving end to original sample bytes.
	// Also sets a desired quantum for the number of bytes to split the original sample buffer at.
	void SetOriginalAudioSampleConversion(int32 InNumerator, int32 InDenominator, int32 InSampleQuantum);

	// Should be called from the provider's EnqueueOriginalSamples
	void EnqueueOriginalSamples(TArrayView<const uint8> InOriginalSamples);

	// Send one chunk of audio to audio2face.
	// Returns false if chunk couldn't be sent for some reason.
	// false does not necessarily mean an error. For example if the consumer was deleted, the receiving end could have closed the stream early.
	bool SendAudioChunk(
		TArrayView<const int16> SamplesInt16,
		TOptional<FAudio2FaceEmotion> EmotionParameters,
		UAudio2FaceParameters* Audio2FaceParameters);

	// Ends the bidirectional stream with audio2face.
	// It's an error to call this object's public interface after EndStream.
	void EndStream();

private:
	mutable FCriticalSection CS{};
	mutable FCriticalSection EndStreamCS{};
	TArray<uint8> OriginalSamples{};
#if ALLOW_DUMPING_A2F
	bool bSentCompleteAudio = false;
	bool bReceivedCompleteStream = false;
	TArray<int16> OutgoingAudio{};
	TArray<int16> ReceivedAudio{};
#endif
	int32 OutgoingAudioSampleCount = 0;
	int32 ReceivedAudioSampleCount = 0;
	int32 Numerator = 0;
	// denominator 0 indicates not to use the original samples buffer
	int32 Denominator = 0;
	int32 OriginalSampleQuantum = 0;
	int32 StreamID = INVALID_STREAM_ID;
	FName Name = NAME_None;
	TOptional<TMap<FString, float>> MaybeDefaultFaceParams;

	enum class EState {
		Available,
		Allocated,
		SessionStarted,
		SessionEnded
	};

	TPimplPtr<nvaim::InferenceExecutionContext> AIMContext{};
	FAIMInferenceInstanceRef AIMInstance{};
	EState State = EState::Available;

private:
	void Reset(EState NewState);
	FACEAnimDataChunk CreateChunkFromAIMOutputs(nvaim::InferenceExecutionState AIMState);
	nvaim::InferenceExecutionState AnimDataFrameCallback(nvaim::InferenceExecutionState AIMState);
	static nvaim::InferenceExecutionState AIMCallback(const nvaim::InferenceExecutionContext* AIMContext, nvaim::InferenceExecutionState AIMState, void* InContext);

	// interface used only by FAIMA2FStreamContextProvider
	bool TryAllocate(FName ProviderName, IACEAnimDataConsumer* CallbackObject, FAIMInferenceInstance& InInstance, TOptional<TMap<FString, float>> InDefaultFaceParams);
	void KillProvider(FName ProviderName);
	friend class FAIMA2FStreamContextProvider;

};

A2FCOMMON_API FAIMA2FStreamContext* CastToAIMA2FContext(IA2FProvider::IA2FStream* Stream, FName ProviderName);

class A2FCOMMON_API FAIMA2FStreamContextProvider: FNoncopyable
{
public:
	static FAIMA2FStreamContextProvider* Get();

	// meant to be called from A2F provider's CreateA2FStream
	// InDefaultFaceParams: if provided, context will set the default values before sending chunks
	FAIMA2FStreamContext* CreateA2FContext(FName ProviderName, IACEAnimDataConsumer* CallbackObject, FAIMInferenceInstance& InInstance,
		TOptional<TMap<FString, float>> InDefaultFaceParams);

	// when an A2F provider is shutting down, use this to ensure that it doesn't hold any active contexts
	void KillAllActiveContexts(FName ProviderName);

private:
	// chunked array so that the memory won't be reallocated or moved around, since pointers to things in this array will come back via callback
	TChunkedArray<FAIMA2FStreamContext> Contexts;
	UE::FMutex ContextArrayGuard{};
};

