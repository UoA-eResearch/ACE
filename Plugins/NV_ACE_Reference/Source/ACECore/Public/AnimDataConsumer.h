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

#include "CoreMinimal.h"


enum class EACEAnimDataStatus
{
	// normal data chunk
	OK,
	// dummy chunk to indicate no more chunks will arrive in this stream
	OK_NO_MORE_DATA,
	// something unusual detected in data received from animation provider, but chunk passed through as-is
	ERROR_UNEXPECTED_OUTPUT
};

class FACEAnimDataChunk
{
public:
	// Typically BlendShapeNames would be sent only once in the first message, with or without any weights
	TArrayView<const FName> BlendShapeNames;

	// The length of BlendShapeWeights is expected to match the length of BlendShapeNames from the initial message
	TArrayView<const float> BlendShapeWeights;

	// Byte array corresponding to audio data samples
	TArrayView<const uint8> AudioBuffer;

	// This should have been named AnimationTimestamp, it tells where to align this chunk's blend shape and joint data
	// relative to the beginning of the audio playback
	double Timestamp;

	EACEAnimDataStatus Status;
};

// Consumers of ACE animation data implement IACEAnimDataConsumer.
// Typically it should be implemented by a component attached to a character actor, and the received data used to animate the character.
// Will only receive one stream at a time. The StreamID parameters are present for logging/tracing only, and don't necessarily need to be used.
class ACECORE_API IACEAnimDataConsumer
{
public:
	IACEAnimDataConsumer();
	virtual ~IACEAnimDataConsumer();

	// called before first ConsumeAnimData_AnyThread callback for a given stream.
	// we promise not to overlap calls to PrepareNewStream_AnyThread with ConsumeAnimData_AnyThread
	virtual void PrepareNewStream_AnyThread(int32 StreamID, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize) = 0;

	// called when new animation data is received from the stream.
	// probably won't be called from the game thread
	virtual void ConsumeAnimData_AnyThread(const FACEAnimDataChunk& AnimData, int32 StreamID) = 0;
};

