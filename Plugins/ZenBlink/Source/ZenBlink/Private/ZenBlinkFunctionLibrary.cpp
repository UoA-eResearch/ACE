// Copyright 2025 JOBUTSU LTD. All rights reserved.

#include "ZenBlinkFunctionLibrary.h"


FVector UZenBlinkFunctionLibrary::ZenGetClampedForwardDirection(
	USkeletalMeshComponent* MeshComp,
	AActor* TargetActor
)
{
	if (!MeshComp || !TargetActor)
	{
		return FVector::ZeroVector;
	}

	const FVector MeshForward = MeshComp->GetForwardVector();

	const FVector ToTarget = TargetActor->GetActorLocation() - MeshComp->GetComponentLocation();

	const float ForwardDist = FVector::DotProduct(MeshForward, ToTarget);

	constexpr float MinForward = 20.0f;
	if (ForwardDist < MinForward)
	{
		return MeshForward;
	}

	return ToTarget.GetSafeNormal();
}


FVector UZenBlinkFunctionLibrary::ZenGenerateRandomLocation(AActor* Actor, float Range, float ConeHalfAngleDeg)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}

	const FVector ActorLocation = Actor->GetActorLocation();
	const FVector ActorForward = Actor->GetActorForwardVector();

	const float ConeHalfAngleRad = FMath::DegreesToRadians(ConeHalfAngleDeg);

	const FVector RandomDirection = FMath::VRandCone(ActorForward, ConeHalfAngleRad);

	const float RandomDistance = FMath::FRandRange(0.0f, Range);

	return ActorLocation + RandomDirection * RandomDistance;
}

