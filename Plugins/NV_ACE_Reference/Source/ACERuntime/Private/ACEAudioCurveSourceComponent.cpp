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

#include "ACEAudioCurveSourceComponent.h"

// engine includes
#include "Async/Async.h"
#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
#include "Engine/Engine.h"
#include "Interfaces/IAudioFormat.h"

// plugin includes
#include "A2XSession.h"
#include "ACEBlueprintLibrary.h"
#include "ACERuntimePrivate.h"
#include "AnimDataConsumer.h"
#include "AnimDataConsumerRegistry.h"
#include "ProceduralSound.h"


const FName UACEAudioCurveSourceComponent::CurveNames[55] =
{
	FName(TEXT("EyeBlinkLeft")),
	FName(TEXT("EyeLookDownLeft")),
	FName(TEXT("EyeLookInLeft")),
	FName(TEXT("EyeLookOutLeft")),
	FName(TEXT("EyeLookUpLeft")),
	FName(TEXT("EyeSquintLeft")),
	FName(TEXT("EyeWideLeft")),
	FName(TEXT("EyeBlinkRight")),
	FName(TEXT("EyeLookDownRight")),
	FName(TEXT("EyeLookInRight")),
	FName(TEXT("EyeLookOutRight")),
	FName(TEXT("EyeLookUpRight")),
	FName(TEXT("EyeSquintRight")),
	FName(TEXT("EyeWideRight")),
	FName(TEXT("JawForward")),
	FName(TEXT("JawLeft")),
	FName(TEXT("JawRight")),
	FName(TEXT("JawOpen")),
	FName(TEXT("MouthClose")),
	FName(TEXT("MouthFunnel")),
	FName(TEXT("MouthPucker")),
	FName(TEXT("MouthLeft")),
	FName(TEXT("MouthRight")),
	FName(TEXT("MouthSmileLeft")),
	FName(TEXT("MouthSmileRight")),
	FName(TEXT("MouthFrownLeft")),
	FName(TEXT("MouthFrownRight")),
	FName(TEXT("MouthDimpleLeft")),
	FName(TEXT("MouthDimpleRight")),
	FName(TEXT("MouthStretchLeft")),
	FName(TEXT("MouthStretchRight")),
	FName(TEXT("MouthRollLower")),
	FName(TEXT("MouthRollUpper")),
	FName(TEXT("MouthShrugLower")),
	FName(TEXT("MouthShrugUpper")),
	FName(TEXT("MouthPressLeft")),
	FName(TEXT("MouthPressRight")),
	FName(TEXT("MouthLowerDownLeft")),
	FName(TEXT("MouthLowerDownRight")),
	FName(TEXT("MouthUpperUpLeft")),
	FName(TEXT("MouthUpperUpRight")),
	FName(TEXT("BrowDownLeft")),
	FName(TEXT("BrowDownRight")),
	FName(TEXT("BrowInnerUp")),
	FName(TEXT("BrowOuterUpLeft")),
	FName(TEXT("BrowOuterUpRight")),
	FName(TEXT("CheekPuff")),
	FName(TEXT("CheekSquintLeft")),
	FName(TEXT("CheekSquintRight")),
	FName(TEXT("NoseSneerLeft")),
	FName(TEXT("NoseSneerRight")),
	FName(TEXT("TongueOut")),
	// Note: HeadRoll/Pitch/Yaw are not real blend shapes and need to be turned into a rotation for the head bone
	FName(TEXT("HeadRoll")),
	FName(TEXT("HeadPitch")),
	FName(TEXT("HeadYaw"))
};


// define this ctor only to make the TUniquePtr dtor happy with our forward-declared types
UACEAudioCurveSourceComponent::UACEAudioCurveSourceComponent(FVTableHelper& Helper) {}

// ctor
UACEAudioCurveSourceComponent::UACEAudioCurveSourceComponent() :
	SoundGroup(ESoundGroup::SOUNDGROUP_Voice),
	Priority(1.0f),
	Volume(1.0f),
	ReceivedAudioSamples(0),
	AudioCompCS(),
	ReceivedBSWeightSamples(0),
	CurrentSessionID(IA2FProvider::IA2FStream::INVALID_STREAM_ID),
	LastSampleIdx(-1),
	LastUpdatedGlobalTime(0),
	AudioPlaybackTimeEstimate(0.0f),
	CurrentPlaybackTime(0.0f),
	LastAnimPlaybackTime(0.0f)
{
	// make this component tickable so we can run audio code from game thread
	PrimaryComponentTick.bCanEverTick = true;
}

// dtor
UACEAudioCurveSourceComponent::~UACEAudioCurveSourceComponent()
{

}

void UACEAudioCurveSourceComponent::BeginDestroy()
{
	// we don't want any more callbacks
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry != nullptr)
	{
		FAnimDataConsumerRegistry::Get()->DetachConsumer_AnyThread(this);
	}

	Super::BeginDestroy();
}

