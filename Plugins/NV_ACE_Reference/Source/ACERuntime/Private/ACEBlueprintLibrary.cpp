/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "ACEBlueprintLibrary.h"

// engine includes
#include "Audio.h"
#include "AudioWaveFormatParser.h"
#include "Engine/Engine.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "DSP/FloatArrayMath.h"


// plugin includes
#include "A2FProvider.h"
#include "ACEAudioCurveSourceComponent.h"
#include "ACERuntimeModule.h"
#include "ACERuntimePrivate.h"
#include "ACESettings.h"
#include "ACETypes.h"
#include "AnimStreamModule.h"
#include "Audio2FaceParameters.h"
#include "SoundWaveConversion.h"

static const TMap<FString,float> DefaultParameters = 
{
	// these names are defined in the A2F-3D protocol description
	{ "skinStrength", 1.0f },
	{ "upperFaceStrength", 1.0f },
	{ "lowerFaceStrength", 1.0f },
	{ "eyelidOpenOffset", 0.0f },
	{ "blinkStrength", 1.0f },
	{ "lipOpenOffset", 0.0f },
	{ "upperFaceSmoothing", 0.001f },
	{ "lowerFaceSmoothing", 0.006f },
	{ "faceMaskLevel", 0.6f },
	{ "faceMaskSoftness", 0.0085f },
	{ "tongueStrength", 1.3f },
	{ "tongueHeightOffset", 0.0f },
	{ "tongueDepthOffset", 0.0f },
};

UACEBlueprintLibrary::UACEBlueprintLibrary(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
}

static IA2FRemoteProvider* GetRemoteProviderFromName(FName ProviderName)
{
	IA2FProvider* Provider = GetProviderFromName(ProviderName);
	if (Provider != nullptr)
	{
		return Provider->GetRemoteProvider();
	}

	return nullptr;
}

void UACEBlueprintLibrary::SetA2XConnectionInfo(const FACEConnectionInfo& ACEConnectionInfo, FName A2FProviderName)
{
	IA2FRemoteProvider* Provider = GetRemoteProviderFromName(A2FProviderName);
	if (Provider != nullptr)
	{
		Provider->SetConnectionInfo(ACEConnectionInfo.DestURL.TrimStartAndEnd(),ACEConnectionInfo.APIKey,ACEConnectionInfo.NvCFFunctionId,ACEConnectionInfo.NvCFFunctionVersion);
	}
	else
	{
		UE_LOG(LogACERuntime, Log, TEXT("%s: Provider %s is not a remote A2F-3D provider, doing nothing"), ANSI_TO_TCHAR(__FUNCTION__), *A2FProviderName.ToString());
	}
}

FACEConnectionInfo UACEBlueprintLibrary::GetA2XConnectionInfo(FName A2FProviderName)
{
	IA2FRemoteProvider* Provider = GetRemoteProviderFromName(A2FProviderName);
	if (Provider == nullptr)
	{
		UE_LOG(LogACERuntime, Log, TEXT("%s: Provider %s is not a remote A2F-3D provider, using project default"), ANSI_TO_TCHAR(__FUNCTION__), *A2FProviderName.ToString());
		return GetDefault<UACESettings>()->ACEConnectionInfo;
	}

	return Provider->GetConnectionInfo();
}

TArray<FName> UACEBlueprintLibrary::GetAvailableA2FProviderNames()
{
	TArray<FName> Names = IA2FProvider::GetAvailableProviderNames();

	int32 DefaultIdx = Names.IndexOfByKey(GetDefaultProviderName());
	if ((DefaultIdx != INDEX_NONE) && (DefaultIdx != 0))
	{
		// make sure the default appears first in the list of names
		Names.Swap(0, DefaultIdx);
	}

	return Names;
}

