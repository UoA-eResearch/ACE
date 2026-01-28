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

#include "GPTLocal.h"

 // engine includes
#include "Containers/StringConv.h"

// plugin includes
#include "GPTLocalModule.h"
#include "GPTLocalPrivate.h"
#include "ACESettings.h"
#include "ACETypes.h"
#include "AIMModule.h"
#include "nvaim.h"
#include "nvaim_ai.h"
#include "nvaim_cuda.h"
#include "nvaim_gpt.h"

#include <condition_variable>
#include <thread>
#include <mutex>

static const FName GLocalGPTProviderName = FName(TEXT("LocalGPT"));

namespace
{
	constexpr const char* const GGUF_MODEL_MINITRON	{ "{8E31808B-C182-4016-9ED8-64804FF5B40D}" };

	// TODO: read the value dynamically using getCapsAndRequirements. The value 4000 came from nvaim.model.config.json
	constexpr std::size_t VRAM_BUDGET_RECOMMENDATION{ 4000 };
	constexpr std::size_t THREAD_NUM_RECOMMENDATION{ 8 };
	constexpr std::size_t CONTEXT_SIZE_RECOMMENDATION{ 4096 };
}

FGPTLocal::FGPTLocal()
{
#if PLATFORM_WINDOWS
	FAIMModule::Get().RegisterAIMFeature(nvaim::plugin::gpt::ggml::cuda::kId, {}, {});
	bIsFeatureAvailable = FAIMModule::Get().IsAIMFeatureAvailable(nvaim::plugin::gpt::ggml::cuda::kId);
	if (bIsFeatureAvailable)
	{
		nvaim::Result Result = FAIMModule::Get().LoadAIMFeature(nvaim::plugin::gpt::ggml::cuda::kId, &Interface, true);
		if (Result != nvaim::ResultOk)
		{
			UE_LOG(LogACEGPTLocal, Log, TEXT("Unable to load AIM gpt.ggml.cuda feature: %s"), *GetAIMStatusString(Result));
			Interface = nullptr;
			return;
		}

		nvaim::CommonCreationParameters common{};
		auto ConvertedString = StringCast<UTF8CHAR>(*FAIMModule::Get().GetModelDirectory());
		common.utf8PathToModels = reinterpret_cast<const char*>(ConvertedString.Get());
		common.numThreads = THREAD_NUM_RECOMMENDATION;
		common.vramBudgetMB = VRAM_BUDGET_RECOMMENDATION;
		common.modelGUID = GGUF_MODEL_MINITRON;

		nvaim::GPTCreationParameters params{};
		params.common = &common;
		params.contextSize = CONTEXT_SIZE_RECOMMENDATION;

		auto CiG = FAIMModule::Get().GetCIGCudaParameters();
		if (CiG)
		{
			// Optimal performance with Compute In Graphics
			params.chain(*CiG);
		}


		Result = Interface->createInstance(params, &Instance);
		if (Result != nvaim::ResultOk)
		{
			UE_LOG(LogACEGPTLocal, Log, TEXT("Unable to create AIM gpt.ggml.cuda instance: %s"), *GetAIMStatusString(Result));
			Instance = nullptr;
		}
	}
	else
	{
		UE_LOG(LogACEGPTLocal, Log, TEXT("Unable to load AIM gpt.ggml.cuda feature, %s provider won't be available"), *GLocalGPTProviderName.ToString());
	}
#else
	UE_LOG(LogACEGPTLocal, Log, TEXT("%s provider is not supported on the current platform"), *GLocalGPTProviderName.ToString());
#endif
}

