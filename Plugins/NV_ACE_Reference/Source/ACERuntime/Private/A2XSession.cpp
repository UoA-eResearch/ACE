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

#include "A2XSession.h"

// engine includes
#include "AudioResampler.h"
#include "DSP/FloatArrayMath.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Templates/TypeCompatibleBytes.h"

// plugin includes
#include "ACEBlueprintLibrary.h"
#include "ACERuntimeModule.h"
#include "ACERuntimePrivate.h"
#include "ACESettings.h"
#include "AnimDataConsumerRegistry.h"


static bool GetBurstMode()
{
	const TOptional<bool>& bOverrideBurstMode = FACERuntimeModule::Get().bOverrideBurstMode;
	if (bOverrideBurstMode.IsSet())
	{
		return bOverrideBurstMode.GetValue();
	}
	return GetDefault<UACESettings>()->BurstMode == EAudio2Face3DBurstMode::ForceBurstMode;
}

static float GetInitialChunkSize()
{
	const TOptional<float>& bOverrideInitialChunkSize = FACERuntimeModule::Get().OverrideMaxInitialAudioChunkSize;
	if (bOverrideInitialChunkSize.IsSet())
	{
		return bOverrideInitialChunkSize.GetValue();
	}

	return GetDefault<UACESettings>()->MaxInitialAudioChunkSize;
}

// ctor
FAudio2XSession::FAudio2XSession(IA2FProvider* InProvider, int32 InNumChannels, uint32 InSampleRate, int32 InSampleByteSize):
	NumChannels(InNumChannels),
	SampleRate(InSampleRate),
	SampleByteSize(InSampleByteSize),
	bBurstAudio(GetBurstMode()),
	MaxInitialAudioChunkSizeSeconds(GetInitialChunkSize()),
	Provider(InProvider),
	Session(nullptr),
	SessionID(IA2FProvider::IA2FStream::INVALID_STREAM_ID),
	bSamplesStarted(false),
	bSamplesEnded(false)
{
}

// dtor
FAudio2XSession::~FAudio2XSession()
{
	// FAudio2XSession represents a session to send audio to an A2F-3D provider
	// IA2FProvider::IA2FStream represents a bidirection stream: audio out, animations and audio in
	//
	// So we intentionally don't release the stream here, since the other half of the stream may still be in progress
}

// Start a session to send audio to an A2F-3D service and receive ACE animation data back
bool FAudio2XSession::StartSession(IACEAnimDataConsumer* CallbackObject)
{
	FScopeLock Lock(&CS);
	if (Session == nullptr)
	{
		if (Provider != nullptr)
		{
			Session = Provider->CreateA2FStream(CallbackObject);
			IA2FRemoteProvider* RemoteProvider = Provider->GetRemoteProvider();
			if (RemoteProvider != nullptr)
			{
				if (Session == nullptr)
				{
					UE_LOG(LogACERuntime, Warning, TEXT("Failed to create %s session at %s"), *Provider->GetName().ToString(), *RemoteProvider->GetA2FURL());
				}
				else
				{
					SessionID = Session->GetID();
					UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] Started %s session at %s"), SessionID, *Provider->GetName().ToString(), *RemoteProvider->GetA2FURL());
				}
			}
			else
			{
				if (Session == nullptr)
				{
					UE_LOG(LogACERuntime, Warning, TEXT("Failed to create %s session"), *Provider->GetName().ToString());
				}
				else
				{
					SessionID = Session->GetID();
					UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] Started %s session"), SessionID, *Provider->GetName().ToString());
				}
			}

			IA2FPassthroughProvider* PassthroughProvider = Provider->GetAudioPassthroughProvider();
			if ((Session != nullptr) && (PassthroughProvider != nullptr))
			{
				PassthroughProvider->SetOriginalAudioParams(Session, SampleRate, NumChannels, SampleByteSize);
			}
		}
		else
		{
			UE_LOG(LogACERuntime, Warning, TEXT("No A2F-3D provider available to create A2F-3D session"));
		}
	}
	else
	{
		UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] StartSession called when A2F-3D session still active"), Session->GetID());
	}

	return (Session != nullptr);
}

