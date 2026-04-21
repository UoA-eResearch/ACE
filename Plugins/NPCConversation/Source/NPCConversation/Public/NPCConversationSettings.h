// Copyright UoA eResearch. MIT License.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "NPCConversationSettings.generated.h"

/** Which TTS backend to use. */
UENUM(BlueprintType)
enum class ENPCTTSProvider : uint8
{
	/** ElevenLabs REST API (high-quality). Falls back to System if the API key is empty or the request fails. */
	ElevenLabs UMETA(DisplayName = "ElevenLabs (with System fallback)"),
	/** Platform text-to-speech (Windows SAPI via PowerShell, macOS say, Linux espeak-ng). */
	SystemOnly  UMETA(DisplayName = "System TTS only"),
};

/** Which STT backend to use. */
UENUM(BlueprintType)
enum class ENPCSTTProvider : uint8
{
	/** OpenAI-compatible Whisper REST API. */
	WhisperAPI UMETA(DisplayName = "Whisper API (OpenAI-compatible)"),
};

/**
 * Project-wide settings for the NPC Conversation plugin.
 * Accessible via Project Settings → Plugins → NPC Conversation.
 *
 * API keys can also be overridden at runtime via the async Blueprint nodes.
 */
UCLASS(Config = Engine, DefaultConfig, DisplayName = "NPC Conversation")
class NPCCONVERSATION_API UNPCConversationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// ─── LLM (OpenAI-compatible chat completions) ────────────────────────────

	/**
	 * Base URL for the OpenAI-compatible chat completions endpoint.
	 * Examples:
	 *   Cerebras  : https://api.cerebras.ai/v1
	 *   OpenAI    : https://api.openai.com/v1
	 *   Local     : http://localhost:11434/v1  (Ollama)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "LLM", meta = (DisplayName = "LLM Base URL"))
	FString LLMBaseURL = TEXT("https://api.cerebras.ai/v1");

	/** API key for the LLM provider. For Cerebras, get one at https://cloud.cerebras.ai. */
	UPROPERTY(Config, EditAnywhere, Category = "LLM", meta = (DisplayName = "LLM API Key"))
	FString LLMAPIKey;

	/** Model name to request. Cerebras examples: llama-3.3-70b, llama-3.1-8b */
	UPROPERTY(Config, EditAnywhere, Category = "LLM", meta = (DisplayName = "LLM Model"))
	FString LLMModel = TEXT("llama-3.3-70b");

	/** Default system prompt injected before each conversation turn. */
	UPROPERTY(Config, EditAnywhere, Category = "LLM", meta = (DisplayName = "Default System Prompt", MultiLine = "true"))
	FString DefaultSystemPrompt = TEXT("You are a helpful NPC. Respond in character. Keep replies to 1-3 short sentences.");

	/** Maximum tokens to generate per reply. */
	UPROPERTY(Config, EditAnywhere, Category = "LLM", meta = (DisplayName = "Max Tokens", ClampMin = "1", ClampMax = "4096"))
	int32 LLMMaxTokens = 200;

	/** Request timeout in seconds for LLM calls. */
	UPROPERTY(Config, EditAnywhere, Category = "LLM", meta = (DisplayName = "LLM Timeout (s)", ClampMin = "5.0"))
	float LLMTimeoutSeconds = 30.0f;

	// ─── TTS ─────────────────────────────────────────────────────────────────

	/** Which TTS backend to use. */
	UPROPERTY(Config, EditAnywhere, Category = "TTS", meta = (DisplayName = "TTS Provider"))
	ENPCTTSProvider TTSProvider = ENPCTTSProvider::ElevenLabs;

	/** ElevenLabs API key. Get one at https://elevenlabs.io. */
	UPROPERTY(Config, EditAnywhere, Category = "TTS", meta = (DisplayName = "ElevenLabs API Key"))
	FString ElevenLabsAPIKey;

	/**
	 * ElevenLabs Voice ID to use.
	 * Default is "Rachel" (21m00Tcm4TlvDq8ikWAM).  Browse voices at https://elevenlabs.io/voice-library.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "TTS", meta = (DisplayName = "ElevenLabs Voice ID"))
	FString ElevenLabsVoiceID = TEXT("21m00Tcm4TlvDq8ikWAM");

	/** ElevenLabs model to use. eleven_turbo_v2_5 is fast and high quality. */
	UPROPERTY(Config, EditAnywhere, Category = "TTS", meta = (DisplayName = "ElevenLabs Model ID"))
	FString ElevenLabsModelID = TEXT("eleven_turbo_v2_5");

	/** Request timeout in seconds for TTS calls. */
	UPROPERTY(Config, EditAnywhere, Category = "TTS", meta = (DisplayName = "TTS Timeout (s)", ClampMin = "5.0"))
	float TTSTimeoutSeconds = 30.0f;

	// ─── STT ─────────────────────────────────────────────────────────────────

	/** Which STT backend to use. */
	UPROPERTY(Config, EditAnywhere, Category = "STT", meta = (DisplayName = "STT Provider"))
	ENPCSTTProvider STTProvider = ENPCSTTProvider::WhisperAPI;

	/**
	 * Base URL for the Whisper-compatible transcription endpoint.
	 * Examples:
	 *   OpenAI : https://api.openai.com/v1
	 *   Local  : http://localhost:9000 (Faster-Whisper server)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "STT", meta = (DisplayName = "Whisper API Base URL"))
	FString STTBaseURL = TEXT("https://api.openai.com/v1");

	/** API key for the Whisper endpoint. Leave empty for local servers that don't require auth. */
	UPROPERTY(Config, EditAnywhere, Category = "STT", meta = (DisplayName = "Whisper API Key"))
	FString STTAPIKey;

	/** Whisper model to use. Examples: whisper-1, large-v3 */
	UPROPERTY(Config, EditAnywhere, Category = "STT", meta = (DisplayName = "Whisper Model"))
	FString STTModel = TEXT("whisper-1");

	/** How many seconds to record from the microphone before transcribing. */
	UPROPERTY(Config, EditAnywhere, Category = "STT", meta = (DisplayName = "Default Recording Duration (s)", ClampMin = "0.5", ClampMax = "60.0"))
	float DefaultRecordingDuration = 5.0f;

	/** Request timeout in seconds for STT calls. */
	UPROPERTY(Config, EditAnywhere, Category = "STT", meta = (DisplayName = "STT Timeout (s)", ClampMin = "5.0"))
	float STTTimeoutSeconds = 60.0f;
};