FGPTLocal::~FGPTLocal()
{
	if (Interface != nullptr)
	{
		if (Instance != nullptr)
		{
			Interface->destroyInstance(Instance);
			Instance = nullptr;
		}

		// unload feature interface
		
		nvaim::Result Result = FAIMModule::Get().UnloadAIMFeature(nvaim::plugin::gpt::ggml::cuda::kId, Interface);
		if (Result != nvaim::ResultOk)
		{
			UE_LOG(LogACEGPTLocal, Log, TEXT("Unable to unload AIM gpt.ggml.cuda feature: %s"), *GetAIMStatusString(Result));
			Interface = nullptr;
		}
	}
}

FString FGPTLocal::ExecuteGpt(const FString& Prompt)
{
	if (!Instance)
		return FString{};

	struct BasicCallbackCtx
	{
		std::mutex callbackMutex;
		std::condition_variable callbackCV;
		std::atomic<nvaim::InferenceExecutionState> callbackState = nvaim::InferenceExecutionState::eDataPending;
		FString gptOutput;
	};
	BasicCallbackCtx cbkCtx;

	auto completionCallback = [](const nvaim::InferenceExecutionContext* ctx, nvaim::InferenceExecutionState state, void* data) -> nvaim::InferenceExecutionState
		{
			if (!data)
				return nvaim::InferenceExecutionState::eInvalid;

			auto cbkCtx = (BasicCallbackCtx*)data;
			std::scoped_lock lck(cbkCtx->callbackMutex);

			// Outputs from GPT
			auto slots = ctx->outputs;
			const nvaim::InferenceDataText* text{};
			slots->findAndValidateSlot(nvaim::kGPTDataSlotResponse, &text);
			auto response = FString(StringCast<UTF8CHAR>(text->getUTF8Text()));
			if (response.Find("<JSON>") != INDEX_NONE)
			{
				auto cpuBuffer = castTo<nvaim::CpuData>(text->utf8Text);
				((uint8_t*)cpuBuffer->buffer)[0] = 0;
				cpuBuffer->sizeInBytes = 0;
			}
			else
			{
				cbkCtx->gptOutput += response;
			}

			cbkCtx->callbackState = state;
			cbkCtx->callbackCV.notify_one();

			return state;
		};

	auto PromptString = StringCast<UTF8CHAR>(*Prompt);

	// Input
	nvaim::CpuData inText(PromptString.Length() + 1, (void*)reinterpret_cast<const char*>(PromptString.Get()));
	nvaim::InferenceDataText inData(inText);

	TArray<nvaim::InferenceDataSlot> inSlots = { {nvaim::kGPTDataSlotUser, &inData} };
	nvaim::InferenceDataSlotArray inputs = { static_cast<size_t>(inSlots.Num()), inSlots.GetData() };

	// Parameters
	nvaim::GPTRuntimeParameters runtime{};
	runtime.seed = -1;
	runtime.tokensToPredict = 200;
	runtime.interactive = false;

	nvaim::InferenceExecutionContext gptCtx{};
	nvaim::InferenceInstance* instance =Instance;
	gptCtx.instance = instance;
	gptCtx.callback = completionCallback;
	gptCtx.callbackUserData = &cbkCtx;
	gptCtx.inputs = &inputs;
	gptCtx.runtimeParameters = runtime;

	cbkCtx.callbackState = nvaim::InferenceExecutionState::eDataPending;
	std::thread infer([instance, &gptCtx]()
		{
			instance->evaluate(&gptCtx);
		});

	{
		std::unique_lock lck(cbkCtx.callbackMutex);
		cbkCtx.callbackCV.wait(lck, [&cbkCtx]()
			{
				return cbkCtx.callbackState != nvaim::InferenceExecutionState::eDataPending;
			});
	}

	infer.join();

	FString response(cbkCtx.gptOutput);

	return response;
}

FGPTLocal* FGPTLocal::Get()
{
	FGPTLocalModule* GPTModule = FModuleManager::GetModulePtr<FGPTLocalModule>(FName(UE_MODULE_NAME));
	if (GPTModule != nullptr)
	{
		FGPTLocal* GPTLocal = GPTModule->Get();
		return GPTLocal;
	}
	return nullptr;
}
