// Copyright UoA eResearch. MIT License.

#include "NPCLLMAsync.h"

#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#include "NPCConversationModule.h"
#include "NPCConversationSettings.h"

// ─────────────────────────────────────────────────────────────────────────────

UNPCLLMAsync* UNPCLLMAsync::AsyncSendToLLM(UObject* WorldContextObject,
	const FString& UserMessage,
	const FString& SystemPromptOverride)
{
	UNPCLLMAsync* Action = NewObject<UNPCLLMAsync>();
	Action->UserMessage           = UserMessage;
	Action->SystemPromptOverride  = SystemPromptOverride;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UNPCLLMAsync::Activate()
{
	if (UserMessage.IsEmpty())
	{
		UE_LOG(LogNPCConversation, Warning, TEXT("LLM: UserMessage is empty, aborting."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	const UNPCConversationSettings* Settings = GetDefault<UNPCConversationSettings>();
	if (!Settings)
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: Could not read NPCConversationSettings."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	if (Settings->LLMAPIKey.IsEmpty())
	{
		UE_LOG(LogNPCConversation, Warning, TEXT("LLM: LLMAPIKey is not configured in Project Settings."));
	}

	// Build endpoint URL: <base>/chat/completions
	FString URL = Settings->LLMBaseURL;
	if (!URL.EndsWith(TEXT("/")))
	{
		URL += TEXT("/");
	}
	URL += TEXT("chat/completions");

	// Build JSON body
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Settings->LLMModel);
	Body->SetNumberField(TEXT("max_tokens"), Settings->LLMMaxTokens);

	TArray<TSharedPtr<FJsonValue>> Messages;

	// System message
	const FString SystemPrompt = SystemPromptOverride.IsEmpty()
		? Settings->DefaultSystemPrompt
		: SystemPromptOverride;

	if (!SystemPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> SysMsg = MakeShared<FJsonObject>();
		SysMsg->SetStringField(TEXT("role"), TEXT("system"));
		SysMsg->SetStringField(TEXT("content"), SystemPrompt);
		Messages.Add(MakeShared<FJsonValueObject>(SysMsg));
	}

	// User message
	TSharedPtr<FJsonObject> UserMsg = MakeShared<FJsonObject>();
	UserMsg->SetStringField(TEXT("role"), TEXT("user"));
	UserMsg->SetStringField(TEXT("content"), UserMessage);
	Messages.Add(MakeShared<FJsonValueObject>(UserMsg));

	Body->SetArrayField(TEXT("messages"), Messages);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	// Create HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"),
		FString::Printf(TEXT("Bearer %s"), *Settings->LLMAPIKey));
	Request->SetContentAsString(BodyStr);
	Request->SetTimeout(Settings->LLMTimeoutSeconds);

	Request->OnProcessRequestComplete().BindUObject(this, &UNPCLLMAsync::OnHttpResponse);

	UE_LOG(LogNPCConversation, Log, TEXT("LLM: Sending request to %s (model=%s)"), *URL, *Settings->LLMModel);

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: Failed to start HTTP request."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
	}
}

void UNPCLLMAsync::OnHttpResponse(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: HTTP request failed."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	const int32 StatusCode = Response->GetResponseCode();
	const FString ResponseStr = Response->GetContentAsString();

	if (StatusCode < 200 || StatusCode >= 300)
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: HTTP %d — %s"), StatusCode, *ResponseStr);
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	// Parse: { "choices": [{ "message": { "content": "..." } }] }
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: Failed to parse JSON response."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!JsonObj->TryGetArrayField(TEXT("choices"), Choices) || !Choices || Choices->IsEmpty())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: No choices in response: %s"), *ResponseStr);
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	TSharedPtr<FJsonObject> FirstChoice = (*Choices)[0]->AsObject();
	if (!FirstChoice.IsValid())
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: choices[0] is not an object."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	const TSharedPtr<FJsonObject>* MessageObjPtr = nullptr;
	if (!FirstChoice->TryGetObjectField(TEXT("message"), MessageObjPtr) || !MessageObjPtr)
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: No 'message' in choices[0]."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	FString Content;
	if (!(*MessageObjPtr)->TryGetStringField(TEXT("content"), Content))
	{
		UE_LOG(LogNPCConversation, Error, TEXT("LLM: No 'content' in message."));
		OnFailure.Broadcast(TEXT(""), false);
		SetReadyToDestroy();
		return;
	}

	UE_LOG(LogNPCConversation, Log, TEXT("LLM response: %s"), *Content);
	OnSuccess.Broadcast(Content, true);
	SetReadyToDestroy();
}