void UZenBlinkFunctionLibrary::ZenCheckInterface(
	USkeletalMeshComponent* SkeletalMesh,
	TSubclassOf<UAnimInstance> ZBAnimClass,
	TSubclassOf<UInterface> InterfaceClass,
	UAnimInstance*& OutAnimInstance,
	bool& bImplementsInterface)
{

	OutAnimInstance = nullptr;
	bImplementsInterface = false;

	if (!SkeletalMesh || !ZBAnimClass || !InterfaceClass)
	{
		return;
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	OutAnimInstance = AnimInstance;

	UObject* LinkedInstance = AnimInstance->GetLinkedAnimLayerInstanceByClass(ZBAnimClass);
	if (!LinkedInstance)
	{
		return;
	}

	bImplementsInterface = LinkedInstance->GetClass()->ImplementsInterface(InterfaceClass);


}

void UZenBlinkFunctionLibrary::ZenCheckPostInterface(
	USkeletalMeshComponent* SkeletalMesh,
	TSubclassOf<UAnimInstance> ZBAnimClass,
	TSubclassOf<UInterface> InterfaceClass,
	UAnimInstance*& OutAnimInstance,
	bool& bImplementsInterface)
{

	OutAnimInstance = nullptr;
	bImplementsInterface = false;

	if (!SkeletalMesh || !ZBAnimClass || !InterfaceClass)
	{
		return;
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetPostProcessInstance();
	if (!AnimInstance)
	{
		return;
	}

	OutAnimInstance = AnimInstance;

	UObject* LinkedInstance = AnimInstance->GetLinkedAnimLayerInstanceByClass(ZBAnimClass);
	if (!LinkedInstance)
	{
		return;
	}

	bImplementsInterface = LinkedInstance->GetClass()->ImplementsInterface(InterfaceClass);


}



UClass* UZenBlinkFunctionLibrary::ZenSetupAnimClass(USkeletalMeshComponent* FaceMesh, TSubclassOf<UAnimInstance> ZBAnimBP)
{
	if (!FaceMesh || !ZBAnimBP)
	{
		return nullptr;
	}

	FaceMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	FaceMesh->SetAnimInstanceClass(ZBAnimBP);

	return ZBAnimBP;
}


 void UZenBlinkFunctionLibrary::ZenCineCameraFocus(
	AActor* FocusActor,
	USkeletalMeshComponent* SkeletalMesh,
	const FString& FocusTarget1Name,
	const FString& FocusTarget2Name,
	const FVector& AutoFocusAdjustment,
	bool bAutoFocus
)
{
	 if (!FocusActor || !SkeletalMesh) return;

	 FVector TargetLocation;

	 if (bAutoFocus)
	 {
		 const FName Socket1(FocusTarget1Name);
		 const FName Socket2(FocusTarget2Name);
		 const FVector SocketLocation1 = SkeletalMesh->GetSocketLocation(Socket1);
		 const FVector SocketLocation2 = SkeletalMesh->GetSocketLocation(Socket2);
		 const FVector AverageLocation = (SocketLocation1 + SocketLocation2) * 0.5f;
		 TargetLocation = AverageLocation + AutoFocusAdjustment;
	 }
	 else
	 {
		 TargetLocation = FocusActor->GetActorLocation();
	 }

	 FocusActor->SetActorLocation(TargetLocation, false); 
}


USkeletalMeshComponent* UZenBlinkFunctionLibrary::ZenFindFaceMesh(AActor* Metahuman, const FString& FaceMeshName)
{
	if (!Metahuman)
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	Metahuman->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);

	for (USkeletalMeshComponent* Component : SkeletalMeshComponents)
	{
		if (Component && Component->GetName() == FaceMeshName)
		{
			return Component;
		}
	}

	return nullptr;
}


void UZenBlinkFunctionLibrary::SetPostProcess(bool bEnable, USkeletalMeshComponent* SkeletalMeshComponent, TSubclassOf<UAnimInstance> InPostProcessAnimBlueprint)
{
#if (ENGINE_MAJOR_VERSION > 5) || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)

	if (!SkeletalMeshComponent)
	{
		return;
	}

	if (bEnable && *InPostProcessAnimBlueprint)
	{
		SkeletalMeshComponent->SetOverridePostProcessAnimBP(InPostProcessAnimBlueprint, true);
	}

#endif
}

// Added for 2.2.1 to stop enum to int crash.
bool UZenBlinkFunctionLibrary::HasEmotionChanged(EMetaCharacterEmotion CurrentEmotion, EMetaCharacterEmotion NextEmotion)
{
	return CurrentEmotion == NextEmotion;
}

EMetaCharacterEmotion UZenBlinkFunctionLibrary::GetCurrentEmotion(FZenBlinkSettings Settings)
{

	return Settings.Emotion;

}


TMap<EMetaCharacterEmotion, FBlinkParametersAnim> UZenBlinkFunctionLibrary::InitializeEmotionBlinkMap()
{
	TMap<EMetaCharacterEmotion, FBlinkParametersAnim> EmotionBlinkMap;

	// Neutral: fairly standard blink rate, moderate amplitude, moderate eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Neutral, FBlinkParametersAnim{
		/* MinBlinkInterval    */ 3.2f,
		/* MaxBlinkInterval    */ 5.5f,
		/* BlinkDurationMin    */ 0.15f,
		/* BlinkDurationMax    */ 0.30f,
		/* BlinkAmplitudeMin   */ 0.30f,
		/* BlinkAmplitudeMax   */ 0.80f,
		/* PostBlinkDelay      */ 0.50f,
		/* EyeMovementDuration */ 3.8f,
		/* EyeMovementRange    */ FVector(6.0f, 4.0f, 4.0f),
		/* HeadEyeMovementWeight */ FVector(0.25f, 0.30f, 0.0f)
		});

	// Happy: more frequent, lighter/faster blinks, higher amplitude (big, bright eyes)
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Happy, FBlinkParametersAnim{
		1.8f, 3.0f, 0.08f, 0.25f, 0.80f, 1.0f, 0.30f, 2.7f,
		FVector(8.0f, 8.0f, 4.0f), FVector(0.50f, 0.55f, 0.0f)
		});

	// Sad: slower, heavier blinks, slightly longer durations, smaller amplitude range
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Sad, FBlinkParametersAnim{
		4.5f, 6.0f, 0.25f, 0.45f, 0.40f, 0.70f, 0.70f, 5.0f,
		FVector(6.0f, 6.0f, 4.0f), FVector(0.30f, 0.30f, 0.0f)
		});

	// Surprised: relatively frequent, very short blinks (quick eyelid flutter), wide eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Surprised, FBlinkParametersAnim{
		1.3f, 2.2f, 0.05f, 0.12f, 0.60f, 0.90f, 0.25f, 2.0f,
		FVector(8.0f, 8.0f, 5.0f), FVector(0.60f, 0.70f, 0.0f)
		});

	// Fearful: short intervals (nervous blinking), moderate blink duration, large eye range
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Fearful, FBlinkParametersAnim{
		1.2f, 2.2f, 0.20f, 0.35f, 0.50f, 1.0f, 0.30f, 1.3f,
		FVector(12.0f, 12.0f, 4.0f), FVector(0.65f, 0.95f, 0.0f)
		});

	// Focused: long intervals (rare blinking), shorter amplitude, narrow eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Focused, FBlinkParametersAnim{
		7.0f, 10.0f, 0.25f, 0.40f, 0.50f, 0.70f, 0.50f, 2.5f,
		FVector(2.0f, 2.0f, 2.0f), FVector(0.20f, 0.25f, 0.0f)
		});

	// Disgusted: moderate intervals, fairly quick but somewhat forceful blinks
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Disgusted, FBlinkParametersAnim{
		3.0f, 5.0f, 0.20f, 0.35f, 0.50f, 0.85f, 0.40f, 2.5f,
		FVector(5.0f, 5.0f, 8.0f), FVector(0.40f, 0.50f, 0.0f)
		});

	// Childish: fast movements
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Childish, FBlinkParametersAnim{
		0.8f, 1.8f, 0.05f, 0.15f, 0.6f, 0.9f, 0.4f, 1.0f,
		FVector(10.0f, 10.0f, 10.0f), FVector(0.6f, 0.95f, 0.0f)
		});

	// Tired: long intervals, slow and heavier blinks, moderate movement range
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Tired, FBlinkParametersAnim{
		6.0f, 9.0f, 0.45f, 0.70f, 0.60f, 0.80f, 0.90f, 4.5f,
		FVector(6.0f, 6.0f, 4.0f), FVector(0.15f, 0.20f, 0.0f)
		});

	// Annoyed: moderately frequent, somewhat snappy blinks
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Annoyed, FBlinkParametersAnim{
		2.0f, 3.5f, 0.10f, 0.30f, 0.60f, 0.85f, 0.40f, 2.0f,
		FVector(4.0f, 4.0f, 9.0f), FVector(0.35f, 0.55f, 0.0f)
		});

	// Confused: mid-range intervals, moderate blink, some extra wandering in the eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Confused, FBlinkParametersAnim{
		2.8f, 5.0f, 0.15f, 0.35f, 0.40f, 0.80f, 0.45f, 2.5f,
		FVector(10.0f, 10.0f, 6.0f), FVector(0.50f, 0.70f, 0.0f)
		});

	// Curious: more frequent blinking, moderate range, slightly faster eye movements
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Curious, FBlinkParametersAnim{
		1.2f, 2.4f, 0.08f, 0.25f, 0.70f, 1.0f, 0.35f, 2.5f,
		FVector(6.0f, 6.0f, 8.0f), FVector(0.50f, 0.80f, 0.0f)
		});

	// Embarrassed: somewhat frequent blinks, gentle closure, maybe looking downward
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Embarrassed, FBlinkParametersAnim{
		2.0f, 3.5f, 0.20f, 0.40f, 0.60f, 0.95f, 0.70f, 3.0f,
		FVector(8.0f, 8.0f, 1.0f), FVector(0.60f, 0.75f, 0.0f)
		});

	// Angry: shorter interval (irritated, frequent blinks), strong closure
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Angry, FBlinkParametersAnim{
		1.8f, 3.5f, 0.15f, 0.30f, 0.70f, 1.0f, 0.30f, 2.0f,
		FVector(10.0f, 10.0f, 7.0f), FVector(0.50f, 0.45f, 0.0f)
		});

	// Bored: longer intervals, lazy, slower blink, reduced eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Bored, FBlinkParametersAnim{
		5.0f, 8.0f, 0.35f, 0.60f, 0.50f, 0.70f, 0.70f, 4.5f,
		FVector(8.0f, 8.0f, 4.0f), FVector(0.20f, 0.20f, 0.0f)
		});

	// Excited: very frequent, quick, big blinks, lots of scanning
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Excited, FBlinkParametersAnim{
		0.8f, 1.8f, 0.05f, 0.18f, 0.85f, 1.0f, 0.20f, 1.0f,
		FVector(10.0f, 10.0f, 4.0f), FVector(0.65f, 0.90f, 0.0f)
		});

	// Relaxed: moderate intervals, easy-going blink speeds, smaller amplitude range
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Relaxed, FBlinkParametersAnim{
		3.0f, 5.5f, 0.15f, 0.35f, 0.60f, 0.80f, 0.60f, 4.5f,
		FVector(7.0f, 7.0f, 3.0f), FVector(0.25f, 0.25f, 0.0f)
		});

	// Suspicious: scanning eyes, moderate frequency with quick, partial blinks
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Suspicious, FBlinkParametersAnim{
		2.5f, 5.0f, 0.15f, 0.30f, 0.60f, 0.85f, 0.60f, 1.0f,
		FVector(10.0f, 10.0f, 2.0f), FVector(0.40f, 0.90f, 0.0f)
		});

	// Proud: moderate frequency, relatively confident (larger amplitude)
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Proud, FBlinkParametersAnim{
		2.2f, 4.2f, 0.10f, 0.25f, 0.70f, 1.0f, 0.40f, 3.0f,
		FVector(6.0f, 6.0f, 8.0f), FVector(0.30f, 0.25f, 0.0f)
		});

	// Pained: slower blinks, heavier closure, tension in the eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Pained, FBlinkParametersAnim{
		4.5f, 6.5f, 0.35f, 0.55f, 0.50f, 0.80f, 0.80f, 2.0f,
		FVector(3.0f, 3.0f, 3.0f), FVector(0.35f, 0.25f, 0.0f)
		});

	// Nervous: short intervals, fast blinks, darting eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Nervous, FBlinkParametersAnim{
		0.8f, 2.2f, 0.10f, 0.28f, 0.50f, 0.90f, 0.40f, 1.0f,
		FVector(8.0f, 8.0f, 3.0f), FVector(0.60f, 0.85f, 0.0f)
		});

	// Love: gentle, warm blinks, moderate frequency, soft eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Love, FBlinkParametersAnim{
		2.0f, 4.0f, 0.10f, 0.30f, 0.80f, 1.0f, 0.50f, 3.5f,
		FVector(9.0f, 9.0f, 9.0f), FVector(0.30f, 0.60f, 0.0f)
		});

	return EmotionBlinkMap;
}



