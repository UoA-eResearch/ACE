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

// project includes
#include "A2FProvider.h"
#include "ACETypes.h"


namespace nvaim
{
	struct InferenceInstance;
	struct InferenceInterface;
}

class A2FREMOTE_API FA2FRemote: public IA2FProvider, public IA2FRemoteProvider, public IA2FPassthroughProvider, FNoncopyable
{
public:
	FA2FRemote();
	~FA2FRemote();

	bool IsAvailable() const { return bIsFeatureAvailable; }

	/////////////////
	// begin IA2FProvider interface

	// Start a session to send audio to an A2F-3D service.
	virtual IA2FStream* CreateA2FStream(IACEAnimDataConsumer* CallbackObject) override final;

	// Send audio samples to an A2F-3D stream, with optional emotion state and A2F-3D parameters
	virtual bool SendAudioSamples(
		IA2FStream* Session,
		TArrayView<const int16> SamplesInt16,
		TOptional<struct FAudio2FaceEmotion> EmotionParameters,
		class UAudio2FaceParameters* Audio2FaceParameters) override final;

	// Indicate no more samples will be sent to an A2F-3D stream
	virtual bool EndOutgoingStream(IA2FStream* Stream) override final;

	// Minimum number of PCM16 samples required in the initial call to SendAudioSamples
	virtual int32 GetMinimumInitialAudioSampleCount() const override final { return 1; }

	// Debug name of the A2F-3D provider. Must match IA2FProvider::IA2FStream::GetProviderName().
	virtual FName GetName() const override final;

	// Disconnect from the service when it is safe to do so
	virtual void FreeResources() override final;

	// we provide remote execution, return the interface
	virtual IA2FRemoteProvider* GetRemoteProvider() override final { return this; }

	// we can pass through arbitrary sample rate audio to the IACEAnimDataConsumer, return the interface
	virtual IA2FPassthroughProvider* GetAudioPassthroughProvider() override final { return this; }

	// end IA2FProvider interface
	/////////////////

	/////////////////
	// begin IA2FRemoteProvider interface

	// Override the provider's connection settings.
	// If any strings are blank, the project default should be used.
	// If a connection is already established and the connection settings are changed by this call, the connection should be immediately terminated.
	virtual void SetConnectionInfo(const FString& URL, const FString& APIKey = FString(), const FString& NvCFFunctionId = FString(), const FString& NvCFFunctionVersion = FString());

	// get current connection information
	virtual FACEConnectionInfo GetConnectionInfo() const override final;

	// end IA2FRemoteProvider interface
	/////////////////

	/////////////////
	// begin IA2FPassthroughProvider interface

	// This should be called once before EnqueueOriginalSamples for a given stream
	virtual void SetOriginalAudioParams(IA2FStream* Stream, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize) override final;

	// This should be called before SendAudioSamples for each chunk of audio
	virtual void EnqueueOriginalSamples(IA2FStream* Stream, TArrayView<const uint8> OriginalSamples) override final;

	// end IA2FPassthroughProvider interface
	/////////////////

private:

	static const int32 DEFAULT_SAMPLE_RATE = 16'000;
	static const int32 DEFAULT_NUM_CHANNELS = 1;
	static const int32 DEFAULT_SAMPLE_BYTE_SIZE = sizeof(int16_t);

	bool bIsFeatureAvailable = false;

	nvaim::InferenceInterface* Interface = nullptr;
	TPimplPtr<class FAIMInferenceInstance> Connection;
	FACEConnectionInfo ACEConnectionInfo;
	FACEConnectionInfo ACEOverrideConnectionInfo;

private:

	bool IsConnectionAvailable();
	nvaim::InferenceInstance* CreateConnection();

};

