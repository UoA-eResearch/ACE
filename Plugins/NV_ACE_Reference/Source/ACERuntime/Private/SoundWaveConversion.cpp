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

#include "SoundWaveConversion.h"

// engine includes
#include "AudioDecompress.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Interfaces/IAudioFormat.h"
#include "Misc/EngineVersionComparison.h"
#include "Sound/SoundWave.h"

// plugin includes
#include "ACERuntimeModule.h"
#include "ACERuntimePrivate.h"


static TAutoConsoleVariable<bool> CVarACERawPCMDataEnable(
	TEXT("au.ace.rawpcmdata.enable"),
	false,
	TEXT("Enable reading from RawPCMData member of a USoundWave for input to Audio2Face-3D. (default: false)\n")
	TEXT("RawPCMData would only be used as a last resort if no audio data could be found in the USoundWave asset.\n")
	TEXT("Note: There is currently a data race if enabled. Care must be taken to ensure that RawPCMData won't be updated while in use."),
	ECVF_Default);

static TArray<uint8> GetPCMBufferFromPrecachedSoundWave(USoundWave* SoundWave, FSoundQualityInfo& QualityInfo)
{
	check(SoundWave->LoadingBehavior == ESoundWaveLoadingBehavior::ForceInline);

	TArray<uint8> SampleBytes;
#if !UE_VERSION_OLDER_THAN(5,4,0)
	if (SoundWave->GetSoundAssetCompressionType() == ESoundAssetCompressionType::Opus)
	{
		UE_LOG(LogACERuntime, Error,
			TEXT("%s uses Opus compression and ForceInline loading behavior. Unreal Engine only supports Opus with streaming audio. Consider using a different compression or loading behavior"),
			*SoundWave->GetFullName());
		return SampleBytes;
	}
#endif

	// Prime the USoundWave if not already initialized
	if (SoundWave->GetResourceSize() == 0)
	{
		FName RuntimeFormat = SoundWave->GetRuntimeFormat();
		SoundWave->InitAudioResource(RuntimeFormat);
	}

	// Decompress audio samples
	uint32 SampleRate = 0;
	uint16 NumChannels = 0;
	if (SoundWave->GetResourceSize() > 0)
	{
		ICompressedAudioInfo* AudioInfo = IAudioInfoFactoryRegistry::Get().Create(SoundWave->GetRuntimeFormat());
		bool bSuccess = AudioInfo->ReadCompressedInfo(SoundWave->GetResourceData(), SoundWave->GetResourceSize(), &QualityInfo);
		check(bSuccess);

		SampleBytes.AddZeroed(QualityInfo.SampleDataSize);
		AudioInfo->ExpandFile(SampleBytes.GetData(), &QualityInfo);

		SampleRate = QualityInfo.SampleRate;
		NumChannels = QualityInfo.NumChannels;
	}

	return SampleBytes;
}

static TArray<uint8> GetPCMBufferFromStreamingSoundWave(FSoundWaveProxyPtr Proxy, FSoundQualityInfo& QualityInfo)
{
	check(Proxy->IsStreaming());

	FName RuntimeFormat = Proxy->GetRuntimeFormat();
	ICompressedAudioInfo* AudioInfo = IAudioInfoFactoryRegistry::Get().Create(RuntimeFormat);

	AudioInfo->StreamCompressedInfo(Proxy, &QualityInfo);
	TArray<uint8> SampleBytes;
	SampleBytes.AddZeroed(QualityInfo.SampleDataSize);
	int32 NumBytesStreamed = 0;
	bool bFinished = AudioInfo->StreamCompressedData(SampleBytes.GetData(), false, SampleBytes.Num(), NumBytesStreamed);

	if (NumBytesStreamed == 0)
	{
		// Streaming didn't work first time. Manually load the first chunk to prime the pump and try StreamCompressedData again
		Proxy->GetZerothChunk(Proxy, true);
		if (Proxy->GetNumChunks() > 1)
		{
			if (FStreamingManagerCollection* StreamingMgr = IStreamingManager::Get_Concurrent())
			{
				IAudioStreamingManager& AudioStreamingMgr = StreamingMgr->GetAudioStreamingManager();
				AudioStreamingMgr.GetLoadedChunk(Proxy, 1, true, true);
			}
		}
		bFinished = AudioInfo->StreamCompressedData(SampleBytes.GetData(), false, SampleBytes.Num(), NumBytesStreamed);
	}

	if (!bFinished)
	{
		// there is probably a way to get the rest of the asset here, but unless we find a case where this path is getting hit we shouldn't worry about it
		UE_LOG(LogACERuntime, Warning, TEXT("Unable to fully decompress streaming USoundWave %s, %d/%d bytes streamed"), *Proxy->GetFName().ToString(), NumBytesStreamed, SampleBytes.Num());
		if (NumBytesStreamed == 0)
		{
			return {};
		}
	}

	// ADPCM can apparently leave some bytes off the end (hopefully silence)
	// and also, maybe the streaming just didn't work for some reason and the buffer is empty
	if (NumBytesStreamed < SampleBytes.Num())
	{
		int32 NumToRemove = SampleBytes.Num() - NumBytesStreamed;
		SampleBytes.RemoveAt(NumBytesStreamed, NumToRemove);
	}

	return SampleBytes;
}