void UACEAudioCurveSourceComponent::PrepareNewAudioComponent_GameThread(int32 StreamID, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize)
{
	check(IsInGameThread());

	// Create/replace audio component
	UAudioComponent* NewAudioComponent = CreateAudioComponent_GameThread(SampleRate, NumChannels, SampleByteSize);
	{
		FScopeLock Lock(&AudioCompCS);

		// clean up any existing audio component
		if (AudioComponent != nullptr)
		{
			// stop the existing AudioComponent in case it's still playing a previously received audio stream
			AudioComponent->Stop();
			// TODO: Is this correct to explictly detach the existing AudioComponent here? Need to think through possible race conditions.
			AudioComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		}

		// finish setting up new audio component
		AudioComponent = NewAudioComponent;
		if (AudioComponent != nullptr)
		{
			AudioComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
			// make this component tickable so we can run audio code from game thread
			PrimaryComponentTick.bCanEverTick = true;
		}
		else
		{
			// no audio available, so no need to tick this component
			PrimaryComponentTick.bCanEverTick = false;
		}
		ReceivedAudioSamples = 0;
		ReceivedBSWeightSamples.store(0);
	}

	// reset blend shape weights, elapsed play time, timestamps, etc
	BSWeightSampleQueue.Empty();
	ResetAnimSamples();
	LastUpdatedGlobalTime = 0;
	AudioPlaybackTimeEstimate = 0.0f;
	CurrentPlaybackTime = 0.0f;
	LastAnimPlaybackTime = 0.0f;
	for (float& RecentPlaybackTime : RecentPlaybackTimes)
	{
		RecentPlaybackTime = 0.0f;
	}
	CurrentSessionID = StreamID;
	FirstACETimestamp.Reset();

	AudioCompReady.Notify();
}

// Note: can only create audio component from game thread
UAudioComponent* UACEAudioCurveSourceComponent::CreateAudioComponent_GameThread(uint32 SampleRate, int32 NumChannels, int32 SampleByteSize)
{
	check(IsInGameThread());
	UAudioComponent* NewAudioComponent = nullptr;

	// find audio device
	FAudioDeviceHandle AudioDevice;
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		AudioDevice = World->GetAudioDevice();
	}
	else
	{
		AudioDevice = GEngine->GetMainAudioDevice();
	}

	if (AudioDevice)
	{
		// Create a procedural sound, where the received audio samples will be queued
		UBetterSoundWaveProcedural* SoundStreaming = NewObject<UBetterSoundWaveProcedural>();
		if (SoundStreaming == nullptr)
		{
			UE_LOG(LogACERuntime, Warning, TEXT("Unable to create audio component for ACE animation"));
			return nullptr;
		}
		SoundStreaming->SetSampleRate(SampleRate);
		SoundStreaming->NumChannels = NumChannels;
		SoundStreaming->SampleByteSize = SampleByteSize;
		SoundStreaming->Duration = INDEFINITELY_LOOPING_DURATION;	// 166m40s ought to be enough for anybody
		SoundStreaming->Priority = Priority;
		SoundStreaming->SoundGroup = SoundGroup;
		SoundStreaming->bLooping = false;
		SoundStreaming->bProcedural = true;
		SoundStreaming->Volume = Volume;
		SoundStreaming->Pitch = 1.0f;
		SoundStreaming->AttenuationSettings = nullptr;
		SoundStreaming->bDebug = bEnableAttenuationDebug;
		SoundStreaming->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
		SoundStreaming->OnSoundWaveProceduralUnderflow.BindUObject(this, &UACEAudioCurveSourceComponent::HandleSoundUnderflow);

		// And now finally create the audio component to play the sound
		FAudioDevice::FCreateComponentParams Params(GetOwner());
		NewAudioComponent = AudioDevice->CreateComponent(SoundStreaming, Params);
		if (NewAudioComponent != nullptr)
		{
			// Should probably have a way to set bIsUISound for animation editor preview once we implement that, but for now it's never true
			const bool bIsUISound = false;
			NewAudioComponent->bIsUISound = bIsUISound;
			bool bAllowSpatialization = !bIsUISound && (AttenuationSettings || bOverrideAttenuation);
			NewAudioComponent->bAllowSpatialization = bAllowSpatialization;
			NewAudioComponent->bAutoActivate = false;
			NewAudioComponent->bAutoDestroy = true;
			NewAudioComponent->bOverrideAttenuation = bOverrideAttenuation;
			if (bOverrideAttenuation)
			{
				NewAudioComponent->AttenuationOverrides = AttenuationOverrides;
			}
			else
			{
				NewAudioComponent->AttenuationSettings = AttenuationSettings;
			}
			NewAudioComponent->OnAudioPlaybackPercentNative.AddUObject(this, &UACEAudioCurveSourceComponent::HandlePlaybackFraction);
		}
	}
	else
	{
		UE_LOG(LogACERuntime, Warning, TEXT("No audio device!"));
	}

	if (NewAudioComponent == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("Unable to create voice audio component!"));
	}

	return NewAudioComponent;
}