TArray<FName> UZenBlinkFunctionLibrary::GetPoseNames(UPoseAsset* PoseAsset)
{

	TArray<FName> PoseNames;

	if (PoseAsset)
	{
		const TArray<FName>& Poses = PoseAsset->GetPoseFNames();
		PoseNames = Poses;
	}

	return PoseNames;
}

float UZenBlinkFunctionLibrary::GetCurveFloatValue(UCurveFloat* FCurve_in, float time_in)
{
	if (FCurve_in)
	{
		TWeakObjectPtr<UCurveFloat> FC = FCurve_in;
		return FC->GetFloatValue(time_in);
	}
	else
	{
		return 0.0f;
	}

}

float UZenBlinkFunctionLibrary::GetCurveVectorValue(UCurveVector* VCurve_in, float time_in, int32 Channel)
{
	if (VCurve_in)
	{
		UCurveVector* VCurve = VCurve_in;
		if (!VCurve)
		{
			return 0.0f;
		}

		FVector Value = VCurve->GetVectorValue(time_in);

		// Channel: Blink,Reserved,Pupil
		switch (Channel)
		{
		case 0: // X
			return Value.X;
		case 1: // Y
			return Value.Y;
		case 2: // Z
			return Value.Z;
		default:
			return 0.0f;
		}
	}
	else
	{
		return 0.0f;
	}
}


