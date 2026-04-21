// Copyright UoA eResearch. MIT License.

#include "NPCSTTAsync.h"

#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "AudioCapture.h"

#include "NPCConversationModule.h"
#include "NPCConversationSettings.h"

// ─────────────────────────────────────────────────────────────────────────────

UNPCSTTAsync* UNPCSTTAsync::AsyncRecordAndTranscribe(UObject* WorldContextObject,
	float RecordingDurationSeconds)
{
	UNPCSTTAsync* Action = NewObject<UNPCSTTAsync>();
	Action->RecordingDurationSeconds = RecordingDurationSeconds;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UNPCSTTAsync::Activate()
{
	const UNPCConversationSettings* Settings = GetDefault<UNPCConversationSettings>();
	const float Duration = (RecordingDurationSeconds > 0.0f)
		? RecordingDurationSeconds
		: (Settings ? Settings->DefaultRecordingDuration : 5.0f);

	UE_LOG(LogNPCConversation, Log, TEXT("STT: Starting microphone capture for %.1f s"), Duration);

	// Run the blocking capture on a background thread so we don't stall the game thread.
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, Duration]()
	{
		Audio::FAudioCapture AudioCapture;
		Audio::FAudioCaptureDeviceParams DeviceParams; // default device

		if (!AudioCapture.OpenDefaultCaptureStream(DeviceParams))
		{
			UE_LOG(LogNPCConversation, Error, TEXT("STT: Failed to open default capture stream."));
			BroadcastFailure();
			return;
		}

		// Retrieve device info to know the native sample rate and channel count.
		Audio::FCaptureDeviceInfo DeviceInfo;
		AudioCapture.GetCaptureDeviceInfo(DeviceInfo);
		const int32 NativeSampleRate = DeviceInfo.PreferredSampleRate > 0
			? DeviceInfo.PreferredSampleRate : 44100;
		const int32 NumChannels = DeviceInfo.InputChannels > 0
			? DeviceInfo.InputChannels : 1;

		UE_LOG(LogNPCConversation, Log,
			TEXT("STT: Capture device: %s | %d Hz | %d ch"),
			*DeviceInfo.DeviceName, NativeSampleRate, NumChannels);

		AudioCapture.StartCaptureStream();

		// Poll the ring-buffer every 50 ms until the requested duration has elapsed.
		TArray<float> AllSamples;
		AllSamples.Reserve(static_cast<int32>(Duration * NativeSampleRate * NumChannels) + 1024);

		const float PollIntervalSec = 0.05f;
		const int32 NumPolls = FMath::CeilToInt(Duration / PollIntervalSec);

		for (int32 i = 0; i < NumPolls; ++i)
		{
			FPlatformProcess::Sleep(PollIntervalSec);

			TArray<float> Chunk;
			AudioCapture.GetAudioData(Chunk);
			AllSamples.Append(Chunk);
		}

		AudioCapture.StopCaptureStream();
		AudioCapture.CloseCaptureStream();

		if (AllSamples.IsEmpty())
		{
			UE_LOG(LogNPCConversation, Error, TEXT("STT: No audio samples captured."));
			BroadcastFailure();
			return;
		}

		UE_LOG(LogNPCConversation, Log,
			TEXT("STT: Captured %d samples at %d Hz (%d ch). Sending to Whisper."),
			AllSamples.Num(), NativeSampleRate, NumChannels);

		// Downmix to mono then encode as WAV.
		TArray<float> MonoSamples;
		if (NumChannels > 1)
		{
			MonoSamples.Reserve(AllSamples.Num() / NumChannels);
			for (int32 s = 0; s < AllSamples.Num(); s += NumChannels)
			{
				float Sum = 0.0f;
				for (int32 c = 0; c < NumChannels; ++c)
				{
					Sum += AllSamples[s + c];
				}
				MonoSamples.Add(Sum / static_cast<float>(NumChannels));
			}
		}
		else
		{
			MonoSamples = MoveTemp(AllSamples);
		}

		TArray<uint8> WavData = BuildWavFromFloatSamples(MonoSamples, NativeSampleRate, 1);

		// HTTP transcription must run on the game thread's HTTP manager but the
		// bind callback will come back on the game thread, so it's safe to call
		// SendToWhisper from here.
		AsyncTask(ENamedThreads::GameThread, [this, WavData = MoveTemp(WavData)]()
		{
			SendToWhisper(WavData);
		});
	});
}

// ─── Whisper API ─────────────────────────────────────────────────────────────

