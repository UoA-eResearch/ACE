// Copyright 2025 JOBUTSU LTD. All rights reserved.

#include "ZenBlinkDataAsset.h"

UZenBlinkEmotionDataAsset::UZenBlinkEmotionDataAsset()
{

	// Neutral: fairly standard blink rate, moderate amplitude, moderate eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Neutral, FBlinkParametersAnim{
		/* MinBlinkInterval    */ 3.2f,
		/* MaxBlinkInterval    */ 5.5f,
		/* BlinkDurationMin    */ 0.15f,
		/* BlinkDurationMax    */ 0.30f,
		/* BlinkAmplitudeMin   */ 0.30f,
		/* BlinkAmplitudeMax   */ 1.0f,
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
		4.5f, 6.0f, 0.25f, 0.45f, 0.40f, 1.0f, 0.70f, 5.0f,
		FVector(6.0f, 6.0f, 4.0f), FVector(0.30f, 0.30f, 0.0f)
		});

	// Surprised: relatively frequent, very short blinks (quick eyelid flutter), wide eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Surprised, FBlinkParametersAnim{
		1.3f, 2.2f, 0.05f, 0.12f, 0.60f, 1.0f, 0.25f, 2.0f,
		FVector(8.0f, 8.0f, 5.0f), FVector(0.60f, 0.70f, 0.0f)
		});

	// Fearful: short intervals (nervous blinking), moderate blink duration, large eye range
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Fearful, FBlinkParametersAnim{
		1.2f, 2.2f, 0.20f, 0.35f, 0.50f, 1.0f, 0.30f, 1.3f,
		FVector(12.0f, 12.0f, 4.0f), FVector(0.65f, 0.95f, 0.0f)
		});

	// Focused: long intervals (rare blinking), shorter amplitude, narrow eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Focused, FBlinkParametersAnim{
		7.0f, 10.0f, 0.25f, 0.40f, 0.50f, 1.0f, 0.50f, 2.5f,
		FVector(2.0f, 2.0f, 2.0f), FVector(0.20f, 0.25f, 0.0f)
		});

	// Disgusted: moderate intervals, fairly quick but somewhat forceful blinks
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Disgusted, FBlinkParametersAnim{
		3.0f, 5.0f, 0.20f, 0.35f, 0.50f, 1.0f, 0.40f, 2.5f,
		FVector(5.0f, 5.0f, 8.0f), FVector(0.40f, 0.50f, 0.0f)
		});

	// Childish: fast movements
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Childish, FBlinkParametersAnim{
		0.8f, 1.8f, 0.05f, 0.15f, 0.6f, 1.0f, 0.4f, 1.0f,
		FVector(10.0f, 10.0f, 10.0f), FVector(0.6f, 0.95f, 0.0f)
		});

	// Tired: long intervals, slow and heavier blinks, moderate movement range
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Tired, FBlinkParametersAnim{
		6.0f, 9.0f, 0.45f, 0.70f, 0.60f, 1.0f, 0.90f, 4.5f,
		FVector(6.0f, 6.0f, 4.0f), FVector(0.15f, 0.20f, 0.0f)
		});

	// Annoyed: moderately frequent, somewhat snappy blinks
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Annoyed, FBlinkParametersAnim{
		2.0f, 3.5f, 0.10f, 0.30f, 0.60f, 1.0f, 0.40f, 2.0f,
		FVector(4.0f, 4.0f, 9.0f), FVector(0.35f, 0.55f, 0.0f)
		});

	// Confused: mid-range intervals, moderate blink, some extra wandering in the eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Confused, FBlinkParametersAnim{
		2.8f, 5.0f, 0.15f, 0.35f, 0.40f, 1.0f, 0.45f, 2.5f,
		FVector(10.0f, 10.0f, 6.0f), FVector(0.50f, 0.70f, 0.0f)
		});

	// Curious: more frequent blinking, moderate range, slightly faster eye movements
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Curious, FBlinkParametersAnim{
		1.2f, 2.4f, 0.08f, 0.25f, 0.70f, 1.0f, 0.35f, 2.5f,
		FVector(6.0f, 6.0f, 8.0f), FVector(0.50f, 0.80f, 0.0f)
		});

	// Embarrassed: somewhat frequent blinks, gentle closure, maybe looking downward
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Embarrassed, FBlinkParametersAnim{
		2.0f, 3.5f, 0.20f, 0.40f, 0.60f, 1.0f, 0.70f, 3.0f,
		FVector(8.0f, 8.0f, 1.0f), FVector(0.60f, 0.75f, 0.0f)
		});

	// Angry: shorter interval (irritated, frequent blinks), strong closure
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Angry, FBlinkParametersAnim{
		1.8f, 3.5f, 0.15f, 0.30f, 0.70f, 1.0f, 0.30f, 2.0f,
		FVector(10.0f, 10.0f, 7.0f), FVector(0.50f, 0.45f, 0.0f)
		});

	// Bored: longer intervals, lazy, slower blink, reduced eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Bored, FBlinkParametersAnim{
		5.0f, 8.0f, 0.35f, 0.60f, 0.50f, 1.0f, 0.70f, 4.5f,
		FVector(8.0f, 8.0f, 4.0f), FVector(0.20f, 0.20f, 0.0f)
		});

	// Excited: very frequent, quick, big blinks, lots of scanning
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Excited, FBlinkParametersAnim{
		0.8f, 1.8f, 0.05f, 0.18f, 0.85f, 1.0f, 0.20f, 1.0f,
		FVector(10.0f, 10.0f, 4.0f), FVector(0.65f, 0.90f, 0.0f)
		});

	// Relaxed: moderate intervals, easy-going blink speeds, smaller amplitude range
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Relaxed, FBlinkParametersAnim{
		3.0f, 5.5f, 0.15f, 0.35f, 0.60f, 1.0f, 0.60f, 4.5f,
		FVector(7.0f, 7.0f, 3.0f), FVector(0.25f, 0.25f, 0.0f)
		});

	// Suspicious: scanning eyes, moderate frequency with quick, partial blinks
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Suspicious, FBlinkParametersAnim{
		2.5f, 5.0f, 0.15f, 0.30f, 0.60f, 1.0f, 0.60f, 1.0f,
		FVector(10.0f, 10.0f, 2.0f), FVector(0.40f, 0.90f, 0.0f)
		});

	// Proud: moderate frequency, relatively confident (larger amplitude)
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Proud, FBlinkParametersAnim{
		2.2f, 4.2f, 0.10f, 0.25f, 0.70f, 1.0f, 0.40f, 3.0f,
		FVector(6.0f, 6.0f, 8.0f), FVector(0.30f, 0.25f, 0.0f)
		});

	// Pained: slower blinks, heavier closure, tension in the eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Pained, FBlinkParametersAnim{
		4.5f, 6.5f, 0.35f, 0.55f, 0.50f, 1.0f, 0.80f, 2.0f,
		FVector(3.0f, 3.0f, 3.0f), FVector(0.35f, 0.25f, 0.0f)
		});

	// Nervous: short intervals, fast blinks, darting eyes
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Nervous, FBlinkParametersAnim{
		0.8f, 2.2f, 0.10f, 0.28f, 0.50f, 1.0f, 0.40f, 1.0f,
		FVector(8.0f, 8.0f, 3.0f), FVector(0.60f, 0.85f, 0.0f)
		});

	// Love: gentle, warm blinks, moderate frequency, soft eye movement
	EmotionBlinkMap.Add(EMetaCharacterEmotion::Love, FBlinkParametersAnim{
		2.0f, 4.0f, 0.10f, 0.30f, 0.80f, 1.0f, 0.50f, 3.5f,
		FVector(9.0f, 9.0f, 9.0f), FVector(0.30f, 0.60f, 0.0f)
		});
}
