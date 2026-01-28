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

#include "Audio2FaceParameters.h"


void UAudio2FaceParameters::SetParameter(const FString& ParamName, float ParamValue)
{
	Audio2FaceParameterMap.FindOrAdd(ParamName, ParamValue);
}

void UAudio2FaceParameters::ClearParameter(const FString& ParamName)
{
	Audio2FaceParameterMap.Remove(ParamName);
}

void UAudio2FaceParameters::BatchSetParameters(const TMap<FString, float>& InParameterMap, bool bReplaceCurrentParams)
{
	if (bReplaceCurrentParams)
	{
		Audio2FaceParameterMap = TMap<FString, float>();
	}

	for (auto Parameter : InParameterMap)
	{
		SetParameter(Parameter.Key, Parameter.Value);
	}
}


void UAudio2FaceParameters::SetParametersFromStruct(const FAudio2FaceParameterHelper& ParameterHelper, bool bReplaceCurrentParams)
{
	BatchSetParameters(ParameterHelper.GetParameterMap(),bReplaceCurrentParams);
}


TMap<FString, float> FAudio2FaceParameterHelper::GetParameterMap() const
{
	return
	{
		// these names are defined in the A2F-3D protocol description
		{ "skinStrength", SkinStrength },
		{ "upperFaceStrength", UpperFaceStrength },
		{ "lowerFaceStrength", LowerFaceStrength },
		{ "eyelidOpenOffset", EyelidOpenOffset },
		{ "blinkStrength", BlinkStrength },
		{ "lipOpenOffset", LipOpenOffset },
		{ "upperFaceSmoothing", UpperFaceSmoothing },
		{ "lowerFaceSmoothing", LowerFaceSmoothing },
		{ "faceMaskLevel", FaceMaskLevel },
		{ "faceMaskSoftness", FaceMaskSoftness },
		{ "tongueStrength", TongueStrength },
		{ "tongueHeightOffset", TongueHeightOffset },
		{ "tongueDepthOffset", TongueDepthOffset },
	};
}