void UACEAudioCurveSourceComponent::PrepareNewStream_AnyThread(int32 StreamID, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize)
{
	AudioCompReady.Reset();
	AudioSampleRate = static_cast<float>(SampleRate);
	NumAudioChannels = NumChannels;
	AudioSampleByteSize = SampleByteSize;

	if (IsInGameThread())
	{
		PrepareNewAudioComponent_GameThread(StreamID, SampleRate, NumChannels, SampleByteSize);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, StreamID, SampleRate, NumChannels, SampleByteSize]()
		{
			PrepareNewAudioComponent_GameThread(StreamID, SampleRate, NumChannels, SampleByteSize);
		});
	}
}

void UACEAudioCurveSourceComponent::ConsumeAnimData_AnyThread(const FACEAnimDataChunk& Chunk, int32 SessionID)
{
	// make sure audio component has been set up on game thread
	AudioCompReady.Wait();

	const int32 ExpectedSessionID = CurrentSessionID;
	if (!ensureMsgf(SessionID == ExpectedSessionID, TEXT("ConsumeAnimData called for ACE SID %d, expected %d"), SessionID, ExpectedSessionID))
	{
		// This should never happen, but if it does it should be safe to just ignore invalid callbacks
		return;
	}

	if (Chunk.Status != EACEAnimDataStatus::OK_NO_MORE_DATA)
	{
		if (Chunk.Status == EACEAnimDataStatus::ERROR_UNEXPECTED_OUTPUT)
		{
			UE_LOG(LogACERuntime, Verbose, TEXT("[ACE SID %d callback] unexpected output but proceeding anyway"), SessionID);
		}

		if (AnimState == EAnimState::IDLE)
		{
			AnimState = EAnimState::STARTING;
		}

		if (AnimState == EAnimState::ENDING)
		{
			// since it's ending, don't process any more chunks
			UE_LOG(LogACERuntime, Verbose, TEXT("[ACE SID %d callback] currently animation is ending, so can't accept any more chunks."), SessionID);
			return;
		}

		if ((Chunk.AudioBuffer.Num() == 0) && (Chunk.BlendShapeWeights.Num() == 0))
		{
			// no data this frame, probably a header
			UE_LOG(LogACERuntime, Verbose, TEXT("[ACE SID %d callback] no data this frame, probably received header"), SessionID);
			return;
		}

		// Frame has data, so handle frame received

		// some providers don't provide timestamps so we have to fake them here
		double CurrentACETimestamp = Chunk.Timestamp;

		// locally we use an animation timestamp that starts at 0.0
		if (!FirstACETimestamp.IsSet())
		{
			FirstACETimestamp = CurrentACETimestamp;
		}
		double LocalTimestamp = CurrentACETimestamp - FirstACETimestamp.GetValue();

#define UNTRUSTWORTHY_ACE_DATA 1
#define CLAMP_BLEND_SHAPE_WEIGHTS 0
		{
			FScopeLock Lock(&AudioCompCS);

			TArrayView<const uint8> AudioBuffer = Chunk.AudioBuffer;
			int32 NumAudioSamples = AudioBuffer.Num() / AudioSampleByteSize;
			// TODO: check if the AudioComponent is invalid, for example during shutdown
			if (AudioComponent != nullptr)
			{
				if (ensure(0 == (AudioBuffer.Num() % AudioSampleByteSize)))
				{
					// Add new samples to streaming queue
					UBetterSoundWaveProcedural* SoundStreaming = CastChecked<UBetterSoundWaveProcedural>(AudioComponent->Sound);
					SoundStreaming->QueueAudio(AudioBuffer.GetData(), AudioBuffer.Num());
					UE_LOG(LogACERuntime, VeryVerbose, TEXT("[ACE SID %d callback] queued %d samples"), SessionID, NumAudioSamples);
				}
				else
				{
					UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d callback] invalid buffer size %d, skipping audio samples!"), SessionID, AudioBuffer.Num());
					NumAudioSamples = 0;
				}
			}
			else
			{
				// TODO: do we want to handle this case and animate anyway without audio? how would we handle that?
				UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d callback] no audio component available, animations will be missing too on %s"), SessionID, *GetOwner()->GetFullName());
			}

			// increment number of samples
			ReceivedAudioSamples += NumAudioSamples;

			// Adjust local timestamp by any extra silence that got queued up
			LocalTimestamp += static_cast<float>(TotalUnderflowSamples) / (AudioSampleRate * static_cast<float>(NumAudioChannels));
		}

		// cache blend shape weights
		if (Chunk.BlendShapeWeights.Num() > 0)
		{
			FBSWeightSample Sample;
			Sample.Weights = Chunk.BlendShapeWeights;
#if UNTRUSTWORTHY_ACE_DATA
			bool bAllWeightsZero = true;
			size_t CurveIdx = 0;
			for (float& Weight : Sample.Weights)
			{
#if CLAMP_BLEND_SHAPE_WEIGHTS
				// We've learned there are valid reasons for a model to output blend shape weights outside the range [0.0, 1.0] so this code is removed for now
				if (!ensure((Weight >= 0.0f) && (Weight <= 1.0f)))
#else
				if (!ensure(FMath::IsFinite(Weight)))
#endif
				{
					const int32 WeightAsInt = BitCast<const int32>(Weight);
					FName CurveName = (CurveIdx < UE_ARRAY_COUNT(CurveNames)) ? CurveNames[CurveIdx] : NAME_None;
					UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d callback] received garbage weight from A2F-3D for %s: %g (0x%x)"), SessionID, *CurveName.ToString(), Weight, WeightAsInt);
					// if we get garbage from the service, just set it to 0.0f
					Weight = 0.0f;
				}
				if (Weight != 0.0f)
				{
					bAllWeightsZero = false;
				}
				++CurveIdx;
			}
			if (bAllWeightsZero)
			{
				UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d callback] all 0 weights from A2F-3D at ACE timestamp %f (internal timestamp %f)"), SessionID, Chunk.Timestamp, LocalTimestamp);
			}
