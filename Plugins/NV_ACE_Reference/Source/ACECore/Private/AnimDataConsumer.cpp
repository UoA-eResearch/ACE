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

#include "AnimDataConsumer.h"

// plugin includes
#include "ACECoreModulePrivate.h"
#include "AnimDataConsumerRegistry.h"

/////////////////////////////
// FAnimDataConsumerRegistry

// Important structures used by this class:
//
// - ActiveConsumers: set of all active IACEAnimDataConsumer objects. Self-registered through ctor/dtor. Protected by critical section.
//
// - StreamToConsumerMap: used to find all IACEAnimDataConsumers to call into for a given stream ID. Currently we only support a single consumer per stream,
// but in the future we may expand this to an array of consumers for a single stream. Protected by critical section.
//
// - ConsumerToStreamMap: guaranteed to map only from active IACEAnimDataConsumer objects. Protected by critical section.

FAnimDataConsumerRegistry* FAnimDataConsumerRegistry::Get()
{
	FACECoreModule* CoreModule = FModuleManager::GetModulePtr<FACECoreModule>(UE_MODULE_NAME);
	if (CoreModule != nullptr)
	{
		return CoreModule->GetAnimDataRegistry();
	}

	return nullptr;
}

int32 FAnimDataConsumerRegistry::CreateStream_AnyThread()
{
	return NextStreamId++;
}

void FAnimDataConsumerRegistry::RemoveStream_AnyThread(int32 StreamID)
{
	IACEAnimDataConsumer* Consumer = nullptr;
	// Find any mapped consumers while removing the stream ID from the map
	FScopeLock Lock(&DataCS);
	StreamToConsumerMap.RemoveAndCopyValue(StreamID, Consumer);
	if (Consumer != nullptr)
	{
		// if there was a consumer, remove consumer → stream mapping also
		int32 NumRemoved = ConsumerToStreamMap.Remove(Consumer);

		if (ensure(NumRemoved > 0))
		{
			UE_LOG(LogACECore, Verbose, TEXT("[ACE SID %d] RemoveStream called, notifying consumer"), StreamID);
			// notify consumer that stream is done
			CancelStreamToConsumer_AnyThread(StreamID, Consumer);
		}
	}
}

void FAnimDataConsumerRegistry::SetAudioParams_AnyThread(int32 StreamID, uint32 NewSampleRate, int32 NewNumChannels, int32 SampleByteSize)
{
	IACEAnimDataConsumer** Consumer = nullptr;
	{
		FScopeLock Lock(&DataCS);
		Consumer = StreamToConsumerMap.Find(StreamID);
		if ((Consumer != nullptr) && ((*Consumer) != nullptr))
		{
			bool bConsumerActive = ConsumerToStreamMap.Contains(*Consumer);
			if (ensure(bConsumerActive))
			{
				(*Consumer)->PrepareNewStream_AnyThread(StreamID, NewSampleRate, NewNumChannels, SampleByteSize);
			}
		}
	}
}

void FAnimDataConsumerRegistry::AttachConsumerToStream_AnyThread(int32 StreamID, IACEAnimDataConsumer* Consumer, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize)
{
	check(Consumer != nullptr);

	FScopeLock Lock(&DataCS);
	if (!ActiveConsumers.Contains(Consumer))
	{
		return;
	}

	StreamToConsumerMap.Add(StreamID, Consumer);
	if (ConsumerToStreamMap.Contains(Consumer))
	{
		// cancel stream
		int32 OldStreamID = ConsumerToStreamMap.FindAndRemoveChecked(Consumer);
		StreamToConsumerMap.Remove(OldStreamID);

		// notify consumer that old stream is done
		// but then again, that's really implied by our call to PrepareNewStream_AnyThread below so maybe we leave it up to the consumer to sort it out?
		UE_LOG(LogACECore, Verbose, TEXT("[ACE SID %d] AttachConsumerToStream called with new stream ID %d, notifying consumer"), OldStreamID, StreamID);
		CancelStreamToConsumer_AnyThread(OldStreamID, Consumer);
	}
	ConsumerToStreamMap.Add(Consumer, StreamID);
	Consumer->PrepareNewStream_AnyThread(StreamID, SampleRate, NumChannels, SampleByteSize);
}

