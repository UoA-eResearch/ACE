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

#include "OmniverseLiveLinkSourceFactory.h"
#include "OmniverseLiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "Features/IModularFeatures.h"
#include "SOmniverseLiveLinkWidget.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Logging/LogMacros.h"


#define LOCTEXT_NAMESPACE "OmniverseLiveLinkSourceFactory"

FText UOmniverseLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "NVIDIA Omniverse LiveLink");
}

FText UOmniverseLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Creates a connection to an Omniverse TCP Stream");
}

TSharedPtr<SWidget> UOmniverseLiveLinkSourceFactory::BuildCreationPanel( FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated ) const
{
	return SNew( SOmniverseLiveLinkWidget )
		.OnOkClicked(SOmniverseLiveLinkWidget::FOnOkClicked::CreateUObject( this, &UOmniverseLiveLinkSourceFactory::OnOkClicked, InOnLiveLinkSourceCreated ) );
}

TSharedPtr<ILiveLinkSource> UOmniverseLiveLinkSourceFactory::CreateSource(const FString& ConnectionString) const
{
	TArray<FString> ConnectionInfos;
	ConnectionString.ParseIntoArray(ConnectionInfos, TEXT(";"), false);
	check(ConnectionInfos.Num() >= 2);
	int Port = FCString::Atoi(*ConnectionInfos[0]);
	int AudioPort = FCString::Atoi(*ConnectionInfos[1]);
	int SampleRate = FCString::Atoi(*ConnectionInfos[2]);
	return MakeShared<FOmniverseLiveLinkSource>(Port, AudioPort, SampleRate);
}

void UOmniverseLiveLinkSourceFactory::OnOkClicked(const FString& ConnectionString, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const
{
	if(ConnectionString.IsEmpty())
	{
		return;
	}

	TArray<FString> ConnectionInfos;
	ConnectionString.ParseIntoArray(ConnectionInfos, TEXT(";"), false);
	check(ConnectionInfos.Num() >= 2);
	int Port = FCString::Atoi(*ConnectionInfos[0]);
	int AudioPort = FCString::Atoi(*ConnectionInfos[1]);
	int SampleRate = FCString::Atoi(*ConnectionInfos[2]);
	OnLiveLinkSourceCreated.ExecuteIfBound(MakeShared<FOmniverseLiveLinkSource>(Port, AudioPort, SampleRate), ConnectionString);
}

#undef LOCTEXT_NAMESPACE
