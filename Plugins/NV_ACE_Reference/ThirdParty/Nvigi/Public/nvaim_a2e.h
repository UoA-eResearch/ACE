// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
//
// NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
// property and proprietary rights in and to this material, related
// documentation and any modifications thereto. Any use, reproduction,
// disclosure or distribution of this material and related documentation
// without an express license agreement from NVIDIA CORPORATION or
// its affiliates is strictly prohibited.

#pragma once

#include "nvaim_ai.h"

namespace nvaim
{

namespace plugin::a2e::trt::cuda
{
constexpr PluginID kId = { {0x70fae4e8, 0xdea3, 0x415c,{0x98, 0x7d, 0xc0, 0x08, 0xd2, 0xc4, 0xee, 0x5d}}, 0x2e578b }; //{70FAE4E8-DEA3-415C-987D-C008D2C4EE5D} [nvaim.plugin.a2e.trt.cuda]
}

//! Data slot keys
constexpr const char* kAudio2EmotionDataSlotAudio = "audio";
constexpr const char* kAudio2EmotionDataSlotEmotions = "emotions";

//! Interface 'Audio2EmotionCreationParameters'
//!
//! {0B16285E-02B1-4549-B618-4F66B38D8758}
struct alignas(8) Audio2EmotionCreationParameters
{
    Audio2EmotionCreationParameters() { };
    NVAIM_UID(UID({ 0x0b16285e, 0x02b1, 0x4549,{0xb6, 0x18, 0x4f, 0x66, 0xb3, 0x8d, 0x87, 0x58} }), kStructVersion1);

    //! v1 members go here, please remember NOT to break the C ABI compatibility:
    CommonCreationParameters* common;

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not insert of change existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(Audio2EmotionCreationParameters)


//! Interface 'Audio2EmotionRuntimeParameters'
//!
//! {3522F4DC-219A-4D98-BA5F-0BD5EDD4BDEF}
struct alignas(8) Audio2EmotionRuntimeParameters
{
    Audio2EmotionRuntimeParameters() { };
    NVAIM_UID(UID({ 0x3522f4dc, 0x219a, 0x4d98,{0xba, 0x5f, 0x0b, 0xd5, 0xed, 0xd4, 0xbd, 0xef} }), kStructVersion4);

    //! v1 members go here, please do NOT break the C ABI compatibility:
    float emotionContrast = kUnassignedF;
    float emotionStrength = kUnassignedF;
    float liveBlendCoef = kUnassignedF;
    float preferredEmotionStrength = kUnassignedF;
    int maxEmotions = kUnassignedI;

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvaim::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2 members go here, please do NOT break the C ABI compatibility:

    // Use an integer for enablePreferredEmotion to have a distinct unassigned value
    // Set to 0 for "false" and any value other than kUnassignedI for "true"
    int enablePreferredEmotion = kUnassignedI;

    //! v3+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
    int inferencesToSkip = kUnassignedI;
    float transitionSmoothing = kUnassignedF;

    //! v4+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
    float dt = kUnassignedF;
};

NVAIM_VALIDATE_STRUCT(Audio2EmotionRuntimeParameters)


//! Interface 'Audio2EmotionPreferredEmotion'
//!
//! {E7CD0BB1-E7B0-4FF9-917B-E741EECDAE09}
struct alignas(8) Audio2EmotionPreferredEmotion
{
    Audio2EmotionPreferredEmotion() { };
    NVAIM_UID(UID({0xe7cd0bb1, 0xe7b0, 0x4ff9,{0x91, 0x7b, 0xe7, 0x41, 0xee, 0xcd, 0xae, 0x09}}), kStructVersion1);

    //! v1 members go here, please do NOT break the C ABI compatibility:
    float timeCode = kUnassignedF;  // currently, not active for runtime.
    float amazement = kUnassignedF;
    float anger = kUnassignedF;
    float cheekiness = kUnassignedF;
    float disgust = kUnassignedF;
    float fear = kUnassignedF;
    float grief = kUnassignedF;
    float joy = kUnassignedF;
    float outofbreath = kUnassignedF;
    float pain = kUnassignedF;
    float sadness = kUnassignedF;

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvaim::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(Audio2EmotionPreferredEmotion)


//! Interface 'Audio2EmotionCapabilitiesAndRequirements'
//!
//! {22FE2D6A-E030-40BE-A6C5-5E0A7D93FDE0}
struct alignas(8) Audio2EmotionCapabilitiesAndRequirements
{
    Audio2EmotionCapabilitiesAndRequirements() { };
    NVAIM_UID(UID({ 0x22fe2d6a, 0xe030, 0x40be,{0xa6, 0xc5, 0x5e, 0x0a, 0x7d, 0x93, 0xfd, 0xe0} }), kStructVersion1);

    //! v1 members go here, please do NOT break the C ABI compatibility:
    CommonCapabilitiesAndRequirements* common;
    
    //! Network input requirements
    size_t audioBufferSize{};
    size_t audioBufferOffset{};
    size_t audioSampleRate{};

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvaim::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(Audio2EmotionCapabilitiesAndRequirements)

//! Audio2Emotion interface
using IAudio2Emotion = InferenceInterface;

}