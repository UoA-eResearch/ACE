// Copyright UoA eResearch. MIT License.

#include "NPCConversationModule.h"

DEFINE_LOG_CATEGORY(LogNPCConversation);

void FNPCConversationModule::StartupModule()
{
	UE_LOG(LogNPCConversation, Log, TEXT("NPCConversation plugin loaded."));
}

void FNPCConversationModule::ShutdownModule()
{
	UE_LOG(LogNPCConversation, Log, TEXT("NPCConversation plugin unloaded."));
}

IMPLEMENT_MODULE(FNPCConversationModule, NPCConversation)
