/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// plugin includes
#include "ACECoreModulePrivate.h"
#include "ACESettings.h"
#include "AnimDataConsumerRegistry.h"

// engine includes
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersionComparison.h"
#include "Templates/SharedPointer.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif


#define LOCTEXT_NAMESPACE "FACECore"
DEFINE_LOG_CATEGORY(LogACECore);


static void GetPluginInternalVersionWarning(FCoreDelegates::FSeverityMessageMap& OutMessages)
{
	OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning,
		LOCTEXT("ACECoreVersionWarning",
			"INTERNAL NVIDIA ACE UNREAL PLUGIN VERSION NOT FOR DISTRIBUTION (\"disableallscreenmessages\" to hide messages)"));
}

void FACECoreModule::StartupModule()
{
	TSharedPtr<IPlugin> ThisPlugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);

	const FString& PluginVersion = ThisPlugin->GetDescriptor().VersionName;
	UE_LOG(LogACECore, Log, TEXT("Loaded ACE plugin version %s"), *PluginVersion);
	if (PluginVersion.Contains(TEXT("internal")))
	{
		FCoreDelegates::OnGetOnScreenMessages.AddStatic(&GetPluginInternalVersionWarning);
	}

	AnimDataRegistry = MakeUnique<FAnimDataConsumerRegistry>();

#if WITH_EDITOR
	// register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		UACESettings* Settings = GetMutableDefault<UACESettings>();
		SettingsModule->RegisterSettings("Project", "Plugins", "ACE",
										 LOCTEXT("ACESettingsName", "NVIDIA ACE"),
										 LOCTEXT("ACESettingsDecription", "Configure the NVIDIA ACE plugin"),
										 Settings
		);
	}
#endif

	// If old 2.0/2.1 plugin settings are found, replace them with the new settings
	UACESettings* ACESettings = GetMutableDefault<UACESettings>();
	if (ACESettings != nullptr)
	{
		FString DefaultConfigPath = ACESettings->GetDefaultConfigFilename();
		const FString OldSectionName = TEXT("/Script/ACERuntime.ACESettings");
		const FString NewSectionName = TEXT("/Script/ACECore.ACESettings");
		FConfigFile* ConfigFile = GConfig->Find(DefaultConfigPath);
		if ((ConfigFile != nullptr) && ConfigFile->DoesSectionExist(*OldSectionName))
		{
#if !UE_VERSION_OLDER_THAN(5,4,0)
			const FConfigSection* OldSection = GConfig->GetSection(*OldSectionName, false, DefaultConfigPath);
			// TODO: fix deprecation warnings for FindOrAddSection
			FConfigSection* NewSection = ConfigFile->FindOrAddSection(NewSectionName);
			if ((OldSection != nullptr) && (NewSection != nullptr))
			{
				// move entries from old section to new section
				for (TPair<FName, FConfigValue> Entry : *OldSection)
				{
					NewSection->Add(Entry);
				}
				// remove old section and write DefaultEngine config file to disk
				GConfig->EmptySection(*OldSectionName, DefaultConfigPath);
				// refresh default ACE settings from the updated config file
				ACESettings->LoadConfig(ACESettings->GetClass(), *DefaultConfigPath, EPropertyPortFlags::PPF_None);
			}
#else
			// we use GetSectionPrivate with Const==false to get the config cache to mark the FConfigFile dirty
			const FConfigSection* OldSection = GConfig->GetSectionPrivate(*OldSectionName, false, false, DefaultConfigPath);
			FConfigSection* NewSection = ConfigFile->FindOrAddSection(NewSectionName);
			if ((OldSection != nullptr) && (NewSection != nullptr))
			{
				// move entries from old section to new section
				for (TPair<FName, FConfigValue> Entry : *OldSection)
				{
					NewSection->Add(Entry);
				}
				ConfigFile->Remove(OldSectionName);
				// write DefaultEngine config file to disk
				GConfig->Flush(false, DefaultConfigPath);
				// refresh default ACE settings from the updated config file
				ACESettings->LoadConfig(ACESettings->GetClass(), *DefaultConfigPath, EPropertyPortFlags::PPF_None);
			}
#endif
		}
	}
}

void FACECoreModule::ShutdownModule()
{}

FAnimDataConsumerRegistry* FACECoreModule::GetAnimDataRegistry()
{
	return AnimDataRegistry.Get();;
}

IMPLEMENT_MODULE(FACECoreModule, ACECore);