#endif
			Sample.Timestamp = static_cast<float>(LocalTimestamp);
			Sample.SessionID = SessionID;
			BSWeightSampleQueue.Enqueue(Sample);
			++ReceivedBSWeightSamples;
		}
#if UNTRUSTWORTHY_ACE_DATA
		else
		{
			UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d callback] no weights from A2F-3D at ACE timestamp %f (internal timestamp %f)"), SessionID, Chunk.Timestamp, LocalTimestamp);
		}
#endif
	}
	else
	{
		bAnimationAllFramesRecived = true;
		// TODO: animation/audio complete. Play back any remaining buffered audio, and be sure to start it if it's not currently playing back
		FScopeLock Lock(&AudioCompCS);
		UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d callback] received %d animation samples, %d audio samples for clip on %s"), SessionID, ReceivedBSWeightSamples.load(), ReceivedAudioSamples, *GetOwner()->GetFullName());
	}
}

void UACEAudioCurveSourceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// this component tick is here only so we can run some work that needs to happen on the game thread
	// but it might be better/clearer to use FFunctionGraphTask::CreateAndDispatchWhenReady to issue work directly when needed
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// if it's ending, we stopped audio, but this keeps playing here.
	if (AnimState != EAnimState::ENDING)
	{
		UBetterSoundWaveProcedural* SoundStreaming = nullptr;
		{
			FScopeLock Lock(&AudioCompCS);
			if (AudioComponent == nullptr)
			{
				return;
			}

			SoundStreaming = CastChecked<UBetterSoundWaveProcedural>(AudioComponent->Sound);

			if ((SoundStreaming != nullptr) && !AudioComponent->IsPlaying())
			{
				// Start playing audio if we've queued enough samples
				int32 QueuedSamples = SoundStreaming->GetAvailableAudioByteCount() / SoundStreaming->SampleByteSize;
				float QueuedTime = static_cast<float>(QueuedSamples) / (AudioSampleRate * static_cast<float>(NumAudioChannels));
				if (QueuedTime >= BufferLengthInSeconds)
				{
					TotalUnderflowSamples = 0;
					AudioComponent->Play();
					UE_LOG(LogACERuntime, Log, TEXT("start playing audio on %s"), *GetOwner()->GetFullName());
				}
			}
		}
	}

	if (AnimState == EAnimState::STARTED)
	{
		AnimState = EAnimState::IN_PROGRESS;
		OnAnimationStarted.Broadcast();
	}
	else if (AnimState == EAnimState::ENDING)
	{
		AnimState = EAnimState::IDLE;
		bAnimationAllFramesRecived = false;
		OnAnimationEnded.Broadcast();
	}

	// Even this workaround wasn't enough, we were still rarely seeing bad CurrentPlaybackTime.
	// Unknown why, possibly a race condition between audio and game thread?
	// Leaving the following comment in place so some future dev doesn't have to figure this out all over again
	//
	// It would be nice if we could trust the time we get from the engine in the
	// UAudioComponent's OnAudioPlaybackPercentNative delegate.
	// Unfortunately there's an engine bug in FMixerSource::GetPlaybackPercent(): Before the source is initialized
	// it will return an old value from a previous playback, so we could see strange values in that delegate
	// for a frame or two. That could lead to animation hitches when the animation first begins.
	// So we manually track down the FSoundSource here and check if it's initialized first before even considering
	// using whatever garbage the engine is passing us. We track the FSoundSource from the audio thread because it's
	// unsafe to be traversing those data structures from the game thread.
#if 0
	if (!bHasPlaybackStarted.load())
	{
		float TotalReceivedAudioTime = static_cast<float>(LocalReceivedAudioSamples) / static_cast<float>(AUDIO_SAMPLE_RATE);
		FAudioThread::RunCommandOnAudioThread([this, SoundStreaming, TotalReceivedAudioTime]()
		{
			const FSoundSource* SoundSource = FindSoundSourceAudioThread(SoundStreaming);
			if ((SoundSource != nullptr) && SoundSource->IsInitialized())
			{
				bHasPlaybackStarted.store(true);
				//ensure(TotalReceivedAudioTime > CurrentPlaybackTime);
			}
		});
	}
#endif
}

