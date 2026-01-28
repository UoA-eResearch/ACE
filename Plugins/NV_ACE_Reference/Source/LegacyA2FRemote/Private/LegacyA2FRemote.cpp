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

#include "LegacyA2FRemote.h"

// engine includes
#include "AudioDecompress.h"
#include "DSP/FloatArrayMath.h"
#include "Engine/CurveTable.h"
#include "Interfaces/IAudioFormat.h"
#include "Sound/SoundWaveProcedural.h"

// plugin includes
#include "ACESettings.h"
#include "ACETypes.h"
#include "AnimDataConsumer.h"
#include "AnimDataConsumerRegistry.h"
#include "Audio2FaceParameters.h"
#include "LegacyA2FRemoteModule.h"
#include "LegacyA2FRemotePrivate.h"
#include "nvacl.h"


static const FName GACLA2FProviderName = FName(TEXT("LegacyA2F"));


static FString GetACEStatusString(NvACEStatus Result)
{
	switch (Result)
	{
	case NvACEStatus::NV_ACE_STATUS_ERROR_CONNECTION:
		return FString(TEXT("error communicating with ACE service"));
	case NvACEStatus::NV_ACE_STATUS_ERROR_INVALID_INPUT:
		return FString(TEXT("ACE client library received invalid input from application"));
	case NvACEStatus::NV_ACE_STATUS_ERROR_UNEXPECTED_OUTPUT:
		return FString(TEXT("received output from ACE service that we couldn't handle"));
	case NvACEStatus::NV_ACE_STATUS_ERROR_UNKNOWN:
		return FString(TEXT("ACE unknown error"));
	case NvACEStatus::NV_ACE_STATUS_OK:
		return FString(TEXT("ACE client library success"));
	case NvACEStatus::NV_ACE_STATUS_OK_NO_MORE_FRAMES:
		return FString(TEXT("last frame received from ACE service"));
	default:
		return FString(TEXT("invalid ACE error code"));
	}
}

static FString GetConnectionInfoString(const FACEConnectionInfo& Connection)
{
	FString Result = FString::Printf(TEXT("URL:\"%s\""), *Connection.DestURL);
	if (!Connection.APIKey.IsEmpty())
	{
		// API key might be too sensitive to write to log in some cases, but we can at least verify it begins with "nvapi-"
		Result.Appendf(TEXT(", APIKey:%s***"), *Connection.APIKey.Left(6));
	}
	if (!Connection.NvCFFunctionId.IsEmpty())
	{
		Result.Appendf(TEXT(", NvCFFunctionId:%s"), *Connection.NvCFFunctionId);
	}
	if (!Connection.NvCFFunctionVersion.IsEmpty())
	{
		Result.Appendf(TEXT(", NvCFFunctionVersion:%s"), *Connection.NvCFFunctionVersion);
	}

	return Result;
}


//////
// Our implementation of IA2FProvider::IA2FStream

FName FLegacyA2FRemote::FAudio2FaceContext::GetProviderName() const
{
	return GACLA2FProviderName;
}

static FLegacyA2FRemote::FAudio2FaceContext* CastToA2FContext(IA2FProvider::IA2FStream* Stream)
{
	if (Stream != nullptr)
	{
		if (Stream->GetProviderName() == GACLA2FProviderName)
		{
			return static_cast<FLegacyA2FRemote::FAudio2FaceContext*>(Stream);
		}
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("Expected %s, received %s"), *GACLA2FProviderName.ToString(), *Stream->GetProviderName().ToString());
	}

	return nullptr;
}


//////
// FLegacyA2FRemote implementation

FLegacyA2FRemote* FLegacyA2FRemote::Get()
{
	return static_cast<FLegacyA2FRemote*>(IA2FProvider::FindProvider(GACLA2FProviderName));
}

FLegacyA2FRemote& FLegacyA2FRemote::GetChecked()
{
	FLegacyA2FRemote* Provider = Get();
	check(Provider != nullptr);
	return *Provider;
}

