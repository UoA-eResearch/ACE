// UE stub: Modules/ModuleInterface.h
#pragma once
#include "CoreMinimal.h"

class IModuleInterface
{
public:
    virtual ~IModuleInterface() = default;
    virtual void StartupModule()  {}
    virtual void ShutdownModule() {}
};
