/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "ACERuntimeModule.h"

// engine includes
#include "Async/Async.h"

// plugin includes
#include "A2XSession.h"
#include "ACERuntimePrivate.h"
#include "AnimDataConsumerRegistry.h"

#define LOCTEXT_NAMESPACE "FACERuntimeModule"

DEFINE_LOG_CATEGORY(LogACERuntime);

///////////////////////////////////
// IModuleInterface implementation
void FACERuntimeModule::StartupModule()
{
	ActiveA2XSessions = MakePimpl<TMap<const IACEAnimDataConsumer*, TUniquePtr<FAudio2XSession>>>();
}

void FACERuntimeModule::ShutdownModule()
{
}

static FAudio2XSession* GetSession(const IACEAnimDataConsumer* Consumer, TMap<const IACEAnimDataConsumer*, TUniquePtr<FAudio2XSession>>& ActiveA2XSessions)
{
	TUniquePtr<FAudio2XSession>* Session = ActiveA2XSessions.Find(Consumer);
	return (Session == nullptr) ? nullptr : Session->Get();
}

template<class T>
static bool AnimateFromAudioSamplesInternal(IACEAnimDataConsumer* Consumer, TArrayView<const T> Samples, int32 NumChannels, int32 SampleRate,
	bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, UAudio2FaceParameters* Audio2FaceParameters,
	FName A2FProviderName, TMap<const IACEAnimDataConsumer*, TUniquePtr<FAudio2XSession>>& ActiveA2XSessions)
{
	TUniquePtr<FAudio2XSession> OneTimeSession;
	FAudio2XSession* Session = GetSession(Consumer, ActiveA2XSessions);
	if (Session == nullptr)
	{
		// need to create a new session first
		IA2FProvider* Provider = GetProviderFromName(A2FProviderName);
		if (bEndOfSamples)
		{
			// we're sending all the audio data in one go so no need to track an active session, we'll just make a temporary session instead
			OneTimeSession = MakeUnique<FAudio2XSession>(Provider, NumChannels, SampleRate, sizeof(T));
			Session = OneTimeSession.Get();
		}
		else
		{
			// store a session for multiple audio sample chunks
			ActiveA2XSessions.Emplace(Consumer, MakeUnique<FAudio2XSession>(Provider, NumChannels, SampleRate, sizeof(T)));
			Session = GetSession(Consumer, ActiveA2XSessions);
		}
		if (!ensure(Session != nullptr))
		{
			// this shouldn't happen
			return false;
		}
		Session->StartSession(Consumer);
	}

	bool bSuccess = Session->SendAudioSamples(Samples, bEndOfSamples, EmotionParameters, Audio2FaceParameters);
	if (bEndOfSamples && !OneTimeSession.IsValid())
	{
		// we're done with the session
		ActiveA2XSessions.Remove(Consumer);
	}

	return bSuccess;
}

// Receive animations using audio from a float sample buffer.
// If bEndOfSamples = true, pending audio data will be flushed and any subsequent call to SendAudioSamples will start a new session.
bool FACERuntimeModule::AnimateFromAudioSamples(IACEAnimDataConsumer* Consumer, TArrayView<const float> SamplesFloat, int32 NumChannels, int32 SampleRate,
	bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, UAudio2FaceParameters* Audio2FaceParameters, FName A2FProviderName)
{
	if (!ensure(ActiveA2XSessions.IsValid()))
	{
		// this shouldn't happen
		return false;
	}

	return AnimateFromAudioSamplesInternal(Consumer, SamplesFloat, NumChannels, SampleRate, bEndOfSamples, EmotionParameters, Audio2FaceParameters, A2FProviderName,
		*ActiveA2XSessions.Get());
}

// Receive animations using audio from an int16 PCM sample buffer.
// If bEndOfSamples = true, pending audio data will be flushed and any subsequent call to SendAudioSamples will start a new session.
bool FACERuntimeModule::AnimateFromAudioSamples(IACEAnimDataConsumer* Consumer, TArrayView<const int16> SamplesInt16, int32 NumChannels, int32 SampleRate,
	bool bEndOfSamples, TOptional<FAudio2FaceEmotion> EmotionParameters, UAudio2FaceParameters* Audio2FaceParameters, FName A2FProviderName)
{
	if (!ensure(ActiveA2XSessions.IsValid()))
	{
		// this shouldn't happen
		return false;
	}

	return AnimateFromAudioSamplesInternal(Consumer, SamplesInt16, NumChannels, SampleRate, bEndOfSamples, EmotionParameters, Audio2FaceParameters, A2FProviderName,
		*ActiveA2XSessions.Get());
}

// Indicate to A2F-3D service that no more samples will be sent. Any subsequent call to SendAudioSamples will start a new session.
// Use this if your last call to SendAudioSamples had bEndOfSamples = false, and now you know the audio stream has ended.
bool FACERuntimeModule::EndAudioSamples(IACEAnimDataConsumer* Consumer)
{
	if (!ensure(ActiveA2XSessions.IsValid()))
	{
		// this shouldn't happen
		return false;
	}

	FAudio2XSession* Session = GetSession(Consumer, *ActiveA2XSessions.Get());
	bool bSuccess = false;
	if (Session != nullptr)
	{
		bSuccess = Session->EndAudioSamples();
		ActiveA2XSessions->Remove(Consumer);
	}
	else
	{
		UE_LOG(LogACERuntime, Warning, TEXT("%s: Application attempted to end an Audio2Face-3D session that doesn't exist or has already been ended!"), ANSI_TO_TCHAR(__FUNCTION__));
	}
	return bSuccess;
}

void FACERuntimeModule::AllocateA2F3DResources(FName ProviderName)
{
	IA2FProvider* Provider = GetProviderFromName(ProviderName);
	if (Provider != nullptr)
	{
		// Request the provider to allocate any resources in a separate thread to avoid blocking anything.
		// This is only a runtime resource optimization hint, not something that anyone needs to wait on for completion,
		// so we discard the TFuture returned by AsyncThread.
		AsyncThread([Provider] { Provider->AllocateResources(); });
	}
}

void FACERuntimeModule::FreeA2F3DResources(FName ProviderName)
{
	IA2FProvider* Provider = GetProviderFromName(ProviderName);
	if (Provider != nullptr)
	{
		// Request the provider to free any resources in a separate thread to avoid blocking anything.
		// This is only a runtime resource optimization hint, not something that anyone needs to wait on for completion,
		// so we discard the TFuture returned by AsyncThread.
		AsyncThread([Provider] { Provider->FreeResources(); });
	}
}

void FACERuntimeModule::CancelAnimationGeneration(IACEAnimDataConsumer* Consumer)
{
	FAnimDataConsumerRegistry* Registry = FAnimDataConsumerRegistry::Get();
	if (Registry != nullptr)
	{
		Registry->DetachConsumer_AnyThread(Consumer);
	}
}

FName GetDefaultProviderName()
{
	return FName("RemoteA2F");
}

IA2FProvider* GetProviderFromName(FName ProviderName)
{
	if ((ProviderName == FName("Default")) || (ProviderName == FName("")))
	{
		ProviderName = GetDefaultProviderName();
	}

	return IA2FProvider::FindProvider(ProviderName);
}



#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FACERuntimeModule, ACERuntime)

