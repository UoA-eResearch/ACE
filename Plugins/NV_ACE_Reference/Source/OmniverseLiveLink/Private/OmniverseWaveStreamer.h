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
#include "CoreMinimal.h"
#include "OmniverseWaveDef.h"
#include "OmniverseBaseListener.h"

class FOmniverseWaveStreamer : public FOmniverseBaseListener
{
public:
	FOmniverseWaveStreamer(ILiveLinkSource* Source, uint32 InPort, uint32 InSampleRate);
    virtual ~FOmniverseWaveStreamer();

	virtual void Stop() override;
	virtual void Start() override;
	virtual void OnPackageDataReceived(const uint8* InPackageData, int32 InPackageSize) override;
	virtual void OnPackageDataPushed(const uint8* InPackageData, int32 InPackageSize, double DeltaTime, bool bBegin = false, bool bEnd = false) override;
	virtual uint32 GetDelayTime() const override;
	virtual bool IsHeaderPackage(const uint8* InPackageData, int32 InPackageSize) const override;

private:
    void ParseWave(const uint8* InReceivedData, int32 InReceivedSize);

private:

	TSharedPtr<class FOmniverseSubmixListener> SubmixListener;
};