bool FAudio2XSession::SendAudioSamples(TArrayView<const int16> InSamples, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, UAudio2FaceParameters* Audio2FaceParameters)
{
	// Use bIsSendingSamples to detect when the application does conflicting things to the session from multiple threads.
	// This is just an atomic so won't catch 100% of cases but it's a simple way to catch and log that the application
	// has done something wrong most of the time. Importantly, it should give no false positives. If you see this message
	// in your application log, your application really did something wrong.
	// We prevent mischief in this case by blocking the caller until their other thread has finished its work, which
	// protects the application but also hides a logic error in the application. Hence the error message.
	if (bIsSendingSamples.load())
	{
		UE_LOG(LogACERuntime, Error,
			TEXT("Application tried to send audio into the same Audio2Face-3D session from multiple threads simultaneously! Blocking one thread to avoid cacaphony"));
	}
	FScopeLock Lock(&CS);

	if (Session == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("SendAudioSamples called when no A2F-3D session active, ignoring %d samples"), InSamples.Num());
		return false;
	}

	if (bSamplesEnded)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] SendAudioSamples called after end of samples, ignoring %d samples"), SessionID, InSamples.Num());
		return false;
	}

	if (SessionID != Session->GetID())
	{
		UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] internal plugin bug, SessionID doesn't match %d"), SessionID, Session->GetID());
		return false;
	}

	bIsSendingSamples = true;
	// pass through original audio if provider supports it
	IA2FPassthroughProvider* PassthroughProvider = Provider->GetAudioPassthroughProvider();
	if (PassthroughProvider != nullptr)
	{
		TArrayView<const uint8> SamplesUint8(reinterpret_cast<const uint8*>(InSamples.GetData()), InSamples.Num() * sizeof(int16));
		PassthroughProvider->EnqueueOriginalSamples(Session, SamplesUint8);
	}

	TArrayView<const int16> SamplesInt16 = InSamples;

	// convert to mono if necessary
	TArray<int16> MonoBuffer;
	if (NumChannels > 1)
	{
		// convert to mono by throwing away extra channels
		// TODO: is there a better way by combining channels somehow?
		int32 NumMonoSamples = SamplesInt16.Num() / NumChannels;
		MonoBuffer.Reserve(NumMonoSamples);
		for (size_t SampleIdx = 0; SampleIdx < NumMonoSamples; ++SampleIdx)
		{
			MonoBuffer.Add(SamplesInt16[SampleIdx * NumChannels]);
		}
		SamplesInt16 = MonoBuffer;
	}

	// resample to 16 kHz if necessary
	TArray<int16> ResampledBuffer;
	if (SampleRate != 16000)
	{
		const float SampleRateRatio = 16000.0f / static_cast<float>(SampleRate);

		// convert int16 → float
		Audio::FAlignedFloatBuffer SamplesFloat;
		SamplesFloat.SetNumUninitialized(SamplesInt16.Num());
		Audio::ArrayPcm16ToFloat(SamplesInt16, SamplesFloat);

		// initialize resampler if needed
		if (!Resampler.IsValid())
		{
			// Technically a race condition here on Resampler creation.
			// But if anyone is sending in audio simultaneously from different threads the audio would be all garbled anyway,
			// so there's not much point in going through the trouble of making a silly thing safer to do.
			Resampler = MakeUnique<Audio::FResampler>();
			Resampler->Init(Audio::EResamplingMethod::BestSinc, SampleRateRatio, 1);
		}

		// throw in 10 extra samples just in case of rounding or jitter or other resampling magic
		const int32 ResampledBufferSize = SamplesFloat.Num() * SampleRateRatio + 10;
		Audio::FAlignedFloatBuffer ResampledFloat;
		ResampledFloat.AddUninitialized(ResampledBufferSize);

		// resample to 16 kHz
		check(Resampler.IsValid());
		int32 OutputFramesGenerated;
		int32 Result = Resampler->ProcessAudio(
			const_cast<float*>(SamplesFloat.GetData()),	// I hope to gosh that FResampler doesn't actually try to modify the input, that would be weird
			SamplesFloat.Num(),
			bEndOfSamples,
			ResampledFloat.GetData(),
			ResampledFloat.Num(),
			OutputFramesGenerated);
		check(Result == 0);
		ResampledFloat.SetNum(OutputFramesGenerated);

		// convert float → int16
		ResampledBuffer.SetNumUninitialized(OutputFramesGenerated);
		Audio::ArrayFloatToPcm16(ResampledFloat, ResampledBuffer);

		SamplesInt16 = ResampledBuffer;
	}

	return SendAudioSamplesInternal(SamplesInt16, bEndOfSamples, EmotionParameters, Audio2FaceParameters);
}

