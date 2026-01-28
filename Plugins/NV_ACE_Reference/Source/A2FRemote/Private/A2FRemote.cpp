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

#include "A2FRemote.h"

// engine includes
#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformHttp.h"

// plugin includes
#include "A2FRemoteModule.h"
#include "A2FRemotePrivate.h"
#include "ACESettings.h"
#include "AIMA2FContext.h"
#include "AIMModule.h"
#include "AnimDataConsumerRegistry.h"
#include "nvaim.h"
#include "nvaim_a2f.h"
#include "nvaim_a2x.h"
#include "nvaim_animgraph.h"
#include "nvaim_cloud.h"


static const FName GRemoteA2FProviderName = FName(TEXT("RemoteA2F"));

static FString GetConnectionInfoString(const FACEConnectionInfo& Connection)
{
	FString Result = FString::Printf(TEXT("URL:\"%s\""), *Connection.DestURL);
	if (!Connection.APIKey.IsEmpty())
	{
		// API key might be too sensitive to write to log in some cases, but we can at least verify it begins with "nvapi-"
		if (Connection.APIKey.StartsWith("nvapi-"))
		{
			Result.Append(TEXT(", APIKey:***"));
		}
		else
		{
			Result.Append(TEXT(", APIKey:<invalid-key>"));
		}
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
// FA2FRemote implementation

FA2FRemote::FA2FRemote() : Interface(nullptr)
{
	FAIMModule::Get().RegisterAIMFeature(nvaim::plugin::a2x::cloud::grpc::kId, {}, {nvaim::plugin::animgraph::kId});
	bIsFeatureAvailable = FAIMModule::Get().IsAIMFeatureAvailable(nvaim::plugin::a2x::cloud::grpc::kId);
	if (!bIsFeatureAvailable)
	{
		UE_LOG(LogACEA2FRemote, Log, TEXT("Unable to load AIM a2f.cloud feature, %s provider won't be available"), *GRemoteA2FProviderName.ToString());
		Interface = nullptr;
	}
}

FA2FRemote::~FA2FRemote()
{
	if (Interface != nullptr)
	{
		// end sessions so that it's safe to close the connection
		FAIMA2FStreamContextProvider* ContextProvider = FAIMA2FStreamContextProvider::Get();
		if (ContextProvider != nullptr)
		{
			ContextProvider->KillAllActiveContexts(GRemoteA2FProviderName);
		}

		if (Connection.IsValid())
		{
			// close connection
			FAIMInferenceInstanceRef ConnectionRef(Connection.Get());
			ConnectionRef.DestroyInstance(Interface);
		}
		Connection.Reset();

		// unload feature interface
		FAIMModule::Get().UnloadAIMFeature(nvaim::plugin::a2x::cloud::grpc::kId, Interface);
	}
}

IA2FProvider::IA2FStream* FA2FRemote::CreateA2FStream(IACEAnimDataConsumer* CallbackObject)
{
	bool bConnectionAvailable = IsConnectionAvailable();
	FAIMA2FStreamContextProvider* ContextProvider = FAIMA2FStreamContextProvider::Get();
	if ((ContextProvider != nullptr) && bConnectionAvailable && Connection.IsValid())
	{
		return ContextProvider->CreateA2FContext(GRemoteA2FProviderName, CallbackObject, *Connection, NullOpt);
	}

	return nullptr;
}

bool FA2FRemote::SendAudioSamples(
	IA2FStream* Stream,
	TArrayView<const int16> SamplesInt16,
	TOptional<FAudio2FaceEmotion> EmotionParameters,
	UAudio2FaceParameters* Audio2FaceParameters)
{
	if (!IsConnectionAvailable())
	{
		return false;
	}
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, GRemoteA2FProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FRemote, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	return A2FStream->SendAudioChunk(SamplesInt16, EmotionParameters, Audio2FaceParameters);
}

bool FA2FRemote::EndOutgoingStream(IA2FStream* Stream)
{
	if (!IsConnectionAvailable())
	{
		return false;
	}
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, GRemoteA2FProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FRemote, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	A2FStream->EndStream();
	return true;
}

FName FA2FRemote::GetName() const
{
	return GRemoteA2FProviderName;
}

void FA2FRemote::FreeResources()
{
	if (Connection.IsValid())
	{
		UE_LOG(LogACEA2FRemote, Log, TEXT("Disconnection of %s requested"), *GetName().ToString());
		// safely disconnect from service
		FAIMInferenceInstanceRef ConnectionRef(Connection.Get());
		ConnectionRef.DestroyInstance(Interface);
		UE_LOG(LogACEA2FRemote, Log, TEXT("%s disconnected"), *GetName().ToString());
	}
}

void FA2FRemote::SetConnectionInfo(const FString& URL, const FString& APIKey, const FString& NvCFFunctionId, const FString& NvCFFunctionVersion)
{
	ACEOverrideConnectionInfo.DestURL = URL;
	ACEOverrideConnectionInfo.APIKey = APIKey;
	ACEOverrideConnectionInfo.NvCFFunctionId = NvCFFunctionId;
	ACEOverrideConnectionInfo.NvCFFunctionVersion = NvCFFunctionVersion;

	// We'd like to destroy the previous connection immediately if the connection info changed. But that could block
	// the caller if there is still an active session, since it's unsafe to destroy the connection while it's in use.
	// So instead we just set the override info here and it will take effect the next time someone tries to send audio.
}

FACEConnectionInfo FA2FRemote::GetConnectionInfo() const
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

void FA2FRemote::SetOriginalAudioParams(IA2FStream* Stream, uint32 SampleRate, int32 NumChannels, int32 SampleByteSize)
{
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, GRemoteA2FProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FRemote, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	if ((SampleRate != DEFAULT_SAMPLE_RATE) || (NumChannels != DEFAULT_NUM_CHANNELS) || (SampleByteSize != DEFAULT_SAMPLE_BYTE_SIZE))
	{
		FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
		if (ensure(Registry != nullptr))
		{
			Registry->SetAudioParams_AnyThread(Stream->GetID(), SampleRate, NumChannels, SampleByteSize);

			// AIM doesn't give us the animation timestamps that the A2F-3D service outputs, so we have to work out indirectly how many original samples we'll need.
			// We figure out a ratio here that will be used in the callback from AIM, and apply it to the number of samples coming out of the service.
			int32 Numerator = static_cast<int32>(SampleRate) * NumChannels * SampleByteSize;
			int32 Denominator = static_cast<int32>(DEFAULT_SAMPLE_RATE) * DEFAULT_NUM_CHANNELS * DEFAULT_SAMPLE_BYTE_SIZE;
			int32 GCD = FMath::GreatestCommonDivisor(Numerator, Denominator);
			A2FStream->SetOriginalAudioSampleConversion(Numerator / GCD, Denominator / GCD, SampleByteSize * NumChannels);
		}
	}
	else
	{
		A2FStream->SetOriginalAudioSampleConversion(0, 0, 0);
	}
}

void FA2FRemote::EnqueueOriginalSamples(IA2FStream* Stream, TArrayView<const uint8> OriginalSamples)
{
	FAIMA2FStreamContext* A2FStream = CastToAIMA2FContext(Stream, GRemoteA2FProviderName);
	if (A2FStream == nullptr)
	{
		UE_LOG(LogACEA2FRemote, Warning, TEXT("%s called without a valid stream"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	A2FStream->EnqueueOriginalSamples(OriginalSamples);
}

static FString CreateMetaDataString(const FString& TokenString, const FString& FuntionIDString, const FString& FunctionVersionIDString)
{
	FString ResultString = "";

	if (!TokenString.IsEmpty())
	{
		ResultString += TEXT("authorization,Bearer ") + TokenString;
	}

	if (!FuntionIDString.IsEmpty())
	{
		if (!ResultString.IsEmpty())
		{
			ResultString += TEXT(",");
		}

		ResultString += TEXT("function-id,") + FuntionIDString;
	}

	if (!FunctionVersionIDString.IsEmpty())
	{
		if (!ResultString.IsEmpty())
		{
			ResultString += TEXT(",");
		}

		ResultString += (TEXT("function-version-id,") + FunctionVersionIDString);
	}

	return ResultString;
}

bool FA2FRemote::IsConnectionAvailable()
{
	if (!bIsFeatureAvailable)
	{
		return false;
	}

	// this CS protects us intially fiddling with Interface and Connection, which shouldn't change after the first time through here
	static FCriticalSection InitialConnectionCreationCS{};
	FScopeLock Lock(&InitialConnectionCreationCS);

	if (Interface == nullptr)
	{
		// load the feature interface if it's not already loaded
		nvaim::Result Result = FAIMModule::Get().LoadAIMFeature(nvaim::plugin::a2x::cloud::grpc::kId, &Interface);
		if (Result != nvaim::ResultOk)
		{
			UE_LOG(LogACEA2FRemote, Error, TEXT("Unable to load AIM a2f.cloud feature: %s"), *GetAIMStatusString(Result));
			Interface = nullptr;
			return false;
		}
	}

	if (Interface != nullptr)
	{
		FACEConnectionInfo NewConnectionInfo = GetConnectionInfo();
		bool bConnectionInfoChanged = ACEConnectionInfo != NewConnectionInfo;
		if (bConnectionInfoChanged && Connection.IsValid())
		{
			// Unfortunately AIM doesn't allow connecting to multiple servers simultaneously, so this could block until other
			// connections are done
			FAIMInferenceInstanceRef ConnectionRef(Connection.Get());
			UE_LOG(LogACEA2FRemote, Log, TEXT("Connection info changed, closing previous A2F-3D connection"));
			ConnectionRef.DestroyInstance(Interface);
			const nvaim::InferenceInstance* NoConnection = nullptr;
			if (ConnectionRef == NoConnection)
			{
				return false;
			}
		}

		if (!Connection.IsValid())
		{
			nvaim::InferenceInstance* RawConnection = CreateConnection();
			if (RawConnection != nullptr)
			{
				Connection = MakePimpl<FAIMInferenceInstance>(RawConnection, [this]() { return CreateConnection(); });
			}
			else
			{
				Connection.Reset();
			}
		}
	}

	return Connection.IsValid();
}

nvaim::InferenceInstance* FA2FRemote::CreateConnection()
{
	static FCriticalSection ConnectionCreationCS{};
	FScopeLock Lock(&ConnectionCreationCS);

	if (Interface != nullptr)
	{
		FACEConnectionInfo ConnectionInfo = GetConnectionInfo();

		nvaim::Audio2FaceCreationParameters A2FCreationParams;

		// connection parameters
		FString URLWithoutScheme = FGenericPlatformHttp::GetUrlDomainAndPort(ConnectionInfo.DestURL);
		auto URLWithoutSchemeUTF8 = StringCast<UTF8CHAR>(*URLWithoutScheme);
		TOptional<bool> MaybeIsHttps = FGenericPlatformHttp::IsSecureProtocol(ConnectionInfo.DestURL);

		FString MetaDataString = CreateMetaDataString(ConnectionInfo.APIKey, ConnectionInfo.NvCFFunctionId, ConnectionInfo.NvCFFunctionVersion);
		auto MetaDataStringUTF8 = StringCast<UTF8CHAR>(*MetaDataString);

		nvaim::RPCParameters GRPCParams{};

		GRPCParams.url = reinterpret_cast<const char*>(URLWithoutSchemeUTF8.Get());
		GRPCParams.useSSL = MaybeIsHttps.IsSet() ? *MaybeIsHttps : false;	// assume http scheme if not specified
		GRPCParams.metaData = reinterpret_cast<const char*>(MetaDataStringUTF8.Get());
		A2FCreationParams.chain(GRPCParams);

		// This string seems to correspond to an AIM Models subfolder
		const char* MODEL_STRING = "{CA7BC62F-BCF5-4981-926E-01CE7E1C6E35}";

		// common parameters
		nvaim::CommonCreationParameters A2FCommonCreationParams;
		A2FCommonCreationParams.numThreads = 4;		// just guessing, I don't know how to tell how many threads AIM needs
		A2FCommonCreationParams.vramBudgetMB = 0;	// If AIM uses any VRAM at all in its gRPC implementation, something has gone horribly awry
		A2FCommonCreationParams.modelGUID = MODEL_STRING;
		auto ConvertedString = StringCast<UTF8CHAR>(*FAIMModule::Get().GetModelDirectory());
		A2FCommonCreationParams.utf8PathToModels = reinterpret_cast<const char*>(ConvertedString.Get());
		A2FCreationParams.common = &A2FCommonCreationParams;

		// enable streaming
		nvaim::A2XStreamingParameters StreamingParams{};
		StreamingParams.streaming = true;
		A2FCreationParams.chain(StreamingParams);

		nvaim::InferenceInstance* NewConnection = nullptr;
		nvaim::Result Result = Interface->createInstance(A2FCreationParams, &NewConnection);
		if (Result == nvaim::ResultOk)
		{
			UE_LOG(LogACEA2FRemote, Log, TEXT("Connected to A2F-3D service at %s"), *GetConnectionInfoString(ConnectionInfo));
			ACEConnectionInfo = ConnectionInfo;
			return NewConnection;
		}
		else
		{
			UE_LOG(LogACEA2FRemote, Warning, TEXT("Failed to connect to A2F-3D service at {%s}: %s"), *GetConnectionInfoString(ConnectionInfo), *GetAIMStatusString(Result));
		}
	}

	return nullptr;
}

