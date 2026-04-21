// Copyright UoA eResearch. MIT License.
#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "NPCLLMAsync.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNPCLLMDelegate, FString, ResponseText, bool, bSuccess);

/**
 * Async Blueprint node: sends a user message to an OpenAI-compatible chat completions endpoint
 * and returns the assistant's reply.
 *
 * Supported providers include:
 *   - Cerebras  (https://api.cerebras.ai/v1)
 *   - OpenAI    (https://api.openai.com/v1)
 *   - Ollama    (http://localhost:11434/v1)
 *   - Any other OpenAI-compatible server
 *
 * Configure the endpoint, API key, and model in Project Settings → Plugins → NPC Conversation.
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncAction))
class NPCCONVERSATION_API UNPCLLMAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	/**
	 * Send a user message to the configured LLM and receive a text response asynchronously.
	 *
	 * @param UserMessage  The player's input text.
	 * @param SystemPromptOverride  If non-empty, overrides the default system prompt from Project Settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "NPC Conversation|LLM",
		meta = (BlueprintInternalUseOnly = "true",
			DisplayName = "Send Message to LLM (Async)",
			WorldContext = "WorldContextObject"))
	static UNPCLLMAsync* AsyncSendToLLM(UObject* WorldContextObject,
		const FString& UserMessage,
		const FString& SystemPromptOverride = TEXT(""));

	/** Called when the LLM returns a successful response. */
	UPROPERTY(BlueprintAssignable)
	FNPCLLMDelegate OnSuccess;

	/** Called when the request fails (network error, bad API key, etc.). */
	UPROPERTY(BlueprintAssignable)
	FNPCLLMDelegate OnFailure;

	// ── Internal ──────────────────────────────────────────────────────────────

	FString UserMessage;
	FString SystemPromptOverride;

	virtual void Activate() override;

private:
	void OnHttpResponse(class FHttpRequestPtr Request, class FHttpResponsePtr Response, bool bWasSuccessful);
};
