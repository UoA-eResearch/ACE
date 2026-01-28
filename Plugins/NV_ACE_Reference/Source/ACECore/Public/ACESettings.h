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

#include "Engine/DeveloperSettings.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "ACETypes.h"

#include "ACESettings.generated.h"


UENUM(BlueprintType, DisplayName="Audio2Face-3D Burst Inference Mode")
enum class EAudio2Face3DBurstMode : uint8
{
	/** Use the default Burst mode setting for the current Audio2Face-3D provider */
	Default,
	/** Process Audio2Face-3D inference as quickly as possible */
	ForceBurstMode,
	/** Limit Audio2Face-3D inference processing to real-time */
	ForceRealTimeMode
};


UCLASS(Config = Engine, DefaultConfig, DisplayName = "NVIDIA ACE")
class ACECORE_API UACESettings : public UObject
{
	GENERATED_BODY()

public:

	/* Connection info
	 * Audio2Face-3D server URL to connect to, for example http://203.0.113.37:52000"
	 */
	UPROPERTY(Config,EditAnywhere, Category = "Audio2Face-3D", meta=(DisplayName = "Default A2F-3D Server Config"))
	FACEConnectionInfo ACEConnectionInfo;

	/**
	 * Whether to burst audio to the Audio2Face-3D provider as fast as possible.
	 * It is not recommended to use Burst mode when Audio2Face-3D processing is running on the same system as rendering.
	 * In general the default is to enable Real-time inference mode for most Audio2Face-3D providers.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Audio2Face-3D", meta=(DisplayName = "Inference Burst Mode"))
	EAudio2Face3DBurstMode BurstMode = EAudio2Face3DBurstMode::Default;

	/**
	 * Limits the size of the initial chunk of audio sent to Audio2Face-3D when Real-time inference mode is enabled.
	 * Decreasing this value may reduce any noticable initial hitch when rendering and inference run on the same system.
	 * Increasing this value may reduce animation pauses if inference can't keep up with real-time animation.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Audio2Face-3D", meta=(DisplayName = "Max Initial Audio Chunk Size (Seconds)", EditCondition="BurstMode != EAudio2Face3DBurstMode::ForceBurstMode"))
	float MaxInitialAudioChunkSize = 0.5f;

	UPROPERTY(Config,EditAnywhere, Category = "Animation Stream", meta=(DisplayName = "Animgraph Service Default URL"))
	FString ACEAnimgraphURL;

	/**
	 * Timeout in seconds for remote calls to NVIDIA Animgraph service.
	 * Setting has no effect on timeout to establish initial connection to the NVIDIA Animgraph service.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Animation Stream", meta = (DisplayName = "Animgraph Service RPC Timeout (Seconds)", ClampMin = "0.0", UIMin = "0.0"))
	float ConnectionTimeout = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category = "Animation Stream", meta = (DisplayName = "Animgraph Service Number Of Connection Attempts", ClampMin = "1",UIMin = "1"))
	int32 NumConnectionAttempts = 1;

	UPROPERTY(Config, EditAnywhere, Category = "Animation Stream", meta = (DisplayName = "Animgraph Service Time Between Connection Retries (Seconds)", ClampMin = "0.0",UIMin = "0.0"))
	float TimeBetweenRetrySeconds = 0.1f;

};
