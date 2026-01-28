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

#include "OmniverseLiveLinkFramePlayer.h"
#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "GenericPlatform/GenericPlatformTime.h"
#include "OmniverseBaseListener.h"

std::unordered_map<const ILiveLinkSource*, TUniquePtr<FOmniverseLiveLinkFramePlayer>> FOmniverseLiveLinkFramePlayer::Instances;

FOmniverseLiveLinkFramePlayer::FOmniverseLiveLinkFramePlayer()
	: Thread(nullptr)
	, ThreadStopping(false)
	, ThreadReset(false)
	, AnimeListener(nullptr)
	, AudioListener(nullptr)
{
	
}

FOmniverseLiveLinkFramePlayer::~FOmniverseLiveLinkFramePlayer()
{
	Stop();

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
	}
}

void FOmniverseLiveLinkFramePlayer::Start()
{
	if (Thread == nullptr)
	{
		FString ThreadName = TEXT("Omniverse LiveLink Replayer ");
		ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
		Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
	}
	ThreadStopping = false;
}

void FOmniverseLiveLinkFramePlayer::Reset()
{
	AudioPendBuffer.Empty();
	AnimePendBuffer.Empty();
	ThreadReset = true;
	// Make sure the instance is properly destroyed
	auto Source = std::find_if(std::begin(Instances), std::end(Instances),
		[this](auto&& p)
		{
			return &*p.second == this;
		}
	)->first;
	Instances[Source].Reset();
	Instances.erase(Source);
}

void FOmniverseLiveLinkFramePlayer::Stop()
{
	ThreadStopping = true;
}

void FOmniverseLiveLinkFramePlayer::RegisterAnime(TSharedPtr<class FOmniverseBaseListener, ESPMode::ThreadSafe> Listener)
{
	AnimeListener = Listener;
}

void FOmniverseLiveLinkFramePlayer::RegisterAudio(TSharedPtr<class FOmniverseBaseListener, ESPMode::ThreadSafe> Listener)
{
	AudioListener = Listener;
}

void FOmniverseLiveLinkFramePlayer::PushAudioData_AnyThread(const uint8* InData, int32 InSize, double DeltaTime, bool bBegin, bool bEnd)
{
	AudioPendBuffer.Enqueue({ std::string((char*)InData, InSize), DeltaTime, bBegin, bEnd });
}

void FOmniverseLiveLinkFramePlayer::PushAnimeData_AnyThread(const uint8* InData, int32 InSize, double DeltaTime, bool bBegin, bool bEnd)
{
	AnimePendBuffer.Enqueue({ std::string((char*)InData, InSize), DeltaTime, bBegin, bEnd });
}

void FOmniverseLiveLinkFramePlayer::PlayAudio(double CurrentTime)
{
	if (AudioListener)
	{
		AudioListener->OnPackageDataReceived((uint8*)CurrentAudio.GetValue().Buffer.data(), CurrentAudio.GetValue().Buffer.size());
	}
	CurrentAudio.Reset();
	LastAudioPlayTime = CurrentTime;
}

void FOmniverseLiveLinkFramePlayer::PlayAnime(double CurrentTime)
{
	if (AnimeListener)
	{
		AnimeListener->OnPackageDataReceived((uint8*)CurrentAnime.GetValue().Buffer.data(), CurrentAnime.GetValue().Buffer.size());
	}
	CurrentAnime.Reset();
	LastAnimePlayTime = CurrentTime;
}

uint32 FOmniverseLiveLinkFramePlayer::Run()
{
	while (!ThreadStopping)
	{
		if (ThreadStopping)
		{
			break;
		}

		if (ThreadReset)
		{
			CurrentAudio.Reset();
			CurrentAnime.Reset();
			ThreadReset = false;
		}

		if (!CurrentAudio.IsSet())
		{
			FPendBuffer DequeueData;
			if (AudioPendBuffer.Dequeue(DequeueData))
			{
				CurrentAudio = DequeueData;
			}
		}

		if (!CurrentAnime.IsSet())
		{
			FPendBuffer DequeueData;
			if (AnimePendBuffer.Dequeue(DequeueData))
			{
				CurrentAnime = DequeueData;
			}
		}

		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentAudio.IsSet() && CurrentTime - LastAudioPlayTime >= CurrentAudio.GetValue().DeltaPendingTime)
		{
			if (CurrentAudio.GetValue().BeginFence)
			{
				Fence &= 0xFE;
			}

			bool IsAvailable = false;
			if (CurrentAudio.GetValue().EndFence)
			{
				Fence |= 0x1;
				if (Fence == UINT8_MAX)
				{
					IsAvailable = true;
				}
			}
			else
			{
				IsAvailable = true;
			}

			if (IsAvailable)
			{
				PlayAudio(CurrentTime);
			}
		}

		if (CurrentAnime.IsSet() && CurrentTime - LastAnimePlayTime >= CurrentAnime.GetValue().DeltaPendingTime)
		{
			if (CurrentAnime.GetValue().BeginFence)
			{
				Fence &= 0xFD;
			}

			bool IsAvailable = false;
			if (CurrentAnime.GetValue().EndFence)
			{
				Fence |= 0x2;
				if (Fence == UINT8_MAX)
				{
					IsAvailable = true;
				}
			}
			else
			{
				IsAvailable = true;
			}

			if (IsAvailable)
			{
				PlayAnime(CurrentTime);
			}
		}
	}
	return 0;
}

FOmniverseLiveLinkFramePlayer& FOmniverseLiveLinkFramePlayer::Get(const ILiveLinkSource* Source)
{
	if (!Instances[Source].IsValid())
	{
		Instances[Source] = MakeUnique<FOmniverseLiveLinkFramePlayer>();
	}
	return *Instances[Source];
}