#if 0
const FSoundSource* UACEAudioCurveSourceComponent::FindSoundSourceAudioThread(const UBetterSoundWaveProcedural* SoundWave)
{
	check(IsInAudioThread());
	const FSoundSource* SoundSource = nullptr;
	if (SoundWave == nullptr)
	{
		return nullptr;
	}

	if (FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice())
	{
		const TMap<FWaveInstance*, FSoundSource*> WaveInstanceSourceMap = AudioDevice->GetWaveInstanceSourceMap();

		// first checked if cached instance is still valid
		if ((CachedWaveInstance != nullptr) && (CachedWaveInstance->WaveData == SoundWave))
		{
			const FSoundSource* const* SSPtr = WaveInstanceSourceMap.Find(CachedWaveInstance);
			if (SSPtr != nullptr)
			{
				SoundSource = *SSPtr;
			}
		}

		// cached instance is not valid, so search for the correct instance
		if (SoundSource == nullptr)
		{
			for (const TPair<FWaveInstance*, FSoundSource*>& Pair : WaveInstanceSourceMap)
			{
				const FWaveInstance* WaveInstance = Pair.Key;
				if ((WaveInstance != nullptr) && (WaveInstance->WaveData == SoundWave))
				{
					CachedWaveInstance = WaveInstance;
					SoundSource = Pair.Value;
					break;
				}
			}
		}
	}

	return SoundSource;
}
#endif

// Track position in played audio (delegate)
void UACEAudioCurveSourceComponent::HandlePlaybackFraction(const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackFraction)
{
	// Warning, there's an engine bug in FMixerSource::GetPlaybackPercent(): Before the source is initialized it will
	// return an old value from a previous playback, so we could see strange values from the engine in this function
	// for a frame or two. We initially tried to detect that case and work around it, but we didn't find a complete
	// solution there. So now we just reject playback values that don't look reasonable. But bad values could still
	// pass through. As a result, playback time used for animation doesn't necessarily move in one direction, it could
	// jump backwards.
	check(IsInGameThread());

	uint64 LocalReceivedAudioSamples = 0;
	{
		FScopeLock Lock(&AudioCompCS);
		if (InComponent != AudioComponent)
		{
			// When transitioning to a new audio component and procedural sound wave, it's possible we can still get called with the old component for a bit
			return;
		}
		LocalReceivedAudioSamples = ReceivedAudioSamples;
	}
	float TotalReceivedAudioTime = static_cast<float>(LocalReceivedAudioSamples + TotalUnderflowSamples) / (AudioSampleRate * static_cast<float>(NumAudioChannels));

	check(InSoundWave != nullptr);

	FScopeLock Lock(&AudioCompCS);
	// note: the engine calls this "percentage" but it's actually a fraction, so we renamed the variable for clarity. No need to multiply by 0.01f
	AudioPlaybackTimeEstimate = InSoundWave->GetDuration() * InPlaybackFraction;
	UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::HandlePlaybackFraction Current Tick %d"), FDateTime::Now().GetTicks());
	UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::HandlePlaybackFraction AudioPlaybackTimeEstimate %f"), AudioPlaybackTimeEstimate);

	if ((AnimState == EAnimState::STARTING) && (AudioPlaybackTimeEstimate <= TotalReceivedAudioTime))
	{
		// It's not officially started until we've seen at least one valid looking playback time value.
		// If we get a playback time past the end of audio before it started, there are 2 possibilities:
		//  1. the engine sent us some garbage that we're better off ignoring
		//  2. we've played past the end of the clip before we even had time to animate it
		// We've seen the first case a lot, and never seen the second case.
		// But in either case, there's no point starting animation here.
		AnimState = EAnimState::STARTED;
	}
}

void UACEAudioCurveSourceComponent::EvaluateAndUpdateCurrentPlaybackTime()
{
	// this function may modify game thread data: CurrentPlaybackTime, LastUpdatedGlobalTime
	check(IsInGameThread());
	FScopeLock Lock(&AudioCompCS);
	const int64 CurrentUpdatedGlobalTime = FDateTime::Now().GetTicks();
	const int64 ElapsedTicks = (LastUpdatedGlobalTime > 0) ? CurrentUpdatedGlobalTime - LastUpdatedGlobalTime : 0;
	const float ElapsedTimeSinceLastUpdate = (float)ElapsedTicks / (float)ETimespan::TicksPerSecond;
	// Update playback time with the past time based on ticks
	CurrentPlaybackTime += ElapsedTimeSinceLastUpdate;
	LastUpdatedGlobalTime = CurrentUpdatedGlobalTime;
	// Check if the playback time is within the valid range of the audio time.
	const float threshold = 1.0f/30.0f; // We set the threshold to the time of one frame for 30fps.	
	const float AnimationPlaybackTimeEstimate = AudioPlaybackTimeEstimate;
	if ((CurrentPlaybackTime > (AnimationPlaybackTimeEstimate - threshold)) && (CurrentPlaybackTime < (AnimationPlaybackTimeEstimate + threshold)))
	{
		// Since we don't know the exact video playback time we use the AnimationPlaybackTimeEstimate to slowly correct the CurrentPlaybackTime towards the expected value of AnimationPlaybackTimeEstimate without suffering from its variance.
		CurrentPlaybackTime += (AnimationPlaybackTimeEstimate - CurrentPlaybackTime) * 0.001f;
	} 
	else
	{
		// We are no longer in sync with the audio playback time, so we need to resync the current playback time with the best guess from AnimationPlaybackTimeEstimate.
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("Resyncing animation with audio playback time: %f -> %f"), CurrentPlaybackTime, AnimationPlaybackTimeEstimate);
		CurrentPlaybackTime = AnimationPlaybackTimeEstimate;
	}

	UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::EvaluateAndUpdateCurrentPlaybackTime LastUpdatedGlobalTime %d"), LastUpdatedGlobalTime);
	UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::EvaluateAndUpdateCurrentPlaybackTime AudioPlaybackTimeEstimate %f"), AudioPlaybackTimeEstimate);
	UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::EvaluateAndUpdateCurrentPlaybackTime AnimationPlaybackTimeEstimate %f"), AnimationPlaybackTimeEstimate);
	UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::EvaluateAndUpdateCurrentPlaybackTime ElapsedTimeSinceLastUpdate %f"), ElapsedTimeSinceLastUpdate);
	UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::EvaluateAndUpdateCurrentPlaybackTime CurrentPlaybackTime %f"), CurrentPlaybackTime);
}

