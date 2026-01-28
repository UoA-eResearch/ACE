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
#include "AudioDevice.h"
#include "ISubmixBufferListener.h"
#include "OmniverseWaveDef.h"

class FOmniverseSubmixListener : public ISubmixBufferListener
{
public:
	FOmniverseSubmixListener();
	virtual ~FOmniverseSubmixListener();

	// Register with audio device my submix listener
	void Activate();
	void Deactivate();

	void SetSampleRate(uint32 InSampleRate)
	{
		SubmixSampleRate = InSampleRate;
	}

	void AddNewWave(const FOmniverseWaveFormatInfo& Format);
	void AppendStream(const uint8* Data, int32 Size);

protected:
	// ISubmixBufferListener
	// when called, submit samples to audio device in OnNewSubmixBuffer
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	
private:
	struct FWaveStream
	{
		FWaveStream(const FOmniverseWaveFormatInfo& NewWaveFormat, uint32 Capacity)
			: WaveFormat(NewWaveFormat)
			, NextStream(nullptr)
		{
			LocklessStreamBuffer.SetCapacity(Capacity);
		}

		bool HasStream() { return LocklessStreamBuffer.Num() > 0; }

		FOmniverseWaveFormatInfo WaveFormat;
		mutable Audio::TCircularAudioBuffer<uint8> LocklessStreamBuffer;
		TSharedPtr<FWaveStream> NextStream;
	};

	void OnDeviceDestroyed(Audio::FDeviceId InDeviceId);
	void TrySwitchToNextStream();

	// Valid in Audio thread
	TSharedPtr<FWaveStream, ESPMode::ThreadSafe> PlayingStream = nullptr;
	TWeakPtr<FWaveStream, ESPMode::ThreadSafe> LastPlayingStream = nullptr;

	FThreadSafeBool bSubmixActivated = false;
	FAudioDeviceHandle AudioDeviceHandle;
	FDelegateHandle DeviceDestroyedHandle;
	uint32 SubmixSampleRate = 16000;
};
