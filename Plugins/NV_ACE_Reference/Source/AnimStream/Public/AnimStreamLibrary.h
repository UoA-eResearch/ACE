/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "Kismet/BlueprintFunctionLibrary.h"

#include "AnimStreamLibrary.generated.h"


UCLASS()
class ANIMSTREAM_API UAnimStreamLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Override the destination URL for the NVIDIA Animgraph service.
	 * The new URL will be used for any new connections to the NVIDIA Animgraph service.
	 * Existing streams will continue uninterrupted.
	 * A non-empty string overrides the project default setting, empty strings restore the project default setting.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|AnimStream")
	static void OverrideAnimStreamConnection(const FString& ACEAnimgraphURL);

	/**
	 * Get the current destination URL for new NVIDIA Animgraph service connections.
	 * Takes into account the project default settings and any runtime connection info overrides.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACE|AnimStream")
	static void GetAnimStreamConnection(FString& ACEAnimgraphURL);
};

