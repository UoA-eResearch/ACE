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
#include "Modules/ModuleManager.h"

// plugin includes
#include "ACETypes.h"

class FAudio2XSession;
class IA2FProvider;
class IACEAnimDataConsumer;
class UAudio2FaceParameters;


class ACERUNTIME_API FACERuntimeModule : public IModuleInterface
{
public:
	static FACERuntimeModule& Get()
	{
		return FModuleManager::GetModuleChecked<FACERuntimeModule>(FName("ACERuntime"));
	}

	// Receive animations using audio from a float sample buffer.
	// If bEndOfSamples = true, pending audio data will be flushed and any subsequent call to SendAudioSamples will start
	// a new session.
	// Will block until all samples have been sent into the Audio2Face-3D provider. Returns true if all samples sent
	// successfully.
	// Safe to call from any thread.
	bool AnimateFromAudioSamples(IACEAnimDataConsumer* Consumer, TArrayView<const float> SamplesFloat, int32 NumChannels,
		int32 SampleRate, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters,
		UAudio2FaceParameters* Audio2FaceParameters, FName A2FProviderName = FName("Default"));

	// Receive animations using audio from an int16 PCM sample buffer.
	// If bEndOfSamples = true, pending audio data will be flushed and any subsequent call to SendAudioSamples will start
	// a new session.
	// Will block until all samples have been sent into the Audio2Face-3D provider. Returns true if all samples sent
	// successfully.
	// Safe to call from any thread.
	bool AnimateFromAudioSamples(IACEAnimDataConsumer* Consumer, TArrayView<const int16> SamplesInt16, int32 NumChannels,
		int32 SampleRate, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters,
		UAudio2FaceParameters* Audio2FaceParameters, FName A2FProviderName = FName("Default"));

	// Indicate no more samples for the current audio clip. Any subsequent call to AnimateFromAudioSamples will start a
	// new session.
	// Use this if your last call to SendAudioSamples had bEndOfSamples = false, and now the audio stream has ended.
	// Safe to call from any thread.
	bool EndAudioSamples(IACEAnimDataConsumer* Consumer);

	// Request resources needed by the provider to be allocated ahead of time.
	// In the case of a remote provider, this may establish the network connection to the server. In the case of a local
	// provider, this may allocate GPU memory to run the inference model.
	//
	// Use this call before you need an Audio2Face-3D provider to reduce latency the first time the provider is used.
	//
	// This call does not block. It only schedules resources to be allocated in the background. It may have no effect if
	// the Audio2Face-3D provider has already run.
	void AllocateA2F3DResources(FName ProviderName);

	// Request any resources allocated by the provider to be freed as soon as it's safe to do so.
	// In the case of a remote provider, this may close the network connection. In the case of a local provider, this
	// may free up GPU memory allocated to run the inference model.
	//
	// This call does not block. It only schedules resources to be freed in the future. If an Audio2Face-3D session is
	// in progress, the resources may be freed after the current session completes.
	//
	// Note that resources could be automatically reallocated later if the provider is used again.
	void FreeA2F3DResources(FName ProviderName);

	// Cancel any in-progress animation generation for the given consumer.
	// Note that any buffered animation data may continue to briefly play after calling this, but no new animation data will be generated.
	// If you still had a session open, then any subsequent calls to AnimateFromAudioSamples() will have no effect until
	// after the session is ended with bEndOfSamples=true or EndAudioSamples().
	// Safe to call from any thread.
	void CancelAnimationGeneration(IACEAnimDataConsumer* Consumer);

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	TOptional<float> OverrideMaxInitialAudioChunkSize{};
	TOptional<bool> bOverrideBurstMode{};

private:
	TPimplPtr<TMap<const IACEAnimDataConsumer*, TUniquePtr<FAudio2XSession>>> ActiveA2XSessions;

};

ACERUNTIME_API IA2FProvider* GetProviderFromName(FName ProviderName);
ACERUNTIME_API FName GetDefaultProviderName();

