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

#include "OmniverseBaseListener.h"
#include "OmniverseLiveLinkFramePlayer.h"
#include "Async/Async.h"
#include "Common/TcpSocketBuilder.h"
#include "Common/TcpListener.h"
#include "HAL/RunnableThread.h"
#include "ILiveLinkClient.h"

#define RECV_BUFFER_SIZE 1024 * 1024
#define RECV_HEADER_SIZE 8


static int32 BytesToInt(uint8* Bytes, int32 Length)
{
	int32 Return = 0;
	int32 Flag = 0;
	for (int32 Index = Length - 1; Index >= 0; --Index)
	{
		Return += (Bytes[Index] & 0xFF) << (8 * Flag);
		++Flag;
	}
	return Return;
}

const FString FOmniverseBaseListener::HeaderSeparator = TEXT(":");

FOmniverseBaseListener::FOmniverseBaseListener(ILiveLinkSource* Source, uint32 InPort)
	: Source(Source)
	, ListenerSocket(nullptr)
	, ConnectionSocket(nullptr)
	, SocketSubsystem(nullptr)
	, SocketThread(nullptr)
	, ThreadStopping(false)
{
	// Create Listener Socket
	ListenerSocket = FTcpSocketBuilder(TEXT("OmniverseLiveLink"))
		.AsReusable()
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(InPort)
		.Listening(8)
		.WithReceiveBufferSize(RECV_BUFFER_SIZE);

	RecvBuffer.SetNumUninitialized(RECV_BUFFER_SIZE);
	if (ListenerSocket != nullptr)
	{
		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	}
}

FOmniverseBaseListener::~FOmniverseBaseListener()
{
	Stop();

	if (SocketThread)
	{
		SocketThread->WaitForCompletion();
		delete SocketThread;
	}
	if (ListenerSocket)
	{
		ListenerSocket->Close();
		SocketSubsystem->DestroySocket(ListenerSocket);
	}
	if (ConnectionSocket)
	{
		ConnectionSocket->Close();
		SocketSubsystem->DestroySocket(ConnectionSocket);
	}

	LiveLinkClient = nullptr;
	SourceGuid.Invalidate();
}

void FOmniverseBaseListener::Start()
{
	if (SocketThread == nullptr)
	{
		FString ThreadName = TEXT("Omniverse LiveLink Receiver ");
		ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
		SocketThread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
	}
	ThreadStopping = false;
}

void FOmniverseBaseListener::Stop()
{
	ThreadStopping = true;
}

uint32 FOmniverseBaseListener::Run()
{
	TSharedRef<FInternetAddr> RemoteAddr = SocketSubsystem->CreateInternetAddr();
	while (!ThreadStopping)
	{
		bool bPending = false;
		if (ListenerSocket->HasPendingConnection(bPending) && bPending)
		{
			// Already have a Connection? destroy previous
			if (ConnectionSocket)
			{
				ConnectionSocket->Close();
				SocketSubsystem->DestroySocket(ConnectionSocket);
			}
			// New Connection receive!
			ConnectionSocket = ListenerSocket->Accept(*RemoteAddr, TEXT("OmniverseLiveLink Received Socket Connection"));
		}

		if (ConnectionSocket)
		{
			double CurrentRecvTime = FPlatformTime::Seconds();
			uint32 PendingSize = 0;
			while (ConnectionSocket->HasPendingData(PendingSize))
			{
				int32 ReadSize = 0;
				if (ConnectionSocket->Recv(RecvBuffer.GetData(), RecvBuffer.Num(), ReadSize))
				{
					if (ReadSize > 0)
					{
						OnRawDataReceived(RecvBuffer.GetData(), ReadSize);
					}
				}
			}

			////const double TimeOutSeconds = CVarOmniverseLiveLinkTimeOutSeconds.GetValueOnAnyThread();
			////// Ping remote address to see if the socket was closed
			////// NOTE: we can't use ConnectionSocket->GetConnectionState() to get the correct state of socket
			////// https://forums.unrealengine.com/t/fsocket-getconnectionstate-always-returns-true/345349
			////// https://issues.unrealengine.com/issue/UE-27542
			////if (TimeOutSeconds > 0.0 && (CurrentRecvTime - LastRecvTime > TimeOutSeconds))
			////{
			////	ConnectionSocket->Close();
			////	SocketSubsystem->DestroySocket(ConnectionSocket);
			////	ConnectionSocket = nullptr;
			////}
		}
	}
	return 0;
}

