// Copyright 2025 JOBUTSU LTD. All rights reserved.

#if WITH_EDITOR
#include "ZenBlinkWorldDetailsCustomization.h"


#define LOCTEXT_NAMESPACE "FZenBlinkWorldDetails"

TSharedRef<IDetailCustomization> FZenBlinkWorldDetails::MakeInstance()
{
    return MakeShareable(new FZenBlinkWorldDetails);
}



void FZenBlinkWorldDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{

    IDetailCategoryBuilder& MetahumanCategory = DetailBuilder.EditCategory("Metahuman");
   // MetahumanCategory.AddProperty(GET_MEMBER_NAME_CHECKED(AZenBlinkWorldActor, placeholder));

    IDetailCategoryBuilder& CameraFocusCategory = DetailBuilder.EditCategory("CameraFocus");

    IDetailCategoryBuilder& FollowTargetCategory = DetailBuilder.EditCategory("TargetFollowing");

    IDetailCategoryBuilder& MovementCategory = DetailBuilder.EditCategory("MovementMode");

    IDetailCategoryBuilder& EmotionCategory = DetailBuilder.EditCategory("Emotion");

    IDetailCategoryBuilder& BlinksCategory = DetailBuilder.EditCategory("Blinks");

    IDetailCategoryBuilder& HeadCategory = DetailBuilder.EditCategory("Head");

    IDetailCategoryBuilder& FaceCategory = DetailBuilder.EditCategory("Face");

    IDetailCategoryBuilder& EyesCategory = DetailBuilder.EditCategory("Eyes");

    IDetailCategoryBuilder& LLinkCategory = DetailBuilder.EditCategory("LiveLink");

    IDetailCategoryBuilder& Global = DetailBuilder.EditCategory("Global");

   
    MetahumanCategory.SetSortOrder(0);
    CameraFocusCategory.SetSortOrder(1);
    FollowTargetCategory.SetSortOrder(2);
    MovementCategory.SetSortOrder(3);
    EmotionCategory.SetSortOrder(4);
    BlinksCategory.SetSortOrder(5);
    HeadCategory.SetSortOrder(6);
    FaceCategory.SetSortOrder(7);
    EyesCategory.SetSortOrder(8);
    LLinkCategory.SetSortOrder(9);
    Global.SetSortOrder(10);

   
}

void FZenBlinkWorldDetails::GenerateFacialGroup(IDetailLayoutBuilder& DetailBuilder, const FString& RegionName, bool Extended)
{
    
    
}


TSharedPtr<IPropertyHandle> FZenBlinkWorldDetails::GetHandle(IDetailLayoutBuilder& DetailBuilder, const FName& PropertyName)
{
    return DetailBuilder.GetProperty(PropertyName, AZenBlinkWorldActor::StaticClass());

}

#undef LOCTEXT_NAMESPACE

#endif