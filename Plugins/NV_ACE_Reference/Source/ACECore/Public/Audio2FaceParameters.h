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

#include "Audio2FaceParameters.generated.h"

USTRUCT(BlueprintType)
struct ACECORE_API FAudio2FaceParameterHelper
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 2))
	float SkinStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 2))
	float UpperFaceStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 2))
	float LowerFaceStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = -1,ClampMax = 1))
	float EyelidOpenOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 2))
	float BlinkStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = -0.2,ClampMax = 0.2))
	float LipOpenOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 0.1))
	float UpperFaceSmoothing = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 0.1))
	float LowerFaceSmoothing = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 1))
	float FaceMaskLevel = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0.001,ClampMax = 0.5))
	float FaceMaskSoftness = 0.0085f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = 0,ClampMax = 3))
	float TongueStrength = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = -3,ClampMax = 3))
	float TongueHeightOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACE|Audio2Face-3D|Parameters",meta=(ClampMin = -3,ClampMax = 3))
	float TongueDepthOffset = 0.f;


public:

	TMap<FString,float> GetParameterMap() const;

};




UCLASS(BlueprintType, meta = (DisplayName = "Audio2Face-3D Parameters"))
class ACECORE_API UAudio2FaceParameters : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D|Parameters")
	void SetParameter(const FString& ParamName,float ParamValue);

	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D|Parameters")
	void ClearParameter(const FString& ParamName);

	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D|Parameters")
	void BatchSetParameters(const TMap<FString,float>& InParameterMap,bool bReplaceCurrentParams);

	UFUNCTION(BlueprintCallable, Category = "ACE|Audio2Face-3D|Parameters")
	void SetParametersFromStruct(const FAudio2FaceParameterHelper& ParameterHelper,bool bReplaceCurrentParams);

public:

	UPROPERTY(BlueprintReadOnly, Category = "ACE|Audio2Face-3D|Parameters", meta = (DisplayName = "Audio2Face-3D Parameter Map"))
	TMap<FString,float> Audio2FaceParameterMap;

};