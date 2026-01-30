// Copyright 2025 JOBUTSU LTD. All rights reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZenBlink.h"
#include "ZenBlinkDataAsset.h"
#include "ZenBlinkFunctionLibrary.generated.h"



UCLASS()
class UZenBlinkFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    
	/** Create Face Curve */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static FZenBlinkCurveFloat CreateFaceCurve(FZenBlinkSettings Settings, float Duration, float StartAlpha, float EndAlpha, const FBlinkParametersAnim& BlinkParams);
	/** Create Blinks Curve */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static FZenBlinkCurveVector CreateCustomBlinkCurve(FZenBlinkSettings Settings, int32 NumberOfBlinks, const FBlinkParametersAnim& BlinkParams);
	/** Curve Used For Emotion Change*/
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static UCurveFloat* CreateEmotionChangeCurve(FZenBlinkSettings Settings, float Duration, float StartAlpha, float EndAlpha);
	/** Create Blink */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static FZenBlinkCurrent CreateBlink(FZenBlinkSettings Settings, const FBlinkParametersAnim& BlinkParams);
	/**  Get a Float Curve Value */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static float GetCurveFloatValue(UCurveFloat * FCurve_in, float time_in);
	/** Get a Vector Curve Value */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static float GetCurveVectorValue(UCurveVector * VCurve_in, float time_in, int32 Channel);
	/** Returns a list of poses in an asset */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static TArray<FName> GetPoseNames(UPoseAsset* PoseAsset);
	/** Returns Current Emotion */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static EMetaCharacterEmotion GetCurrentEmotion(FZenBlinkSettings Settings);
	/** Create a default Emotion Map */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static TMap<EMetaCharacterEmotion, FBlinkParametersAnim> InitializeEmotionBlinkMap();
	/** Create Emotion Map from data Asset */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static FBlinkParametersAnim ZenBlinkGenMap(FZenBlinkSettings Settings, UZenBlinkEmotionDataAsset* ZenDataAsset);
	/** Update Actor Location */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static FVector LookAtUpdate(FZenBlinkLookAtSettings LookAtSettings);
	/** Test if Emotion has changed */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static bool HasEmotionChanged(EMetaCharacterEmotion CurrentEmotion, EMetaCharacterEmotion NextEmotion);
	/** Set post process blueprint in 5.5 and above */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static void SetPostProcess(bool bEnable, USkeletalMeshComponent* SkeletalMeshComponent, TSubclassOf<UAnimInstance> InPostProcessAnimBlueprint);

	/** Find Face Mesh from name */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static USkeletalMeshComponent* ZenFindFaceMesh(AActor* Metahuman, const FString& FaceMeshName);

	/** Set Focus Target */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static void ZenCineCameraFocus(
		AActor* FocusActor,
		USkeletalMeshComponent* SkeletalMesh,
		const FString& FocusTarget1Name,
		const FString& FocusTarget2Name,
		const FVector& AutoFocusAdjustment,
		bool bAutoFocus
	);

	/** Setup Anim Instance mode and class */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static UClass* ZenSetupAnimClass(USkeletalMeshComponent* FaceMesh, TSubclassOf<UAnimInstance> ZBAnimBP);

	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static void ZenCheckInterface(
		USkeletalMeshComponent* SkeletalMesh,
		TSubclassOf<UAnimInstance> ZBAnimClass,
		TSubclassOf<UInterface> InterfaceClass,
		UAnimInstance*& OutAnimInstance,
		bool& bImplementsInterface);

	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static void ZenCheckPostInterface(
		USkeletalMeshComponent* SkeletalMesh,
		TSubclassOf<UAnimInstance> ZBAnimClass,
		TSubclassOf<UInterface> InterfaceClass,
		UAnimInstance*& OutAnimInstance,
		bool& bImplementsInterface);

	/** Generates a random vector within a given radius around a location */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static FVector ZenGenerateRandomLocation(AActor* Actor, float Range, float ConeHalfAngleDeg);

	/** Ensure the target is always in front of the character */
	UFUNCTION(BlueprintCallable, Category = "ZenBlink")
	static FVector ZenGetClampedForwardDirection(
		USkeletalMeshComponent* MeshComp,
		AActor* TargetActor
	);


};
