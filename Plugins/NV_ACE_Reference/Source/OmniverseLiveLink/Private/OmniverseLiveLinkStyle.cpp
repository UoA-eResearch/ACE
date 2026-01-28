/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "OmniverseLiveLinkStyle.h"
#include "OmniverseLiveLink.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr< FSlateStyleSet > FOmniverseLiveLinkStyle::StyleInstance = NULL;

void FOmniverseLiveLinkStyle::Initialize()
{
    if( !StyleInstance.IsValid() )
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle( *StyleInstance );
    }
}

void FOmniverseLiveLinkStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle( *StyleInstance );
    ensure( StyleInstance.IsUnique() );
    StyleInstance.Reset();
}

FName FOmniverseLiveLinkStyle::GetStyleSetName()
{
    static FName StyleSetName( TEXT( "OmniverseLiveLinkStyle" ) );
    return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

const FVector2D Icon16x16( 16.0f, 16.0f );
const FVector2D Icon20x20( 20.0f, 20.0f );
const FVector2D Icon48x48( 48.0f, 48.0f );

TSharedRef< FSlateStyleSet > FOmniverseLiveLinkStyle::Create()
{
    TSharedRef< FSlateStyleSet > Style = MakeShareable( new FSlateStyleSet( "OmniverseLiveLinkStyle" ) );
    Style->SetContentRoot( IPluginManager::Get().FindPlugin( "NV_ACE_Reference" )->GetBaseDir() / TEXT( "Resources" ) );

    Style->Set( "OmniverseLiveLink.PluginAction", new IMAGE_BRUSH( TEXT( "nvidia-omniverse-button-icon-48x48" ), Icon48x48 ) );

    return Style;
}

#undef IMAGE_BRUSH

void FOmniverseLiveLinkStyle::ReloadTextures()
{
    if( FSlateApplication::IsInitialized() )
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

const ISlateStyle& FOmniverseLiveLinkStyle::Get()
{
    return *StyleInstance;
}
