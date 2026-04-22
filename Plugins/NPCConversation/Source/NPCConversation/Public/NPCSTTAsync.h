// Copyright UoA eResearch. MIT License.
#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "Interfaces/IHttpRequest.h"
#include "NPCSTTAsync.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNPCSTTDelegate, FString, TranscribedText, bool, bSuccess);

/**
 * Async Blueprint node: records microphone audio for a configurable duration and then
 * transcribes it, with an automatic fallback to system speech recognition.
 *
 * Steps performed internally:
 *   1. Open the default system microphone via UE AudioCapture.
 *   2. Record float PCM samples for `RecordingDurationSeconds`.
 *   3. Encode samples as a 16-bit mono WAV at the device's native sample rate.
 *   4. POST the WAV to the configured Whisper API endpoint (multipart/form-data).
 *      If the Whisper API is unavailable or fails, the node falls back to system STT.
 *   5. Parse the response and broadcast the transcribed text.
 *
 * System STT fallback:
 *   - Windows : PowerShell + System.Speech.Recognition (built-in, no install required).
 *   - macOS / Linux : not available; node fires OnFailure with a log warning.
 *
 * Configure the Whisper endpoint, API key, and model in Project Settings → Plugins → NPC Conversation.
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class NPCCONVERSATION_API UNPCSTTAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	/**
	 * Record microphone audio for the given duration and transcribe it.
	 *
	 * @param RecordingDurationSeconds  How many seconds to record.  0 uses the project default.
	 */
	UFUNCTION(BlueprintCallable, Category = "NPC Conversation|STT",
		meta = (BlueprintInternalUseOnly = "true",
			DisplayName = "Record and Transcribe (STT Async)",
			WorldContext = "WorldContextObject"))
	static UNPCSTTAsync* AsyncRecordAndTranscribe(UObject* WorldContextObject,
		float RecordingDurationSeconds = 0.0f);

	/** Called when transcription succeeds. */
	UPROPERTY(BlueprintAssignable)
	FNPCSTTDelegate OnSuccess;

	/** Called when recording or transcription fails. */
	UPROPERTY(BlueprintAssignable)
	FNPCSTTDelegate OnFailure;

	// ── Internal ──────────────────────────────────────────────────────────────

	float RecordingDurationSeconds = 0.0f;

	virtual void Activate() override;

private:
	void SendToWhisper(const TArray<uint8>& WavData);
	void OnWhisperResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/** Platform speech recognition fallback (Windows: PowerShell + System.Speech.Recognition). */
	void RunSystemSTT();

	/** WAV bytes stored before the Whisper request so RunSystemSTT can use them on failure. */
	TArray<uint8> PendingWavData;

	void BroadcastSuccess(const FString& Text);
	void BroadcastFailure();
};