bool UACEBlueprintLibrary::AnimateCharacterFromSoundWave(AActor* Character, USoundWave* SoundWave, const FAudio2FaceEmotion& ACEEmotionParameters, UAudio2FaceParameters* Parameters, FName A2FProviderName)
{
	if (Character == nullptr)
	{
		UE_LOG(LogACERuntime, Error, TEXT("AnimateCharacterFromSoundWave called with no Character input"));
		return false;
	}
	if (SoundWave == nullptr)
	{
		UE_LOG(LogACERuntime, Error, TEXT("AnimateCharacterFromSoundWave called with no SoundWave input"));
		return false;
	}
	IA2FProvider* Provider = GetProviderFromName(A2FProviderName);
	if (Provider == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: No Audio2Face-3D provider found for name %s, doing nothing"), ANSI_TO_TCHAR(__FUNCTION__), *A2FProviderName.ToString());
		return false;
	}

	UACEAudioCurveSourceComponent* ACEComp = Character->GetComponentByClass<UACEAudioCurveSourceComponent>();
	if (ACEComp == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: No UACEAudioCurveSourceComponent found on actor %s"),StringCast<TCHAR>(__FUNCTION__).Get(), *Character->GetFullName());
		return false;
	}

	UE_LOG(LogACERuntime, Log, TEXT("sending %s to %s"), *SoundWave->GetFullName(), *Provider->GetName().ToString());
	return AnimateFromSoundWave(ACEComp, SoundWave, ACEEmotionParameters, Parameters, A2FProviderName);
}

bool UACEBlueprintLibrary::AnimateCharacterFromWavFile(AActor* Character, const FString& PathToWav, const FAudio2FaceEmotion& ACEEmotionParameters, UAudio2FaceParameters* Audio2FaceParameters, FName A2FProviderName)
{
	if (Character == nullptr)
	{
		UE_LOG(LogACERuntime, Error, TEXT("%s called with no Character input"), StringCast<TCHAR>(__FUNCTION__).Get());
		return false;
	}
	if (PathToWav.IsEmpty())
	{
		UE_LOG(LogACERuntime, Error, TEXT("%s: empty wav file path when attempting to animate %s"), StringCast<TCHAR>(__FUNCTION__).Get(), *Character->GetFullName());
		return false;
	}
	if (!PathToWav.EndsWith(TEXT(".wav")))
	{
		UE_LOG(LogACERuntime, Error, TEXT("%s: file \"%s\" does not end in .wav, when attempting to animate %s"), StringCast<TCHAR>(__FUNCTION__).Get(), *PathToWav, *Character->GetFullName());
		return false;
	}
	IA2FProvider* Provider = GetProviderFromName(A2FProviderName);
	if (Provider == nullptr)
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: No Audio2Face-3D provider found for name %s, doing nothing"), ANSI_TO_TCHAR(__FUNCTION__), *A2FProviderName.ToString());
		return false;
	}

	if (UACEAudioCurveSourceComponent* ACEComp = Character->GetComponentByClass<UACEAudioCurveSourceComponent>())
	{
		TArray<uint8> FileData;
		FWaveModInfo WaveInfo;
		FString Error;

		if (FFileHelper::LoadFileToArray(FileData, *PathToWav))
		{
			if (WaveInfo.ReadWaveInfo(FileData.GetData(), FileData.Num(), &Error))
			{
				if (*WaveInfo.pChannels <= 2)
				{
					UE_LOG(LogACERuntime, Log, TEXT("sending wav file %s to %s"), *PathToWav, *Provider->GetName().ToString());
					TArray<uint8> AudioData;

					//if wav format is IEEE 32bit float we convert the data to pcm 16bit first.
					if (*WaveInfo.pFormatTag == 3 && *WaveInfo.pBitsPerSample == 32)
					{
						// cancel any in-progress animation first
						FACERuntimeModule::Get().CancelAnimationGeneration(ACEComp);
						TArrayView<const float> SamplesFloat(reinterpret_cast<const float*>(WaveInfo.SampleDataStart), WaveInfo.SampleDataSize / sizeof(float));
						return FACERuntimeModule::Get().AnimateFromAudioSamples(ACEComp, SamplesFloat, *WaveInfo.pChannels, *WaveInfo.pSamplesPerSec,
							true, ACEEmotionParameters, Audio2FaceParameters, A2FProviderName);
					}
					else if (*WaveInfo.pBitsPerSample == 16)
					{
						// cancel any in-progress animation first
						FACERuntimeModule::Get().CancelAnimationGeneration(ACEComp);
						TArrayView<const int16> SamplesInt16(reinterpret_cast<const int16*>(WaveInfo.SampleDataStart), WaveInfo.SampleDataSize / sizeof(int16));
						return FACERuntimeModule::Get().AnimateFromAudioSamples(ACEComp, SamplesInt16, *WaveInfo.pChannels, *WaveInfo.pSamplesPerSec,
							true, ACEEmotionParameters, Audio2FaceParameters, A2FProviderName);
					}
					else
					{
						UE_LOG(LogACERuntime, Warning, TEXT("unsupported wav file format, format tag: %d bits per sample: %d. supported combinations: PCM 16bit and IEEE float 32bit."), *WaveInfo.pFormatTag, *WaveInfo.pBitsPerSample);
						return false;
					}
				}
				else
				{
					UE_LOG(LogACERuntime, Warning, TEXT("wav file contains unsupported number of channels: %d. only mono and stereo is supported."), *WaveInfo.pChannels);
					return false;
				}
			}
			else
			{
				UE_LOG(LogACERuntime, Warning, TEXT("unable to parse wav file %s: %s"), *PathToWav, *Error);
			}
		}
		else
		{
			UE_LOG(LogACERuntime, Error, TEXT("%s: unable to load file %s"), StringCast<TCHAR>(__FUNCTION__).Get(), *PathToWav);
		}
	}
	else
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: No UACEAudioCurveSourceComponent found on actor %s"), StringCast<TCHAR>(__FUNCTION__).Get(), *Character->GetFullName());
	}

	return false;
}

