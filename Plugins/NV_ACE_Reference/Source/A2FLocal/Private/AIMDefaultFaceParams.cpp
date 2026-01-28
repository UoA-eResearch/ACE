/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

// plugin includes
#include "A2FLocal.h"
#include "A2FLocalPrivate.h"

// engine includes
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"


TMap<FString, float> GetDefaultFaceParams30(FString A2F3DModelDir)
{
	TMap<FString, float> FaceParams{};

	// parse model config file path from global config file
	FString ModelFile{};
	{
		FString ConfigPath = FPaths::Combine(A2F3DModelDir, "data", "a2f_config.json");
		if (!FPaths::FileExists(ConfigPath))
		{
			UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to find config file %s, face parameter default values won't be loaded"), *ConfigPath);
			return FaceParams;
		}

		FString ConfigJsonString;
		if (!FFileHelper::LoadFileToString(ConfigJsonString, *ConfigPath))
		{
			UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to load config file %s, face parameter default values won't be loaded"), *ConfigPath);
			return FaceParams;
		}

		TSharedRef<TJsonReader<>> ConfigReader = TJsonReaderFactory<>::Create(ConfigJsonString);
		TSharedPtr<FJsonObject> ConfigJson;
		if (!FJsonSerializer::Deserialize(ConfigReader, ConfigJson) || !ConfigJson.IsValid())
		{
			UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to parse config file %s, face parameter default values won't be loaded"), *ConfigPath);
			return FaceParams;
		}

		if (!ConfigJson->TryGetStringField(TEXT("modelConfigPath"), ModelFile))
		{
			UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to find \"modelConfigPath\" in config file % s, face parameter default values won't be loaded"), *ConfigPath);
			return FaceParams;
		}
	}

	// parse model config file
	FString ModelConfigPath = FPaths::Combine(A2F3DModelDir, "data", ModelFile);
	if (!FPaths::FileExists(ModelConfigPath))
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to find model config file %s, face parameter default values won't be loaded"), *ModelConfigPath);
		return FaceParams;
	}

	FString ModelConfigJsonString;
	if (!FFileHelper::LoadFileToString(ModelConfigJsonString, *ModelConfigPath))
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to load model config file %s, face parameter default values won't be loaded"), *ModelConfigPath);
		return FaceParams;
	}

	TSharedRef<TJsonReader<>> ModelConfigReader = TJsonReaderFactory<>::Create(ModelConfigJsonString);
	TSharedPtr<FJsonObject> ModelConfigJson;
	if (!FJsonSerializer::Deserialize(ModelConfigReader, ModelConfigJson) || !ModelConfigJson.IsValid())
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to parse model config file %s, face parameter default values won't be loaded"), *ModelConfigPath);
		return FaceParams;
	}

	const TSharedPtr<FJsonObject>* ConfigFieldJson = nullptr;
	if (!ModelConfigJson->TryGetObjectField(TEXT("config"), ConfigFieldJson) || (ConfigFieldJson == nullptr) || !ConfigFieldJson->IsValid())
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to find \"config\" in model config file %s, face parameter default values won't be loaded"), *ModelConfigPath);
		return FaceParams;
	}

	const TMap<FString, FString> UnrealFromAIMParamNames = {
		{"lower_face_smoothing", "lowerFaceSmoothing"},
		{"upper_face_smoothing", "upperFaceSmoothing"},
		{"lower_face_strength", "lowerFaceStrength"},
		{"upper_face_strength", "upperFaceStrength"},
		{"face_mask_level", "faceMaskLevel"},
		{"face_mask_softness", "faceMaskSoftness"},
		{"skin_strength", "skinStrength"},
		{"blink_strength", "blinkStrength"},
		{"eyelid_open_offset", "eyelidOpenOffset"},
		{"lip_open_offset", "lipOpenOffset"},
		{"tongue_strength", "tongueStrength"},
		{"tongue_height_offset", "tongueHeightOffset"},
		{"tongue_depth_offset", "tongueDepthOffset"},
		{"input_strength", "inputStrength"},
		{"blink_offset", "blinkOffset"}
	};

	for (const TPair<FString, FString>& KV : UnrealFromAIMParamNames)
	{
		float DefaultVal = 0.0f;
		if (ConfigFieldJson->Get()->TryGetNumberField(KV.Key, DefaultVal))
		{
			FaceParams.Add(KV.Value, DefaultVal);
			UE_LOG(LogACEA2FLocal, VeryVerbose, TEXT("found default %s = %f"), *KV.Key, DefaultVal);
		}
	}

	if (!FaceParams.IsEmpty())
	{
		// work around AIM bug where there is no blinkOffset default
		if (!FaceParams.Contains("blinkOffset"))
		{
			FaceParams.Add("blinkOffset", 0.0f);
		}
	}

	return FaceParams;
}

