/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "GPTLocalAsyncSendTextToGPT.h"

#include "GPTLocalModule.h"
#include "GPTLocal.h"
#include "GPTLocalPrivate.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"

std::atomic<bool> UGPTLocalAsyncSendTextToGPT::IsRunning = false;

UGPTLocalAsyncSendTextToGPT* UGPTLocalAsyncSendTextToGPT::AsyncSendTextToGPT(const FString& Prompt)
{
	if (IsRunning)
	{
		UE_LOG(LogACEGPTLocal, Log, TEXT("%s: GPT is already running! Request was ignored."), ANSI_TO_TCHAR(__FUNCTION__));
		return nullptr;
	}

	UGPTLocalAsyncSendTextToGPT* BlueprintNode = NewObject<UGPTLocalAsyncSendTextToGPT>();
	BlueprintNode->Input = Prompt;
	BlueprintNode->AddToRoot();

	return BlueprintNode;
}

void UGPTLocalAsyncSendTextToGPT::Activate()
{
	if (Input.IsEmpty())
	{
		UE_LOG(LogACEGPTLocal, Log, TEXT("%s: GPT called with empty input prompt!"), ANSI_TO_TCHAR(__FUNCTION__));
	}
	else
	{
		IsRunning = true;

		UE_LOG(LogACEGPTLocal, Log, TEXT("%s: sending to GPT: %s"), ANSI_TO_TCHAR(__FUNCTION__), *Input);
		
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]()
			{
				FString result;
				FGPTLocal* GPTLocal{ FGPTLocal::Get() };
				if (GPTLocal != nullptr)
				{
					result = GPTLocal->ExecuteGpt(Input);

					AsyncTask(ENamedThreads::GameThread, [this, result]()
						{
							OnResponse.Broadcast(result);
						});

					UE_LOG(LogACEGPTLocal, Log, TEXT("%s: response from GPT: %s"), ANSI_TO_TCHAR(__FUNCTION__), *result);
				}

				IsRunning = false;
				this->RemoveFromRoot();
			});
	}
}
