// Copyright 2025 JOBUTSU LTD. All rights reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ZenBlinkComponentDetailsCustomization.h"
#include "ZenBlinkWorldDetailsCustomization.h"
#include "PropertyEditorModule.h"


class FZenBlinkEditorModule : public IModuleInterface
{
public:

    virtual void StartupModule() override
    {

            auto& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
            PropertyModule.RegisterCustomClassLayout("ZenBlinkComponent",FOnGetDetailCustomizationInstance::CreateStatic(&FZenBlinkComponentDetails::MakeInstance));
            PropertyModule.RegisterCustomClassLayout( "ZenBlinkWorldActor",FOnGetDetailCustomizationInstance::CreateStatic(&FZenBlinkWorldDetails::MakeInstance));
            PropertyModule.NotifyCustomizationModuleChanged();


    }

    virtual void ShutdownModule() override
    {
            if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
            {
                auto& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
                PropertyModule.UnregisterCustomClassLayout("ZenBlinkComponent");
                PropertyModule.UnregisterCustomClassLayout("ZenBlinkWorldActor");
            }


    }
};


IMPLEMENT_MODULE(FZenBlinkEditorModule, ZenBlinkEditor)