FZenBlinkCurveVector UZenBlinkFunctionLibrary::CreateCustomBlinkCurve(FZenBlinkSettings Settings, int32 NumberOfBlinks, const FBlinkParametersAnim& BlinkParams)
{

	if (NumberOfBlinks <= 0)
	{
		NumberOfBlinks = 1;
	}

	// Safely swap if min > max
	float Min = (BlinkParams.BlinkDurationMin > BlinkParams.BlinkDurationMax)
		? BlinkParams.BlinkDurationMax
		: BlinkParams.BlinkDurationMin;
	float Max = (BlinkParams.BlinkDurationMin > BlinkParams.BlinkDurationMax)
		? BlinkParams.BlinkDurationMin
		: BlinkParams.BlinkDurationMax;

	// Modified: Enforce faster blink.
	Min /= 2;
	Max /= 2;

	FZenBlinkCurveVector OutCurve;
	OutCurve.CurveEndTime = 0.0f;
	OutCurve.Curve = nullptr;


	UCurveVector* Curve = NewObject<UCurveVector>();
	if (!Curve || !Curve->IsValidLowLevelFast())
	{
		
		return OutCurve;
	}

	FRichCurve& XCurve = Curve->FloatCurves[0]; // Eyelid blink (X)
	FRichCurve& YCurve = Curve->FloatCurves[1]; // reserved (Y)
	FRichCurve& ZCurve = Curve->FloatCurves[2]; // Pupil constriction/dilation (Z)

	float TotalDuration = 0.0f;
	float PostBlinkDelay = FMath::Max(0.0f, BlinkParams.PostBlinkDelay);

	TArray<FKeyHandle> XKeyHandles;
	TArray<FKeyHandle> YKeyHandles;
	TArray<FKeyHandle> ZKeyHandles;

	float ROpen = BlinkParams.BlinkAmplitudeMin / 4.0f;

	for (int32 i = 0; i < NumberOfBlinks; ++i)
	{
		// Random blink duration in the specified range
		float BlinkDuration = FMath::FRandRange(FMath::Max(0.0f, Min), Max);
		float BlinkAmplitude = FMath::FRandRange(BlinkParams.BlinkAmplitudeMin, BlinkParams.BlinkAmplitudeMax);

		UE_LOG(LogTemp, Log, TEXT("Blink Duration %f"), BlinkDuration);

		// Time points
		float OpenTime = TotalDuration;
		float CloseTime = OpenTime + BlinkDuration;

		// Random hold/extension around closed state
		float RandomDelay = FMath::FRandRange(Min, Max);
		float CloseExtTime = CloseTime + RandomDelay;

		// Reopen time
		float ReOpenTime = CloseExtTime + BlinkDuration + RandomDelay;

		XKeyHandles.Add(XCurve.AddKey(OpenTime, ROpen));
		XKeyHandles.Add(XCurve.AddKey(CloseTime, BlinkAmplitude));
		XKeyHandles.Add(XCurve.AddKey(CloseExtTime, BlinkAmplitude));
		XKeyHandles.Add(XCurve.AddKey(ReOpenTime, ROpen));

		YKeyHandles.Add(YCurve.AddKey(OpenTime, 0.0f));
		YKeyHandles.Add(YCurve.AddKey(CloseTime, 0.0f));
		YKeyHandles.Add(YCurve.AddKey(CloseExtTime, 0.0f));
		YKeyHandles.Add(YCurve.AddKey(ReOpenTime, 0.0f));

		float PupilTimeOffset = FMath::Clamp(BlinkDuration * 0.2f, 0.2f, 0.5f);
		float PupilAmplitude = FMath::Clamp(1.0f - 0.3f * BlinkAmplitude, 0.6f, 1.0f);

		ZKeyHandles.Add(ZCurve.AddKey(OpenTime, 0.0f));
		ZKeyHandles.Add(ZCurve.AddKey(CloseTime, PupilAmplitude));
		ZKeyHandles.Add(ZCurve.AddKey(CloseExtTime, PupilAmplitude));
		ZKeyHandles.Add(ZCurve.AddKey(ReOpenTime + PupilTimeOffset, 0.0f));

		TotalDuration = ReOpenTime + PostBlinkDelay + PupilTimeOffset;
	}

	float EndDelay = FMath::FRandRange(
		FMath::Max(0.0f, BlinkParams.MinBlinkInterval),
		BlinkParams.MaxBlinkInterval
	);

	// Ensure the curves extend to that end time so it won't be GC'd prematurely
	float FinalTime = TotalDuration + EndDelay;
	XCurve.AddKey(FinalTime, ROpen);
	YCurve.AddKey(FinalTime, 0.0f);
	float FinalPupilTime = FinalTime + FMath::Clamp(PostBlinkDelay * 0.2f, 0.05f, 0.4f);
	ZCurve.AddKey(FinalPupilTime, 0.0f);

	// Set up tangents for smoother interpolation
	for (const FKeyHandle& Handle : XKeyHandles)
	{
		XCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic);
		XCurve.SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto);
	}
	for (const FKeyHandle& Handle : YKeyHandles)
	{
		YCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic);
		YCurve.SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto);
	}
	for (const FKeyHandle& Handle : ZKeyHandles)
	{
		ZCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic);
		ZCurve.SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto);
	}

	// Store the last key time
	if (XCurve.GetNumKeys() > 0)
	{
		OutCurve.Curve = Curve;
		OutCurve.CurveEndTime = XCurve.GetLastKey().Time;
	}
	else
	{
		OutCurve.CurveEndTime = 0.0f;
		OutCurve.Curve = nullptr;
		return OutCurve;
	}

	return OutCurve;
}




