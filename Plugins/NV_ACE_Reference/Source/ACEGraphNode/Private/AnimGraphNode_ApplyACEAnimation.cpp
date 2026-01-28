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

#include "AnimGraphNode_ApplyACEAnimation.h"

#define LOCTEXT_NAMESPACE "ACE"

FText UAnimGraphNode_ApplyACEAnimation::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_ApplyACEAnimation_Tooltip", "Apply face animations from an ACE Audio Curve Source Component attached to this character");
}

FText UAnimGraphNode_ApplyACEAnimation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_ApplyACEAnimation_Title", "Apply ACE Face Animations");
}

FText UAnimGraphNode_ApplyACEAnimation::GetMenuCategory() const
{
	return LOCTEXT("AnimGraphNode_ApplyACEAnimation_Category", "ACE");
}

#undef LOCTEXT_NAMESPACE

