// Copyright UoA eResearch. MIT License.
#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "NPCSTTAsync.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNPCSTTDelegate, FString, TranscribedText, bool, bSuccess);

/**
 * Async Blueprint node: records microphone audio for a configurable duration and then
 * transcribes it via a Whisper-compatible API endpoint.
 *
 * Steps performed internally:
 *   1. Open the default system microphone via UE AudioCapture.
 *   2. Record float PCM samples for `RecordingDurationSeconds`.
 *   3. Encode samples as a 16-bit mono WAV (resampled to 16 kHz if needed).
 *   4. POST the WAV to the configured Whisper API endpoint (multipart/form-data).
 *   5. Parse the JSON response and broadcast the transcribed text.
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
	void OnWhisperResponse(class FHttpRequestPtr Request, class FHttpResponsePtr Response, bool bWasSuccessful);

	/** Convert float PCM samples ([-1,1]) to a 16-bit mono WAV byte array. */
	static TArray<uint8> BuildWavFromFloatSamples(const TArray<float>& Samples, int32 SampleRate, int32 NumChannels);

	void BroadcastSuccess(const FString& Text);
	void BroadcastFailure();
};
