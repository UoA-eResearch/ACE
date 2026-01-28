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

#include "ACETypes.h"


bool FAudio2FaceEmotion::IsEmotionOverrideActive() const
{
	// Emotion overrides need to be enabled, and the strength has to be non-zero, and at least one override has to be active
	return bEnableEmotionOverride &&
		(EmotionOverrideStrength > UE_KINDA_SMALL_NUMBER) &&
		(EmotionOverrides.bOverrideAmazement ||
			EmotionOverrides.bOverrideAnger ||
			EmotionOverrides.bOverrideCheekiness ||
			EmotionOverrides.bOverrideDisgust ||
			EmotionOverrides.bOverrideFear ||
			EmotionOverrides.bOverrideGrief ||
			EmotionOverrides.bOverrideJoy ||
			EmotionOverrides.bOverrideOutOfBreath ||
			EmotionOverrides.bOverridePain ||
			EmotionOverrides.bOverrideSadness);
}

bool FACEConnectionInfo::operator==(const FACEConnectionInfo& Other) const
{
	return DestURL == Other.DestURL &&
		   APIKey == Other.APIKey &&
		   NvCFFunctionId == Other.NvCFFunctionId &&
		   NvCFFunctionVersion == Other.NvCFFunctionVersion;
}

bool FACEConnectionInfo::operator!=(const FACEConnectionInfo& Other) const
{
	return	DestURL != Other.DestURL ||
			APIKey != Other.APIKey ||
			NvCFFunctionId != Other.NvCFFunctionId ||
			NvCFFunctionVersion != Other.NvCFFunctionVersion;
}

