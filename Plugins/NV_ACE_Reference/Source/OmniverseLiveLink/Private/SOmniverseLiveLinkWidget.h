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

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Misc/Guid.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Types/SlateEnums.h"

class SCheckBox;
class SEditableTextBox;
class SOmniverseLiveLinkWidget : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam( FOnOkClicked, const FString& );
	FOnOkClicked OkClicked;

	SLATE_BEGIN_ARGS(SOmniverseLiveLinkWidget) {}
	    SLATE_EVENT( FOnOkClicked, OnOkClicked )
	SLATE_END_ARGS()

	void Construct( const FArguments& Args );

private:
	void OnPortChanged(const FText& strNewValue, ETextCommit::Type);
	void OnAudioPortChanged(const FText& strNewValue, ETextCommit::Type);
	void OnComboBoxChanged(TSharedPtr<FName> InItem, ESelectInfo::Type InSeletionInfo);
	TSharedRef<SWidget> OnGetComboBoxWidget(TSharedPtr<FName> InItem);
	FText GetCurrentNameAsText() const;

	FReply OnOkClicked();
	FReply OnCancelClicked();

	FString GetPort() const
	{
		return PortNumber;
	};

	FString GetAudioPort() const
	{
		return AudioPortNumber;
	};

private:
	TWeakPtr<SEditableTextBox> PortEditabledText;
	TWeakPtr<SEditableTextBox> AudioPortEditabledText;
	FString PortNumber = "12030";
	FString AudioPortNumber = "12031";
	TArray<TSharedPtr<FName>> SampleRateOptions;
	TSharedPtr<FName> SelectedSampleRate;
};
