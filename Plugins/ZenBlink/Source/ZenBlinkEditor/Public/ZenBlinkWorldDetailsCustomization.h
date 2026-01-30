// Copyright 2025 JOBUTSU LTD. All rights reserved.
#pragma once

#if WITH_EDITOR

#include "IDetailCustomization.h"
#include "Layout/Visibility.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "ZenBlinkWorldActor.h"


class IDetailLayoutBuilder;

class FZenBlinkWorldDetails : public IDetailCustomization
{
public:

    static TSharedRef<IDetailCustomization> MakeInstance();


    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

    void GenerateFacialGroup(IDetailLayoutBuilder& DetailBuilder, const FString& RegionName, bool Extended);

    TSharedPtr<IPropertyHandle> GetHandle(IDetailLayoutBuilder& DetailBuilder, const FName& PropertyName);

};

#endif