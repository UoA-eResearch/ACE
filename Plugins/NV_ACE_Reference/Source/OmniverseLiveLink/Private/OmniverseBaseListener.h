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
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "ILiveLinkClient.h"
#include "OmniverseLiveLinkFramePlayer.h"


class FOmniverseBaseListener : public FRunnable
{
public:
	FOmniverseBaseListener(ILiveLinkSource* Source, uint32 InPort);
	virtual ~FOmniverseBaseListener();

	// Begin FRunnable Interface
	virtual void Stop() override;
	// End FRunnable Interface

	// Begin FOmniverseBaseListener Interface
	virtual void Start();
	virtual bool IsValid() const;
	virtual bool IsSocketReady() const;
	// Get the raw data from network
	virtual void OnRawDataReceived(const uint8* InReceivedData, int32 InReceivedSize);
	// Get the size-checked package
	virtual void OnPackageDataReceived(const uint8* InPackageData, int32 InPackageSize) {};
	virtual void OnPackageDataPushed(const uint8* InPackageData, int32 InPackageSize, double DeltaTime, bool bBegin = false, bool bEnd = false) {};
	virtual uint32 GetDelayTime() const { return 0; }

	virtual bool IsEOSPackage(const uint8* InPackageData, int32 InPackageSize) const;
	virtual bool IsHeaderPackage(const uint8* InPackageData, int32 InPackageSize) const { return false; }
	virtual bool GetFPSInHeader(const uint8* InPackageData, int32 InPackageSize, double& OutFPS) const { return false; }

	// End FOmniverseBaseListener Interface

	void SetClient(class ILiveLinkClient* InClient, FGuid InSourceGuid);
protected:
	// Begin FRunnable Interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Exit() override {}
	// End FRunnable Interface

	// Live link client
	class ILiveLinkClient* LiveLinkClient = nullptr;
	// Source GUID in LiveLink
	FGuid SourceGuid;
	// Link to parent LiveLink source
	ILiveLinkSource* Source;

	const static FString HeaderSeparator;

private:
	void PushPackageData(const uint8* InPackageData, int32 InPackageSize);

	// Tcp Server
	class FSocket* ListenerSocket;
	class FSocket* ConnectionSocket;
	class ISocketSubsystem* SocketSubsystem;
	// Thread to run socket operations on
	class FRunnableThread* SocketThread;

	// Threadsafe Bool for terminating the main thread loop
	FThreadSafeBool ThreadStopping;

	// Buffer to receive socket data into
	// Only in socket thread
	TArray<uint8> RecvBuffer;
	TArray<uint8> IncompleteData;
	TOptional<int32> DataSizeInHeader;

	TOptional<double> CustomDeltaTime;
	TOptional<double> LastPushTime;
	bool bInBurst = false;
};
