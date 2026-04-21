// Copyright UoA eResearch. MIT License.

#include "NPCTTSAsync.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "NPCConversationModule.h"
#include "NPCConversationSettings.h"

// ─────────────────────────────────────────────────────────────────────────────

UNPCTTSAsync* UNPCTTSAsync::AsyncSpeakText(UObject* WorldContextObject, const FString& Text)
{
	UNPCTTSAsync* Action = NewObject<UNPCTTSAsync>();
	Action->TextToSpeak = Text;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UNPCTTSAsync::Activate()
{
	if (TextToSpeak.IsEmpty())
	{
		UE_LOG(LogNPCConversation, Warning, TEXT("TTS: TextToSpeak is empty, aborting."));
		BroadcastFailure();
		return;
	}

	const UNPCConversationSettings* Settings = GetDefault<UNPCConversationSettings>();

	if (Settings && Settings->TTSProvider == ENPCTTSProvider::ElevenLabs
		&& !Settings->ElevenLabsAPIKey.IsEmpty())
	{
		TryElevenLabs();
	}
	else
	{
		// No ElevenLabs key or SystemOnly mode — go straight to system TTS.
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]()
		{
			RunSystemTTS();
		});
	}
}

// ─── ElevenLabs ──────────────────────────────────────────────────────────────

void UNPCTTSAsync::TryElevenLabs()
{
	const UNPCConversationSettings* Settings = GetDefault<UNPCConversationSettings>();
	check(Settings);

	// ElevenLabs v1 TTS endpoint with PCM output at 22050 Hz
	const FString URL = FString::Printf(
		TEXT("https://api.elevenlabs.io/v1/text-to-speech/%s?output_format=pcm_22050"),
		*Settings->ElevenLabsVoiceID);

	// JSON body
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("text"), TextToSpeak);
	Body->SetStringField(TEXT("model_id"), Settings->ElevenLabsModelID);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("xi-api-key"), Settings->ElevenLabsAPIKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(BodyStr);
	Request->SetTimeout(Settings->TTSTimeoutSeconds);
	Request->OnProcessRequestComplete().BindUObject(this, &UNPCTTSAsync::OnElevenLabsResponse);

	UE_LOG(LogNPCConversation, Log, TEXT("TTS: Sending to ElevenLabs (voice=%s, model=%s)"),
		*Settings->ElevenLabsVoiceID, *Settings->ElevenLabsModelID);

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogNPCConversation, Warning, TEXT("TTS: Failed to start ElevenLabs request. Falling back to system TTS."));
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]() { RunSystemTTS(); });
	}
}

void UNPCTTSAsync::OnElevenLabsResponse(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogNPCConversation, Warning,
			TEXT("TTS: ElevenLabs request failed (code=%d). Falling back to system TTS."),
			Response.IsValid() ? Response->GetResponseCode() : -1);

		// Fallback to system TTS on a background thread so we don't block the game thread.
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]() { RunSystemTTS(); });
		return;
	}

	// Response is raw int16 PCM at 22050 Hz, mono.
	const TArray<uint8>& PCMBytes = Response->GetContent();
	if (PCMBytes.IsEmpty())
	{
		UE_LOG(LogNPCConversation, Warning, TEXT("TTS: ElevenLabs returned empty audio. Falling back."));
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]() { RunSystemTTS(); });
		return;
	}

	TArray<uint8> WavData = BuildWavFromPCM(PCMBytes, 22050, 1);

	// Save to a temp file
	const FString TempPath = FPaths::CreateTempFilename(
		*FPlatformProcess::UserTempDir(), TEXT("npc_tts"), TEXT(".wav"));

	if (!FFileHelper::SaveArrayToFile(WavData, *TempPath))
	{
		UE_LOG(LogNPCConversation, Error, TEXT("TTS: Failed to write temp WAV: %s"), *TempPath);
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]() { RunSystemTTS(); });
		return;
	}

	UE_LOG(LogNPCConversation, Log, TEXT("TTS: ElevenLabs audio written to %s"), *TempPath);
	BroadcastSuccess(TempPath);
}

// ─── System TTS (platform fallback) ──────────────────────────────────────────

