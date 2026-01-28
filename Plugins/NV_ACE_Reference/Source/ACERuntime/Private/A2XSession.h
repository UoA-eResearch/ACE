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
#include "Containers/ArrayView.h"
#include "Templates/UnrealTemplate.h"

// project includes
#include "A2FProvider.h"


namespace Audio
{
	class FResampler;
}
struct FAudio2FaceEmotion;
class IACEAnimDataConsumer;

// Represents one session to send audio to an A2F-3D service
class FAudio2XSession: FNoncopyable
{

public:
	// By default, expects mono 16 kHz int16 samples. Anything else will be converted internally
	// Note: If the destructor is called without calling AbortCallbacks first, you may continue to receive callbacks unless bAbortCallbacksOnRelease is true
	FAudio2XSession(IA2FProvider* Provider, int32 InNumChannels, uint32 InSampleRate, int32 InSampleByteSize);
	~FAudio2XSession();

	// Start a session to send audio to an A2F-3D service and receive ACE animation data back.
	// Returns whether an active session is available
	bool StartSession(IACEAnimDataConsumer* CallbackObject);

	// Send audio samples from a float sample buffer.
	// If bEndOfSamples = true, any subsequent call to SendAudioSamples will be ignored.
	bool SendAudioSamples(TArrayView<const float> SamplesFloat, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, class UAudio2FaceParameters* Audio2FaceParameters);

	// Send audio samples from an int16 PCM sample buffer.
	// If bEndOfSamples = true, any subsequent call to SendAudioSamples will be ignored.
	bool SendAudioSamples(TArrayView<const int16> SamplesInt16, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, class UAudio2FaceParameters* Audio2FaceParameters);

	// Indicate to A2F-3D service that no more samples will be sent. Any subsequent call to SendAudioSamples will be ignored.
	// Use this if your last call to SendAudioSamples had bEndOfSamples = false, and now you know the audio stream has ended.
	bool EndAudioSamples();

private:
	const int32 NumChannels;
	const uint32 SampleRate;
	const int32 SampleByteSize;

	const bool bBurstAudio;
	const float MaxInitialAudioChunkSizeSeconds;

	IA2FProvider* const Provider;
	IA2FProvider::IA2FStream* Session;
	int32 SessionID;
	bool bSamplesStarted;
	bool bSamplesEnded;
	TArray<int16> QueuedSamples;
	TUniquePtr<Audio::FResampler> Resampler;
	TUniquePtr<class FSendRateLimiter> SendRateLimiter;

	FCriticalSection CS{};
	std::atomic<bool> bIsSendingSamples{};

	bool SendAudioSamplesInternal(TArrayView<const int16> SamplesInt16, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, class UAudio2FaceParameters* Audio2FaceParameters);
	bool EndAudioSamplesInternal();
};