FZenBlinkCurveFloat UZenBlinkFunctionLibrary::CreateFaceCurve(FZenBlinkSettings Settings, float Duration, float StartAlpha, float EndAlpha, const FBlinkParametersAnim& BlinkParams)
{

	FZenBlinkCurveFloat OutCurve;
	OutCurve.CurveEndTime = 0.0f;
	OutCurve.Curve = nullptr;

	UCurveFloat* AdjustmentCurve = NewObject<UCurveFloat>();
	if (!AdjustmentCurve->IsValidLowLevelFast())
	{
		if (Settings.ZenBlinkDebug)
		{
			UE_LOG(LogTemp, Error, TEXT("CreatefaceCurve: Failed to create UCurveFloat object."));
		}
		OutCurve.CurveEndTime = 0.0f;
		OutCurve.Curve = nullptr;
		return OutCurve;
	}

	FRichCurve* RichCurve = &AdjustmentCurve->FloatCurve;


	float Delay = 0.0f;
	float Amplitude = 0.0f;
	Duration = FMath::FRandRange(8.0f, Duration);

	FKeyHandle key1 = RichCurve->AddKey(Delay, Amplitude);

	Delay = FMath::FRandRange((Duration / 8), Duration / 9);
	Amplitude = BlinkParams.BlinkAmplitudeMax;
	FKeyHandle key2 = RichCurve->AddKey(Delay, Amplitude);

	Delay = Duration - (Duration / 10);
	Amplitude = BlinkParams.BlinkAmplitudeMax;
	FKeyHandle key3 = RichCurve->AddKey(Delay, Amplitude);

	FKeyHandle key4 = RichCurve->AddKey(Duration, 0.0f);

	RichCurve->SetKeyInterpMode(key1, ERichCurveInterpMode::RCIM_Cubic);
	RichCurve->SetKeyTangentMode(key1, ERichCurveTangentMode::RCTM_Auto);
	RichCurve->SetKeyInterpMode(key2, ERichCurveInterpMode::RCIM_Cubic);
	RichCurve->SetKeyTangentMode(key2, ERichCurveTangentMode::RCTM_Auto);
	RichCurve->SetKeyInterpMode(key3, ERichCurveInterpMode::RCIM_Cubic);
	RichCurve->SetKeyTangentMode(key3, ERichCurveTangentMode::RCTM_Auto);


	if (RichCurve->GetNumKeys() > 0)
	{
		OutCurve.Curve = AdjustmentCurve;
		OutCurve.CurveEndTime = RichCurve->GetLastKey().Time;
	}
	else
	{
		OutCurve.CurveEndTime = 0.0f;
		OutCurve.Curve = nullptr;
		return OutCurve;
	}

	return OutCurve;

}

