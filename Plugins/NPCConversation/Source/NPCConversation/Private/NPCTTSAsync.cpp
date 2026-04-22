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

// Standalone C++ core — no UE dependency.
#include "NPCWavEncoder.h"
#include <vector>

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
	const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : -1;
	if (!bWasSuccessful || !Response.IsValid() || ResponseCode < 200 || ResponseCode >= 300)
	{
		UE_LOG(LogNPCConversation, Warning,
			TEXT("TTS: ElevenLabs request failed (code=%d). Falling back to system TTS."),
			ResponseCode);

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

	TArray<uint8> WavData;
	{
		const std::vector<uint8_t> PCMStd(PCMBytes.GetData(), PCMBytes.GetData() + PCMBytes.Num());
		const std::vector<uint8_t> WavStd = NPCConversationCore::BuildWavFromPCM(PCMStd, 22050, 1);
		WavData.Append(reinterpret_cast<const uint8*>(WavStd.data()), static_cast<int32>(WavStd.size()));
	}

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

	int32 ReturnCode = -1;
	FString StdOut, StdErr;

#if PLATFORM_WINDOWS
	// Use Windows Speech API via PowerShell (no external binary needed).
	// Paths use forward slashes inside PowerShell to avoid backslash escape issues.
	const FString Exe = TEXT("powershell.exe");
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

	UE_LOG(LogNPCConversation, Log, TEXT("TTS: Running system TTS: %s %s"), *Exe, *Args);
	FPlatformProcess::ExecProcess(*Exe, *Args, &ReturnCode, &StdOut, &StdErr);

#elif PLATFORM_MAC
	// macOS: invoke 'say' and 'afconvert' directly (no shell wrapper).
	// ExecProcess uses fork/execvp, so shell quote syntax is not interpreted; paths are passed
	// verbatim. FPaths::CreateTempFilename produces GUID-based names with no spaces or
	// shell-special characters, so no quoting is needed.
	const FString TempAiffPath = FPaths::CreateTempFilename(
		*FPlatformProcess::UserTempDir(), TEXT("npc_tts"), TEXT(".aiff"));

	{
		const FString SayArgs = FString::Printf(TEXT("-f %s -o %s"), *TempTextPath, *TempAiffPath);
		UE_LOG(LogNPCConversation, Log, TEXT("TTS: Running: /usr/bin/say %s"), *SayArgs);
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/say"), *SayArgs, &ReturnCode, &StdOut, &StdErr);
	}

	if (ReturnCode == 0)
	{
		const FString ConvertArgs = FString::Printf(
			TEXT("-f WAVE -d LEI16 %s %s"), *TempAiffPath, *TempWavPath);
		UE_LOG(LogNPCConversation, Log, TEXT("TTS: Running: /usr/bin/afconvert %s"), *ConvertArgs);
		StdErr.Empty();
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/afconvert"), *ConvertArgs, &ReturnCode, &StdOut, &StdErr);
	}

	IFileManager::Get().Delete(*TempAiffPath);

#elif PLATFORM_LINUX
	// espeak-ng: invoke directly with -f (input file) and -w (WAV output) flags.
	// No shell is involved; paths are passed verbatim to execvp.
	const FString EspeakArgs = FString::Printf(TEXT("-f %s -w %s"), *TempTextPath, *TempWavPath);
	UE_LOG(LogNPCConversation, Log, TEXT("TTS: Running: espeak-ng %s"), *EspeakArgs);
	FPlatformProcess::ExecProcess(TEXT("espeak-ng"), *EspeakArgs, &ReturnCode, &StdOut, &StdErr);

#else
	UE_LOG(LogNPCConversation, Error, TEXT("TTS: System TTS is not supported on this platform."));
	IFileManager::Get().Delete(*TempTextPath);
	BroadcastFailure();
	return;
#endif

	IFileManager::Get().Delete(*TempTextPath);

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
