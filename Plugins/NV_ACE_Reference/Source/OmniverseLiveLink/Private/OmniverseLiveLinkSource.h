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
#include "ILiveLinkSource.h"
#include "Tickable.h"
#include "Interfaces/IPv4/IPv4Address.h"

class FOmniverseLiveLinkSource : public ILiveLinkSource
{
public:
    FOmniverseLiveLinkSource(uint32 InPort, uint32 InAudioPort, uint32 InSampleRate);
    virtual ~FOmniverseLiveLinkSource();

    // Begin ILiveLinkSource Interface
    virtual void ReceiveClient( ILiveLinkClient* InClient, FGuid InSourceGuid ) override;
    virtual bool IsSourceStillValid() const override;
    virtual bool RequestSourceShutdown() override;
    virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;
	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override;
    // End ILiveLinkSource Interface

private:
	void Start();
	void Stop();

private:
    
	TSharedPtr<class FOmniverseWaveStreamer, ESPMode::ThreadSafe> WaveStreamer;
	TSharedPtr<class FOmniverseLiveLinkListener, ESPMode::ThreadSafe> LiveLinkListener;

	FText SourceStatus;
};
