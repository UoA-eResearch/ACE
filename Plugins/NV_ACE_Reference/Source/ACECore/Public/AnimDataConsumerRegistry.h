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


class FACEAnimDataChunk;
class IACEAnimDataConsumer;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FAnimDataConsumerRegistry abstracts away consumers and providers of ACE animation data, so that they may be
/// implemented independently in different modules or even in different Unreal plugins. The registry is a singleton
/// that should be acquired with FAnimDataConsumerRegistry::Get().
/// Once acquired, any member functions named with _AnyThread are safe to call from any thread.
///
/// A minimal animation data provider implementation will do the following things:
/// - Use CreateStream_AnyThread to get a unique int32 stream ID that identifies a stream of animation data from the
///   provider.
/// - Either directly call AttachConsumerToStream_AnyThread to connect the stream to a specific consumer, or pass the
///   new stream ID to some other system that will eventually call AttachConsumerToStream_AnyThread.
/// - Call SendAnimData_AnyThread one or more times with FACEAnimDataChunk data, one chunk per animation frame.
///   - The last frame should have FACEAnimDataChunk::Status set to EACEAnimDataStatus::OK_NO_MORE_DATA to indicate end
///     of animation.
///   - Note that in the case of animation with no audio, the current convention is for the provider to set
///     FACEAnimDataChunk::AudioBuffer to a buffer of silence corresponding to the length of the animation frame. A
///     buffer can be filled with zeroes for the appropriate length. For example, if int16 samples at 16000 samples
///     per second are being used, then 0.03 seconds of silence would be equivalent to:
///     0.03 seconds * 16000 samples/second * 2 bytes/sample = a length 960 TArrayView<const uint8> of all 0's.
class ACECORE_API FAnimDataConsumerRegistry {
public:
	// get the singleton registry
	static FAnimDataConsumerRegistry* Get();

	// creates a new stream
	int32 CreateStream_AnyThread();

	// explicitly removes a stream and its mapping to any registered consumers.
	// may call ConsumeAnimData_AnyThread with EACEAnimDataStatus::OK_NO_MORE_DATA to end the stream.
	// note that streams also remove themselves when SendAnimData_AnyThread is called with AnimData.Status == EACEAnimDataStatus::OK_NO_MORE_DATA
	void RemoveStream_AnyThread(int32 StreamID);

	// consumer will receive output of stream
	void AttachConsumerToStream_AnyThread(int32 StreamID, IACEAnimDataConsumer* Consumer, uint32 SampleRate = 16'000, int32 NumChannels = 1, int32 SampleByteSize = 2);

	// Change the sample rate and/or number of channels that this stream will produce.
	// It is an error to call this after any data has already been produced with SendAnimData_AnyThread
	void SetAudioParams_AnyThread(int32 StreamID, uint32 NewSampleRate, int32 NewNumChannels, int32 SampleByteSize);

	// cancel consumer receiving output from any stream.
	// may call ConsumeAnimData_AnyThread with EACEAnimDataStatus::OK_NO_MORE_DATA to end the stream
	void DetachConsumer_AnyThread(IACEAnimDataConsumer* Consumer);

	// calls ConsumeAnimData_AnyThread on all mapped consumers for a given stream ID.
	// returns number of mapped consumers
	int32 SendAnimData_AnyThread(const FACEAnimDataChunk& AnimData, int32 StreamID);

	// returns whether the given stream has anyone listening any more. Can be used to avoid doing extra work if stream is no longer useful
	bool DoesStreamHaveConsumers_AnyThread(int32 StreamID);

private:
	void CancelStreamToConsumer_AnyThread(int32 StreamID, IACEAnimDataConsumer* Consumer);

	friend class IACEAnimDataConsumer;
	// called by IACEAnimDataConsumer ctor
	void RegisterConsumer_AnyThread(IACEAnimDataConsumer* Consumer);
	// called by IACEAnimDataConsumer dtor
	void UnregisterConsumer_AnyThread(IACEAnimDataConsumer* Consumer);

private:

	FCriticalSection DataCS;
	TSet<IACEAnimDataConsumer*> ActiveConsumers;
	TMap<int32, IACEAnimDataConsumer*> StreamToConsumerMap;
	TMap<IACEAnimDataConsumer*, int32> ConsumerToStreamMap;

	std::atomic<int32> NextStreamId = 0;
};