void FLegacyA2FRemote::SetConnectionInfo(const FString& URL, const FString& APIKey, const FString& NvCFFunctionId, const FString& NvCFFunctionVersion)
{
	ACEOverrideConnectionInfo.DestURL = URL;
	ACEOverrideConnectionInfo.APIKey = APIKey;
	ACEOverrideConnectionInfo.NvCFFunctionId = NvCFFunctionId;
	ACEOverrideConnectionInfo.NvCFFunctionVersion = NvCFFunctionVersion;

	// if connection info changed, destroy previous connection immediately
	FACEConnectionInfo NewConnectionInfo = GetConnectionInfo();
	if ((NewConnectionInfo.DestURL != ACEConnectionInfo.DestURL ||
		 NewConnectionInfo.APIKey  != ACEConnectionInfo.APIKey ||
		 NewConnectionInfo.NvCFFunctionId != ACEConnectionInfo.NvCFFunctionId ||
		 NewConnectionInfo.NvCFFunctionVersion != ACEConnectionInfo.NvCFFunctionVersion)
		&& A2XConnection != nullptr)
	{
		nvace_release_a2x_connection(A2XConnection);
		A2XConnection = nullptr;
	}

}

FACEConnectionInfo FLegacyA2FRemote::GetConnectionInfo() const
{
	FACEConnectionInfo ConnectionInfoDefault = GetDefault<UACESettings>()->ACEConnectionInfo;
	FACEConnectionInfo ConnectionInfo = ACEOverrideConnectionInfo;

	if (ConnectionInfo.DestURL.IsEmpty())
	{
		ConnectionInfo.DestURL = ConnectionInfoDefault.DestURL;
	}
	if (ConnectionInfo.APIKey.IsEmpty())
	{
		ConnectionInfo.APIKey = ConnectionInfoDefault.APIKey;
	}
	if (ConnectionInfo.NvCFFunctionId.IsEmpty())
	{
		ConnectionInfo.NvCFFunctionId = ConnectionInfoDefault.NvCFFunctionId;
	}
	if (ConnectionInfo.NvCFFunctionVersion.IsEmpty())
	{
		ConnectionInfo.NvCFFunctionVersion = ConnectionInfoDefault.NvCFFunctionVersion;
	}

	return ConnectionInfo;
}

NvACEA2XConnection* FLegacyA2FRemote::GetA2XConnection(bool bRecreate)
{
	FACEConnectionInfo NewConnectionInfo = GetConnectionInfo();

	bool bConnectionInfoChanged = ACEConnectionInfo != NewConnectionInfo;
	if ((bRecreate || bConnectionInfoChanged) && (A2XConnection != nullptr))
	{
		NvACEStatus Result = nvace_release_a2x_connection(A2XConnection);
		A2XConnection = nullptr;
		UE_LOG(LogACEA2FLegacy, Log, TEXT("Releasing previous A2F-3D connection: %s"), *GetACEStatusString(Result));
	}

	if (A2XConnection == nullptr)
	{
		// create ACL if needed
		if (ACL == nullptr)
		{
			NvACEStatus Result = nvace_create_client_library(&ACL);
			if (Result != NvACEStatus::NV_ACE_STATUS_OK)
			{
				UE_LOG(LogACEA2FLegacy, Warning, TEXT("Failed to create ACE Client Library: %s"), *GetACEStatusString(Result));
				ACL = nullptr;
				A2XConnection = nullptr;
				return A2XConnection;
			}
		}

		// create connection
		FTCHARToUTF8 DestURLUTF8(*NewConnectionInfo.DestURL);
		FTCHARToUTF8 APIKeyUTF8(*NewConnectionInfo.APIKey);
		FTCHARToUTF8 FuncIDUTF8(*NewConnectionInfo.NvCFFunctionId);
		FTCHARToUTF8 FuncVerUTF8(*NewConnectionInfo.NvCFFunctionVersion);

		NvACEConnectionInfo NvConnection = {DestURLUTF8.Get(),
											APIKeyUTF8.Get(),
											FuncIDUTF8.Get(),
											FuncVerUTF8.Get()};

		NvACEStatus Result = nvace_create_a2x_connection(ACL, &NvConnection, &A2XConnection);
		if (Result != NvACEStatus::NV_ACE_STATUS_OK)
		{
			UE_LOG(LogACEA2FLegacy, Warning, TEXT("Failed to connect to A2F-3D service at {%s}: %s"), *GetConnectionInfoString(NewConnectionInfo), *GetACEStatusString(Result));
			A2XConnection = nullptr;
		}
		else
		{
			UE_LOG(LogACEA2FLegacy, Log, TEXT("Connected to A2F-3D service at %s"), *GetConnectionInfoString(NewConnectionInfo));
			ACEConnectionInfo = NewConnectionInfo;
		}
	}

	return A2XConnection;
}

