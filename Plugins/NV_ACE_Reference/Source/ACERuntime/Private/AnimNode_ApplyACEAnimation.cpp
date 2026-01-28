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
#include "AnimNode_ApplyACEAnimation.h"

// engine includes
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimTrace.h"
#include "Animation/AnimTypes.h"
#include "Misc/EngineVersionComparison.h"

// plugin includes
#include "ACEAudioCurveSourceComponent.h"
#include "ACERuntimePrivate.h"


void FAnimNode_ApplyACEAnimation::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FAnimNode_ApplyACEAnimation::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	FAnimNode_Base::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);

	if (Context.AnimInstanceProxy)
	{
		// convert from mesh bone index to compact pose index
		const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
		int32 MeshPoseIndex = BoneContainer.GetPoseBoneIndexForBoneName(HeadBone);
		HeadBoneCompactPoseIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshPoseIndex)).GetInt();
	}
}

void FAnimNode_ApplyACEAnimation::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)
	// The next line is required for the animation node's inputs to get evaluated
	GetEvaluateGraphExposedInputs().Execute(Context);

	Source.Update(Context);
}

void FAnimNode_ApplyACEAnimation::PreUpdate(const UAnimInstance* InAnimInstance)
{
	// PreUpdate executes in the game thread, unlike Update_AnyThread. This was inspired by FAnimNode_CurveSource.
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(PreUpdate)

	// re-bind to our ACE curve source in pre-update
	// we do this here to allow re-binding of the source without reinitializing the whole
	// anim graph. If the source goes away (e.g. if an audio component is destroyed) then
	// we can re-bind to a new object
	if (CurveSource == nullptr)
	{
		AActor* Actor = InAnimInstance->GetOwningActor();
		if (Actor != nullptr)
		{
			const TSet<UActorComponent*>& ActorOwnedComponents = Actor->GetComponents();
			for (UActorComponent* OwnedComponent : ActorOwnedComponents)
			{
				UACEAudioCurveSourceComponent* PotentialCurveSource = Cast<UACEAudioCurveSourceComponent>(OwnedComponent);
				if (PotentialCurveSource != nullptr)
				{
					CurveSource = PotentialCurveSource;
					break;
				}
			}
		}
	}

	if (CurveSource != nullptr)
	{
		CachedWeights.Reset();

		if (bInterpolate)
		{
			CurveSource->GetCurveOutputsInterp(CachedWeights);
		}
		else
		{
			CurveSource->GetCurveOutputs(CachedWeights);
		}
	}
}

static FName GetCurveName(size_t CurveIdx)
{
	constexpr size_t NumCurves = sizeof(UACEAudioCurveSourceComponent::CurveNames) / sizeof(FName);
	if (CurveIdx >= NumCurves)
	{
		return NAME_None; // Prevent out-of-bounds access.
	}
	return UACEAudioCurveSourceComponent::CurveNames[CurveIdx];
}

static void SetCurveVal(FPoseContext& Output, size_t CurveIdx, float CurveVal)
{
	FName CurveName = GetCurveName(CurveIdx);
	if (CurveName == NAME_None)
	{
		return;
	}
#if !UE_VERSION_OLDER_THAN(5,3,0)
	Output.Curve.Set(CurveName, CurveVal);
	TRACE_ANIM_NODE_VALUE(Output, *CurveName.ToString(), CurveVal);
#else
	const USkeleton* Skeleton = Output.AnimInstanceProxy->GetSkeleton();
	SmartName::UID_Type UID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, CurveName);
	if (ensureMsgf(UID != SmartName::MaxUID, TEXT("Couldn't find curve %s on skeleton"), *CurveName.ToString()))
	{
		Output.Curve.Set(UID, CurveVal);
		TRACE_ANIM_NODE_VALUE(Output, *CurveName.ToString(), CurveVal);
	}
#endif
}

void FAnimNode_ApplyACEAnimation::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	Source.Evaluate(Output);

	if (!CachedWeights.IsEmpty())
	{
		LastCurveVals.Reset(CachedWeights.Num());
		size_t CurveIdx = 0;

		const bool bHasOffsets = BlendshapeOffsets.Num() > 0;
		const bool bHasMultipliers = BlendshapeMultipliers.Num() > 0;
		
		for (float Weight : CachedWeights)
		{
			FName CurveName = NAME_None;
			if (bHasMultipliers || bHasOffsets)
			{
				CurveName = GetCurveName(CurveIdx);
			}

			if (bHasMultipliers)
			{
				const float* BlendshapeMultiplier = BlendshapeMultipliers.Find(CurveName);
				if(BlendshapeMultiplier)
				{
					Weight *= *BlendshapeMultiplier;
				}
			}
			
			if (bHasOffsets)
			{
				const float* BlendshapeOffset = BlendshapeOffsets.Find(CurveName);
				if(BlendshapeOffset)
				{
					Weight += *BlendshapeOffset;
				}
			}

			if (bBlendOutToZero)
			{
				// save the curve value to blend out later
				LastCurveVals.Add(Weight);
			}

			SetCurveVal(Output, CurveIdx++, Weight);
		}
	}
	else if (bBlendOutToZero && !LastCurveVals.IsEmpty())
	{
		if (BlendOutRate <= 0.0f)
		{
			// just hold the last values if the blend out rate is 0
			size_t CurveIdx = 0;
			for (float CurveVal : LastCurveVals)
			{
				SetCurveVal(Output, CurveIdx++, CurveVal);
			}
		}
		else
		{
			// Exponential decay to zero
			// FMath::FInterpTo will force to zero once the values get small enough so it really does reach zero
			const float DeltaTime = Output.AnimInstanceProxy->GetDeltaSeconds();
			size_t CurveIdx = 0;
			int32 LastNonZeroIdxPlusOne = 0;
			for (float& CurveVal : LastCurveVals)
			{
				CurveVal = FMath::FInterpTo(CurveVal, 0.0f, DeltaTime, BlendOutRate);
				SetCurveVal(Output, CurveIdx++, CurveVal);
				if (CurveVal != 0.0f)
				{
					LastNonZeroIdxPlusOne = static_cast<int32>(CurveIdx);
				}
			}

			// trim zeroes from the end, we're done with them
			if (LastNonZeroIdxPlusOne > 0)
			{
				LastCurveVals.SetNum(LastNonZeroIdxPlusOne);
			}
			else
			{
				LastCurveVals.Empty();
			}
		}
	}

	// TODO: apply head rotation
#if 0
	FTransform& OutHeadTransform = Output.Pose.GetMutableBones()[HeadBoneCompactPoseIndex];
	FQuat OldRotation = OutHeadTransform.GetRotation();
	OutHeadTransform.SetRotation(HeadRotation * OldRotation);
#endif
}

void FAnimNode_ApplyACEAnimation::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FAnimNode_Base::GatherDebugData(DebugData);
	Source.GatherDebugData(DebugData);
}

