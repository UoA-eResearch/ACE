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
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ACETypes.h"

#include "ACEBlueprintLibrary.generated.h"


UCLASS()
class ACERUNTIME_API UACEBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Override the destination URL and other optional connection info for a specific Audio2Face-3D implementation.
	 * If an established connection already exists with different parameters, it will be immediately disconnected.
	 * Multiple simultaneous connections with different connection parameters are unsupported.
	 * Any ACE Connection Info members with a non-empty string override the project default setting, empty strings restore the project default setting.
	 * If no A2FProviderName is specified, the default implementation will be used.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (DisplayName = "Override Audio2Face-3D Connection Info"))
	static void SetA2XConnectionInfo(const FACEConnectionInfo& ACEConnectionInfo, FName A2FProviderName = FName("Default"));

	/**
	 * Get the current destination URL and other optional connection info for the Audio2Face-3D connection.
	 * Takes into account the project default settings and any runtime connection info overrides.
	 * If no A2FProviderName is specified, the default implementation will be used.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (DisplayName = "Get Audio2Face-3D Connection Info"))
	static FACEConnectionInfo GetA2XConnectionInfo(FName A2FProviderName = FName("Default"));

	/** Get a list of currently available A2F-3D providers */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (DisplayName = "Get Available Audio2Face-3D Provider Names"))
	static TArray<FName> GetAvailableA2FProviderNames();

	/**
	 * Send given sound wave asset to Audio2Face-3D service to animate a character.
	 * If no A2FProviderName is specified, the default implementation will be used.
	 *
	 * Note: This may block the application while audio data is sent to Audio2Face-3D.
	 * It's recommended to use "Animate Character from Sound Wave Async" instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta=(AutoCreateRefTerm="ACEEmotionParameters", DeprecatedFunction, DeprecationMessage="Use Animate Character From Sound Wave Async instead"))
	static UPARAM(DisplayName = "Success") bool AnimateCharacterFromSoundWave(AActor * Character, class USoundWave* SoundWave, const FAudio2FaceEmotion &ACEEmotionParameters = FAudio2FaceEmotion(), UAudio2FaceParameters* Audio2FaceParameters = nullptr, FName A2FProviderName = FName("Default"));

	/**
	 * Send given wav file to Audio2Face-3D service to animate a character.
	 * If no A2FProviderName is specified, the default implementation will be used.
	 *
	 * Note: This may block the application while audio data is sent to Audio2Face-3D.
	 * It's recommended to use "Animate Character from Wave File Async" instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (AutoCreateRefTerm = "ACEEmotionParameters", DeprecatedFunction, DeprecationMessage="Use Animate Character From Wav File Async instead"))
	static UPARAM(DisplayName = "Success") bool AnimateCharacterFromWavFile(AActor* Character, const FString& PathToWav, const FAudio2FaceEmotion& ACEEmotionParameters = FAudio2FaceEmotion(), UAudio2FaceParameters* Audio2FaceParameters = nullptr, FName A2FProviderName = FName("Default"));

	/**
	 * Override "Inference Burst Mode" project setting.
	 * Controls whether to burst audio to the Audio2Face-3D provider as fast as possible.
	 * It is not recommended to use Burst mode when Audio2Face-3D processing is running on the same system as rendering.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (DisplayName = "Override Audio2Face-3D Inference Burst Mode"))
	static void OverrideA2F3DInferenceMode(bool bForceBurstMode = false);

	/**
	 * Override "Max Initial Audio Chunk Size (Seconds)" project setting.
	 * Limits the size of the initial chunk of audio sent to Audio2Face-3D. Only has an effect when Real-time inference mode is enabled (Burst mode disabled).
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (DisplayName = "Override Audio2Face-3D Inference Max Initial Chunk Size"))
	static void OverrideA2F3DRealtimeInitialChunkSize(float MaxInitialChunkSizeSeconds = 0.5f);

	/**
	 * Request any resources needed for the given Audio2Face-3D provider to be pre-allocated.
	 *
	 * This is optional. Use it before you need an Audio2Face-3D provider to reduce latency the first time the provider is
	 * used. It may have no effect if the Audio2Face-3D provider has already run before.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (DisplayName = "Allocate Audio2Face-3D Resources"))
	static void AllocateA2F3DResources(FName A2FProviderName = FName("Default"));

	/**
	 * Request any resources allocated for the given Audio2Face-3D provider to be freed as soon as it's safe to do so.
	 *
	 * This is optional. Resources will be freed on application exit. But if you won't be using the Audio2Face-3D
	 * provider for a while, you can use this to free them sooner.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D", meta = (DisplayName = "Free Audio2Face-3D Resources"))
	static void FreeA2F3DResources(FName A2FProviderName = FName("Default"));

	/** Character will connect to an ACE animgraph server to receive animations from the requested stream
	 * If character is already subscribed to another stream, that stream will be automatically unsubscribed.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|AnimStream", meta = (AutoCreateRefTerm = "ACEEmotionParameters"))
	static UPARAM(DisplayName = "Success") bool SubscribeCharacterToStream(AActor* Character, const FString& StreamName);

	/** Cancel receiving animations from any stream */
	UFUNCTION(BlueprintCallable, Category = "ACE|AnimStream")
	static UPARAM(DisplayName = "Success") bool UnsubscribeFromStream(AActor* Character);

	/**
	 * Cancel character animations
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|Animation")
	static void StopCharacter(AActor* Character);

	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D|Parameters", meta = (WorldContext = "WorldContextObject"))
	static UAudio2FaceParameters* CreateAudio2FaceParameters(UObject* WorldContextObject);

	/** Get a map of strings to floats filled with the default Audio2Face-3D parameter values */
	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D|Parameters")
	static TMap<FString,float> GetDefaultParameterMap();

	/** Get a map of FName to floats filled with the default Audio2Face-3D blendshape values */
	UFUNCTION(BlueprintPure, Category = "ACE|Audio2Face-3D|Parameters")
	static TMap<FName, float> GetDefaultBlendshapeMap(const float DefaultValue = 1.0f);
};
