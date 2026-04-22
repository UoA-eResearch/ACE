// UE stub: Kismet/BlueprintAsyncActionBase.h
#pragma once
#include "CoreMinimal.h"

class UBlueprintAsyncActionBase : public UObject
{
public:
    virtual void Activate() {}
    void RegisterWithGameInstance(UObject*) {}
    void SetReadyToDestroy() {}
};
