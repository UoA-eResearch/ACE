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

#include "AnimStreamThread.h"

// engine includes
#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"

// plugin includes
#include "AIMModule.h"
#include "AnimDataConsumerRegistry.h"
#include "AnimStream.h"
#include "AnimStreamPrivate.h"
#include "nvaim.h"
#include "nvaim_ai.h"
#include "nvaim_animgraph.h"
#include "nvaim_cloud.h"


static nvaim::InferenceInstance* CreateAnimgraphConnection(nvaim::InferenceInterface* AnimgraphFeature, FString DestURL, int32 NumOfRetries, float TimeBetweenRetries, float RPCTimeout)
{
	if (!ensure(AnimgraphFeature != nullptr))
	{
		return nullptr;
	}

	nvaim::AnimgraphCreationParameters AnimgraphCreationParams;
	nvaim::CommonCreationParameters CommonCreationParams;

	auto ModelDirUTF8 = StringCast<UTF8CHAR>(*FAIMModule::Get().GetModelDirectory());
	CommonCreationParams.utf8PathToModels = reinterpret_cast<const char*>(ModelDirUTF8.Get());
	CommonCreationParams.numThreads = 4;	// just guessing, I don't know how to tell how many threads AIM needs
	CommonCreationParams.vramBudgetMB = 0;	// If AIM uses any VRAM at all in its gRPC implementation, something has gone horribly awry
	CommonCreationParams.modelGUID = MODEL_STRING;
	AnimgraphCreationParams.common = &CommonCreationParams;

	// connection timeout in ms
	float RPCTimeoutMs = 1000.0f * RPCTimeout;
	AnimgraphCreationParams.connection_timeout_in_ms = static_cast<uint32_t>(FMath::Clamp(static_cast<int32_t>(RPCTimeoutMs), 1, nvaim::ANIMGRAPH_MAX_CONNECTION_TIMEOUT_IN_MS));
	UE_LOG(LogACEAnimStream, Verbose, TEXT("Animgraph gRPC timeout = %d ms"), AnimgraphCreationParams.connection_timeout_in_ms);


	if (DestURL.IsEmpty())
	{
		UE_LOG(LogACEAnimStream, Warning, TEXT("No server address configured, please configure in Project Settings->ACE Settings->Default Animgraph Server URL"));
		return nullptr;
	}
	if (!DestURL.StartsWith("http"))
	{
		UE_LOG(LogACEAnimStream, Warning, TEXT("Server address does not start with http or https, defaulting to non secure connection"));
	}

	nvaim::RPCParameters GRPCParams{};
	FString URLWithoutScheme = FGenericPlatformHttp::GetUrlDomainAndPort(DestURL);
	auto URLWithoutSchemeUTF8 = StringCast<UTF8CHAR>(*URLWithoutScheme);
	TOptional<bool> MaybeIsHttps = FGenericPlatformHttp::IsSecureProtocol(DestURL);

	GRPCParams.url = reinterpret_cast<const char*>(URLWithoutSchemeUTF8.Get());
	GRPCParams.useSSL = MaybeIsHttps.IsSet() ? *MaybeIsHttps : false;	// assume http scheme if not specified
	GRPCParams.metaData = "";	// AIM will refuse to create a connection with null metaData, we have to provide empty string
	AnimgraphCreationParams.chain(GRPCParams);

	nvaim::InferenceInstance* Connection = nullptr;
	nvaim::Result Result = nvaim::ResultInvalidState;

	for (int32 i = 0 ; i < NumOfRetries ; ++i)
	{
		Result = AnimgraphFeature->createInstance(AnimgraphCreationParams, &Connection);
		if (Result != nvaim::ResultOk)
		{

			UE_LOG(LogACEAnimStream, Warning, TEXT("Unable to create animgraph instance, try %d of %d"),i,NumOfRetries);
			
			if (TimeBetweenRetries != 0.f)
			{
				UE_LOG(LogACEAnimStream, Warning, TEXT("Retring in: %f"),TimeBetweenRetries);
				FPlatformProcess::Sleep(TimeBetweenRetries);
			}
		}
		else
		{
			break;
		}

	}
	
	if (Result != nvaim::ResultOk)
	{
		UE_LOG(LogACEAnimStream, Warning,
			TEXT("Unable to create animgraph instance (%s). nvaim::RPCParameters::url=\"%s\", nvaim::RPCParameters::useSSL=%s, nvaim::RPCParameters::metaData=\"%s\""),
			*GetAIMStatusString(Result), UTF8_TO_TCHAR(GRPCParams.url), GRPCParams.useSSL ? TEXT("true") : TEXT("false"), UTF8_TO_TCHAR(GRPCParams.metaData));
		return nullptr;
	}

	return Connection;
}