void UNPCSTTAsync::SendToWhisper(const TArray<uint8>& WavData)
{
	const UNPCConversationSettings* Settings = GetDefault<UNPCConversationSettings>();
	if (!Settings)
	{
		BroadcastFailure();
		return;
	}

	FString URL = Settings->STTBaseURL;
	if (!URL.EndsWith(TEXT("/")))
	{
		URL += TEXT("/");
	}
	URL += TEXT("audio/transcriptions");

	// Build multipart/form-data body manually.
	const FString Boundary = TEXT("----NPCConvBoundary7MA4YWxkTrZu0gW");

	TArray<uint8> Body;

	auto AppendStr = [&Body](const FString& Str)
	{
		FTCHARToUTF8 Conv(*Str);
		Body.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
	};

	// ── model field ──
	AppendStr(FString::Printf(TEXT("--%s\r\n"), *Boundary));
	AppendStr(TEXT("Content-Disposition: form-data; name=\"model\"\r\n\r\n"));
	AppendStr(Settings->STTModel);
	AppendStr(TEXT("\r\n"));

	// ── response_format field (plain text) ──
	AppendStr(FString::Printf(TEXT("--%s\r\n"), *Boundary));
	AppendStr(TEXT("Content-Disposition: form-data; name=\"response_format\"\r\n\r\n"));
	AppendStr(TEXT("json"));
	AppendStr(TEXT("\r\n"));

	// ── audio file ──
	AppendStr(FString::Printf(TEXT("--%s\r\n"), *Boundary));
	AppendStr(TEXT("Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"));
	AppendStr(TEXT("Content-Type: audio/wav\r\n\r\n"));
	Body.Append(WavData);
	AppendStr(TEXT("\r\n"));

	// ── closing boundary ──
	AppendStr(FString::Printf(TEXT("--%s--\r\n"), *Boundary));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),
		FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
	if (!Settings->STTAPIKey.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"),
			FString::Printf(TEXT("Bearer %s"), *Settings->STTAPIKey));
	}
	Request->SetContent(Body);
	Request->SetTimeout(Settings->STTTimeoutSeconds);
	Request->OnProcessRequestComplete().BindUObject(this, &UNPCSTTAsync::OnWhisperResponse);

	UE_LOG(LogNPCConversation, Log, TEXT("STT: Sending %d bytes to %s"), Body.Num(), *URL);

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("STT: Failed to start Whisper HTTP request."));
		BroadcastFailure();
	}
}

void UNPCSTTAsync::OnWhisperResponse(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("STT: HTTP request failed."));
		BroadcastFailure();
		return;
	}

	const int32 StatusCode = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();

	if (StatusCode < 200 || StatusCode >= 300)
	{
		UE_LOG(LogNPCConversation, Error, TEXT("STT: HTTP %d — %s"), StatusCode, *Body);
		BroadcastFailure();
		return;
	}

	// OpenAI Whisper returns: { "text": "transcribed text" }
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("STT: Could not parse Whisper JSON response."));
		BroadcastFailure();
		return;
	}

	FString Text;
	if (!JsonObj->TryGetStringField(TEXT("text"), Text))
	{
		UE_LOG(LogNPCConversation, Error, TEXT("STT: No 'text' field in response: %s"), *Body);
		BroadcastFailure();
		return;
	}

	Text = Text.TrimStartAndEnd();
	UE_LOG(LogNPCConversation, Log, TEXT("STT: Transcription: %s"), *Text);
	BroadcastSuccess(Text);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

TArray<uint8> UNPCSTTAsync::BuildWavFromFloatSamples(
	const TArray<float>& Samples, int32 SampleRate, int32 NumChannels)
{
	// Convert float [-1, 1] → int16
	TArray<int16> PCM;
	PCM.Reserve(Samples.Num());
	for (float S : Samples)
	{
		PCM.Add(static_cast<int16>(FMath::Clamp(S, -1.0f, 1.0f) * 32767.0f));
	}

	const int32 PCMBytes    = PCM.Num() * sizeof(int16);
	const int16 BitsPerSample   = 16;
	const int32 ByteRate    = SampleRate * NumChannels * (BitsPerSample / 8);
	const int16 BlockAlign  = static_cast<int16>(NumChannels * (BitsPerSample / 8));
	const int32 ChunkSize   = 36 + PCMBytes;

	TArray<uint8> Wav;
	Wav.Reserve(44 + PCMBytes);

	auto Append4CC  = [&](const char* s) { Wav.Append(reinterpret_cast<const uint8*>(s), 4); };
	auto AppendI16  = [&](int16 v)       { Wav.Append(reinterpret_cast<const uint8*>(&v), 2); };
	auto AppendI32  = [&](int32 v)       { Wav.Append(reinterpret_cast<const uint8*>(&v), 4); };

	Append4CC("RIFF");
	AppendI32(ChunkSize);
	Append4CC("WAVE");
	Append4CC("fmt ");
	AppendI32(16);                          // fmt chunk size
	AppendI16(1);                           // PCM format
	AppendI16(static_cast<int16>(NumChannels));
	AppendI32(SampleRate);
	AppendI32(ByteRate);
	AppendI16(BlockAlign);
	AppendI16(BitsPerSample);
	Append4CC("data");
	AppendI32(PCMBytes);
	Wav.Append(reinterpret_cast<const uint8*>(PCM.GetData()), PCMBytes);

	return Wav;
}

void UNPCSTTAsync::BroadcastSuccess(const FString& Text)
{
	AsyncTask(ENamedThreads::GameThread, [this, Text]()
	{
		OnSuccess.Broadcast(Text, true);
		SetReadyToDestroy();
	});
}

void UNPCSTTAsync::BroadcastFailure()
{
	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
	});
}
