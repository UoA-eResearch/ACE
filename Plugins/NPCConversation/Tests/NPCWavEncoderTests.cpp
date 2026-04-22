// Copyright UoA eResearch. MIT License.
//
// Standalone C++ tests for the NPCConversationCore WAV encoding library.
// No Unreal Engine dependency — compile and run with a plain C++17 toolchain.
// Build command (run from repo root):
//   g++ -std=c++17
//       -I Plugins/NPCConversation/Core
//       Plugins/NPCConversation/Source/NPCConversation/Private/NPCWavEncoder.cpp
//       Plugins/NPCConversation/Tests/NPCWavEncoderTests.cpp
//       -o /tmp/npc_core_tests && /tmp/npc_core_tests

#include "NPCWavEncoder.h"

#include <cassert>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static int16_t readI16LE(const uint8_t* p)
{
    return static_cast<int16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

static int32_t readI32LE(const uint8_t* p)
{
    return static_cast<int32_t>(
        static_cast<uint32_t>(p[0])        |
        (static_cast<uint32_t>(p[1]) <<  8) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24));
}

// ─── Tests ───────────────────────────────────────────────────────────────────

static void TestWavHeaderMagic()
{
    const std::vector<float> silence(100, 0.0f);
    const auto wav = NPCConversationCore::BuildWavFromFloatSamples(silence, 44100);

    assert(wav.size() >= 44 && "WAV too small");

    // RIFF magic
    assert(wav[0]=='R' && wav[1]=='I' && wav[2]=='F' && wav[3]=='F');
    // WAVE magic
    assert(wav[8]=='W' && wav[9]=='A' && wav[10]=='V' && wav[11]=='E');
    // fmt sub-chunk
    assert(wav[12]=='f' && wav[13]=='m' && wav[14]=='t' && wav[15]==' ');
    // data sub-chunk
    assert(wav[36]=='d' && wav[37]=='a' && wav[38]=='t' && wav[39]=='a');

    std::cout << "  [PASS] TestWavHeaderMagic\n";
}

static void TestWavHeaderFields()
{
    const std::vector<float> silence(200, 0.0f); // 200 mono samples
    const auto wav = NPCConversationCore::BuildWavFromFloatSamples(silence, 44100);

    // PCM format (1 = linear PCM)
    assert(readI16LE(&wav[20]) == 1 && "audio format must be PCM");
    // Mono
    assert(readI16LE(&wav[22]) == 1 && "channel count must be 1");
    // Sample rate
    assert(readI32LE(&wav[24]) == 44100 && "sample rate mismatch");
    // Bits per sample
    assert(readI16LE(&wav[34]) == 16 && "bits per sample must be 16");
    // data chunk size = 200 samples * 2 bytes
    assert(readI32LE(&wav[40]) == 400 && "data chunk size mismatch");
    // Total file size = 44 header + 400 data
    assert(wav.size() == 444 && "total WAV size mismatch");

    std::cout << "  [PASS] TestWavHeaderFields\n";
}

static void TestFloatToPCMClamping()
{
    // Values outside [-1, 1] must be clamped; exact boundary values must map
    // to ±32767 (not ±32768, to keep symmetry).
    const std::vector<float> samples = {2.0f, -2.0f, 1.0f, -1.0f, 0.0f};
    const auto wav = NPCConversationCore::BuildWavFromFloatSamples(samples, 16000);

    // Read PCM samples using the safe byte-level helper to avoid aliasing UB.
    assert(readI16LE(&wav[44 + 0*2]) == 32767  && "2.0f should clamp to max int16");
    assert(readI16LE(&wav[44 + 1*2]) == -32767 && "-2.0f should clamp to min int16");
    assert(readI16LE(&wav[44 + 2*2]) == 32767  && "1.0f should map to max int16");
    assert(readI16LE(&wav[44 + 3*2]) == -32767 && "-1.0f should map to min int16");
    assert(readI16LE(&wav[44 + 4*2]) == 0      && "0.0f should map to 0");

    std::cout << "  [PASS] TestFloatToPCMClamping\n";
}

static void TestDownmixStereoToMono()
{
    // Stereo: L=0.5, R=0.5 → mono should average to 0.5
    const std::vector<float> stereo = {0.5f, 0.5f,   // frame 0
                                       0.8f, 0.2f};   // frame 1 (avg = 0.5)
    const auto mono = NPCConversationCore::DownmixToMono(stereo, 2);

    assert(mono.size() == 2 && "mono should have half as many samples");
    assert(std::abs(mono[0] - 0.5f) < 0.0001f && "frame 0 average wrong");
    assert(std::abs(mono[1] - 0.5f) < 0.0001f && "frame 1 average wrong");

    std::cout << "  [PASS] TestDownmixStereoToMono\n";
}

static void TestDownmixMonoPassthrough()
{
    const std::vector<float> mono = {0.3f, -0.7f, 0.0f};
    const auto out = NPCConversationCore::DownmixToMono(mono, 1);

    assert(out.size() == 3);
    for (size_t i = 0; i < mono.size(); ++i)
    {
        assert(std::abs(out[i] - mono[i]) < 0.0001f && "mono passthrough modified samples");
    }

    std::cout << "  [PASS] TestDownmixMonoPassthrough\n";
}

static void TestBuildWavFromPCM()
{
    // 200 bytes of raw PCM = 100 int16 samples at 22050 Hz, mono
    const std::vector<uint8_t> pcm(200, 0);
    const auto wav = NPCConversationCore::BuildWavFromPCM(pcm, 22050, 1);

    assert(wav.size() == 244 && "total WAV size wrong (44 + 200)");
    // RIFF magic
    assert(wav[0]=='R' && wav[1]=='I' && wav[2]=='F' && wav[3]=='F');
    // Sample rate
    assert(readI32LE(&wav[24]) == 22050 && "sample rate mismatch");
    // Channels
    assert(readI16LE(&wav[22]) == 1 && "channel count wrong");
    // data chunk size
    assert(readI32LE(&wav[40]) == 200 && "data chunk size wrong");

    std::cout << "  [PASS] TestBuildWavFromPCM\n";
}

static void TestBuildWavFromPCMStereo()
{
    // Stereo 22050 Hz — byte-rate and block-align fields change
    const std::vector<uint8_t> pcm(440, 0); // 110 stereo frames
    const auto wav = NPCConversationCore::BuildWavFromPCM(pcm, 22050, 2);

    // byte rate = 22050 * 2 * 2 = 88200
    assert(readI32LE(&wav[28]) == 88200 && "byte rate wrong for stereo");
    // block align = 2 * 2 = 4
    assert(readI16LE(&wav[32]) == 4 && "block align wrong for stereo");
    // channels
    assert(readI16LE(&wav[22]) == 2 && "channel count wrong for stereo");

    std::cout << "  [PASS] TestBuildWavFromPCMStereo\n";
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main()
{
    std::cout << "Running NPCConversationCore tests...\n";

    TestWavHeaderMagic();
    TestWavHeaderFields();
    TestFloatToPCMClamping();
    TestDownmixStereoToMono();
    TestDownmixMonoPassthrough();
    TestBuildWavFromPCM();
    TestBuildWavFromPCMStereo();

    std::cout << "\nAll tests passed.\n";
    return 0;
}