template<class T>
static T& GetValueFromAIMParameter(const nvaim::NVAIMParameter* AIMParameter)
{
	const nvaim::CpuData* AIMCpuData{ nvaim::castTo<nvaim::CpuData>(AIMParameter) };
	check(AIMCpuData->sizeInBytes == sizeof(T));
	return *static_cast<T*>(AIMCpuData->buffer);
}

template<class T>
static TArrayView<T> GetArrayViewFromAIMParameter(const nvaim::NVAIMParameter* AIMParameter)
{
	const nvaim::CpuData* AIMCpuData{ nvaim::castTo<nvaim::CpuData>(AIMParameter) };
	check(0 == AIMCpuData->sizeInBytes % sizeof(T));
	return TArrayView<T>(static_cast<T*>(AIMCpuData->buffer), AIMCpuData->sizeInBytes / sizeof(T));
}

struct FAnimStreamAudioParams
{
	// audio sample rate
	uint32 SampleRate = 16'000;
	// number of audio channels (1 = mono, 2 = stereo)
	int32 NumChannels = 1;
	// 2 = PCM16, 4 = float32
	int32 SampleByteSize = 2;
	// Audio timestamp
	double Timestamp = 0.0;
};

static TTuple<FACEAnimDataChunk, FAnimStreamAudioParams> CreateChunkFromAIMOutputs(const nvaim::InferenceDataSlotArray* AIMOutputs, int32 StreamID)
{
	TTuple<FACEAnimDataChunk, FAnimStreamAudioParams> Output;
	FACEAnimDataChunk& Chunk = Output.Get<0>();
	FAnimStreamAudioParams& AudioParams = Output.Get<1>();
	Chunk.Status = EACEAnimDataStatus::ERROR_UNEXPECTED_OUTPUT;
	if (AIMOutputs == nullptr)
	{
		return Output;
	}

	const nvaim::InferenceDataByteArray* BlendShapeWeightSlot = nullptr;
	const nvaim::InferenceDataAudio* AudioSampleSlot = nullptr;
	const nvaim::InferenceDataByteArray* TimeCodeSlot = nullptr;

	if (!ensure(AIMOutputs->findAndValidateSlot(nvaim::kAnimgraphDataSlotBlendshapes, &BlendShapeWeightSlot)))
	{
		return Output;
	}
	if (!ensure(AIMOutputs->findAndValidateSlot(nvaim::kAnimgraphDataSlotAudio, &AudioSampleSlot)))
	{
		return Output;
	}
	if (!ensure(AIMOutputs->findAndValidateSlot(nvaim::kAnimgraphDataSlotTimeCodes, &TimeCodeSlot)))
	{
		return Output;
	}

	check(BlendShapeWeightSlot != nullptr);
	// we assume PCM16 output at the moment
	check(AudioSampleSlot != nullptr && (AudioSampleSlot->bitsPerSample % 8 == 0));
	check(TimeCodeSlot != nullptr);

	Chunk.BlendShapeWeights = GetArrayViewFromAIMParameter<const float>(BlendShapeWeightSlot->bytes);
	Chunk.AudioBuffer = GetArrayViewFromAIMParameter<const uint8>(AudioSampleSlot->audio);
	AudioParams.SampleRate = static_cast<uint32>(AudioSampleSlot->samplingRate);
	AudioParams.NumChannels = AudioSampleSlot->channels;
	AudioParams.SampleByteSize = AudioSampleSlot->bitsPerSample / 8;
	TArrayView<const double> TimeCodes = GetArrayViewFromAIMParameter<const double>(TimeCodeSlot->bytes);
	check(TimeCodes.Num() == 5);
	Chunk.Timestamp = TimeCodes[0];	// undocumented: blend shape weight time code is offset 0
	AudioParams.Timestamp = TimeCodes[4];	// undocumented: audio time code is offset 4
	Chunk.Status = EACEAnimDataStatus::OK;
	return Output;
}

