// Copyright UoA eResearch. MIT License.

#include "Misc/AutomationTest.h"
#include "NPCSTTAsync.h"
#include "NPCLLMAsync.h"
#include "NPCTTSAsync.h"
#include "NPCConversationSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─── STT Tests ───────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCSTTAsyncCreationTest,
	"NPCConversation.STT.Creation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCSTTAsyncCreationTest::RunTest(const FString& Parameters)
{
	// Test that we can create an STT async node
	UNPCSTTAsync* STTNode = NewObject<UNPCSTTAsync>();
	TestNotNull(TEXT("STT node should be created"), STTNode);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCSTTAsyncStaticFactoryTest,
	"NPCConversation.STT.StaticFactory",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCSTTAsyncStaticFactoryTest::RunTest(const FString& Parameters)
{
	// Test that the static factory method works
	UNPCSTTAsync* STTNode = UNPCSTTAsync::AsyncRecordAndTranscribe(nullptr, 5.0f);
	TestNotNull(TEXT("STT node should be created via static factory"), STTNode);
	TestEqual(TEXT("Recording duration should be set"), STTNode->RecordingDurationSeconds, 5.0f);

	return true;
}

// ─── LLM Tests ───────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCLLMAsyncCreationTest,
	"NPCConversation.LLM.Creation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCLLMAsyncCreationTest::RunTest(const FString& Parameters)
{
	// Test that we can create an LLM async node
	UNPCLLMAsync* LLMNode = NewObject<UNPCLLMAsync>();
	TestNotNull(TEXT("LLM node should be created"), LLMNode);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCLLMAsyncStaticFactoryTest,
	"NPCConversation.LLM.StaticFactory",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCLLMAsyncStaticFactoryTest::RunTest(const FString& Parameters)
{
	// Test that the static factory method works
	const FString TestMessage = TEXT("Hello, NPC!");
	const FString TestSystemPrompt = TEXT("You are a test NPC.");

	UNPCLLMAsync* LLMNode = UNPCLLMAsync::AsyncSendToLLM(nullptr, TestMessage, TestSystemPrompt);
	TestNotNull(TEXT("LLM node should be created via static factory"), LLMNode);
	TestEqual(TEXT("User message should be set"), LLMNode->UserMessage, TestMessage);
	TestEqual(TEXT("System prompt override should be set"), LLMNode->SystemPromptOverride, TestSystemPrompt);

	return true;
}

// ─── TTS Tests ───────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCTTSAsyncCreationTest,
	"NPCConversation.TTS.Creation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCTTSAsyncCreationTest::RunTest(const FString& Parameters)
{
	// Test that we can create a TTS async node
	UNPCTTSAsync* TTSNode = NewObject<UNPCTTSAsync>();
	TestNotNull(TEXT("TTS node should be created"), TTSNode);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCTTSAsyncStaticFactoryTest,
	"NPCConversation.TTS.StaticFactory",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCTTSAsyncStaticFactoryTest::RunTest(const FString& Parameters)
{
	// Test that the static factory method works
	const FString TestText = TEXT("Hello, this is a test.");

	UNPCTTSAsync* TTSNode = UNPCTTSAsync::AsyncSpeakText(nullptr, TestText);
	TestNotNull(TEXT("TTS node should be created via static factory"), TTSNode);
	TestEqual(TEXT("Text to speak should be set"), TTSNode->TextToSpeak, TestText);

	return true;
}

// ─── Settings Tests ──────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCConversationSettingsTest,
	"NPCConversation.Settings.Defaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNPCConversationSettingsTest::RunTest(const FString& Parameters)
{
	// Test that we can access the settings object
	const UNPCConversationSettings* Settings = GetDefault<UNPCConversationSettings>();
	TestNotNull(TEXT("Settings should be accessible"), Settings);

	// Verify some default values are set
	TestFalse(TEXT("LLM Base URL should not be empty"), Settings->LLMBaseURL.IsEmpty());
	TestFalse(TEXT("LLM Model should not be empty"), Settings->LLMModel.IsEmpty());
	TestTrue(TEXT("Max tokens should be positive"), Settings->LLMMaxTokens > 0);
	TestTrue(TEXT("Recording duration should be positive"), Settings->DefaultRecordingDuration > 0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
