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

#include "Kismet/BlueprintAsyncActionBase.h"

#include <atomic>
#include "GPTLocalAsyncSendTextToGPT.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncSendTextToGPTOutputPin, FString, Response);

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class GPTLOCAL_API UGPTLocalAsyncSendTextToGPT : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "ACE|GPTLocal", meta = (DisplayName = "Send text to GPT (Async)", BlueprintInternalUseOnly = "true"))
	static UGPTLocalAsyncSendTextToGPT* AsyncSendTextToGPT(const FString& Prompt);

	UPROPERTY(BlueprintAssignable)
	FAsyncSendTextToGPTOutputPin OnResponse;

	UPROPERTY(BlueprintReadOnly, Category = "ACE|GPTLocal", meta = (BBlueprintInternalUseOnly = "true"))
	FString Input;

private:
	virtual void Activate() override;

	static std::atomic<bool> IsRunning;
};
