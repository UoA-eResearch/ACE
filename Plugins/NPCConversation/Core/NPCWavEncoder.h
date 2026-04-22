// Copyright UoA eResearch. MIT License.
//
// Standalone C++ WAV encoding utilities.
// This file has NO Unreal Engine dependencies and can be compiled and tested
// with a plain C++17 compiler (g++, clang++, MSVC).
//
// Usage from within the UE plugin:
//   #include "NPCWavEncoder.h"   // Build.cs adds Core/ to include path
//
// Usage for standalone tests:
//   g++ -std=c++17 -I Core/ Core/NPCWavEncoder.cpp Tests/NPCWavEncoderTests.cpp
#pragma once

#include <cstdint>
#include <vector>

namespace NPCConversationCore {

/// Downmix interleaved multi-channel float PCM to mono by averaging channels.
/// @param samples     Interleaved float PCM in [-1, 1].  Length must be a
///                    multiple of numChannels.
/// @param numChannels Number of audio channels.  Pass 1 to return a copy unchanged.
/// @return Mono float samples.
std::vector<float> DownmixToMono(const std::vector<float>& samples, int numChannels);

/// Encode mono float PCM as a 16-bit WAV byte stream.
/// Float values outside [-1, 1] are clamped before conversion.
/// @param samples    Mono float PCM in [-1, 1].
/// @param sampleRate Sample rate in Hz (e.g. 44100).
/// @return Complete WAV file bytes (44-byte RIFF header + 16-bit PCM payload).
std::vector<uint8_t> BuildWavFromFloatSamples(
    const std::vector<float>& samples,
    int sampleRate);

/// Wrap raw 16-bit little-endian PCM bytes in a RIFF/WAV container.
/// @param pcmData    Raw int16 PCM bytes.
/// @param sampleRate Sample rate in Hz.
/// @param numChannels Channel count (1 = mono, 2 = stereo, …).
/// @return Complete WAV file bytes (44-byte RIFF header + pcmData).
std::vector<uint8_t> BuildWavFromPCM(
    const std::vector<uint8_t>& pcmData,
    int sampleRate,
    int numChannels);

} // namespace NPCConversationCore
