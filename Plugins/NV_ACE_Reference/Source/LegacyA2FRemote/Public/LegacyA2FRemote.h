/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

class LEGACYA2FREMOTE_API FLegacyA2FRemote: public IA2FProvider, public IA2FRemoteProvider
{
public:
	/////////////////
	// begin IA2FProvider interface
	virtual ~FLegacyA2FRemote();

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
	virtual int32 GetMinimumInitialAudioSampleCount() const override final;

	// Debug name of the A2F-3D provider. Must match IA2FProvider::IA2FStream::GetProviderName().
	virtual FName GetName() const override final;

	// If the IA2FProvider provides remote execution, return the interface
	virtual class IA2FRemoteProvider* GetRemoteProvider() override final { return this; }

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

	// get a singleton from the owning ACERuntime module
	static FLegacyA2FRemote* Get();

	// get a singleton from the owning ACERuntime module
	static FLegacyA2FRemote& GetChecked();

	// internally used by Audio2Face-3D callback and session management
	void RemoveContext(int32 StreamID);

	struct FAudio2FaceContext: public IA2FStream
	{
		// begin IA2FStream
		virtual int32 GetID() const override { return StreamID;  }
		virtual FName GetProviderName() const override;
		// end IA2FStream

		struct NvACEA2XSession* Session = nullptr;
		int32 StreamID = INVALID_STREAM_ID;
	};

private:

	FACEConnectionInfo ACEConnectionInfo;
	FACEConnectionInfo ACEOverrideConnectionInfo;

	struct NvACEClientLibrary* ACL = nullptr;

	struct NvACEA2XConnection* A2XConnection = nullptr;
	struct NvACEA2XParameters* A2XParameterHandle = nullptr;
	TMap<FString, float> CachedA2FParams;

	// chunked array so that the memory won't be reallocated or moved around, since pointers to things in this array will come back via callback
	TChunkedArray<FAudio2FaceContext> Contexts;

private:

	FAudio2FaceContext* AddContext(IACEAnimDataConsumer* CallbackObject);
	struct NvACEA2XConnection* GetA2XConnection(bool bRecreate = false);
	struct NvACEA2XParameters* GetA2XParameterHandle(class UAudio2FaceParameters* A2FParameters);

};

