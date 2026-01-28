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

#include "OmniverseLiveLinkSource.h"
#include "OmniverseWaveStreamer.h"
#include "OmniverseLiveLinkListener.h"
#include "OmniverseLiveLinkSourceSettings.h"

#include "ILiveLinkClient.h"
#include "Logging/LogMacros.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "OmniverseLiveLinkSource"

FOmniverseLiveLinkSource::FOmniverseLiveLinkSource(uint32 InPort, uint32 InAudioPort, uint32 InSampleRate)
{
	SourceStatus = LOCTEXT("OmniverseLiveLinkSource", "Device Not Found");
	FOmniverseLiveLinkFramePlayer::Get(this).Start();
	WaveStreamer = MakeShareable(new FOmniverseWaveStreamer(this, InAudioPort, InSampleRate));
	LiveLinkListener = MakeShareable(new FOmniverseLiveLinkListener(this, InPort));

	FOmniverseLiveLinkFramePlayer::Get(this).RegisterAnime(LiveLinkListener);
	FOmniverseLiveLinkFramePlayer::Get(this).RegisterAudio(WaveStreamer);

	if (LiveLinkListener->IsSocketReady() && WaveStreamer->IsSocketReady())
	{
		Start();
		SourceStatus = LOCTEXT("OmniverseLiveLinkSource", "Active");
	}
}

FOmniverseLiveLinkSource::~FOmniverseLiveLinkSource()
{
	FOmniverseLiveLinkFramePlayer::Get(this).Reset();
  Stop();

	WaveStreamer.Reset();
	LiveLinkListener.Reset();
}

TSubclassOf<ULiveLinkSourceSettings> FOmniverseLiveLinkSource::GetSettingsClass() const
{ 
	return UOmniverseLiveLinkSourceSettings::StaticClass();
}

FText FOmniverseLiveLinkSource::GetSourceMachineName() const
{
	return LOCTEXT("OmniverseLiveLinkSourceMachineName", "localhost");
}

FText FOmniverseLiveLinkSource::GetSourceStatus() const
{
	return SourceStatus;
}

FText FOmniverseLiveLinkSource::GetSourceType() const
{
	return LOCTEXT("OmniverseLiveLinkSourceType", "NVIDIA Omniverse LiveLink");
}

void FOmniverseLiveLinkSource::ReceiveClient( ILiveLinkClient* InClient, FGuid InSourceGuid )
{
	LiveLinkListener->SetClient(InClient, InSourceGuid);
	WaveStreamer->SetClient(InClient, InSourceGuid);
}

bool FOmniverseLiveLinkSource::IsSourceStillValid() const
{
  // Source is valid if we have a valid thread and socket
  return LiveLinkListener->IsValid();
}

bool FOmniverseLiveLinkSource::RequestSourceShutdown()
{
  Stop();
	LiveLinkListener->ClearAllSubjects();
  return true;
}

void FOmniverseLiveLinkSource::Start()
{
	WaveStreamer->Start();
	LiveLinkListener->Start();
}

void FOmniverseLiveLinkSource::Stop()
{
	LiveLinkListener->Stop();
	WaveStreamer->Stop();
}

#undef LOCTEXT_NAMESPACE
