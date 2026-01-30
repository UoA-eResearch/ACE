// Copyright 2025 JOBUTSU LTD. All rights reserved.

#pragma once

#include "ZenBlink.h"
#include "ZenBlinkWorldActor.generated.h"

/**
 * ZenBlink Actor Parent
 */
UCLASS(Blueprintable, BlueprintType)
class ZENBLINK_API AZenBlinkWorldActor : public AActor
{
	GENERATED_BODY()

public:

	AZenBlinkWorldActor();

	/** The Metahuman In The Scene */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metahuman", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<AActor> Metahuman = nullptr;

	/** Turn Camera Auto Focus On or Off */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraFocus", Interp, meta = (MakeStructureDefaultValue = "true", ExposeOnSpawn = "true"))
	bool AutoFocus = true;

	/** Choose The Actor to Focus Camera On (MUST BE A BLANK ACTOR) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraFocus", Interp, meta = (ExposeOnSpawn = "true"))
	TObjectPtr<AActor> CameraFocusTarget = nullptr;

	/** Target Actor To Follow */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TargetFollowing", Interp, meta = (ExposeOnSpawn = "true"))
	TObjectPtr<AActor> TargetToFollow = nullptr;

	/** Enable Auto Blink */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Blinks", Interp, meta = (MakeStructureDefaultValue = "true", ExposeOnSpawn = "true"))
	bool AutoBlink = true;

	/** Adjust the focus point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "CameraFocus", Interp, meta = (ExposeOnSpawn = "true"))
	FVector AutoFocusAdjustment = FVector(0.0, 0.0, 0.0);

	/** Character Emotion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Emotion", Interp, meta = (ExposeOnSpawn = "true"))
	EMetaCharacterEmotion Emotion = EMetaCharacterEmotion::Neutral;

	/** Eye Movement Type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MovementMode", Interp, meta = (ExposeOnSpawn = "true"))
	EZenBlinkEyeMovement MovementType = EZenBlinkEyeMovement::Random;

	/** Enable or Disable Procedural Head Rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Head", Interp, meta = (MakeStructureDefaultValue = "true", ExposeOnSpawn = "true"))
	bool UseHeadMovement = true;

	/** Adjust Head movement Strength */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Head", Interp, meta = (MakeStructureDefaultValue = "1.0", ExposeOnSpawn = "true", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float HeadMovementStrength = 1.0f;

	/** Adjust Head Animation Blend */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Head", Interp, meta = (MakeStructureDefaultValue = "1.0", ExposeOnSpawn = "true", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float HeadMovementBlend = 1.0f;

	/** Speed of Head Movement in Random Movement Mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Head", Interp, meta = (MakeStructureDefaultValue = "1.0", UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "10"))
	double HeadMovementRandomSpeed = 1.0f;

	/** Enable or Disable Procedural Face Emotion Animation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Face", Interp, meta = (MakeStructureDefaultValue = "true", ExposeOnSpawn = "true"))
	bool FaceAnimation = true;

	/** Adjust Face Emotion Animation Strength */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Face", Interp, meta = (MakeStructureDefaultValue = "0.66", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	double FaceEmotionStrength = 0.8;

	/** Blend Between DefaultSlot and ZenBlink Animation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Face", Interp, meta = (MakeStructureDefaultValue = "0.55", ExposeOnSpawn = "true", UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float FaceAnimationBlend = 0.77f;

	/** Emotion maps control speeds and timing of blinking and eye movement */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Emotion")
	uint8 EmotionMap = 0;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	bool isUpdating = false;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink", meta = (MakeStructureDefaultValue = "Body", MultiLine = "true"))
	FString BodyMeshName = "Body";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink", meta = (MakeStructureDefaultValue = "Face", MultiLine = "true"))
	FString FaceMeshName = "Face";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	bool IsSetupCompleted = false;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	TObjectPtr<UClass> MetahumanAnimationBlueprint = nullptr;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	TSubclassOf<UAnimInstance> ZenBlinkAnimationBP = nullptr;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	TObjectPtr<UAnimInstance> InternalFaceAnimClass = nullptr;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink", meta = (MakeStructureDefaultValue = "FACIAL_L_Pupil"))
	FName FocusTarget1 = "FACIAL_L_Pupil";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink", meta = (MakeStructureDefaultValue = "FACIAL_R_Pupil"))
	FName FocusTarget2 = "FACIAL_R_Pupil";

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Default")
	TObjectPtr<USkeletalMeshComponent> FaceMesh = nullptr;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	TObjectPtr<AActor> CurrentActor = nullptr;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Default")
	TObjectPtr<USkeletalMeshComponent> BodyMesh = nullptr;