UCurveFloat* UZenBlinkFunctionLibrary::CreateEmotionChangeCurve(FZenBlinkSettings Settings, float Duration, float StartAlpha, float EndAlpha)
{

	UCurveFloat* AdjustmentCurve = NewObject<UCurveFloat>();
	if (!AdjustmentCurve->IsValidLowLevelFast())
	{
		if (Settings.ZenBlinkDebug)
		{
			UE_LOG(LogTemp, Error, TEXT("ChangeEmotionCurve: Failed to create UCurveFloat object."));
		}
		return nullptr;
	}

	FRichCurve* RichCurve = &AdjustmentCurve->FloatCurve;

	FKeyHandle StartKeyHandle = RichCurve->AddKey(0.0f, EndAlpha);//1
	FKeyHandle MidKeyHandle = RichCurve->AddKey(Duration / 2, StartAlpha);//0
	RichCurve->SetKeyInterpMode(MidKeyHandle, ERichCurveInterpMode::RCIM_Cubic);
	RichCurve->SetKeyTangentMode(MidKeyHandle, ERichCurveTangentMode::RCTM_Auto);
	FKeyHandle EndKeyHandle = RichCurve->AddKey((Duration - (Duration / 10)), EndAlpha); //1
	FKeyHandle EndKeyHandleExt = RichCurve->AddKey(Duration, EndAlpha); //1

	return AdjustmentCurve;

}