void UNPCTTSAsync::RunSystemTTS()
{
	// Write text to a temp file to avoid command-line quoting issues.
	const FString TempTextPath = FPaths::CreateTempFilename(
		*FPlatformProcess::UserTempDir(), TEXT("npc_text"), TEXT(".txt"));
	const FString TempWavPath = FPaths::CreateTempFilename(
		*FPlatformProcess::UserTempDir(), TEXT("npc_tts"), TEXT(".wav"));

	if (!FFileHelper::SaveStringToFile(TextToSpeak, *TempTextPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogNPCConversation, Error, TEXT("TTS: Failed to write temp text file."));
		IFileManager::Get().Delete(*TempTextPath);
		BroadcastFailure();
		return;
	}

#if PLATFORM_WINDOWS
	// Use Windows Speech API via PowerShell (no external binary needed)
	const FString Exe = TEXT("powershell.exe");
	// Paths use forward slashes inside PowerShell to avoid escape issues.
	const FString FwdTextPath = TempTextPath.Replace(TEXT("\\"), TEXT("/"));
	const FString FwdWavPath  = TempWavPath.Replace(TEXT("\\"), TEXT("/"));
	const FString Args = FString::Printf(
		TEXT("-NoProfile -NonInteractive -Command \""
			"Add-Type -AssemblyName System.Speech; "
			"$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
			"$s.SetOutputToWaveFile('%s'); "
			"$s.Speak((Get-Content -LiteralPath '%s' -Raw).Trim()); "
			"$s.Dispose()\""),
		*FwdWavPath, *FwdTextPath);

#elif PLATFORM_MAC
	// macOS built-in TTS — produces AIFF by default; redirect to WAV via afconvert
	const FString TempAiffPath = FPaths::CreateTempFilename(
		*FPlatformProcess::UserTempDir(), TEXT("npc_tts"), TEXT(".aiff"));
	const FString Exe = TEXT("/bin/sh");
	const FString Args = FString::Printf(
		TEXT("-c \"say -f '%s' -o '%s' && afconvert -f WAVE -d LEI16 '%s' '%s'\""),
		*TempTextPath, *TempAiffPath, *TempAiffPath, *TempWavPath);

#elif PLATFORM_LINUX
	// espeak-ng is commonly available on Linux desktops
	const FString Exe = TEXT("/bin/sh");
	const FString Args = FString::Printf(
		TEXT("-c \"espeak-ng -f '%s' -w '%s'\""), *TempTextPath, *TempWavPath);

#else
	UE_LOG(LogNPCConversation, Error, TEXT("TTS: System TTS is not supported on this platform."));
	IFileManager::Get().Delete(*TempTextPath);
	BroadcastFailure();
	return;
#endif

	UE_LOG(LogNPCConversation, Log, TEXT("TTS: Running system TTS: %s %s"), *Exe, *Args);

	int32 ReturnCode = -1;
	FString StdOut, StdErr;
	FPlatformProcess::ExecProcess(*Exe, *Args, &ReturnCode, &StdOut, &StdErr);

	IFileManager::Get().Delete(*TempTextPath);

#if PLATFORM_MAC
	IFileManager::Get().Delete(*TempAiffPath);
#endif

	if (ReturnCode == 0 && IFileManager::Get().FileSize(*TempWavPath) > 0)
	{
		UE_LOG(LogNPCConversation, Log, TEXT("TTS: System TTS produced %s"), *TempWavPath);
		BroadcastSuccess(TempWavPath);
	}
	else
	{
		UE_LOG(LogNPCConversation, Error,
			TEXT("TTS: System TTS failed (rc=%d). stderr: %s"), ReturnCode, *StdErr);
		IFileManager::Get().Delete(*TempWavPath);
		BroadcastFailure();
	}
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

TArray<uint8> UNPCTTSAsync::BuildWavFromPCM(const TArray<uint8>& PCMData, int32 SampleRate, int16 NumChannels)
{
	// RIFF/WAVE header for 16-bit PCM
	const int32 PCMSize    = PCMData.Num();
	const int16 BitsPerSample  = 16;
	const int32 ByteRate   = SampleRate * NumChannels * (BitsPerSample / 8);
	const int16 BlockAlign = NumChannels * (BitsPerSample / 8);
	const int32 ChunkSize  = 36 + PCMSize; // file size minus 8

	TArray<uint8> Wav;
	Wav.Reserve(44 + PCMSize);

	auto Append4CC  = [&](const char* s) { Wav.Append(reinterpret_cast<const uint8*>(s), 4); };
	auto AppendI16  = [&](int16 v)       { Wav.Append(reinterpret_cast<const uint8*>(&v), 2); };
	auto AppendI32  = [&](int32 v)       { Wav.Append(reinterpret_cast<const uint8*>(&v), 4); };

	Append4CC("RIFF");
	AppendI32(ChunkSize);
	Append4CC("WAVE");
	Append4CC("fmt ");
	AppendI32(16);           // fmt chunk size
	AppendI16(1);            // PCM format
	AppendI16(NumChannels);
	AppendI32(SampleRate);
	AppendI32(ByteRate);
	AppendI16(BlockAlign);
	AppendI16(BitsPerSample);
	Append4CC("data");
	AppendI32(PCMSize);
	Wav.Append(PCMData);

	return Wav;
}

void UNPCTTSAsync::BroadcastSuccess(const FString& WavPath)
{
	AsyncTask(ENamedThreads::GameThread, [this, WavPath]()
	{
		OnSuccess.Broadcast(WavPath, true);
		SetReadyToDestroy();
	});
}

void UNPCTTSAsync::BroadcastFailure()
{
	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
	});
}
