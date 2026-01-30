// Copyright 2025 JOBUTSU LTD. All rights reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "ZenBlink.h"
#include "ZenBlinkDataAsset.generated.h"

/**
 * Data asset to hold the blink parameters for each emotion.
 */
UCLASS(BlueprintType, Category = "ZenBlink")
class UZenBlinkEmotionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Blink Parameters Map data Asset **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZenBlink")
	TMap<EMetaCharacterEmotion, FBlinkParametersAnim> EmotionBlinkMap;

	UZenBlinkEmotionDataAsset();
};