bool FOmniverseBaseListener::IsEOSPackage(const uint8* InPackageData, int32 InPackageSize) const
{
	const char MagicWord[] = { 'E', 'O', 'S' };

	const int32 MagicSize = sizeof(MagicWord) / sizeof(MagicWord[0]);

	bool bEndOfSteam = InPackageSize == MagicSize;
	if (bEndOfSteam)
	{
		for (int32 Index = 0; Index < MagicSize; ++Index)
		{
			if (InPackageData[Index] != MagicWord[Index])
			{
				bEndOfSteam = false;
				break;
			}
		}
	}

	return bEndOfSteam;
}

void FOmniverseBaseListener::PushPackageData(const uint8* InPackageData, int32 InPackageSize)
{
	double CurrentTime = FPlatformTime::Seconds();
	if (IsEOSPackage(InPackageData, InPackageSize))
	{
		CustomDeltaTime.Reset();
		LastPushTime.Reset();
		bInBurst = false;
		OnPackageDataPushed(InPackageData, InPackageSize, 0.0, false, true);
		return;
	}

	if (IsHeaderPackage(InPackageData, InPackageSize))
	{
		double FPS;
		if (GetFPSInHeader(InPackageData, InPackageSize, FPS))
		{
			CustomDeltaTime = 1.0 / FPS;
		}
		else
		{
			CustomDeltaTime.Reset();
		}
		LastPushTime.Reset();
		bInBurst = true;
		OnPackageDataPushed(InPackageData, InPackageSize, 0.0, true);
		return;
	}

	if (bInBurst)
	{
		double DeltaTime = 0.0;
		if (LastPushTime.IsSet())
		{
			DeltaTime = CustomDeltaTime.IsSet() ? CustomDeltaTime.GetValue() : (CurrentTime - LastPushTime.GetValue());
		}
		else
		{
			// NOTE: Delay time is in millisecond
			DeltaTime = (double)GetDelayTime() / 1000.0;
		}

		OnPackageDataPushed(InPackageData, InPackageSize, DeltaTime);
		LastPushTime = CurrentTime;
	}
	else
	{
		OnPackageDataReceived(InPackageData, InPackageSize);
	}
}

void FOmniverseBaseListener::OnRawDataReceived(const uint8* InReceivedData, int32 InReceivedSize)
{
	TArray<uint8> ReceivedData;
	ReceivedData.Append(IncompleteData);
	ReceivedData.Append(InReceivedData, InReceivedSize);
	int ReceivedSize = ReceivedData.Num();

	IncompleteData.Empty();

	while (true)
	{
		if (!DataSizeInHeader.IsSet())
		{
			if (ReceivedSize < RECV_HEADER_SIZE)
			{
				//UE_LOG(LogACE, Warning, TEXT("Incomplete Data: %d"), ReceivedSize);
				IncompleteData = ReceivedData;
				break;
			}

			DataSizeInHeader = BytesToInt(ReceivedData.GetData(), RECV_HEADER_SIZE);
			ReceivedData.RemoveAt(0, RECV_HEADER_SIZE);
			ReceivedSize = ReceivedData.Num();
		}
		// This's the complete package, ready to parse 
		else if (ReceivedSize == DataSizeInHeader.GetValue())
		{
			PushPackageData(ReceivedData.GetData(), ReceivedSize);
			DataSizeInHeader.Reset();

			//UE_LOG(LogACE, Warning, TEXT("Complete package, ready to parse: %d, %d"), ReceivedSize, ReceivedData.Num());
			break;
		}
		// This's the incomplete data, need to be completed next time
		else if (ReceivedSize < DataSizeInHeader.GetValue())
		{
			IncompleteData = ReceivedData;

			//UE_LOG(LogACE, Warning, TEXT("Incomplete package, complete next time: %d, %d"), ReceivedSize, ReceivedData.Num());
			break;
		}
		// This's mixing data, need to be separated
		else if (ReceivedSize > DataSizeInHeader.GetValue())
		{
			int32 DataSize = DataSizeInHeader.GetValue();
			PushPackageData(ReceivedData.GetData(), DataSize);
			ReceivedData.RemoveAt(0, DataSize);
			ReceivedSize = ReceivedData.Num();
			DataSizeInHeader.Reset();

			//UE_LOG(LogACE, Warning, TEXT("Mixing data, need to be separated: %d, %d"), ReceivedSize, ReceivedData.Num());
		}
	}
}

bool FOmniverseBaseListener::IsSocketReady() const
{
	return ListenerSocket != nullptr && SocketSubsystem != nullptr;
}

bool FOmniverseBaseListener::IsValid() const
{
	// Source is valid if we have a valid thread and socket
	return !ThreadStopping && SocketThread != nullptr && ListenerSocket != nullptr;
}

void FOmniverseBaseListener::SetClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	LiveLinkClient = InClient;
	SourceGuid = InSourceGuid;
}