TMap<FString, float> GetDefaultFaceParams23(FString A2F3DModelDir)
{
	TMap<FString, float> FaceParams{};

	// parse global config file
	FString ConfigPath = FPaths::Combine(A2F3DModelDir, "data", "config.json");
	if (!FPaths::FileExists(ConfigPath))
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to find config file %s, face parameter default values won't be loaded"), *ConfigPath);
		return FaceParams;
	}

	FString ConfigJsonString;
	if (!FFileHelper::LoadFileToString(ConfigJsonString, *ConfigPath))
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to load config file %s, face parameter default values won't be loaded"), *ConfigPath);
		return FaceParams;
	}

	TSharedRef<TJsonReader<>> ConfigReader = TJsonReaderFactory<>::Create(ConfigJsonString);
	TSharedPtr<FJsonObject> ConfigJson;
	if (!FJsonSerializer::Deserialize(ConfigReader, ConfigJson) || !ConfigJson.IsValid())
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to parse config file %s, face parameter default values won't be loaded"), *ConfigPath);
		return FaceParams;
	}

	const TSharedPtr<FJsonObject>* ConfigFieldJson = nullptr;
	if (!ConfigJson->TryGetObjectField(TEXT("config"), ConfigFieldJson) || (ConfigFieldJson == nullptr) || !ConfigFieldJson->IsValid())
	{
		UE_LOG(LogACEA2FLocal, Warning, TEXT("Unable to find \"config\" in config file %s, face parameter default values won't be loaded"), *ConfigPath);
		return FaceParams;
	}

	const TMap<FString, FString> UnrealFromAIMParamNames = {
		{"lower_face_smoothing", "lowerFaceSmoothing"},
		{"upper_face_smoothing", "upperFaceSmoothing"},
		{"lower_face_strength", "lowerFaceStrength"},
		{"upper_face_strength", "upperFaceStrength"},
		{"face_mask_level", "faceMaskLevel"},
		{"face_mask_softness", "faceMaskSoftness"},
		{"skin_strength", "skinStrength"},
		{"blink_strength", "blinkStrength"},
		{"eyelid_open_offset", "eyelidOpenOffset"},
		{"lip_open_offset", "lipOpenOffset"},
		{"tongue_strength", "tongueStrength"},
		{"tongue_height_offset", "tongueHeightOffset"},
		{"tongue_depth_offset", "tongueDepthOffset"},
		{"input_strength", "inputStrength"},
		{"blink_offset", "blinkOffset"}
	};

	for (const TPair<FString, FString>& KV : UnrealFromAIMParamNames)
	{
		float DefaultVal = 0.0f;
		if (ConfigFieldJson->Get()->TryGetNumberField(KV.Key, DefaultVal))
		{
			FaceParams.Add(KV.Value, DefaultVal);
			UE_LOG(LogACEA2FLocal, VeryVerbose, TEXT("found default %s = %f"), *KV.Key, DefaultVal);
		}
	}

	if (!FaceParams.IsEmpty())
	{
		// work around AIM bug where there is no blinkOffset default
		if (!FaceParams.Contains("blinkOffset"))
		{
			FaceParams.Add("blinkOffset", 0.0f);
		}
	}

	return FaceParams;
}