bool FAudio2XSession::SendAudioSamples(TArrayView<const float> InSamples, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, UAudio2FaceParameters* Audio2FaceParameters)
{
	// Use bIsSendingSamples to detect when the application does conflicting things to the session from multiple threads.
	// This is just an atomic so won't catch 100% of cases but it's a simple way to catch and log that the application
	// has done something wrong most of the time. Importantly, it should give no false positives. If you see this message
	// in your application log, your application really did something wrong.
	// We prevent mischief in this case by blocking the caller until their other thread has finished its work, which
	// protects the application but also hides a logic error in the application. Hence the error message.
	if (bIsSendingSamples.load())
	{
		UE_LOG(LogACERuntime, Error,
			TEXT("Application tried to send audio into the same Audio2Face-3D session from multiple threads simultaneously! Blocking one thread to avoid cacaphony"));
	}
	FScopeLock Lock(&CS);

	if (Session == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("SendAudioSamples called when no A2F-3D session active, ignoring %d samples"), InSamples.Num());
		return false;
	}

	if (bSamplesEnded)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] SendAudioSamples called after end of samples, ignoring %d samples"), SessionID, InSamples.Num());
		return false;
	}

	if (SessionID != Session->GetID())
	{
		UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] internal plugin bug, SessionID doesn't match %d"), SessionID, Session->GetID());
		return false;
	}

	bIsSendingSamples = true;
	// pass through original audio if provider supports it
	IA2FPassthroughProvider* PassthroughProvider = Provider->GetAudioPassthroughProvider();
	if (PassthroughProvider != nullptr)
	{
		TArrayView<const uint8> SamplesUint8(reinterpret_cast<const uint8*>(InSamples.GetData()), InSamples.Num() * sizeof(float));
		PassthroughProvider->EnqueueOriginalSamples(Session, SamplesUint8);
	}

	TArrayView<const float> SamplesFloat = InSamples;

	// convert to mono if necessary
	Audio::FAlignedFloatBuffer MonoBuffer;
	if (NumChannels > 1)
	{
		// convert to mono by throwing away extra channels
		// TODO: is there a better way by combining channels somehow?
		int32 NumMonoSamples = SamplesFloat.Num() / NumChannels;
		MonoBuffer.Reserve(NumMonoSamples);
		for (size_t SampleIdx = 0; SampleIdx < NumMonoSamples; ++SampleIdx)
		{
			MonoBuffer.Add(SamplesFloat[SampleIdx * NumChannels]);
		}
		SamplesFloat = MonoBuffer;
	}

	// resample to 16 kHz if necessary
	Audio::FAlignedFloatBuffer ResampledBuffer;
	if (SampleRate != 16000)
	{
		const float SampleRateRatio = 16000.0f / static_cast<float>(SampleRate);

		// initialize resampler if needed
		if (!Resampler.IsValid())
		{
			// Technically a race condition here on Resampler creation.
			// But if anyone is sending in audio simultaneously from different threads the audio would be all garbled anyway,
			// so there's not much point in going through the trouble of making a silly thing safer to do.
			Resampler = MakeUnique<Audio::FResampler>();
			Resampler->Init(Audio::EResamplingMethod::BestSinc, SampleRateRatio, 1);
		}

		// throw in 10 extra samples just in case of rounding or jitter or other resampling magic
		const int32 ResampledBufferSize = SamplesFloat.Num() * SampleRateRatio + 10;
		ResampledBuffer.AddUninitialized(ResampledBufferSize);

		// resample to 16 kHz
		check(Resampler.IsValid());
		int32 OutputFramesGenerated;
		int32 Result = Resampler->ProcessAudio(
			const_cast<float*>(SamplesFloat.GetData()),	// I hope to gosh that FResampler doesn't actually try to modify the input, that would be weird
			SamplesFloat.Num(),
			bEndOfSamples,
			ResampledBuffer.GetData(),
			ResampledBuffer.Num(),
			OutputFramesGenerated);
		check(Result == 0);
		ResampledBuffer.SetNum(OutputFramesGenerated);

		SamplesFloat = ResampledBuffer;
	}

	// Special case: no conversion occurred but we still need to copy to an aligned buffer before conversion to int16.
	// Other paths already created an aligned buffer as a side-effect of conversion
	Audio::FAlignedFloatBuffer AlignedBuffer;
	if ((NumChannels == 1) && (SampleRate == 16'000))
	{
		AlignedBuffer = SamplesFloat;	// copy from the old arrayview
		SamplesFloat = AlignedBuffer;	// create a new aligned arrayview
	}

	// convert float → int16
	TArray<int16> SamplesInt16;
	SamplesInt16.SetNumUninitialized(SamplesFloat.Num());
	Audio::ArrayFloatToPcm16(SamplesFloat, SamplesInt16);

	return SendAudioSamplesInternal(SamplesInt16, bEndOfSamples, EmotionParameters, Audio2FaceParameters);
}

// goal is to send 1 35ms chunk worth of samples 30 times per second
class FSendRateLimiter
{
public:
	FSendRateLimiter(int32 InChunkSize): ChunkSize(InChunkSize)
	{
		// We should round current time to the nearest send time interval to decide if it's a good time to send. For
		// example, if the first sleep only lasts 30 ms instead of 33 ms we'd still sleep again. We offset the send
		// time by a half-interval to achieve that.
		//
		// The result is the first Tick should send immediately, then the second one will be at least 16.667 ms after
		// that, and average Tick time will be about 33.333 ms.
		NextSendTime = FPlatformTime::Seconds() - (SEND_INTERVAL / 2.0);
	}

	// call before every send to limit send rate to 30 Hz
	void TickIfEnoughSamples(int32 NumSamples)
	{
		if (ensure(NumSamples > 0))
		{
			// don't tick if less than 1 chunk worth of samples will be sent
			AccumulatedSamples += NumSamples;
			if (AccumulatedSamples < ChunkSize)
			{
				return;
			}

			// proceed with ticking
			AccumulatedSamples -= ChunkSize;
			Tick();
		}
	}

	void Tick()
	{
		// Wait only if not enough time has elapsed: NextSendTime - half-interval.
		// So if CurrentTime <= NextSendTime - half-interval, then wait until the next interval,
		// and WaitTime will be NextSendTime - CurrentTime.
		// Therefore if WaitTime >= half-interval, we sleep.

		float WaitTime = static_cast<float>(NextSendTime - FPlatformTime::Seconds());
		if (WaitTime >= static_cast<float>(SEND_INTERVAL / 2.0))
		{
			FPlatformProcess::Sleep(WaitTime);
		}
		NextSendTime += SEND_INTERVAL;
	}

private:
	double NextSendTime;
	const double SEND_INTERVAL = 1.0 / 30.0;
	int32 AccumulatedSamples = 0;
	const int32 ChunkSize;
};

bool FAudio2XSession::SendAudioSamplesInternal(TArrayView<const int16> SamplesInt16, bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, UAudio2FaceParameters* Audio2FaceParameters)
{
	ensure(bIsSendingSamples.load());
	if (Provider == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] Unable to get A2F-3D provider, ignoring %d samples"), SessionID, SamplesInt16.Num());
		bIsSendingSamples = false;
		return false;
	}
	if (Session == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("No active A2F-3D session, ignoring %d samples"), SamplesInt16.Num());
		bIsSendingSamples = false;
		return false;
	}

	// If we send less than a minimum number of samples the first time we send audio samples, the connection may not be
	// properly established.
	// To detect if we're about to do this, we keep track of whether we've already sent samples.
	// To avoid issues, we can save the incomplete samples for later sending.
	bool bSkipSendingSamples = false;
	const int32 MinimumInitialSamples = Provider->GetMinimumInitialAudioSampleCount();
	if (!bSamplesStarted && (SamplesInt16.Num() < MinimumInitialSamples))
	{
		QueuedSamples.Append(SamplesInt16.GetData(), SamplesInt16.Num());
		if (QueuedSamples.Num() >= MinimumInitialSamples)
		{
			// Yay we have enough samples now to make the initial send!
			SamplesInt16 = QueuedSamples;
		}
		else
		{
			bSkipSendingSamples = true;
			UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] Cached %d samples to send later"), SessionID, SamplesInt16.Num());
		}
	}

	bool bSuccess = bSkipSendingSamples;
	if (!bSkipSendingSamples)
	{
		// The non-burst logic below is for the cases where Audio2Face-3D inference is executing on the same machine as
		// rendering. The Audio2Face-3D documentation recommends limiting the rate of sending audio, so that inference isn't
		// running any faster than it needs to. This leave more resources available for rendering.
		//
		// Also in the case of remote inference, recent Audio2Face-3D service versions (1.2 or later?) will drop the RPC early
		// if the RPC isn't kept fed with data for some period of time. So using non-burst mode prevents dropped connection
		// issues when an application is sending audio into this plugin in relatively largish chunks, relatively infrequently.
		//
		// After an initial chunk of 500 ms (default), we try to send samples in 35ms chunks 30 times a second as recommended
		// here:
		// https://docs.nvidia.com/ace/latest/modules/a2f-docs/text/sharing_a2f_compute_resources.html

		int32 NumSamples = SamplesInt16.Num();
		const int16* SampleSlicePtr = SamplesInt16.GetData();
		if (!bSamplesStarted)
		{
			// send initial chunk
			int32 SliceSize = NumSamples;
			if (!bBurstAudio)
			{
				const int32 MaxInitialAudioChunkSize = FMath::Max(static_cast<int32>(16000.0f * MaxInitialAudioChunkSizeSeconds), MinimumInitialSamples);
				SliceSize = FMath::Min(MaxInitialAudioChunkSize, NumSamples);
			}
			TArrayView<const int16> SampleSlice(SampleSlicePtr, SliceSize);
			bSuccess = Provider->SendAudioSamples(Session, SampleSlice, EmotionParameters, Audio2FaceParameters);
			if (bSuccess)
			{
				bSamplesStarted = true;
				UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] Sent %d samples to %s"), SessionID, SampleSlice.Num(), *Provider->GetName().ToString());
			}
			else
			{
				UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] Failed sending %d samples to %s"), SessionID, SampleSlice.Num(), *Provider->GetName().ToString());
			}
			NumSamples -= SliceSize;
			SampleSlicePtr += SliceSize;
		}

		if (bSamplesStarted)
		{
			// send samples in 35ms chunks
			const int32 ChunkSize35ms = static_cast<int32>(16000.0f * 0.035f);

			while (NumSamples > 0)
			{
				int32 SliceSize = NumSamples;
				// limit input rate into A2F-3D inference if requested
				if (!bBurstAudio)
				{
					SliceSize = FMath::Min(ChunkSize35ms, NumSamples);
					if (!SendRateLimiter.IsValid())
					{
						SendRateLimiter = MakeUnique<FSendRateLimiter>(ChunkSize35ms);
					}
					if (ensure(SendRateLimiter.IsValid()))
					{
						SendRateLimiter->TickIfEnoughSamples(SliceSize);
					}
				}

				// bail early if the session has ended
				bool bSessionStillActive = FAnimDataConsumerRegistry::Get()->DoesStreamHaveConsumers_AnyThread(Session->GetID());
				if (!bSessionStillActive)
				{
					break;
				}

				// send audio
				TArrayView<const int16> SampleSlice(SampleSlicePtr, SliceSize);
				bSuccess = Provider->SendAudioSamples(Session, SampleSlice, EmotionParameters, Audio2FaceParameters);
				if (bSuccess)
				{
					UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] Sent %d samples to %s"), SessionID, SampleSlice.Num(), *Provider->GetName().ToString());
				}
				else
				{
					UE_LOG(LogACERuntime, Warning, TEXT("[ACE SID %d] Failed sending %d samples to %s"), SessionID, SliceSize, *Provider->GetName().ToString());
					break;
				}
				NumSamples -= SliceSize;
				SampleSlicePtr += SliceSize;
			}
			if (QueuedSamples.Num())
			{
				UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] clearing %d cached samples"), SessionID, QueuedSamples.Num());
				QueuedSamples.Empty();
			}
		}
	}

	if (bEndOfSamples)
	{
		bSuccess = EndAudioSamplesInternal() && bSuccess;
	}
	bIsSendingSamples = false;
	return bSuccess;
}

