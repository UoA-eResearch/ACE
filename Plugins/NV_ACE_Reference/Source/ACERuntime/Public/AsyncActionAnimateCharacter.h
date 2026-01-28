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
#include "Kismet/BlueprintAsyncActionBase.h"

#include "ACETypes.h"

#include "AsyncActionAnimateCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncAnimateCharacterOutputPin, bool, bSuccess);

class FAnimateCharacterRunnable;

UCLASS(MinimalAPI)
class UAsyncActionAnimateCharacter : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	virtual ~UAsyncActionAnimateCharacter();

	/**
	 * Send given sound wave asset to an Audio2Face-3D provider to animate a character.
	 *
	 * When sending the audio has succeeded or failed, the Audio Send Completed pin is activated with success/failure.
	 *
	 * @param Character				Character actor with attached UACEAudioCurveSourceComponent
	 * @param SoundWave				Sound Wave asset with voice audio clip
	 * @param ACEEmotionParameters	Optional character emotion
	 * @param Audio2FaceParameters	Optional facial animation parameters
	 * @param A2FProviderName		The name of the Audio2Face-3D provider. If unspecified, the default implementation will be used
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", Category = "ACE|Audio2Face-3D", AutoCreateRefTerm="ACEEmotionParameters", WorldContext = "WorldContextObject"))
	static ACERUNTIME_API UAsyncActionAnimateCharacter* AnimateCharacterFromSoundWaveAsync(UObject* WorldContextObject, AActor * Character, class USoundWave* SoundWave,
		const FAudio2FaceEmotion &ACEEmotionParameters = FAudio2FaceEmotion(), UAudio2FaceParameters* Audio2FaceParameters = nullptr, FName A2FProviderName = FName("Default"));


	/**
	 * Send given WAV file to an Audio2Face-3D provider to animate a character.
	 *
	 * When sending the audio has succeeded or failed, the completed pin is activated with success/failure.
	 *
	 * @param Character				Character actor with attached UACEAudioCurveSourceComponent
	 * @param SoundWave				Precached Sound Wave asset with voice audio clip
	 * @param ACEEmotionParameters	Optional character emotion
	 * @param Audio2FaceParameters	Optional facial animation parameters
	 * @param A2FProviderName		The name of the Audio2Face-3D provider. If unspecified, the default implementation will be used
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", Category = "ACE|Audio2Face-3D", AutoCreateRefTerm="ACEEmotionParameters", WorldContext = "WorldContextObject"))
	static ACERUNTIME_API UAsyncActionAnimateCharacter* AnimateCharacterFromWavFileAsync(UObject* WorldContextObject, AActor* Character, const FString& PathToWav,
		const FAudio2FaceEmotion& ACEEmotionParameters = FAudio2FaceEmotion(), UAudio2FaceParameters* Audio2FaceParameters = nullptr, FName A2FProviderName = FName("Default"));

	/** Delegate called when sending the audio data completes */
	UPROPERTY(BlueprintAssignable)
	FAsyncAnimateCharacterOutputPin AudioSendCompleted;

	///////////////////////////////////
	// begin UBlueprintAsyncActionBase
	ACERUNTIME_API virtual void Activate() override;
	// end UBlueprintAsyncActionBase
	///////////////////////////////////

	virtual bool IsReadyForFinishDestroy() override;


protected:
	enum class EAnimationSource
	{
		SoundWave,
		WavFile,
	};

	/** Which animation source to read from */
	EAnimationSource Source;

	UPROPERTY()
	TObjectPtr<AActor> Character;

	UPROPERTY()
	TObjectPtr<USoundWave> SoundWave;

	FString PathToWav;
	FAudio2FaceEmotion EmotionParameters;

	UPROPERTY()
	TObjectPtr<UAudio2FaceParameters> FaceParameters;

	FName ProviderName;

private:
	std::atomic<bool> bIsActive;
	FAnimateCharacterRunnable* Runnable;

	friend class FAnimateCharacterRunnable;
};
