// Copyright 2025 JOBUTSU LTD. All rights reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FZenBlinkModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {

 
    }

    virtual void ShutdownModule() override
    {
 
    }
};


IMPLEMENT_MODULE(FZenBlinkModule, ZenBlink)
