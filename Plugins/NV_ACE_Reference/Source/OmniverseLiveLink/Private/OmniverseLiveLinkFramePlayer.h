/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include <string>
#include <unordered_map>

class ILiveLinkSource;

struct FPendBuffer
{
	// use std::string to avoid TChar to uint8 conversion
	std::string Buffer;
	double DeltaPendingTime = 0.0;
	bool BeginFence = false;
	bool EndFence = false;
};

class FOmniverseLiveLinkFramePlayer : public FRunnable
{
public:
	DECLARE_DELEGATE_TwoParams(FOnFramePlayed, const uint8*, int32);
public:
	FOmniverseLiveLinkFramePlayer();
	virtual ~FOmniverseLiveLinkFramePlayer();

	// Begin FRunnable Interface
	virtual void Stop() override;
	// End FRunnable Interface

	void Start();
	void Reset();

	void PushAnimeData_AnyThread(const uint8* InData, int32 InSize, double DeltaTime, bool bBegin, bool bEnd);
	void PushAudioData_AnyThread(const uint8* InData, int32 InSize, double DeltaTime, bool bBegin, bool bEnd);

	void RegisterAnime(TSharedPtr<class FOmniverseBaseListener, ESPMode::ThreadSafe> Listener);
	void RegisterAudio(TSharedPtr<class FOmniverseBaseListener, ESPMode::ThreadSafe> Listener);


	static FOmniverseLiveLinkFramePlayer& Get(const ILiveLinkSource* Source);

protected:
	// Begin FRunnable Interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Exit() override {}
	// End FRunnable Interface

private:
	void PlayAudio(double CurrentTime);
	void PlayAnime(double CurrentTime);

	// Thread to run work operations on
	class FRunnableThread* Thread;

	// Threadsafe Bool for terminating the main thread loop
	FThreadSafeBool ThreadStopping;

	TQueue<FPendBuffer> AudioPendBuffer;
	TQueue<FPendBuffer> AnimePendBuffer;

	double LastAnimePlayTime = 0.0;
	double LastAudioPlayTime = 0.0;


	static std::unordered_map<const ILiveLinkSource*, TUniquePtr<class FOmniverseLiveLinkFramePlayer>> Instances;

	TOptional<FPendBuffer> CurrentAudio;
	TOptional<FPendBuffer> CurrentAnime;

	uint8 Fence = UINT8_MAX;
	FThreadSafeBool ThreadReset;

	TSharedPtr<class FOmniverseBaseListener, ESPMode::ThreadSafe> AnimeListener;
	TSharedPtr<class FOmniverseBaseListener, ESPMode::ThreadSafe> AudioListener;
};
