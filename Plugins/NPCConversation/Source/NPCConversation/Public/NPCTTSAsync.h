// Copyright UoA eResearch. MIT License.
#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "Interfaces/IHttpRequest.h"
#include "NPCTTSAsync.generated.h"

/**
 * Delegate fired by the TTS async node.
 * @param WavFilePath  Absolute path to the generated WAV file.  Pass this directly to
 *                     "Animate Character From Wav File Async" (ACE plugin) to drive MetaHuman
 *                     facial animation, or load it with a UAudioComponent for playback.
 *                     The caller is responsible for deleting the temp file when done.
 * @param bSuccess     Whether synthesis succeeded.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNPCTTSDelegate, FString, WavFilePath, bool, bSuccess);

/**
 * Async Blueprint node: converts text to speech and writes the result to a temporary WAV file.
 *
 * Provider priority:
 *   1. ElevenLabs REST API  (if an API key is configured and the call succeeds)
 *   2. System TTS fallback  (Windows: PowerShell/SAPI, macOS: say, Linux: espeak-ng)
 *
 * Configure provider preference, API key, and voice in Project Settings → Plugins → NPC Conversation.
 *
 * Typical usage in Blueprints:
 *   Speak Text → OnSuccess → Animate Character From Wav File Async
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class NPCCONVERSATION_API UNPCTTSAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	/**
	 * Synthesize speech from text and return the path to a temporary WAV file.
	 *
	 * @param Text  The text to speak.  Must be non-empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "NPC Conversation|TTS",
		meta = (BlueprintInternalUseOnly = "true",
			DisplayName = "Speak Text (TTS Async)",
			WorldContext = "WorldContextObject"))
	static UNPCTTSAsync* AsyncSpeakText(UObject* WorldContextObject, const FString& Text);

	/** Called when audio synthesis succeeds. WavFilePath is a temp file path. */
	UPROPERTY(BlueprintAssignable)
	FNPCTTSDelegate OnSuccess;

	/** Called when audio synthesis fails (all providers exhausted). */
	UPROPERTY(BlueprintAssignable)
	FNPCTTSDelegate OnFailure;

	// ── Internal ──────────────────────────────────────────────────────────────

	FString TextToSpeak;

	virtual void Activate() override;

private:
	// ElevenLabs path
	void TryElevenLabs();
	void OnElevenLabsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// System TTS fallback
	void RunSystemTTS();

	// Helpers
	/** Broadcast success on the game thread and clean up. */
	void BroadcastSuccess(const FString& WavPath);

	/** Broadcast failure on the game thread and clean up. */
	void BroadcastFailure();
};
