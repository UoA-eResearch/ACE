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

#include "AnimStream.h"

// engine includes
#include "Containers/StringConv.h"

// plugin includes
#include "AIMModule.h"
#include "AnimDataConsumerRegistry.h"
#include "AnimStreamPrivate.h"
#include "nvaim.h"
#include "nvaim_a2x.h"
#include "nvaim_ai.h"
#include "nvaim_animgraph.h"


const char* MODEL_STRING = "{CA7BC62F-BCF5-4981-926E-01CE7E1C6E35}";


////////////////////////
// FAIMAnimgraphFeature

static void WorkAroundAIMCrash(nvaim::InferenceInterface* Interface)
{
	// we get nothing useful from querying caps, but a bug in AIM animgraph will cause things to crash in execute if we don't call this first 🙄
	const nvaim::AnimgraphCapabilitiesAndRequirements* DummyOutput = nullptr;
	nvaim::CommonCreationParameters DummyCCP;
	auto ModelDirUTF8 = StringCast<UTF8CHAR>(*FAIMModule::Get().GetModelDirectory());
	DummyCCP.utf8PathToModels = reinterpret_cast<const char*>(ModelDirUTF8.Get());
	DummyCCP.modelGUID = MODEL_STRING;
	nvaim::AnimgraphCreationParameters DummyACP;
	DummyACP.common = &DummyCCP;
	nvaim::getCapsAndRequirements(Interface, DummyACP, &DummyOutput);
}

FAIMAnimgraphFeature::FAIMAnimgraphFeature()
{
	FAIMModule::Get().LoadAIMFeature(nvaim::plugin::animgraph::kId, &Interface);

	if (Interface != nullptr)
	{
		WorkAroundAIMCrash(Interface);
	}
}

FAIMAnimgraphFeature::~FAIMAnimgraphFeature()
{
	if (Interface != nullptr)
	{
		FAIMModule::Get().UnloadAIMFeature(nvaim::plugin::animgraph::kId, Interface);
	}
}

// FAIMAnimgraphFeature
////////////////////////


//////////////////
// FACEAnimStream

FACEAnimStream::FACEAnimStream()
{
	FAIMModule::Get().RegisterAIMFeature(nvaim::plugin::animgraph::kId, {}, { nvaim::plugin::a2x::cloud::grpc::kId });
}

int32 FACEAnimStream::CreateStream(IACEAnimDataConsumer* Consumer, FString InStreamName, FString InURL, int32 InNumOfRetries, float InTimeBetweenRetries, float RPCTimeout)
{
	// clean up any old dead streams we have lying around
	GC();

	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (!ensure(Registry != nullptr))
	{
		UE_LOG(LogACEAnimStream, Log, TEXT("Unable to create new ACE animation stream, no registry available"));
		return -1;
	}

	// create animgraph feature if it hasn't already been created
	if (!Animgraph.IsValid())
	{
		Animgraph = MakeShared<FAIMAnimgraphFeature>();
	}
	if (Animgraph->Interface == nullptr)
	{
		UE_LOG(LogACEAnimStream, Log, TEXT("Unable to create new ACE animation stream, no AIM animgraph feature available"));
		return -1;
	}

	int32 StreamID = Registry->CreateStream_AnyThread();
	if (Consumer != nullptr)
	{
		// Assume 16000 samples per second mono audio, since that's the default.
		// The anim stream thread will call SetAudioParams_AnyThread if that assumption is wrong.
		Registry->AttachConsumerToStream_AnyThread(StreamID, Consumer, DEFAULT_SAMPLE_RATE, DEFAULT_NUM_CHANNELS);
	}

	StreamThreads.Emplace(
		MakeUnique<FAnimStreamThread>(StreamID, InURL, InStreamName, Animgraph,
			InNumOfRetries, InTimeBetweenRetries, RPCTimeout));

	if (StreamThreads.Last()->GetState() == EACEAnimStreamState::CONNECTION_FAILED)
	{
		GC();
		return -1;
	}

	return StreamID;
}

void FACEAnimStream::CancelStream(int32 StreamID)
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (ensure(Registry != nullptr))
	{
		Registry->RemoveStream_AnyThread(StreamID);
	}
}

void FACEAnimStream::CancelStream(IACEAnimDataConsumer* Consumer)
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (ensure(Registry != nullptr))
	{
		Registry->DetachConsumer_AnyThread(Consumer);
	}
}

void FACEAnimStream::GC()
{
	// clean up any runnable objects in their final state
	StreamThreads.RemoveAll([](const TUniquePtr<class FAnimStreamThread>& StreamThread)
	{
		return !StreamThread.IsValid() || IsFinalState(StreamThread->GetState());
	});
}

// FACEAnimStream
//////////////////

