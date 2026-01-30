// Copyright 2025 JOBUTSU LTD. All rights reserved.

#pragma once

// Basics
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Launch/Resources/Version.h"
// Curves
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h" 
#include "Curves/RichCurve.h"
// Actors and components
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
// interfaces and World
#include "Engine/World.h"
#include "UObject/Interface.h"
// Animation
#include "Animation/PoseAsset.h"
#include "Animation/AnimInstance.h"
// Generated
#include "ZenBlink.generated.h"

class UZenBlinkEmotionDataAsset;


UENUM(BlueprintType)
enum class EMetaCharacterEmotion : uint8
{
	Neutral,
	Happy,
	Sad,
	Surprised,
	Fearful,
	Focused,
	Disgusted,
	Childish,
	Tired,
	Annoyed,
	Confused,
	Curious,
	Embarrassed,
	Angry,
	Bored,
	Excited,
	Relaxed,
	Suspicious,
	Proud,
	Pained,
	Nervous,
	Love
};


UENUM(BlueprintType)
enum class EZenBlinkEyeMovement : uint8
{
	None,
	Random,
	FollowTarget
};

USTRUCT(BlueprintType, Category = "ZenBlink")
struct FZenBlinkCurveVector
{
	GENERATED_BODY();

	/** Vector curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	UCurveVector* Curve = nullptr;

	/** Current Time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float CurveCurrentTime = 0.0f;

	/** End Time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float CurveEndTime = 0.0f;

};

USTRUCT(BlueprintType, Category = "ZenBlink")
struct FZenBlinkCurveFloat
{
	GENERATED_BODY();

	/** float curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	UCurveFloat* Curve = nullptr;

	/** Current Time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float CurveCurrentTime = 0.0f;

	/** End Time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float CurveEndTime = 0.0f;

};

USTRUCT(BlueprintType, Category = "ZenBlink")
struct FZenBlinkCurrent
{
	GENERATED_BODY();

	/** Curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	FZenBlinkCurveVector CurrentCurve;

	/** Blinking Status */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	bool bIsBlinking = false;

};


USTRUCT(BlueprintType, Category = "ZenBlink")
struct FBlinkParametersAnim
{
	GENERATED_BODY();
	/** Min Blink Interval - Min Interval between each blink */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float MinBlinkInterval = 1.0f;
	/** Max Blink Interval - Max Interval between each blink */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float MaxBlinkInterval = 2.0f;
	/** Blink Duration Min - Min How long a blink lasts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float BlinkDurationMin = 0.8f;
	/** Blink Duration Max - Min How long a blink lasts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float BlinkDurationMax = 1.0f;
	/** Blink Amplitude Min - Min Blink amount open close */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float BlinkAmplitudeMin = 0.8f;
	/** Blink Amplitude Max - Max Blink amount open close */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float BlinkAmplitudeMax = 1.0f;
	/** Post Blink Delay  - How often blinking occurs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float PostBlinkDelay = 0.3f; 
	/** Eye Movement Duration - The speed of eye and head movements */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float EyeMovementDuration = 3.5f; // eye movement duration
	/** Eye Movement Range - How much head and eyes move on axis xyz */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	FVector EyeMovementRange = FVector(5.0f, 5.0f, 5.0f);  // Range for x, y, z movements
	/** Head EyeMovement Weight - Weight 0-1 of movements x=head y=eyes z=reserved */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	FVector HeadEyeMovementWeight = FVector(5.0f, 5.0f, 5.0f);

};


USTRUCT(Category = "ZenBlink")
struct FZenBlinkCurve
{
	GENERATED_BODY();
	/** INTERNAL USE ONLY **/
	UPROPERTY()
	UCurveVector* CurrentBlinkCurve = nullptr;
	/** INTERNAL USE ONLY **/
	UPROPERTY()
	UCurveFloat* TransitionCurve = nullptr;

	/** INTERNAL USE ONLY **/
	UPROPERTY()
	UCurveFloat* ChangeEmotionCurve = nullptr;

	/** INTERNAL USE ONLY **/
	UPROPERTY()
	UCurveFloat* FaceEmotionCurve = nullptr;

};

USTRUCT(BlueprintType, Category = "ZenBlink")
struct FZenBlinkAnimChannel
{
	GENERATED_BODY()
	/** Blink Channel **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float Blink = 0.0f;
	/** Pupil Channel **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float Pupil = 0.0f;
	/** Eye Channel **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	FTransform Eye;
	/** Target Channel **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	FTransform Target;
	/** Face Channel **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	float Face = 0.0f;
	/** Current Emotion Switch **/
	UPROPERTY(BlueprintReadWrite, Category = "ZenBlink")
	EMetaCharacterEmotion CurrentEmotion = EMetaCharacterEmotion::Neutral;
	/** Current Blink Params **/
	UPROPERTY(BlueprintReadWrite, Category = "ZenBlink")
	FBlinkParametersAnim BlinkParams;

};

USTRUCT(BlueprintType, Category = "ZenBlink")
struct FZenBlinkSettings
{
	GENERATED_BODY()
	/** Enable Auto Blink **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	bool AutoBlink = true;
	/** Enable or Disable Procedural Head Rotation **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	bool UseHeadMovement = false;
	/** Use Baked Head Rotation **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	bool UseBakedHeadMovement = false;
	/** Enable or Disable Procedural Facial Animation **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	bool UseFacialAnimation = true;
	/** Character Emotion **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	EMetaCharacterEmotion Emotion = EMetaCharacterEmotion::Neutral;
	/** Eye Movement Type **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	EZenBlinkEyeMovement EyeMovementType = EZenBlinkEyeMovement::None;
	/** Procedural Head Animation Weight **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float HeadMovementStrength = 1.0f;
	/** Procedural Head Animation Blend **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float HeadMovementBlend = 1.0f;
	/** Procedural Head Animation Speed **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float HeadMovementSpeed = 1.0f;
	/** Procedural Face Animation Weight **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float FaceAnimationWeight = 1.0f;
	/** Blend Face Animation **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float FaceAnimationBlend = 0.75f;
	/** Procedural Animation Weight **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float Weight = 1.0f;
	/** Adjust Eye Aim **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	FVector EyeAimAdjust = FVector::ZeroVector;
	/** Advanced Use for Logging the AnimInstance **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	bool ZenBlinkDebug = false;
	/** Micro Saccadic Weight **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	FVector2D MicroSaccadicWeight = FVector2D(10.0f, 20.0f);
	/** Micro Saccadic Speed **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	FVector2D MicroSaccadicSpeed = FVector2D(1.0f, 2.0f);
	/** Micro Saccadic Enable **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	bool MicroSaccadicEnable = true;
	/** Eye Movement Interpolation Speed **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float EyeMovementInterpSpeed = 10.0f;
	/** Head Movement Interpolation Speed **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float HeadMovementInterpSpeed = 6.0f;
	/** Emotion Blend Speed **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = "ZenBlink")
	float EmotionBlendSpeed = 0.5f;

};

USTRUCT(BlueprintType, Category = "ZenBlink")
struct FZenBlinkLookAtSettings
{
	GENERATED_BODY()
	/** Skeletal Mesh Component **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	/** Target to follow **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	AActor* TargetToFollowBP = nullptr;
	/** Character **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	AActor* MetahumanCharacter = nullptr;
};