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

#include "GPTLocalModule.h"

#include "GPTLocal.h"
#include "GPTLocalPrivate.h"

#define LOCTEXT_NAMESPACE "FGPTLocal"

DEFINE_LOG_CATEGORY(LogACEGPTLocal);

FGPTLocal* FGPTLocalModule::Get()
{
	FScopeLock Lock(&CS);

	if (!Provider.IsValid())
	{
		Provider = MakeUnique<FGPTLocal>();
	}

	if (!Provider.IsValid())
	{
		UE_LOG(LogACEGPTLocal, Log, TEXT("Unable to return provider, make sure it loaded properly."));
		return nullptr;
	}

	return Provider.Get();
}

void FGPTLocalModule::StartupModule()
{
}

void FGPTLocalModule::ShutdownModule()
{
	FScopeLock Lock(&CS);

	if (Provider.IsValid())
	{
		Provider.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGPTLocalModule, GPTLocal)