static bool AnimateFromSoundWaveRawPCMData(IACEAnimDataConsumer* Consumer, USoundWave* SoundWave, TOptional<FAudio2FaceEmotion> EmotionParameters,
	UAudio2FaceParameters* Audio2FaceParameters, FName A2FProviderName)
{
	int32 NumChannels = SoundWave->NumChannels;
	float SampleRate = SoundWave->GetSampleRateForCurrentPlatform();
	if (SampleRate <= 0.0f)
	{
		// GetSampleRateForCurrentPlatform() could return -1.0f if there's no specific sample rate for this platform
		UE_LOG(LogACERuntime, Verbose, TEXT("Unknown sample rate on %s, skipping"), *SoundWave->GetFullName());
		return false;
	}

	// This might work. Really we're just hoping to get lucky and find something useful in your USoundWave's RawPCMData
	// which would only work in some cases. If your code reaches this point and it works, let us know! If your code
	// reaches this point and it blows up, also let us know!
	// This is a little sketchy. Usually the worst case would be an incomplete buffer so we'd only get part of the clip.
	// But also there's a race condition here, since nothing prevents the RawPCMData from getting updated before/during
	// the time we copy out of it. So there's that.
	if ((SoundWave->RawPCMData != nullptr) && (SoundWave->RawPCMDataSize > 0))
	{
		TArray<uint8> SampleBytes(SoundWave->RawPCMData, SoundWave->RawPCMDataSize);
		if (SampleBytes.IsEmpty())
		{
			// just in case the RawPCMData got removed before we read it
			return false;
		}

		bool bSuccess = false;

		Audio::EAudioMixerStreamDataFormat::Type PCMDataFormat = SoundWave->GetGeneratedPCMDataFormat();
		switch (PCMDataFormat)
		{
		case Audio::EAudioMixerStreamDataFormat::Int16:
		{
			// cancel any in-progress animation first
			FACERuntimeModule::Get().CancelAnimationGeneration(Consumer);
			const int32 NumSamples = SampleBytes.Num() / sizeof(int16);
			TArrayView<const int16> SamplesInt16 = MakeArrayView(BitCast<const int16*>(SampleBytes.GetData()), NumSamples);
			bSuccess = FACERuntimeModule::Get().AnimateFromAudioSamples(Consumer, SamplesInt16, NumChannels, SampleRate, true, EmotionParameters, Audio2FaceParameters, A2FProviderName);
		}
		break;
		case Audio::EAudioMixerStreamDataFormat::Float:
		{
			// cancel any in-progress animation first
			FACERuntimeModule::Get().CancelAnimationGeneration(Consumer);
			const int32 NumSamples = SampleBytes.Num() / sizeof(float);
			TArrayView<const float> SamplesFloat = MakeArrayView(BitCast<const float*>(SampleBytes.GetData()), NumSamples);
			bSuccess = FACERuntimeModule::Get().AnimateFromAudioSamples(Consumer, SamplesFloat, NumChannels, SampleRate, true, EmotionParameters, Audio2FaceParameters, A2FProviderName);
		}
		break;
		default:
			UE_LOG(LogACERuntime, Log, TEXT("Unknown PCM data format on %s, skipping"), *SoundWave->GetFullName());
			return false;
		}
		if (!bSuccess)
		{
			UE_LOG(LogACERuntime, Warning, TEXT("Failed sending %s to %s"), *SoundWave->GetFullName(), *A2FProviderName.ToString());
		}
		return bSuccess;
	}
	return false;
}

bool AnimateFromSoundWave(IACEAnimDataConsumer* Consumer, USoundWave* SoundWave, TOptional<FAudio2FaceEmotion> EmotionParameters,
	UAudio2FaceParameters* Audio2FaceParameters, FName A2FProviderName)
{
	check(Consumer != nullptr);
	check(SoundWave != nullptr);

	TArray<uint8> SampleBytes;
	FSoundQualityInfo QualityInfo{};

	FSoundWaveProxyPtr Proxy = SoundWave->CreateSoundWaveProxy();
	if (Proxy->IsStreaming())
	{
		SampleBytes = GetPCMBufferFromStreamingSoundWave(Proxy, QualityInfo);
	}
	else if (SoundWave->GetLoadingBehavior() == ESoundWaveLoadingBehavior::ForceInline)
	{
		SampleBytes = GetPCMBufferFromPrecachedSoundWave(SoundWave, QualityInfo);
	}

	if (SampleBytes.IsEmpty())
	{
		if (CVarACERawPCMDataEnable.GetValueOnAnyThread())
		{
			// We really don't know what sort of USoundWave asset we've got at this point. It might instead be something
			// derived from USoundWave like a USoundWaveProcedural for example. We don't know a generic way to get PCM
			// data out of any arbitrary USoundWave-derived thing, so at this point we're just hoping that there's
			// something useful in RawPCMData.
			// This code isn't really safe so currently it's protected behind a console variable that defaults to false.
			// Use "au.ace.rawpcmdata.enable true" from the console to enable this code. If you do so, it's up to you
			// to ensure that RawPCMData never gets updated while this code is running.
			return AnimateFromSoundWaveRawPCMData(Consumer, SoundWave, EmotionParameters, Audio2FaceParameters, A2FProviderName);
		}
		else
		{
			UE_LOG(LogACERuntime, Warning, TEXT("Unable to read audio from %s, skipping"), *SoundWave->GetFullName());
			return false;
		}
	}

	// cancel any in-progress animation first
	FACERuntimeModule::Get().CancelAnimationGeneration(Consumer);

	// view as int16
	const int32 NumSamples = SampleBytes.Num() / 2;
	TArrayView<const int16> SamplesInt16 = MakeArrayView(BitCast<const int16*>(SampleBytes.GetData()), NumSamples);

	// send to a2f-3d
	bool bSuccess = FACERuntimeModule::Get().AnimateFromAudioSamples(Consumer, SamplesInt16, QualityInfo.NumChannels, QualityInfo.SampleRate, true, EmotionParameters, Audio2FaceParameters, A2FProviderName);
	if (!bSuccess)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("Failed sending %s to %s"), *SoundWave->GetFullName(), *A2FProviderName.ToString());
	}
	return bSuccess;
}

