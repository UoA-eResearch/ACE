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

// engine
#include "Animation/AnimNodeBase.h"
#include "CoreMinimal.h"

// plugin
#include "AnimNode_ApplyACEAnimation.generated.h"


class UACEAudioCurveSourceComponent;


/** Apply face expression weights from a face expression tracker */
USTRUCT(BlueprintInternalUseOnly)
struct ACERUNTIME_API FAnimNode_ApplyACEAnimation : public FAnimNode_Base
{
	GENERATED_BODY()

	/** Input pose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink Source;

	/**
	  * Whether to blend curve values to zero when animation is inactive.
	  * If this is enabled, this node will blend curve values to 0 when the animation is inactive until the values reach zero
	  * and then stop applying curve values.
	  * If this is not enabled, this node will write no curve values when the animation is inactive.
	  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BlendOut, meta = (PinHiddenByDefault))
	bool bBlendOutToZero = false;

	/**
	  * Rate to blend curves to zero when animation is inactive.
	  * If rate is 0, the last curve values will be held until the next time animation is active.
	  * If rate is non-zero an exponential decay will be applied.
	  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BlendOut, meta = (PinHiddenByDefault, EditCondition="bBlendOutToZero", ClampMin="0.0", UIMin="0.0"))
	float BlendOutRate = 10.0f;

	/** Bone name to apply head rotation to (not yet implemented) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Target, meta = (PinHiddenByDefault))
	FName HeadBone = FName(TEXT("Head"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation, meta = (PinHiddenByDefault))
	TMap<FName, float> BlendshapeMultipliers;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation, meta = (PinHiddenByDefault))
	TMap<FName, float> BlendshapeOffsets;

	/** Apply linear interpolation for smoother animations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation, meta = (PinHiddenByDefault))
	bool			bInterpolate = true;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	// End of FAnimNode_Base interface

private:
	UACEAudioCurveSourceComponent* CurveSource;
	int32 HeadBoneCompactPoseIndex;
	TArray<float> CachedWeights;
	TArray<float> LastCurveVals;
};

