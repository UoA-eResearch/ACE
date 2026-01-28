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

#include "AsyncActionAnimateCharacter.h"

// engine includes
#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/EngineVersionComparison.h"

// plugin includes
#include "ACEBlueprintLibrary.h"
#include "ACERuntimePrivate.h"


static void RunBroadastOnGameThread(FAsyncAnimateCharacterOutputPin&& AudioSendCompleted, bool bSuccess)
{
	if (ensure(!IsInGameThread()))
	{
		AsyncTask(ENamedThreads::GameThread, [AudioSendCompleted = MoveTemp(AudioSendCompleted), bSuccess]()
		{
			AudioSendCompleted.Broadcast(bSuccess);
		});
	}
	else
	{
		AudioSendCompleted.Broadcast(bSuccess);
	}
}

class FAnimateCharacterRunnable : public FRunnable
{
public:
	FAnimateCharacterRunnable(UAsyncActionAnimateCharacter* InAction)
		: Action(InAction), TaskCompletedEvent(FPlatformProcess::GetSynchEventFromPool(true))
	{
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("FAnimateCharacterRunnable thread created"));
	}

	virtual ~FAnimateCharacterRunnable() override
	{
		if (TaskCompletedEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(TaskCompletedEvent);
			TaskCompletedEvent = nullptr;
		}
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("FAnimateCharacterRunnable thread destroyed"));
	}

	virtual bool Init() override
	{
		// Initialization logic here
		return true;
	}

	virtual uint32 Run() override
	{
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("FAnimateCharacterRunnable thread start run"));
		// Thread logic here
		bool bSuccess = false;
		switch (Action->Source)
		{
		case UAsyncActionAnimateCharacter::EAnimationSource::SoundWave:
			bSuccess = UACEBlueprintLibrary::AnimateCharacterFromSoundWave(Action->Character, Action->SoundWave, Action->EmotionParameters, Action->FaceParameters, Action->ProviderName);
			break;
		case UAsyncActionAnimateCharacter::EAnimationSource::WavFile:
			bSuccess = UACEBlueprintLibrary::AnimateCharacterFromWavFile(Action->Character, Action->PathToWav, Action->EmotionParameters, Action->FaceParameters, Action->ProviderName);
			break;
		default:
			unimplemented();
		}

		// Run the broadcast on the game thread
		// Note: be sure this is the only place we use AudioSendCompleted because it gets moved to the game thread here!
		RunBroadastOnGameThread(MoveTemp(Action->AudioSendCompleted), bSuccess);
		Action->bIsActive.store(false);
		Action->SetReadyToDestroy();

		// Signal that the task is completed
		TaskCompletedEvent->Trigger();

		UE_LOG(LogACERuntime, VeryVerbose, TEXT("FAnimateCharacterRunnable thread end run"));

		return 0;
	}

	virtual void Exit() override
	{
		Action->bIsActive.store(false);
	}

	void WaitForCompletion()
	{
		if (TaskCompletedEvent)
		{
			TaskCompletedEvent->Wait();
		}
	}

private:
	UAsyncActionAnimateCharacter* Action;
	FEvent* TaskCompletedEvent;
};

UAsyncActionAnimateCharacter* UAsyncActionAnimateCharacter::AnimateCharacterFromSoundWaveAsync(UObject* WorldContextObject, AActor* Character,
	USoundWave* SoundWave, const FAudio2FaceEmotion& EmotionParameters, UAudio2FaceParameters* FaceParameters, FName ProviderName)
{
	UAsyncActionAnimateCharacter* Action = NewObject<UAsyncActionAnimateCharacter>();

	if (Action != nullptr)
	{
		Action->Source = EAnimationSource::SoundWave;
		Action->Character = Character;
		Action->SoundWave = SoundWave;
		Action->EmotionParameters = EmotionParameters;
		Action->FaceParameters = FaceParameters;
		Action->ProviderName = ProviderName;
		Action->RegisterWithGameInstance(WorldContextObject);
	}

	return Action;
}

UAsyncActionAnimateCharacter* UAsyncActionAnimateCharacter::AnimateCharacterFromWavFileAsync(UObject* WorldContextObject, AActor* Character,
	const FString& PathToWav, const FAudio2FaceEmotion& EmotionParameters, UAudio2FaceParameters* FaceParameters, FName ProviderName)
{
	UAsyncActionAnimateCharacter* Action = NewObject<UAsyncActionAnimateCharacter>();

	if (Action != nullptr)
	{
		Action->Source = EAnimationSource::WavFile;
		Action->Character = Character;
		Action->PathToWav = PathToWav;
		Action->EmotionParameters = EmotionParameters;
		Action->FaceParameters = FaceParameters;
		Action->ProviderName = ProviderName;
		Action->RegisterWithGameInstance(WorldContextObject);
	}

	return Action;
}

void UAsyncActionAnimateCharacter::Activate()
{
	bIsActive.store(true);
	Runnable = new FAnimateCharacterRunnable(this);
	FRunnableThread* Thread = FRunnableThread::Create(Runnable, TEXT("FAnimateCharacterRunnable"), 0, TPri_Highest);

	if (Thread)
	{
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("AsyncActionAnimateCharacter thread started"));
	}
	else
	{
		delete Runnable;
		bIsActive.store(false);
		UE_LOG(LogACERuntime, Error, TEXT("Failed to start AsyncActionAnimateCharacter thread"));
	}
}

bool UAsyncActionAnimateCharacter::IsReadyForFinishDestroy()
{
	// Prevent FinishDestroy from running while the async task is active. Addresses crashes seen on exit.
	// The fact that we even need this points to RegisterWithGameInstance/SetReadyToDestroy not working as advertised.
	return Super::IsReadyForFinishDestroy() && !bIsActive.load();
}

UAsyncActionAnimateCharacter::~UAsyncActionAnimateCharacter()
{
	if (Runnable)
	{
		Runnable->WaitForCompletion();
		delete Runnable;
		Runnable = nullptr;
	}
}
