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

// project includes
#include "ACETypes.h"


// base class for providers of a bidirectional stream to send audio out, get audio and animations in
class ACECORE_API IA2FProvider {
public:
	////////////////////
	// Common interface
	virtual ~IA2FProvider() {};

	struct IA2FStream
	{
		// Implementations may provide a unique identifier for each stream for use in logging/tracing
		virtual int32 GetID() const = 0;
		static const int32 INVALID_STREAM_ID = -1;

		// Debug name of the provider of the bidirectional stream. Must match corresponding IA2FProvider::GetName().
		// Internal to the IA2FProvider implementation, this can be used to ensure that the provider is being given a compatible stream
		virtual FName GetProviderName() const = 0;
	};

	// Start a stream to send audio to an A2F-3D service.
	virtual IA2FStream* CreateA2FStream(class IACEAnimDataConsumer* CallbackObject) = 0;

	// Send audio samples to an A2F-3D stream, with optional emotion state.
	// Samples are PCM16 mono, 16 kHz sample rate.
	virtual bool SendAudioSamples(
		IA2FStream* Stream,
		TArrayView<const int16> SamplesInt16,
		TOptional<struct FAudio2FaceEmotion> EmotionParameters,
		class UAudio2FaceParameters* Audio2FaceParameters) = 0;

	// Indicate no more samples will be sent to an A2F-3D stream
	virtual bool EndOutgoingStream(IA2FStream* Stream) = 0;

	// Minimum number of PCM16 samples required in the initial call to SendAudioSamples
	// If an implementation has no such limitation, this could return 1 or even 0
	virtual int32 GetMinimumInitialAudioSampleCount() const = 0;

	// Debug name of the A2F-3D provider. Must match IA2FProvider::IA2FStream::GetProviderName().
	// External to the IA2FProvider implementation, this can be used to log which A2F-3D provider is being used
	virtual FName GetName() const = 0;

	// Optionally pre-allocate any resources needed by the provider
	virtual void AllocateResources() {}

	// Optionally free any resources allocated by the provider when it is safe to do so
	virtual void FreeResources() {}

	// If the IA2FProvider provides remote execution, return the interface
	virtual class IA2FRemoteProvider* GetRemoteProvider() { return nullptr; }

	// If the IA2FProvider can pass through arbitrary sample rate audio to the IACEAnimDataConsumer, return the interface
	virtual class IA2FPassthroughProvider* GetAudioPassthroughProvider() { return nullptr; }

	// Should be called by whatever instantiates the singleton object of the derived class
	void Register();

	///////////
	// static methods

	// Find provider by name. May return nullptr if no provider by that name is registered
	static IA2FProvider* FindProvider(FName ProviderName);

	// Get names of all registered providers
	static TArray<FName> GetAvailableProviderNames();

private:
	static TMap<FName, IA2FProvider*> RegisteredProviders;
};


class IA2FRemoteProvider
{
public:
	// Override the provider's connection settings.
	// If any strings are blank, the project default should be used.
	// If a connection is already established and the connection settings are changed by this call, the connection should be immediately terminated.
	virtual void SetConnectionInfo(const FString& URL, const FString& APIKey = FString(), const FString& NvCFFunctionId = FString(), const FString& NvCFFunctionVersion = FString()) = 0;

	// Get current connection information
	virtual FACEConnectionInfo GetConnectionInfo() const = 0;

	////////////////////
	// Convenience functions
	FString GetA2FURL() const
	{
		FACEConnectionInfo ConnectionInfo = GetConnectionInfo();
		return ConnectionInfo.DestURL;
	}
};


class IA2FPassthroughProvider
{
public:
	// This should be called once before EnqueueOriginalSamples for a given stream
	virtual void SetOriginalAudioParams(IA2FProvider::IA2FStream* Stream, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize) = 0;

	// This should be called before SendAudioSamples for each chunk of audio
	virtual void EnqueueOriginalSamples(IA2FProvider::IA2FStream* Stream, TArrayView<const uint8> OriginalSamples) = 0;
};