static FACEAnimDataChunk CreateChunkFromACLFrame(const NvACEAnimDataFrame* Frame, TArray<FName>& MemorySpaceForChunkNames)
{
	FACEAnimDataChunk Chunk;
	if (Frame == nullptr)
	{
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("Null frame received from ACL"));
		Chunk.Status = EACEAnimDataStatus::ERROR_UNEXPECTED_OUTPUT;
		return Chunk;
	}

	// fun fact, there's the equivalent of a reintepret_cast _and_ a const_cast hidden in this BlendShapeNames BitCast
	TArrayView<const UTF8CHAR*> BlendShapeNames = MakeArrayView<const UTF8CHAR*>(BitCast<const UTF8CHAR**>(Frame->blend_shape_names), Frame->blend_shape_name_count);
	for (const UTF8CHAR* BlendShapeName : BlendShapeNames)
	{
		MemorySpaceForChunkNames.Add(BlendShapeName);
	}
	Chunk.BlendShapeNames = MemorySpaceForChunkNames;
	Chunk.BlendShapeWeights = MakeArrayView<const float>(Frame->blend_shape_weights, Frame->blend_shape_weight_count);
	Chunk.AudioBuffer = MakeArrayView<const uint8>(BitCast<const uint8*>(Frame->audio_samples), Frame->audio_sample_count * sizeof (int16_t));
	Chunk.Timestamp = Frame->timestamp;
	switch (Frame->status)
	{
	case NV_ACE_STATUS_OK:
		Chunk.Status = EACEAnimDataStatus::OK;
		break;
	case NV_ACE_STATUS_OK_NO_MORE_FRAMES:
		Chunk.Status = EACEAnimDataStatus::OK_NO_MORE_DATA;
		break;
	case NV_ACE_STATUS_ERROR_UNEXPECTED_OUTPUT:
		Chunk.Status = EACEAnimDataStatus::ERROR_UNEXPECTED_OUTPUT;
		break;
	default:
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("Unexpected ACL frame status %s"), *GetACEStatusString(Frame->status));
		Chunk.Status = EACEAnimDataStatus::ERROR_UNEXPECTED_OUTPUT;
	}

	return Chunk;
}

static void AnimDataFrameCallback(const NvACEAnimDataFrame* Frame, AnimDataContext InContext)
{
	FLegacyA2FRemote::FAudio2FaceContext* ACEContext = reinterpret_cast<FLegacyA2FRemote::FAudio2FaceContext*>(InContext);
	if (ensure(ACEContext != nullptr))
	{
		// TODO: check that context is still valid by checking that it's still in our list of active contexts
		// will require some sort of locking so that ACEContext doesn't get deleted under our noses
		int32 StreamID = ACEContext->StreamID;
		if (StreamID != IA2FProvider::IA2FStream::INVALID_STREAM_ID)
		{
			FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
			if (Registry != nullptr)
			{
				TArray<FName> TempNames;
				Registry->SendAnimData_AnyThread(CreateChunkFromACLFrame(Frame, TempNames), StreamID);
			}
		}
		if (Frame->status == NvACEStatus::NV_ACE_STATUS_OK_NO_MORE_FRAMES)
		{
			// this session is over
			FLegacyA2FRemote* ACEManager = FLegacyA2FRemote::Get();
			if (ACEManager != nullptr)
			{
				ACEManager->RemoveContext(StreamID);
			}
		}
	}
}

FLegacyA2FRemote::~FLegacyA2FRemote()
{
	// release parameters
	if (A2XParameterHandle != nullptr)
	{
		nvace_release_a2x_params(A2XParameterHandle);
		A2XParameterHandle = nullptr;
	}

	// end sessions
	if (ACL != nullptr)
	{
		for (FAudio2FaceContext& Context : Contexts)
		{
			if ((Context.StreamID != IA2FStream::INVALID_STREAM_ID) && (Context.Session != nullptr))
			{
				Context.StreamID = IA2FStream::INVALID_STREAM_ID;
				nvace_release_a2x_session(ACL, Context.Session, true);
				Context.Session = nullptr;
			}
		}
	}

	// close connection
	if (A2XConnection != nullptr)
	{
		nvace_release_a2x_connection(A2XConnection);
	}

	// release ACL
	if (ACL != nullptr)
	{
		nvace_release_client_library(ACL);
	}
}

