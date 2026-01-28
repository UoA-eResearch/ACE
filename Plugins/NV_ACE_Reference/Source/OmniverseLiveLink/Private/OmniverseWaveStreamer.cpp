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

#include "OmniverseWaveStreamer.h"
#include "ACEPrivate.h"
#include "OmniverseLiveLinkSourceSettings.h"
#include "OmniverseSubmixListener.h"

#include "ILiveLinkClient.h"

#define LOCTEXT_NAMESPACE "OmniverseWaveStreamer"


FOmniverseWaveStreamer::FOmniverseWaveStreamer(ILiveLinkSource* Source, uint32 InPort, uint32 InSampleRate)
    : FOmniverseBaseListener(Source, InPort)
{
	SubmixListener = MakeShareable(new FOmniverseSubmixListener());
	SubmixListener->SetSampleRate(InSampleRate);
}

FOmniverseWaveStreamer::~FOmniverseWaveStreamer()
{
	SubmixListener.Reset();
}

// Helper functions
static bool IsDataEmpty(const uint8* data, size_t size)
{
	//unsigned char* mm = (unsigned char*)data;
	bool isEmpty = (*data == 0) && memcmp(data, data + 1, size - 1) == 0;
	return isEmpty;
}

// FRunnable interface
void FOmniverseWaveStreamer::Start()
{
	FOmniverseBaseListener::Start();
	SubmixListener->Activate();
}

void FOmniverseWaveStreamer::Stop()
{
	FOmniverseBaseListener::Stop();
	SubmixListener->Deactivate();
}

void FOmniverseWaveStreamer::OnPackageDataReceived(const uint8* InPackageData, int32 InPackageSize)
{
	ParseWave(InPackageData, InPackageSize);
}

void FOmniverseWaveStreamer::OnPackageDataPushed(const uint8* InPackageData, int32 InPackageSize, double DeltaTime, bool bBegin, bool bEnd)
{
	if (IsDataEmpty(InPackageData, InPackageSize)) return;
	FOmniverseLiveLinkFramePlayer::Get(Source).PushAudioData_AnyThread(InPackageData, InPackageSize, DeltaTime, bBegin, bEnd);
}

uint32 FOmniverseWaveStreamer::GetDelayTime() const
{
	if (LiveLinkClient)
	{
		UOmniverseLiveLinkSourceSettings* Settings = Cast<UOmniverseLiveLinkSourceSettings>(LiveLinkClient->GetSourceSettings(SourceGuid));
		if (Settings)
		{
			return Settings->AudioDelayTime;
		}
	}

	return 0;
}

bool FOmniverseWaveStreamer::IsHeaderPackage(const uint8* InPackageData, int32 InPackageSize) const
{
	const char MagicWord[] = { 'W', 'A', 'V', 'E' };

	const int32 MagicSize = sizeof(MagicWord) / sizeof(MagicWord[0]);

	bool bIsHeader = InPackageSize > MagicSize;
	if (bIsHeader)
	{
		for (int32 Index = 0; Index < MagicSize; ++Index)
		{
			if (InPackageData[Index] != MagicWord[Index])
			{
				bIsHeader = false;
				break;
			}
		}
	}

	return bIsHeader;
}

void FOmniverseWaveStreamer::ParseWave(const uint8* InReceivedData, int32 InReceivedSize)
{
	if (IsEOSPackage(InReceivedData, InReceivedSize) || IsDataEmpty(InReceivedData, InReceivedSize))
	{
		return;
	}

	// The first batch will be a string containing wave format info
	// "SamplesPerSecond Channels BitsPerSample Samples SampleType"
	// This could also be JSON formatted instead
	
	if (IsHeaderPackage(InReceivedData, InReceivedSize))
	{
		FString ReceievedString = FString(InReceivedSize, (ANSICHAR*)InReceivedData);
		UE_LOG(LogACE, Warning, TEXT("Received wave format info: '%s'"), *ReceievedString);
		
		FOmniverseWaveFormatInfo WaveInfo;

		TArray<FString> WaveFormatInfoStrings;
		ReceievedString.ParseIntoArray(WaveFormatInfoStrings, *HeaderSeparator);
		if (WaveFormatInfoStrings.Num() == FOmniverseWaveFormatInfo::NumMembers + 1)
		{
			int32 InfoIndex = 1; // The first is MagicWord
			WaveInfo.SamplesPerSecond = FCString::Atoi(*WaveFormatInfoStrings[InfoIndex++]);
			WaveInfo.NumChannels = FCString::Atoi(*WaveFormatInfoStrings[InfoIndex++]);
			WaveInfo.BitsPerSample = FCString::Atoi(*WaveFormatInfoStrings[InfoIndex++]);
			WaveInfo.SampleType = FCString::Atoi(*WaveFormatInfoStrings[InfoIndex++]);
			SubmixListener->AddNewWave(WaveInfo);
		}
	}
	else
	{
		//UE_LOG(LogACE, Warning, TEXT("Wav bytes received: %i"), ReceivedData.Num());
		SubmixListener->AppendStream(InReceivedData, InReceivedSize);
	}
}

#undef LOCTEXT_NAMESPACE
