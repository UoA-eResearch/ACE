/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include "ACEEditorModule.h"

// engine includes
#include "IPersonaPreviewScene.h"
#include "Modules/ModuleManager.h"
#include "PersonaModule.h"

// plugin includes
#include "ACEAudioCurveSourceComponent.h"

#define LOCTEXT_NAMESPACE "ACEEditor"

IMPLEMENT_MODULE(FACEEditorModule, ACEEditor)

void FACEEditorModule::StartupModule()
{
	// register for when persona is loaded
	OnModulesChangedDelegate = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FACEEditorModule::HandleModulesChanged);
}

void FACEEditorModule::ShutdownModule()
{
	FPersonaModule* PersonaModule = FModuleManager::GetModulePtr<FPersonaModule>(TEXT("Persona"));
	if (PersonaModule && OnPreviewSceneCreatedDelegate.IsValid())
	{
		PersonaModule->OnPreviewSceneCreated().Remove(OnPreviewSceneCreatedDelegate);
	}

	if (OnModulesChangedDelegate.IsValid())
	{
		FModuleManager::Get().OnModulesChanged().Remove(OnModulesChangedDelegate);
	}
}

static void CreatePersonaPreviewAudioComponent(const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	if (AActor* Actor = InPreviewScene->GetActor())
	{
		// Create the preview audio component
		//UACEAudioCurveSourceComponent* AudioCurveSourceComponent = NewObject<UACEAudioCurveSourceComponent>(Actor);
		//AudioCurveSourceComponent->bPreviewComponent = true;
		//AudioCurveSourceComponent->bAlwaysPlay = true;
		//AudioCurveSourceComponent->bIsPreviewSound = true;
		//AudioCurveSourceComponent->bIsUISound = true;
		//AudioCurveSourceComponent->CurveSourceBindingName = ICurveSourceInterface::DefaultBinding;

		//InPreviewScene->AddComponent(AudioCurveSourceComponent, FTransform::Identity);
	}
}


void FACEEditorModule::HandleModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason)
{

	if (InModuleName == TEXT("Persona") && InModuleChangeReason == EModuleChangeReason::ModuleLoaded)
	{
		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));
		OnPreviewSceneCreatedDelegate = PersonaModule.OnPreviewSceneCreated().AddStatic(&CreatePersonaPreviewAudioComponent);
	}

}