IA2FProvider::IA2FStream* FLegacyA2FRemote::CreateA2FStream(IACEAnimDataConsumer* CallbackObject)
{
	NvACEA2XConnection* Connection = GetA2XConnection(false);
	if (Connection == nullptr)
	{
		return nullptr;
	}

	FAudio2FaceContext* Context = AddContext(CallbackObject);

	// Create session and send audio to a2x
	NvACEStatus Result = nvace_create_a2x_session(ACL, Connection, &AnimDataFrameCallback, Context, &Context->Session);

	// If we get NvACEStatus::NV_ACE_STATUS_ERROR_CONNECTION, it's likely because the connection timed out and needs to be re-established
	if (Result == NvACEStatus::NV_ACE_STATUS_ERROR_CONNECTION)
	{
		// attempt to recreate the connection and try again
		UE_LOG(LogACEA2FLegacy, Log, TEXT("Recreating A2F-3D connection at %s"), *GetA2FURL());
		Connection = GetA2XConnection(true);
		if (Connection != nullptr)
		{
			Result = nvace_create_a2x_session(ACL, Connection, &AnimDataFrameCallback, Context, &Context->Session);
		}
	}

	if (Result != NvACEStatus::NV_ACE_STATUS_OK)
	{
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("Failed to create A2F-3D session: %s"), *GetACEStatusString(Result));
		Context->Session = nullptr;
		RemoveContext(Context->StreamID);
		return nullptr;
	}

	return Context;
}

static TOptional<NvACEEmotionState> ToNvEmotionState(const TOptional<FAudio2FaceEmotion>& InEmotionParams)
{
	if (!InEmotionParams.IsSet())
	{
		return NullOpt;
	}

	if (!InEmotionParams->IsEmotionOverrideActive())
	{
		return NullOpt;
	}

	NvACEEmotionState OutEmotion;
	const FAudio2FaceEmotionOverride& InEmotion = InEmotionParams->EmotionOverrides;
	const float UNSET = -1.0f;	// values outside the range 0.0 - 1.0 are ignored by ACL
	OutEmotion.amazement = InEmotion.bOverrideAmazement ? InEmotion.Amazement : UNSET;
	OutEmotion.anger = InEmotion.bOverrideAnger ? InEmotion.Anger : UNSET;
	OutEmotion.cheekiness = InEmotion.bOverrideCheekiness ? InEmotion.Cheekiness : UNSET;
	OutEmotion.disgust = InEmotion.bOverrideDisgust ? InEmotion.Disgust : UNSET;
	OutEmotion.fear = InEmotion.bOverrideFear ? InEmotion.Fear : UNSET;
	OutEmotion.grief = InEmotion.bOverrideGrief ? InEmotion.Grief : UNSET;
	OutEmotion.joy = InEmotion.bOverrideJoy ? InEmotion.Joy : UNSET;
	OutEmotion.out_of_breath = InEmotion.bOverrideOutOfBreath ? InEmotion.OutOfBreath : UNSET;
	OutEmotion.pain = InEmotion.bOverridePain ? InEmotion.Pain : UNSET;
	OutEmotion.sadness = InEmotion.bOverrideSadness ? InEmotion.Sadness : UNSET;

	return OutEmotion;
}

static TOptional<NvACEEmotionParameters> ToNvEmotionParameters(const TOptional<FAudio2FaceEmotion> InEmotionParameters)
{
	if (!InEmotionParameters.IsSet())
	{
		return NullOpt;
	}

	NvACEEmotionParameters NvParams;
	NvParams.emotion_contrast = InEmotionParameters->DetectedEmotionContrast;
	NvParams.live_blend_coef = InEmotionParameters->DetectedEmotionSmoothing;
	NvParams.enable_preferred_emotion = InEmotionParameters->bEnableEmotionOverride;
	NvParams.preferred_emotion_strength = InEmotionParameters->EmotionOverrideStrength;
	NvParams.emotion_strength = InEmotionParameters->OverallEmotionStrength;
	NvParams.max_emotions = InEmotionParameters->MaxDetectedEmotions;

	return NvParams;
}