	/** The Emotion Map Asset Used For ZenBlink */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UZenBlinkEmotionDataAsset> CustomEmotionMap = nullptr;

	/** The Speed at which Emotions Change */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Emotion", Interp, meta = (MakeStructureDefaultValue = "0.25", UIMin = "0.1", UIMax = "5", ClampMin = "0.1", ClampMax = "5", ForceUnits = "Seconds"))
	double EmotionBlendSpeed = 0.25;

	/** Interpolation Speed for Eye Movements */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Head", Interp, meta = (MakeStructureDefaultValue = "6.0", ExposeOnSpawn = "true", UIMin = "0", UIMax = "50", ClampMin = "0", ClampMax = "50"))
	float HeadMovementInterpolationSpeed = 6.0f;

	/** Use ZenBlink Baked Animation Movement Curves */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Head", Interp, meta = (MakeStructureDefaultValue = "false", ExposeOnSpawn = "true"))
	bool UseBakedHeadMovement = false;

	/** Enable Or Disable Saccadic Movement */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Eyes", Interp, meta = (MakeStructureDefaultValue = "true", ExposeOnSpawn = "true"))
	bool MicroSaccadicEnable = true;

	/** Adjustment for Micro Saccadic Movement Speed Left-Right and Up-Down */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Eyes", Interp, meta = (ExposeOnSpawn = "true", UIMin = "0", UIMax = "10", ClampMin = "0", ClampMax = "10"))
	FVector2D MicroSaccadicSpeed = FVector2D(2.0f, 1.5f);

	/** Adjustment for Micro Saccadic Movement Strength Left-Right and Up-Down */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Eyes", Interp, meta = (ExposeOnSpawn = "true", UIMin = "0", UIMax = "50", ClampMin = "0", ClampMax = "50"))
	FVector2D MicroSaccadicStrength = FVector2D(20.0f, 10.0f);

	/** Interpolation Speed for Eye Movements */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Eyes", Interp, meta = (MakeStructureDefaultValue = "4.0", ExposeOnSpawn = "true", UIMin = "0", UIMax = "50", ClampMin = "0", ClampMax = "50"))
	float EyeMovementInterpolationSpeed = 4.0f;

	/** Adjust As Required to Ensure Eyes look At Target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Eyes", Interp, meta = (ExposeOnSpawn = "true", UIMin = "-100", UIMax = "100", ClampMin = "-100", ClampMax = "100"))
	FVector EyeAimAdjustment = FVector(0.0f, 0.0f, 0.0f);

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	FBlinkParametersAnim BlinkParams;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	FZenBlinkSettings SettingsBP;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	FZenBlinkLookAtSettings LookatSettingsBP;

	/** ZenBlink OverAll Animation Weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global", Interp, meta = (MakeStructureDefaultValue = "1.0", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	double ZenBlinkStrength = 1.0;

	/** Use ZenBlink PostProcess In UE 5.5+ */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global", meta = (MakeStructureDefaultValue = "true", ExposeOnSpawn = "true"))
	bool UseZenBlinkPostProcess = true;

	/** Advanced Use for Logging the AnimInstance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global", meta = (MakeStructureDefaultValue = "true", ExposeOnSpawn = "true"))
	bool Debug = false;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	TSubclassOf<UAnimInstance> ZenBlinkPostProcessBP = nullptr;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "ZenBlink")
	TSubclassOf<UInterface> ZenInterfaceClass = nullptr;



};