void UACEBlueprintLibrary::OverrideA2F3DInferenceMode(bool bForceBurstMode)
{
	FACERuntimeModule::Get().bOverrideBurstMode = bForceBurstMode;
}

void UACEBlueprintLibrary::OverrideA2F3DRealtimeInitialChunkSize(float MaxInitialChunkSizeSeconds)
{
	FACERuntimeModule::Get().OverrideMaxInitialAudioChunkSize = MaxInitialChunkSizeSeconds;
}

void UACEBlueprintLibrary::AllocateA2F3DResources(FName A2FProviderName)
{
	FACERuntimeModule::Get().AllocateA2F3DResources(A2FProviderName);
}

void UACEBlueprintLibrary::FreeA2F3DResources(FName A2FProviderName)
{
	FACERuntimeModule::Get().FreeA2F3DResources(A2FProviderName);
}

bool UACEBlueprintLibrary::SubscribeCharacterToStream(AActor* Character, const FString& StreamID)
{
	if (UACEAudioCurveSourceComponent* ACEComp = Character->GetComponentByClass<UACEAudioCurveSourceComponent>())
	{
		return FAnimStreamModule::Get().SubscribeCharacterToStream(ACEComp, StreamID);
	}
	else
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: No UACEAudioCurveSourceComponent found on actor %s"), StringCast<TCHAR>(__FUNCTION__).Get(), *Character->GetFullName());
	}

	return false;
}

void UACEBlueprintLibrary::StopCharacter(AActor* Character)
{
	if (Character == nullptr)
	{
		UE_LOG(LogACERuntime, Error, TEXT("%s called with no Character input"), StringCast<TCHAR>(__FUNCTION__).Get());
		return;
	}

	if (UACEAudioCurveSourceComponent* ACEComp = Character->GetComponentByClass<UACEAudioCurveSourceComponent>())
	{
		ACEComp->Stop();
	}
	else
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: No UACEAudioCurveSourceComponent found on actor %s"), StringCast<TCHAR>(__FUNCTION__).Get(), *Character->GetFullName());
	}
}

UAudio2FaceParameters* UACEBlueprintLibrary::CreateAudio2FaceParameters(UObject* WorldContextObject)
{
	return NewObject<UAudio2FaceParameters>(WorldContextObject);
}

TMap<FString, float> UACEBlueprintLibrary::GetDefaultParameterMap()
{
	return DefaultParameters;
}

TMap<FName, float> UACEBlueprintLibrary::GetDefaultBlendshapeMap(const float DefaultValue)
{
	TMap<FName, float> OutMap;
	// Reserve Memory to avoid reallocations for each element added.
	OutMap.Reserve(sizeof(UACEAudioCurveSourceComponent::CurveNames) / sizeof(FName));
	
	for (const FName& CurveName : UACEAudioCurveSourceComponent::CurveNames)
	{
		OutMap.Add(CurveName, DefaultValue);
	}
	return OutMap;
}

bool UACEBlueprintLibrary::UnsubscribeFromStream(AActor* Character)
{
	if (UACEAudioCurveSourceComponent* ACEComp = Character->GetComponentByClass<UACEAudioCurveSourceComponent>())
	{
		return FAnimStreamModule::Get().UnsubscribeFromStream(ACEComp);
	}
	else
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: No UACEAudioCurveSourceComponent found on actor %s"), StringCast<TCHAR>(__FUNCTION__).Get(), *Character->GetFullName());
	}

	return false;
}
