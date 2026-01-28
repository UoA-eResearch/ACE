/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "CoreMinimal.h"
#include "LiveLinkSourceSettings.h"
#include "OmniverseLiveLinkSourceSettings.generated.h"

/** Class for Omniverse live link source settings */
UCLASS()
class UOmniverseLiveLinkSourceSettings : public ULiveLinkSourceSettings
{
public:
	GENERATED_BODY()
	/**  Milliseconds delay to wait before playing blendshapes animation (Burst mode only) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = 0, ClampMax = 1000))
	uint32 AnimationDelayTime = 150;

	/**  Milliseconds delay to wait before playing audio. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = 0, ClampMax = 1000))
	uint32 AudioDelayTime = 0;

};