enum class EStreamState : uint8
{
	None,
	InvalidStreamID,
	ConnectionLost,
	Success,
};

struct FCallbackUserData
{
public:
	
	int32 StreamID = 0;
	uint32 SampleRate = DEFAULT_SAMPLE_RATE;
	int32 NumChannels = DEFAULT_NUM_CHANNELS;
	int32 SampleByteSize = DEFAULT_SAMPLE_BYTE_SIZE;
	TOptional<double> FirstAnimTimestamp{};
	int64 ReceivedAudioSamples = 0;
	bool bHasSentData = false;
	
	EStreamState StreamState = EStreamState::None;

};

static FString GetAIMAnimgraphStatusString(nvaim::AnimgraphStatusCode Status)
{
	switch (Status)
	{
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeOK:
		return FString(TEXT("Success"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeCancelled:
		return FString(TEXT("Operation cancelled"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeUnknown:
		return FString(TEXT("Unknown gRPC error"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeInvalidArgument:
		return FString(TEXT("Invalid argument from client"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeDeadlineExceeded:
		return FString(TEXT("Deadline expired before operation completed"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeNotFound:
		return FString(TEXT("Requested resource not found"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeAlreadyExists:
		return FString(TEXT("Resource already exists"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodePermissionDenied:
		return FString(TEXT("Permission denied"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeResourceExhausted:
		return FString(TEXT("Resource exhausted"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeFailedPrecondition:
		return FString(TEXT("Failed precondition state"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeAborted:
		return FString(TEXT("Operation aborted"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeOutOfRange:
		return FString(TEXT("Operation out of range"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeUnimplemented:
		return FString(TEXT("Operation unimplemented"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeInternal:
		return FString(TEXT("Internal error"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeUnavailable:
		return FString(TEXT("Service unavailable"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeDataLoss:
		return FString(TEXT("Unrecoverable data loss"));
	case nvaim::AnimgraphStatusCode::eGrpcStatusCodeUnauthenticated:
		return FString(TEXT("No valid authentication"));
	case nvaim::AnimgraphStatusCode::eAceStatusCodeErrorUnknown:
		return FString(TEXT("Unknown ACE error"));
	case nvaim::AnimgraphStatusCode::eAceStatusCodeErrorStreamIdDoesNotExist:
		return FString(TEXT("Invalid animgraph stream ID"));
	case nvaim::AnimgraphStatusCode::eAimUnknown:
		return FString(TEXT("Unknown AIM error"));
	case nvaim::AnimgraphStatusCode::eAimGrpcDataHeapAllocationError:
		return FString(TEXT("AIM gRPC data heap allocation error"));
	case nvaim::AnimgraphStatusCode::eAimGrpcConnectionProblem:
		return FString(TEXT("AIM gRPC connection problem"));
	case nvaim::AnimgraphStatusCode::eAimReaderThreadCannotStart:
		return FString(TEXT("AIM reader thread can't start"));
	case nvaim::AnimgraphStatusCode::eAimReaderThreadCannotJoin:
		return FString(TEXT("AIM reader thread can't join"));
	default:
		return FString(TEXT("Invalid AIM animgraph status code"));
	}
}


static nvaim::InferenceExecutionState AnimgraphCallback(const nvaim::InferenceExecutionContext* AIMContext, nvaim::InferenceExecutionState State, void* InContext)
{
	FCallbackUserData* UserData = reinterpret_cast<FCallbackUserData*>(InContext);

	if (ensure(AIMContext && UserData))
	{
		// read status code
		const nvaim::InferenceDataByteArray* AnimgraphState{};
		auto Slots = AIMContext->outputs;
		if (!ensure((AIMContext->outputs != nullptr) && AIMContext->outputs->findAndValidateSlot(nvaim::kAnimgraphStatusCode, &AnimgraphState)))
		{
			UE_LOG(LogACEAnimStream, Warning, TEXT("can't find AIM animgraph status code, ignoring callback"));
			return State;
		}
		const nvaim::AnimgraphStatusCode& StatusCode = GetValueFromAIMParameter<const nvaim::AnimgraphStatusCode>(AnimgraphState->bytes);

		if (nvaim::AnimgraphStatusCode::eAceStatusCodeErrorStreamIdDoesNotExist == StatusCode)
		{
			UE_LOG(LogACEAnimStream, Warning, TEXT("Stream ID does not exist in server"));
			UserData->StreamState = EStreamState::InvalidStreamID;
			return State;
		}

		if (nvaim::AnimgraphStatusCode::eGrpcStatusCodeOK != StatusCode)
		{
			// Undocumented AIM behavior!
			// If AIM provides any status code other than:
			// - nvaim::AnimgraphStatusCode::eGrpcStatusCodeOK
			// - nvaim::AnimgraphStatusCode::eAceStatusCodeErrorStreamIdDoesNotExist
			// then AIM considers this a lost connection.
			// It will try to reconnect unless we return nvaim::InferenceExecutionState::eCancel here.
			// Since so far our customers seem to want a robust animation data service connection we never explicitly
			// cancel, we always let AIM reconnect.
			UE_LOG(LogACEAnimStream, Warning, TEXT("Error in animgraph stream, assuming connection lost: %s"), *GetAIMAnimgraphStatusString(StatusCode));
			UserData->StreamState = EStreamState::ConnectionLost;

			// If something happens to the connection that the application is aware of, AIM will keep retrying to
			// connect here even if the application cancels the stream. It's harmless but noisy in the log.
			// We'd have to add an API to the registry to look up whether a stream ID is still valid to detect that
			// there's nothing to receive our data. That would be one case where it would make sense to return
			// nvaim::InferenceExecutionState::eCancel here instead.

			return State;
		}

		// Consume output
		FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
		if (Registry != nullptr)
		{
			TTuple<FACEAnimDataChunk, FAnimStreamAudioParams> ChunkAudioParams =
				CreateChunkFromAIMOutputs(AIMContext->outputs, UserData->StreamID);
			FACEAnimDataChunk& Chunk = ChunkAudioParams.Get<0>();
			const FAnimStreamAudioParams& Audio = ChunkAudioParams.Get<1>();
			// inform consumer if the sample rate or number of channels wasn't as expected
			if ((Audio.SampleRate != UserData->SampleRate) || (Audio.NumChannels != UserData->NumChannels) || (Audio.SampleByteSize != UserData->SampleByteSize))
			{
				if (ensure(!UserData->bHasSentData))
				{
					Registry->SetAudioParams_AnyThread(UserData->StreamID, Audio.SampleRate, Audio.NumChannels, Audio.SampleByteSize);
					UserData->SampleRate = Audio.SampleRate;
					UserData->NumChannels = Audio.NumChannels;
					UserData->SampleByteSize = Audio.SampleByteSize;
				}
				else
				{
					UE_LOG(LogACEAnimStream, Warning,
						TEXT("[ACE SID %d] Animgraph service changed audio parameters mid-stream, aborting animation stream! Sample rate %d (expected %d), channels %d (expected %d), bytes %d (expected %d)"),
						UserData->StreamID, Audio.SampleRate, UserData->SampleRate, Audio.NumChannels, UserData->NumChannels, Audio.SampleByteSize, UserData->SampleByteSize);
					FACEAnimDataChunk EndChunk;
					EndChunk.Status = EACEAnimDataStatus::OK_NO_MORE_DATA;
					Registry->SendAnimData_AnyThread(EndChunk, UserData->StreamID);
					return nvaim::InferenceExecutionState::eCancel;
				}
			}

			// Timestamps from the service don't necessarily start at 0. Convert to 0-based timestamps to make the math simpler
			if (!UserData->FirstAnimTimestamp.IsSet())
			{
				UserData->FirstAnimTimestamp = Chunk.Timestamp;
			}
			const double LocalAnimTimestamp = Chunk.Timestamp - *UserData->FirstAnimTimestamp;
			const double LocalAudioTimestamp = Audio.Timestamp - *UserData->FirstAnimTimestamp;

			// Pad start of audio buffer with silence if necessary to align with the audio timestamp
			const int32 BytesPerSample = Audio.SampleByteSize * Audio.NumChannels;
			TArray<uint8> PaddedAudioBuffer;
			const int64 AUDIO_SAMPLE_FUDGE_FACTOR = 2;
			// ...but we only need to do so if we got audio samples
			if (!Chunk.AudioBuffer.IsEmpty())
			{
				int64 ExpectedTotalAudioSamples = FMath::RoundToInt64(LocalAudioTimestamp * Audio.SampleRate);
				if (UserData->ReceivedAudioSamples < ExpectedTotalAudioSamples)
				{
					const int32 ExtraSamplesNeeded = static_cast<int32>(ExpectedTotalAudioSamples - UserData->ReceivedAudioSamples);
					PaddedAudioBuffer.AddZeroed(BytesPerSample * ExtraSamplesNeeded);
					PaddedAudioBuffer.Append(Chunk.AudioBuffer);
					Chunk.AudioBuffer = PaddedAudioBuffer;
				}
				else if (UserData->ReceivedAudioSamples > (ExpectedTotalAudioSamples + AUDIO_SAMPLE_FUDGE_FACTOR))
				{
					// we don't know whether this is a bug in AIM or a bug in the service, but we often get audio timestamps
					// of 0.0 when the audio buffer contains complete silence. So we won't fill the log with spew about those
					// 0.0 timestamps
					if (Audio.Timestamp != 0.0)
					{
						double MinExpectedTimestamp = static_cast<double>(UserData->ReceivedAudioSamples) / static_cast<double>(Audio.SampleRate)
							+ *UserData->FirstAnimTimestamp;
						UE_LOG(LogACEAnimStream, Log, TEXT("[ACE SID %d] service sent bogus audio timestamps, expected at least %f, received %f"),
							UserData->StreamID, MinExpectedTimestamp, Audio.Timestamp);
					}
				}
			}

			// Pad end of audio buffer with silence if necessary to match animation timestamp
			const int32 SamplesReceivedThisFrame = Chunk.AudioBuffer.Num() / BytesPerSample;
			UserData->ReceivedAudioSamples += static_cast<int64>(SamplesReceivedThisFrame);
			int64 ExpectedTotalAudioSamples = static_cast<int64>(LocalAnimTimestamp * static_cast<double>(Audio.SampleRate));

			if (UserData->ReceivedAudioSamples + AUDIO_SAMPLE_FUDGE_FACTOR < ExpectedTotalAudioSamples)
			{
				const int32 ExtraSamplesNeeded = static_cast<int32>(ExpectedTotalAudioSamples - UserData->ReceivedAudioSamples);
				if (PaddedAudioBuffer.IsEmpty())
				{
					// we didn't pad the start above, so initialize the array here
					PaddedAudioBuffer.Append(Chunk.AudioBuffer);
				}
				PaddedAudioBuffer.AddZeroed(BytesPerSample * ExtraSamplesNeeded);
				Chunk.AudioBuffer = PaddedAudioBuffer;
				UserData->ReceivedAudioSamples += static_cast<int64>(ExtraSamplesNeeded);
			}

			// send chunk to consumer
			int32 NumConsumers = Registry->SendAnimData_AnyThread(Chunk, UserData->StreamID);
			UserData->bHasSentData = true;
			if (NumConsumers < 1)
			{
				// no one is consuming stream, so cancel it
				return nvaim::InferenceExecutionState::eCancel;
			}

			if (State == nvaim::InferenceExecutionState::eDone)
			{
				UE_LOG(LogACEAnimStream, Log, TEXT("[ACE SID %d] stream done"), UserData->StreamID);
				// one final dummy chunk to indicate no more data
				FACEAnimDataChunk EndChunk;
				EndChunk.Status = EACEAnimDataStatus::OK_NO_MORE_DATA;
				Registry->SendAnimData_AnyThread(EndChunk, UserData->StreamID);
			}
		}
		UserData->StreamState = EStreamState::Success;
	}

	return State;
}

/////////////////////
// FAnimStreamThread

FAnimStreamThread::FAnimStreamThread(int32 InStreamID, FString InURL, FString InStreamName, TSharedPtr<FAIMAnimgraphFeature> InAnimgraph,
	int32 InNumOfRetries, float InTimeBetweenRetries, float InRPCTimeout) :
	StreamID(InStreamID), DestURL(InURL), NumOfRetries(InNumOfRetries), TimeBetweenRetries(InTimeBetweenRetries),
	RPCTimeout(InRPCTimeout), StreamName(InStreamName), Animgraph(InAnimgraph)
{
	State = EACEAnimStreamState::CONNECTING;
	Thread.Reset(FRunnableThread::Create(this, *FString::Printf(TEXT("ACE AnimStream %d"), StreamID)));
}

EACEAnimStreamState FAnimStreamThread::GetState() const
{
	return State.load();
}

// @return True if initialization was successful, false otherwise
bool FAnimStreamThread::Init()
{
	// create connection to animgraph service
	Connection = CreateAnimgraphConnection(Animgraph->Interface, DestURL, NumOfRetries, TimeBetweenRetries, RPCTimeout);
	if (Connection == nullptr)
	{
		FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
		if (ensure(Registry != nullptr))
		{
			Registry->RemoveStream_AnyThread(StreamID);
		}
		State = EACEAnimStreamState::CONNECTION_FAILED;
		return false;
	}

	State = EACEAnimStreamState::STREAMING;
	return true;
}

// @return The exit code of the runnable object
uint32 FAnimStreamThread::Run()
{
	// convert StreamName to something AIM likes
	const auto StreamNameUTF8 = StringCast<UTF8CHAR>(*StreamName);
	nvaim::CpuData StreamNameUTF8Wrapper(StreamNameUTF8.Length() + 1, StreamNameUTF8.Get());
	nvaim::InferenceDataText StreamNameUTF8WrapperWrapper(StreamNameUTF8Wrapper);

	const char DummyString[] = "The protocol doesn't require a value here, but AIM does!";
	nvaim::CpuData DummyStringWrapper(UE_ARRAY_COUNT(DummyString), DummyString);
	nvaim::InferenceDataText DummyStringWrapperWrapper(DummyStringWrapper);

	nvaim::InferenceDataSlot AnimgraphInputs[] =
	{
		{ nvaim::kAnimgraphDataSlotStreamId, &StreamNameUTF8WrapperWrapper },
		{ nvaim::kAnimgraphDataSlotRequestId, &DummyStringWrapperWrapper },
		{ nvaim::kAnimgraphDataSlotTargetObjectId, &DummyStringWrapperWrapper },
	};
	nvaim::InferenceDataSlotArray AnimgraphInputsWrapper = { UE_ARRAY_COUNT(AnimgraphInputs), AnimgraphInputs };

	// set up AIM context and call RPC
	nvaim::InferenceExecutionContext AIMContext{};
	AIMContext.instance = Connection;
	AIMContext.inputs = &AnimgraphInputsWrapper;
	AIMContext.callback = &AnimgraphCallback;
	nvaim::AnimgraphRuntimeParameters DummyRuntimeParams{};
	AIMContext.runtimeParameters = DummyRuntimeParams;
	
	FCallbackUserData UserData;
	UserData.StreamID = StreamID;

	AIMContext.callbackUserData = &UserData;
	
	bool bRun = true;

	nvaim::Result Result = nvaim::ResultOk;

	int32 CurrentTry = 0;
	int32 CurrentConnectionTries = 0;

	while(bRun)
	{

		Result = Connection->evaluate(&AIMContext);

		if (UserData.StreamState != EStreamState::Success)
		{
				 
			switch (UserData.StreamState)
			{

			case EStreamState::InvalidStreamID:
				CurrentTry++;
				if (CurrentTry >= NumOfRetries)
				{
					bRun = false;
					UE_LOG(LogACEAnimStream, Warning, TEXT("Invalid Stream ID, number of retries reached maximum"));
				}
				else
				{
					if (TimeBetweenRetries != 0.f)
					{
						FPlatformProcess::Sleep(TimeBetweenRetries);
					}

					UE_LOG(LogACEAnimStream,Warning,TEXT("Invalid Stream ID, Retrying %d of %d"),CurrentTry,NumOfRetries);
				}
				break;

			case EStreamState::ConnectionLost:
				// This shouldn't happen because AIM retries the connection when it's lost until we return nvaim::InferenceExecutionState::eCancel
				// to AIM, and our callback doesn't return that
				bRun = false;// currently we are not handling connection lost scenario
				UE_LOG(LogACEAnimStream, Log, TEXT("[ACE SID %d] stream completed due to connection lost"), StreamID);
 				break;

			case EStreamState::None:
				UE_LOG(LogACEAnimStream, Log, TEXT("[ACE SID %d] stream completed, unexpected EStreamState::None"), StreamID);
				break;

			default:
				UE_LOG(LogACEAnimStream, Log, TEXT("[ACE SID %d] stream completed, invalid EStreamState %d"), StreamID, UserData.StreamState);
				break;
			}
		}
		else
		{
			UE_LOG(LogACEAnimStream, Log, TEXT("[ACE SID %d] stream successfully completed"), StreamID);
			bRun = false;
		}
	}

	// RPC completed, clean up and exit
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (ensure(Registry != nullptr))
	{
		Registry->RemoveStream_AnyThread(StreamID);
	}

	if (Result != nvaim::ResultOk)
	{
		State = EACEAnimStreamState::STREAM_FAILED;
		UE_LOG(LogACEAnimStream, Warning, TEXT("Failed receiving ACE animation stream: %s"), *GetAIMStatusString(Result));
		return 1;
	}

	State = EACEAnimStreamState::STREAM_COMPLETE;
	return 0;
}

// This is called if a thread is requested to terminate early.
void FAnimStreamThread::Stop()
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (ensure(Registry != nullptr))
	{
		// this will prevent consumer from getting any more callbacks, just in case
		Registry->RemoveStream_AnyThread(StreamID);
	}
}

// Called in the context of the aggregating thread to perform any cleanup.
void FAnimStreamThread::Exit()
{
	if ((Connection != nullptr) && ensure(Animgraph.IsValid()) && (Animgraph->Interface != nullptr))
	{
		Animgraph->Interface->destroyInstance(Connection);
	}

	EACEAnimStreamState LocalState = State;
	if (!ensureMsgf(IsFinalState(LocalState), TEXT("%d is not final FAnimStreamThread state, forcing STREAM_COMPLETE"), LocalState))
	{
		FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
		if (ensure(Registry != nullptr))
		{
			// this will prevent consumer from getting any more callbacks, just in case
			Registry->RemoveStream_AnyThread(StreamID);
		}
		State = EACEAnimStreamState::STREAM_COMPLETE;
	}
}