bool FAudio2XSession::EndAudioSamples()
{
	// Use bIsSendingSamples to detect when the application does conflicting things to the session from multiple threads.
	// This is just an atomic so won't catch 100% of cases but it's a simple way to catch and log that the application
	// has done something wrong most of the time. Importantly, it should give no false positives. If you see this message
	// in your application log, your application really did something wrong.
	// We prevent mischief in this case by blocking the caller until their other thread has finished its work, which
	// protects the application but also hides a logic error in the application. Hence the error message.
	if (bIsSendingSamples.load())
	{
		UE_LOG(LogACERuntime, Error,
			TEXT("Application tried to remove the Audio2Face-3D session from one thread while sending audio into the session from another thread!"));
	}
	FScopeLock Lock(&CS);

	return EndAudioSamplesInternal();
}

bool FAudio2XSession::EndAudioSamplesInternal()
{
	bool bSuccess = true;
	if (Session == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("EndAudioSamples called when no A2F-3D session active"));
	}
	else if (!bSamplesEnded)
	{
		if (QueuedSamples.Num() > 0)
		{
			UE_LOG(LogACERuntime, Verbose, TEXT("[ACE SID %d] very short audio clip detected, emotion and face parameters not supported"), SessionID);
		}

		// empty resample buffer if needed
		if ((SampleRate != 16000) && Resampler.IsValid())
		{
			// resample to 16 kHz
			Audio::FAlignedFloatBuffer ResampledBuffer;
			// no idea how many samples are leftover, we're just guessing 500 ought to be more than enough (represents 31.25 ms of audio)
			ResampledBuffer.AddUninitialized(500);
			// Resampler won't process the remaining audio in the buffer without a non-null InAudioBuffer pointer, even if it's 0 samples 🙄
			float Dummy = 0.0f;
			int32 OutputFramesGenerated;
			int32 Result = Resampler->ProcessAudio(
				&Dummy,
				0,
				true,
				ResampledBuffer.GetData(),
				ResampledBuffer.Num(),
				OutputFramesGenerated);
			check(Result == 0);

			if (OutputFramesGenerated > 0)
			{
				ResampledBuffer.SetNum(OutputFramesGenerated);

				// convert float → int16
				TArray<int16> SamplesInt16;
				SamplesInt16.SetNumUninitialized(OutputFramesGenerated);
				Audio::ArrayFloatToPcm16(ResampledBuffer, SamplesInt16);
				QueuedSamples.Append(SamplesInt16.GetData(), SamplesInt16.Num());
			}
		}

		// send leftover samples
		if (QueuedSamples.Num() > 0)
		{
			if (Provider != nullptr)
			{
				if (!bSamplesStarted)
				{
					// The first send must have at least MinimumInitialSamples audio samples so pad if necessary 🙄
					const int32 MinimumInitialSamples = Provider->GetMinimumInitialAudioSampleCount();
					if (QueuedSamples.Num() < MinimumInitialSamples)
					{
						QueuedSamples.AddZeroed(MinimumInitialSamples - QueuedSamples.Num());
						UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] padding audio samples to make %s happy"), SessionID, *Provider->GetName().ToString());
					}
				}

				bSuccess = Provider->SendAudioSamples(Session, QueuedSamples, NullOpt, nullptr);
				UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] Sent %d samples to %s"), SessionID, QueuedSamples.Num(), *Provider->GetName().ToString());
			}
			else
			{
				bSuccess = false;
				UE_LOG(LogACERuntime, Warning, TEXT("Unable to send audio samples, ignoring %d samples"), QueuedSamples.Num());
			}
			QueuedSamples.Empty();
		}

		if (Provider != nullptr)
		{
			Provider->EndOutgoingStream(Session);
		}
		UE_LOG(LogACERuntime, Log, TEXT("[ACE SID %d] End of samples"), SessionID);
		bSamplesEnded = true;
	}
	return bSuccess;
}