void FAnimDataConsumerRegistry::DetachConsumer_AnyThread(IACEAnimDataConsumer* Consumer)
{
	if (Consumer == nullptr)
	{
		return;
	}

	int32 StreamID = -1;
	FScopeLock Lock(&DataCS);
	bool bFound = ConsumerToStreamMap.RemoveAndCopyValue(Consumer, StreamID);
	if (bFound)
	{
		int32 NumRemoved = StreamToConsumerMap.Remove(StreamID);

		if (ensure(NumRemoved > 0))
		{
			// notify consumer that stream is done
			UE_LOG(LogACECore, Verbose, TEXT("[ACE SID %d] DetachConsumer called, notifying consumer"), StreamID);
			CancelStreamToConsumer_AnyThread(StreamID, Consumer);
		}
	}
}

int32 FAnimDataConsumerRegistry::SendAnimData_AnyThread(const FACEAnimDataChunk& AnimData, int32 StreamID)
{
	// Note: often this will NOT be called from game thread, but from an external callback

	IACEAnimDataConsumer** Consumer = nullptr;
	{
		FScopeLock Lock(&DataCS);
		Consumer = StreamToConsumerMap.Find(StreamID);
		if ((Consumer != nullptr) && ((*Consumer) != nullptr))
		{
			bool bConsumerActive = ConsumerToStreamMap.Contains(*Consumer);
			if (ensure(bConsumerActive))
			{
				(*Consumer)->ConsumeAnimData_AnyThread(AnimData, StreamID);
				if (AnimData.Status == EACEAnimDataStatus::OK_NO_MORE_DATA)
				{
					// stream is done, so clean up
					ConsumerToStreamMap.Remove(*Consumer);
					StreamToConsumerMap.Remove(StreamID);
				}
				return 1;
			}
		}
	}
	return 0;
}

bool FAnimDataConsumerRegistry::DoesStreamHaveConsumers_AnyThread(int32 StreamID)
{
	FScopeLock Lock(&DataCS);
	return StreamToConsumerMap.Contains(StreamID);
}


void FAnimDataConsumerRegistry::CancelStreamToConsumer_AnyThread(int32 StreamID, IACEAnimDataConsumer* Consumer)
{
	// not sure if this is a good idea or even necessary
	if (Consumer != nullptr)
	{
		// notify any mapped consumer that no more data is coming
		FACEAnimDataChunk EndChunk{};
		EndChunk.Status = EACEAnimDataStatus::OK_NO_MORE_DATA;
		Consumer->ConsumeAnimData_AnyThread(EndChunk, StreamID);
	}
}

void FAnimDataConsumerRegistry::RegisterConsumer_AnyThread(IACEAnimDataConsumer* Consumer)
{
	FScopeLock Lock(&DataCS);
	ActiveConsumers.Add(Consumer);
}

void FAnimDataConsumerRegistry::UnregisterConsumer_AnyThread(IACEAnimDataConsumer* Consumer)
{
	FScopeLock Lock(&DataCS);
	ActiveConsumers.Remove(Consumer);

	// remove from mappings to ensure the consumer doesn't receive any more callbacks
	int32 StreamIDToRemove = -1;
	bool bFound = ConsumerToStreamMap.RemoveAndCopyValue(Consumer, StreamIDToRemove);
	if (bFound)
	{
		StreamToConsumerMap.Remove(StreamIDToRemove);
	}
}

////////////////////////
// IACEAnimDataConsumer

// ctor
IACEAnimDataConsumer::IACEAnimDataConsumer()
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry != nullptr)
	{
		Registry->RegisterConsumer_AnyThread(this);
	}
}

IACEAnimDataConsumer::~IACEAnimDataConsumer()
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry != nullptr)
	{
		Registry->UnregisterConsumer_AnyThread(this);
	}
}

