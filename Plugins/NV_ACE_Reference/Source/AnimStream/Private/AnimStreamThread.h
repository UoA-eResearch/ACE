/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"


struct FAIMAnimgraphFeature;

namespace nvaim
{
	struct InferenceInstance;
}


const uint32 DEFAULT_SAMPLE_RATE = 16'000;
const int32 DEFAULT_NUM_CHANNELS = 1;
const int32 DEFAULT_SAMPLE_BYTE_SIZE = sizeof(int16_t);

enum class EACEAnimStreamState: int32
{
	// still connecting to the service, no data streaming yet
	CONNECTING,
	// connection to service failed, final state
	CONNECTION_FAILED,
	// successfully connected and streaming data
	STREAMING,
	// streaming RPC failed, final state
	STREAM_FAILED,
	// streaming complete, final state
	STREAM_COMPLETE,
};

inline bool IsFinalState(EACEAnimStreamState State)
{
	return (State == EACEAnimStreamState::STREAM_COMPLETE) ||
		(State == EACEAnimStreamState::STREAM_FAILED) ||
		(State == EACEAnimStreamState::CONNECTION_FAILED);
}

class FAnimStreamThread : public FRunnable
{
public:
	FAnimStreamThread(int32 InStreamID, FString InURL, FString InStreamName, TSharedPtr<FAIMAnimgraphFeature> InAnimgraph, int32 NumOfRetries, float TimeBetweenRetries, float RPCTimeout);

	EACEAnimStreamState GetState() const;

	///////////////////
	// begin FRunnable

	// @return True if initialization was successful, false otherwise
	virtual bool Init() override;

	// @return The exit code of the runnable object
	virtual uint32 Run() override;

	// This is called if a thread is requested to terminate early.
	virtual void Stop() override;

	// Called in the context of the aggregating thread to perform any cleanup.
	virtual void Exit() override;

	// end FRunnable
	///////////////////

private:
	int32 StreamID;
	FString DestURL;
	int32 NumOfRetries;
	float TimeBetweenRetries;
	float RPCTimeout;
	FString StreamName;
	TUniquePtr<FRunnableThread> Thread;
	TSharedPtr<FAIMAnimgraphFeature> Animgraph;
	nvaim::InferenceInstance* Connection = nullptr;
	std::atomic<EACEAnimStreamState> State;
};