// Handle audio playback running out of received audio samples (delegate)
void UACEAudioCurveSourceComponent::HandleSoundUnderflow(USoundWaveProcedural* InProceduralWave, int32 SamplesRequired)
{
	// this gets called from an audio worker thread, mind the thread safety!

	if ((InProceduralWave->GetAvailableAudioByteCount() == 0) && IsPlaybackActive())
	{
		FScopeLock Lock(&AudioCompCS);
		UBetterSoundWaveProcedural* SoundStreaming = CastChecked<UBetterSoundWaveProcedural>(InProceduralWave);
		TArray<uint8> AudioBuffer;
		AudioBuffer.AddZeroed(SamplesRequired * SoundStreaming->SampleByteSize);
		SoundStreaming->QueueAudio(AudioBuffer.GetData(), AudioBuffer.Num());
		TotalUnderflowSamples += SamplesRequired;
	}
}

const int32 UACEAudioCurveSourceComponent::GetCurrentSampleIdx()
{
	// this function modifies game thread data directly: RecentPlaybackIdx, RecentPlaybackTimes, BSWeightSamples
	// and may modify other game thread data via EvaluateAndUpdateCurrentPlaybackTime: CurrentPlaybackTime, LastUpdatedGlobalTime
	check(IsInGameThread());	// ensure safe access of game thread data
	EvaluateAndUpdateCurrentPlaybackTime();

	bool bCachedAnimationAllFramesRecived = bAnimationAllFramesRecived;

	// first empty the incoming queue into game thread storage
	FBSWeightSample Sample;
	while (BSWeightSampleQueue.Dequeue(Sample))
	{
		BSWeightSamples.Add(Sample);
	}

	if (!IsPlaybackActive())
	{
		// playback isn't active, so no curves avaiable
		if (LastSampleIdx >= 0)
		{
			// This might happen because we've reached the end of the animation clip
			UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d]: resetting animation on %s"), BSWeightSamples[LastSampleIdx].SessionID, *GetOwner()->GetFullName());
		}
		else
		{
			// note that CurrentPlaybackTime might be bogus in the STARTING state due to engine bug in FMixerSource::GetPlaybackPercent
			UE_LOG(LogACERuntime, VeryVerbose, TEXT("no animation yet at %f on %s"), CurrentPlaybackTime, *GetOwner()->GetFullName());
		}

		return -1;
	}

	if (BSWeightSamples.IsEmpty())
	{
		// we haven't received samples yet so LastSampleIdx is not valid yet
		ensure(LastSampleIdx == -1);
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("no samples yet on %s"), *GetOwner()->GetFullName());
		return -1;
	}

	// find starting point to search for next sample index
	int32 CurrentSampleIdx = LastSampleIdx;
	if (CurrentPlaybackTime < LastAnimPlaybackTime)
	{
		// time moved backwards so just start over at the first sample and work it out again from the beginning
		UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] animation time moved backwards %f → %f"), BSWeightSamples[0].SessionID, LastAnimPlaybackTime, CurrentPlaybackTime);
		CurrentSampleIdx = 0;
	}

	if (CurrentSampleIdx < 0)
	{
		// start at the first sample
		CurrentSampleIdx = 0;
		UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] begin animation on %s at %f"), BSWeightSamples[0].SessionID, *GetOwner()->GetFullName(), CurrentPlaybackTime);
	}

	// Keep track of recent playback times, and discard any old samples.
	// We store a few old playback times because the engine occasionally gives us bogus playback times for a frame or two, and we don't want to throw away good data
	if (CurrentPlaybackTime != LastAnimPlaybackTime)
	{
		RecentPlaybackIdx = (RecentPlaybackIdx + 1) % NUM_RECENT_PLAYBACK_TIMES;
		RecentPlaybackTimes[RecentPlaybackIdx] = CurrentPlaybackTime;

		const float MinPlaybackTime = *Algo::MinElement(RecentPlaybackTimes);
		int32 NumSamplesToDiscard = 0;
		const int32 MaxSamplesToDiscard = FMath::Min(BSWeightSamples.Num() - 1, CurrentSampleIdx);
		for (const FBSWeightSample& BSWeightSample : BSWeightSamples)
		{
			if ((BSWeightSample.Timestamp >= MinPlaybackTime) || (NumSamplesToDiscard >= MaxSamplesToDiscard))
			{
				break;
			}
			++NumSamplesToDiscard;
		}
		if (NumSamplesToDiscard > 0)
		{
			// removing elements from the front of BSWeightSamples invalidates CurrentSampleIdx and LastSampleIdx
			// we update CurrentSampleIdx immediately, and LastSampleIdx will be set by the caller of this function
			BSWeightSamples.PopFront(NumSamplesToDiscard);
			CurrentSampleIdx -= NumSamplesToDiscard;
		}
	}

	// find the next sample index
	float CurrentTimestamp = BSWeightSamples[CurrentSampleIdx].Timestamp;
	if (CurrentPlaybackTime <= CurrentTimestamp)
	{
		// Playback still hasn't passed the current sample so return it
		return CurrentSampleIdx;
	}

	if ((CurrentSampleIdx + 1) >= BSWeightSamples.Num())
	{
		// No more samples available so return the current sample
		if ((AnimState == EAnimState::IN_PROGRESS) && bCachedAnimationAllFramesRecived)
		{
			AnimState = EAnimState::ENDING;
		}
		return CurrentSampleIdx;
	}

	// skip ahead if we missed a sample (could happen with low frame rates for example)
	float NextTimestamp = BSWeightSamples[CurrentSampleIdx + 1].Timestamp;
	while (CurrentPlaybackTime > NextTimestamp)
	{
		++CurrentSampleIdx;
		if ((CurrentSampleIdx + 1) >= BSWeightSamples.Num())
		{
			// No more samples available so return the current sample
			if ((AnimState == EAnimState::IN_PROGRESS) && bCachedAnimationAllFramesRecived)
			{
				AnimState = EAnimState::ENDING;
			}
			return CurrentSampleIdx;
		}
		NextTimestamp = BSWeightSamples[CurrentSampleIdx + 1].Timestamp;
	}
	CurrentTimestamp = BSWeightSamples[CurrentSampleIdx].Timestamp;

	// CurrentPlaybackTime is now somewhere in the range (CurrentTimestamp, NextTimestamp]
	// TODO: eventually we'd like to support interpolation but for now just return the closest sample
	float LastDistance = CurrentPlaybackTime - CurrentTimestamp;
	float NextDistance = NextTimestamp - CurrentPlaybackTime;
	if (LastDistance >= NextDistance)
	{
		// next sample is the closest, so it's our new current sample
		++CurrentSampleIdx;
	}

	return CurrentSampleIdx;
}

