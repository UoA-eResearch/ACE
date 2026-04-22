// Copyright UoA eResearch. MIT License.
//
// Implementation of the standalone WAV encoding utilities declared in
// Core/NPCWavEncoder.h.
//
// This file intentionally contains NO Unreal Engine headers or types so that
// it can be compiled and tested in isolation:
//   g++ -std=c++17 -I Plugins/NPCConversation/Core \
//       Plugins/NPCConversation/Source/NPCConversation/Private/NPCWavEncoder.cpp \
//       Plugins/NPCConversation/Tests/NPCWavEncoderTests.cpp

#include "NPCWavEncoder.h"

#include <algorithm>
#include <cstring>

namespace NPCConversationCore {

// ─── Internal helpers ─────────────────────────────────────────────────────────

static void writeU16LE(uint8_t* dst, uint16_t v)
{
    dst[0] = static_cast<uint8_t>(v      );
    dst[1] = static_cast<uint8_t>(v >>  8);
}

static void writeU32LE(uint8_t* dst, uint32_t v)
{
    dst[0] = static_cast<uint8_t>(v      );
    dst[1] = static_cast<uint8_t>(v >>  8);
    dst[2] = static_cast<uint8_t>(v >> 16);
    dst[3] = static_cast<uint8_t>(v >> 24);
}

/// Build a 44-byte RIFF/WAV header for 16-bit PCM audio.
static std::vector<uint8_t> MakeWavHeader(int sampleRate, int numChannels, int pcmBytes)
{
    const int bitsPerSample = 16;
    const int byteRate      = sampleRate * numChannels * (bitsPerSample / 8);
    const int blockAlign    = numChannels * (bitsPerSample / 8);

    std::vector<uint8_t> hdr(44, 0);

    // RIFF chunk descriptor
    std::memcpy(&hdr[0],  "RIFF", 4);
    writeU32LE(&hdr[4],  static_cast<uint32_t>(36 + pcmBytes));
    std::memcpy(&hdr[8],  "WAVE", 4);

    // fmt sub-chunk
    std::memcpy(&hdr[12], "fmt ", 4);
    writeU32LE(&hdr[16], 16);                                        // sub-chunk size
    writeU16LE(&hdr[20], 1);                                         // PCM format
    writeU16LE(&hdr[22], static_cast<uint16_t>(numChannels));
    writeU32LE(&hdr[24], static_cast<uint32_t>(sampleRate));
    writeU32LE(&hdr[28], static_cast<uint32_t>(byteRate));
    writeU16LE(&hdr[32], static_cast<uint16_t>(blockAlign));
    writeU16LE(&hdr[34], static_cast<uint16_t>(bitsPerSample));

    // data sub-chunk
    std::memcpy(&hdr[36], "data", 4);
    writeU32LE(&hdr[40], static_cast<uint32_t>(pcmBytes));

    return hdr;
}

// ─── Public API ───────────────────────────────────────────────────────────────

std::vector<float> DownmixToMono(const std::vector<float>& samples, int numChannels)
{
    if (numChannels <= 1)
    {
        return samples;
    }

    const size_t numFrames = samples.size() / static_cast<size_t>(numChannels);
    std::vector<float> mono;
    mono.reserve(numFrames);

    for (size_t i = 0; i < numFrames; ++i)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            sum += samples[i * static_cast<size_t>(numChannels) + ch];
        }
        mono.push_back(sum / static_cast<float>(numChannels));
    }

    return mono;
}

std::vector<uint8_t> BuildWavFromFloatSamples(
    const std::vector<float>& samples,
    int sampleRate)
{
    const int numSamples = static_cast<int>(samples.size());
    const int pcmBytes   = numSamples * 2; // 16-bit = 2 bytes per sample

    std::vector<uint8_t> wav = MakeWavHeader(sampleRate, /*numChannels=*/1, pcmBytes);
    wav.resize(44 + pcmBytes);

    for (int i = 0; i < numSamples; ++i)
    {
        float v = samples[i];
        if (v >  1.0f) v =  1.0f;
        if (v < -1.0f) v = -1.0f;
        const int16_t s16 = static_cast<int16_t>(v * 32767.0f);
        writeU16LE(&wav[44 + i * 2], static_cast<uint16_t>(s16));
    }

    return wav;
}

std::vector<uint8_t> BuildWavFromPCM(
    const std::vector<uint8_t>& pcmData,
    int sampleRate,
    int numChannels)
{
    std::vector<uint8_t> wav = MakeWavHeader(
        sampleRate, numChannels, static_cast<int>(pcmData.size()));
    wav.insert(wav.end(), pcmData.begin(), pcmData.end());
    return wav;
}

} // namespace NPCConversationCore