bool FLegacyA2FRemote::SendAudioSamples(
	IA2FStream* Stream,
	TArrayView<const int16> SamplesInt16,
	TOptional<FAudio2FaceEmotion> EmotionParameters,
	UAudio2FaceParameters* Audio2FaceParameters)
{
	NvACEA2XConnection* Connection = GetA2XConnection(false);
	if (Connection == nullptr)
	{
		return false;
	}
	FAudio2FaceContext* ACLStream = CastToA2FContext(Stream);
	if (ACLStream == nullptr)
	{
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	NvACEStatus Result = nvace_send_audio_samples(ACL, Connection, ACLStream->Session, SamplesInt16.GetData(), SamplesInt16.Num(),
												  ToNvEmotionState(EmotionParameters).GetPtrOrNull(),
												  ToNvEmotionParameters(EmotionParameters).GetPtrOrNull(),
												  GetA2XParameterHandle(Audio2FaceParameters) );
	if (Result != NvACEStatus::NV_ACE_STATUS_OK)
	{
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("Failed to send audio samples to A2F-3D: %s"), *GetACEStatusString(Result));
		return false;
	}

	return true;
}

bool FLegacyA2FRemote::EndOutgoingStream(IA2FStream* Stream)
{
	FAudio2FaceContext* ACLStream = CastToA2FContext(Stream);
	if (ACLStream == nullptr)
	{
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	NvACEStatus Result = nvace_close_a2x_session(ACLStream->Session);

	if (Result != NvACEStatus::NV_ACE_STATUS_OK)
	{
		UE_LOG(LogACEA2FLegacy, Warning, TEXT("Failed to close session: %s"), *GetACEStatusString(Result));
		return false;
	}

	return true;
}

int32 FLegacyA2FRemote::GetMinimumInitialAudioSampleCount() const
{
	return AUDIO_PREFERRED_CHUNK_SIZE;
}

FName FLegacyA2FRemote::GetName() const
{
	return GACLA2FProviderName;
}

void FLegacyA2FRemote::RemoveContext(int32 StreamID)
{
	for (FAudio2FaceContext& Context : Contexts)
	{
		if (Context.StreamID == StreamID)
		{
			Context.StreamID = IA2FStream::INVALID_STREAM_ID;
			Context.Session = nullptr;
		}
	}
}

FLegacyA2FRemote::FAudio2FaceContext* FLegacyA2FRemote::AddContext(IACEAnimDataConsumer* CallbackObject)
{
    FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry == nullptr)
	{
		return nullptr;
	}

	// first look for an available entry to reuse
	for (FAudio2FaceContext& Context : Contexts)
	{
		if (Context.StreamID == IA2FStream::INVALID_STREAM_ID)
		{
			Context.StreamID = Registry->CreateStream_AnyThread();
			Registry->AttachConsumerToStream_AnyThread(Context.StreamID, CallbackObject);
			return &Context;
		}
	}

	// no suitable entry, so create a new one
	int32 NewIdx = Contexts.Add();
	FAudio2FaceContext& Context = Contexts[NewIdx];

	Context.StreamID = Registry->CreateStream_AnyThread();
	Registry->AttachConsumerToStream_AnyThread(Context.StreamID, CallbackObject);
	return &Context;
}

NvACEA2XParameters* FLegacyA2FRemote::GetA2XParameterHandle(UAudio2FaceParameters* InParameters)
{
	if (InParameters == nullptr)
	{
		return nullptr;
	}

	// create parameter handle if necessary
	if (A2XParameterHandle == nullptr)
	{
		nvace_create_a2x_params(&A2XParameterHandle);
	}

	const TMap<FString, float>& NewParams = InParameters->Audio2FaceParameterMap;

	// remove any extra parameters that shouldn't be there
	TArray<FString> CachedKeys;
	CachedA2FParams.GetKeys(CachedKeys);
	for (const FString& CachedKey : CachedKeys)
	{
		if (!NewParams.Contains(CachedKey))
		{
			nvace_clear_a2x_param(A2XParameterHandle, TCHAR_TO_UTF8(*CachedKey));
			CachedA2FParams.Remove(CachedKey);
		}
	}

	// add or update parameters
	for (const TPair<FString, float>& NewParam : NewParams)
	{
		float* MaybeCachedVal = CachedA2FParams.Find(NewParam.Key);
		if ((MaybeCachedVal == nullptr) || (*MaybeCachedVal != NewParam.Value))
		{
			nvace_set_a2x_param(A2XParameterHandle, TCHAR_TO_UTF8(*NewParam.Key), NewParam.Value);
			CachedA2FParams.FindOrAdd(NewParam.Key, NewParam.Value);
		}
	}

	check(CachedA2FParams.Num() == NewParams.Num());

	return A2XParameterHandle;
}