FZenBlinkCurrent UZenBlinkFunctionLibrary::CreateBlink(FZenBlinkSettings Settings, const FBlinkParametersAnim& BlinkParams)
{

	// Determine if a double blink should occur based on emotion or randomness
	bool bDoubleBlink = false;
	float DoubleBlinkChance = 0.1f; // 10% chance
	bDoubleBlink = (FMath::FRand() < DoubleBlinkChance);
	int32 NumberOfBlinks = bDoubleBlink ? 2 : 1;

	// Create Curve Structure
	FZenBlinkCurrent BlinkCurves;
	BlinkCurves.CurrentCurve.CurveCurrentTime = 0.0f;
	BlinkCurves.CurrentCurve.CurveEndTime = 0.0f;
	BlinkCurves.CurrentCurve.Curve = nullptr;

	// Create Curve
	BlinkCurves.CurrentCurve = CreateCustomBlinkCurve(Settings, NumberOfBlinks, BlinkParams);

	if (BlinkCurves.CurrentCurve.Curve)
	{

		BlinkCurves.bIsBlinking = true;
		BlinkCurves.CurrentCurve.CurveEndTime = BlinkCurves.CurrentCurve.Curve->FloatCurves[2].GetLastKey().Time;
		BlinkCurves.CurrentCurve.CurveCurrentTime = 0.0f;
	}
	else
	{
		BlinkCurves.bIsBlinking = false;
		BlinkCurves.CurrentCurve.CurveEndTime = 0.0f;
		BlinkCurves.CurrentCurve.Curve = nullptr;
		return BlinkCurves;
	}

	return BlinkCurves;

}


FBlinkParametersAnim UZenBlinkFunctionLibrary::ZenBlinkGenMap(FZenBlinkSettings Settings, UZenBlinkEmotionDataAsset* ZenDataAsset )
{
	FBlinkParametersAnim BlinkParams = FBlinkParametersAnim{
		/* MinBlinkInterval    */ 3.2f,
		/* MaxBlinkInterval    */ 5.5f,
		/* BlinkDurationMin    */ 0.15f,
		/* BlinkDurationMax    */ 0.30f,
		/* BlinkAmplitudeMin   */ 0.30f,
		/* BlinkAmplitudeMax   */ 0.80f,
		/* PostBlinkDelay      */ 0.50f,
		/* EyeMovementDuration */ 3.8f,
		/* EyeMovementRange    */ FVector(6.0f, 4.0f, 4.0f),
		/* HeadEyeMovementWeight */ FVector(0.25f, 0.30f, 0.0f)
	};

	if(ZenDataAsset)
	{

		if (!ZenDataAsset->EmotionBlinkMap.Contains(Settings.Emotion))
		{
			Settings.Emotion = EMetaCharacterEmotion::Neutral; // Fallback
		}

		// Use Find to get a pointer to the BlinkParams
		BlinkParams = *ZenDataAsset->EmotionBlinkMap.Find(Settings.Emotion);

		
	}

	return BlinkParams;
}

FVector UZenBlinkFunctionLibrary::LookAtUpdate(FZenBlinkLookAtSettings LookAtSettings)
{
	FVector Location = FVector(FVector::ZeroVector);

	if (LookAtSettings.TargetToFollowBP)
	{
		Location = LookAtSettings.TargetToFollowBP->GetActorTransform().GetLocation();
	}


	return Location;
}