void UACEAudioCurveSourceComponent::GetCurveOutputs(TArray<float>& OutWeights)
{
	// this function may modify game thread data: LastSampleIdx, LastAnimPlaybackTime
	// and may modify other game thread data via GetCurrentSampleIdx: RecentPlaybackIdx, RecentPlaybackTimes, BSWeightSamples, CurrentPlaybackTime, LastUpdatedGlobalTime
	check(IsInGameThread());	// ensure safe access of game thread data
	LastSampleIdx = GetCurrentSampleIdx();
	if ((LastSampleIdx >= 0) && ensure(LastSampleIdx < BSWeightSamples.Num()))
	{
		const FBSWeightSample& Sample = BSWeightSamples[LastSampleIdx];
		ensure(Sample.SessionID == CurrentSessionID);
		LastAnimPlaybackTime = CurrentPlaybackTime;
		OutWeights = Sample.Weights;
	}
}

void UACEAudioCurveSourceComponent::GetCurveOutputsInterp(TArray<float>& OutWeights)
{
	// this function may modify game thread data: LastSampleIdx, LastAnimPlaybackTime, RecentPlaybackIdx, RecentPlaybackTimes, BSWeightSamples
	// and may modify other game thread data via EvaluateAndUpdateCurrentPlaybackTime: CurrentPlaybackTime, LastUpdatedGlobalTime
	// and may modify other game thread data via ResetAnimData: BSWeightSamples, LastSampleIdx
	check(IsInGameThread());	// ensure safe access of game thread data
	EvaluateAndUpdateCurrentPlaybackTime();

	// first empty the incoming queue into game thread storage
	if (IsAnimationActive())
	{
		FBSWeightSample Sample;
		while (BSWeightSampleQueue.Dequeue(Sample))
		{
			BSWeightSamples.Add(Sample);
		}
	}

	// let it process till the end of the buffer
	// Animation latency could delay the play while the anim state ended for interpolate option
	if (BSWeightSamples.IsEmpty())
	{
		// we haven't received samples yet so LastSampleIdx is not valid yet
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("no samples yet on %s"), *GetOwner()->GetFullName());

		if ((AnimState == EAnimState::IN_PROGRESS) && bAnimationAllFramesRecived)
		{
			AnimState = EAnimState::ENDING;
		}

		return;
	}
	else
	{
		const auto TotalSamples = BSWeightSamples.Num();
		UE_LOG(LogACERuntime, VeryVerbose, TEXT("CurrentPlaybackTime %0.5f, Current Num Samples %d"), CurrentPlaybackTime, TotalSamples);
		bool bIsPlayed = false;
		for (int32_t i = 0; i < TotalSamples; ++i)
		{
			const FBSWeightSample& BSWeightSample = BSWeightSamples[i];

			if (CurrentPlaybackTime <= BSWeightSample.Timestamp)
			{
				ensure(BSWeightSample.SessionID == CurrentSessionID);
				// found the sample, interpolate before and this
				// nothing to interpolate from
				if (i == 0)
				{
					OutWeights = BSWeightSample.Weights;
					UE_LOG(LogACERuntime, VeryVerbose, TEXT("Playing 0 index of time stamp of %f"), BSWeightSample.Timestamp);
				}
				else
				{
					const FBSWeightSample& PrevSample = BSWeightSamples[i - 1];
					UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::GetCurveOutputsInterp[%d] AnimSample.Timestamp %f"), RecentPlaybackIdx, BSWeightSample.Timestamp);
					UE_LOG(LogACERuntime, VeryVerbose, TEXT("UACEAudioCurveSourceComponent::GetCurveOutputsInterp[%d] PrevSample.Timestamp %f"), RecentPlaybackIdx, PrevSample.Timestamp);
					const float TotalTime = BSWeightSample.Timestamp - PrevSample.Timestamp;
					const float CurTime = CurrentPlaybackTime - PrevSample.Timestamp;
					ensure(TotalTime >= CurTime);

					const float Alpha = TotalTime > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(CurTime / TotalTime, 0.0f, 1.0f) : 0.0f;
					OutWeights.AddZeroed(BSWeightSample.Weights.Num());
					// we blend by Alpha
					for (int32_t j = 0; j < OutWeights.Num(); ++j)
					{
						OutWeights[j] = PrevSample.Weights[j] + (BSWeightSample.Weights[j] - PrevSample.Weights[j]) * Alpha;
					}

					UE_LOG(LogACERuntime, VeryVerbose, TEXT("Playing index [%d, %d] of time stamp of [%f, %f] with alpha of (%0.2f)"), i - 1, i, PrevSample.Timestamp, BSWeightSample.Timestamp, Alpha);
				}

				LastAnimPlaybackTime = CurrentPlaybackTime;

				// check if the queue needs readjust
				RecentPlaybackIdx = (RecentPlaybackIdx + 1) % NUM_RECENT_PLAYBACK_TIMES;
				RecentPlaybackTimes[RecentPlaybackIdx] = CurrentPlaybackTime;

				bIsPlayed = true;
				break;
			}
		}

		// animation buffer ran out - nothing to play new, return default pose from last frame
		if (!bIsPlayed)
		{
			// clear the buffer, it played
			ResetAnimSamples();
			if ((AnimState == EAnimState::IN_PROGRESS) && bAnimationAllFramesRecived)
			{
				AnimState = EAnimState::ENDING;
			}

			return;
		}

		// if not, go through buffer to clean it upt
		const float MinPlaybackTime = *Algo::MinElement(RecentPlaybackTimes);
		int32 NumSamplesToDiscard = 0;
		for (const FBSWeightSample& BSWeightSample : BSWeightSamples)
		{
			if (BSWeightSample.Timestamp >= MinPlaybackTime)
			{
				break;
			}

			++NumSamplesToDiscard;
		}

		if (NumSamplesToDiscard > 0)
		{
			BSWeightSamples.PopFront(NumSamplesToDiscard);
			// Removing elements from the front of BSWeightSamples invalidates LastSampleIdx.
			// LastSampleIdx is only used by the non-interpolated path which is probably not active, but if it is active
			// LastSampleIdx will be non-negative and that indicates we need to adjust it here.
			if (LastSampleIdx >= 0)
			{
				LastSampleIdx -= NumSamplesToDiscard;
				if (LastSampleIdx < 0)
				{
					LastSampleIdx += BSWeightSamples.Num();
				}
			}

			UE_LOG(LogACERuntime, VeryVerbose, TEXT("CurrentPlaybackTime %0.5f, Discarding %d Num Samples %d"), CurrentPlaybackTime, NumSamplesToDiscard, BSWeightSamples.Num());
		}
	}
}

void UACEAudioCurveSourceComponent::Stop()
{
	check(IsInGameThread());	// ensure safe access of AnimState, BSWeightSamples, LastSampleIdx
	// stop audio playing
	FScopeLock Lock(&AudioCompCS);
	if (AudioComponent)
	{
		AudioComponent->SetVolumeMultiplier(0.f);
		AudioComponent->Stop();
	}

	// stop listening for any new animations or audio
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry != nullptr)
	{
		Registry->DetachConsumer_AnyThread(this);
	}

	AnimState = EAnimState::ENDING;

	// clear the animation buffer
	ResetAnimSamples();
}

void UACEAudioCurveSourceComponent::ResetAnimSamples()
{
	check(IsInGameThread());	// ensure safe access of BSWeightSamples, LastSampleIdx
	BSWeightSamples.Empty();
	LastSampleIdx = -1;
}

