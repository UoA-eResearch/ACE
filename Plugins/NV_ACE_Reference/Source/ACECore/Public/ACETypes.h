/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 - 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "ACETypes.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName="Audio2Face-3D Emotion Overrides"))
struct FAudio2FaceEmotionOverride
{
	GENERATED_BODY()

public:
	/** whether to allow application override of Amazement emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideAmazement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideAmazement"))
	float Amazement = 0.0f;

	/** whether to allow application override of Anger emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideAnger = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideAnger"))
	float Anger = 0.0f;

	/** whether to allow application override of Cheekiness emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideCheekiness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideCheekiness"))
	float Cheekiness = 0.0f;

	/** whether to allow application override of Disgust emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideDisgust = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideDisgust"))
	float Disgust = 0.0f;

	/** whether to allow application override of Fear emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideFear = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideFear"))
	float Fear = 0.0f;

	/** whether to allow application override of Grief emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideGrief = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideGrief"))
	float Grief = 0.0f;

	/** whether to allow application override of Joy emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideJoy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideJoy"))
	float Joy = 0.0f;

	/** whether to allow application override of OutOfBreath emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideOutOfBreath = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideOutOfBreath"))
	float OutOfBreath = 0.0f;

	/** whether to allow application override of Pain emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverridePain = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverridePain"))
	float Pain = 0.0f;

	/** whether to allow application override of Sadness emotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bOverrideSadness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideSadness"))
	float Sadness = 0.0f;
};

/**
 * Parameters relative to the emotion blending and processing before using it to generate blendshapes
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Audio2Face-3D Emotion Parameters"))
struct ACECORE_API FAudio2FaceEmotion
{
	GENERATED_BODY()

public:

	/**
	 * Sets the strength of generated emotions relative to neutral emotion.
	 * This multiplier is applied globally after the mix of emotion is done.
	 * If set to 0, emotion will be neutral.
	 * If set to 1, the blend of emotion will be fully used. (can be too intense)
	 * Default value: 0.6
	 * Min: 0
	 * Max: 1
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverallEmotionStrength = 0.6f;

	/**
	  * Increases the spread between Audio2Face-3D-detected emotion values by pushing them higher or lower.
	  * Default value: 1
	  * Min: 0.3
	  * Max: 3
	  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detected Emotion", meta = (ClampMin = "0.3", ClampMax = "3.0"))
	float DetectedEmotionContrast = 1.0f;

	/**
	 * Sets a firm limit on the quantity of emotion sliders engaged by A2E
	 * emotions with highest weight will be prioritized
	 * Default value: 3
	 * Min: 1
	 * Max: 6
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detected Emotion", meta = (ClampMin = "1", ClampMax = "6"))
	int32 MaxDetectedEmotions = 3;

	/**
	 * Coefficient for smoothing Audio2Face-3D-detected emotions over time
	 *  0 means no smoothing at all (can be jittery)
	 *  1 means extreme smoothing (emotion values not updated over time)
	 * Default value: 0.7
	 * Min: 0
	 * Max: 1
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detected Emotion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DetectedEmotionSmoothing = 0.7f;
	
	/**
	 * Activate blending between the application-provided emotion overrides and the emotions detected by Audio2Face-3D.
	 *  Setting to false is equivalent to setting EmotionOverrideStrength=0.0f
	 * Default: True
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Application Overrides", meta = (InlineEditConditionToggle))
	bool bEnableEmotionOverride = true;
	
	/**
	 * Sets the strength of the application-provided emotion overrides relative to emotions detected by Audio2Face-3D.
	 * 0 means only A2F-3D output will be used for emotion rendering.
	 * 1 means only the application-provided emotion overrides will be used for emotion rendering.
	 * Default value: 0.5
	 * Min: 0
	 * Max: 1
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Application Overrides", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableEmotionOverride"))
	float EmotionOverrideStrength = 0.5f;
	
	/** Optional application-provided emotion overrides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Application Overrides", meta = (EditCondition = "bEnableEmotionOverride && (EmotionOverrideStrength > 0.0)"))
	FAudio2FaceEmotionOverride EmotionOverrides;

public:

	bool IsEmotionOverrideActive() const;
};


USTRUCT(BlueprintType,meta=(DisplayName="ACE Connection Info"))
struct ACECORE_API FACEConnectionInfo
{
	GENERATED_BODY()

public:

	/* Server URL to connect to, for example http://203.0.113.37:52000 or https://ace.example.com:52010 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta = (DisplayName = "Dest URL"))
	FString DestURL;

	/**
	 * API Key, starts with "nvapi-".
	 * Get an API key through https://build.nvidia.com to connect to NVIDIA-hosted ACE services.
	 * Leave this blank if you are connecting to a separately hosted service.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta = (DisplayName = "API Key"))
	FString APIKey;

	/**
	 * NVCF Function Id
	 * Get an NVCF Function ID through https://build.nvidia.com to connect to NVIDIA-hosted services.
	 * Leave this blank if you are connecting to a separately hosted service.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta = (DisplayName = "NvCF Function Id"))
	FString NvCFFunctionId;
	
	/*
	 * NVCF Function Version
	 * Optional, it's OK to leave this blank.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta=(DisplayName="NvCF Function Version"))
	FString NvCFFunctionVersion;

public:

	bool operator== (const FACEConnectionInfo &Other) const;

	bool operator!= (const FACEConnectionInfo& Other) const;
};



