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

#include "AnimStreamModule.h"

// engine includes
#include "Misc/CommandLine.h"

//  plugin includes
#include "ACESettings.h"
#include "AnimStream.h"
#include "AnimStreamPrivate.h"


#define LOCTEXT_NAMESPACE "FAnimStream"

DEFINE_LOG_CATEGORY(LogACEAnimStream);


void FAnimStreamModule::StartupModule()
{
	Provider = MakeUnique<FACEAnimStream>();
}

void FAnimStreamModule::ShutdownModule()
{
	if (Provider.IsValid())
	{
		Provider.Reset();
	}
}

static float GetAnimgraphRPCTimeout()
{
	float RPCTimeout;
	if (FParse::Value(FCommandLine::Get(), TEXT("-animgraphtimeout="), RPCTimeout))
	{
		return RPCTimeout;
	}
	return GetDefault<UACESettings>()->ConnectionTimeout;
}

bool FAnimStreamModule::SubscribeCharacterToStream(IACEAnimDataConsumer* Consumer, const FString& StreamName)
{
	if (ensure(Provider.IsValid()))
	{
		FString DestURL = GetAnimStreamURL();
		float RPCTimeout = GetAnimgraphRPCTimeout();
		const UACESettings* ACESettings = GetDefault<UACESettings>();
		int32 StreamID = Provider->CreateStream(Consumer, StreamName, DestURL,
			ACESettings->NumConnectionAttempts, ACESettings->TimeBetweenRetrySeconds, RPCTimeout);
		return StreamID != -1;
	}

	return false;
}

bool FAnimStreamModule::UnsubscribeFromStream(IACEAnimDataConsumer* Consumer)
{
	if (ensure(Provider.IsValid()))
	{
		Provider->CancelStream(Consumer);
		return true;
	}

	return false;
}

void FAnimStreamModule::OverrideAnimStreamURL(const FString& ACEAnimgraphURL)
{
	OverrideURL = ACEAnimgraphURL;
}

FString FAnimStreamModule::GetAnimStreamURL() const
{
	// Priority order:
	// 1. Runtime override
	// 2. Command line override
	// 3. Project default setting

	if (!OverrideURL.IsEmpty())
	{
		return OverrideURL;
	}

	FString ServerAddress;
	if (FParse::Value(FCommandLine::Get(), TEXT("-animgraphserver="), ServerAddress))
	{
		return ServerAddress;
	}

	return GetDefault<UACESettings>()->ACEAnimgraphURL;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAnimStreamModule, AnimStream)

