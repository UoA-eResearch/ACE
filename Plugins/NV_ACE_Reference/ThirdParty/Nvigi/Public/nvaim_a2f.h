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

namespace plugin
{
namespace a2f
{
namespace trt::cuda 
{
constexpr PluginID kId = { {0x2e8df233, 0x8224, 0x4159,{0xba, 0xbe, 0x5a, 0x39, 0x61, 0x4e, 0x57, 0x04}}, 0xb68bb8 }; //{2E8DF233-8224-4159-BABE-5A39614E5704} [nvaim.plugin.a2f.trt.cuda]
}

}
}

//! Data slot keys
constexpr const char* kAudio2FaceDataSlotAudio = "audio";
constexpr const char* kAudio2FaceDataSlotBlendshapes = "blendshapes";
constexpr const char* kAudio2FaceDataSlotBlendshapesTongue = "blendshapestongue";

//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
// {FB1DE887-6828-45C8-A9CF-81819BF98F27}
struct alignas(8) Audio2FaceCreationParameters {
    Audio2FaceCreationParameters() {}; 
    NVAIM_UID(UID({ 0xfb1de887, 0x6828, 0x45c8,{ 0xa9, 0xcf, 0x81, 0x81, 0x9b, 0xf9, 0x8f, 0x27 } }), kStructVersion2);
    CommonCreationParameters* common;

    bool solveTongueBlendshapeWeights{ false };

    // Ideally, you wouldn't need to specify this, it would just come from the model.
    // For this demo there is a hard-coded list of GUID tagged for diffusion, so this
    // value can remain unassigned.  We just leave the option of specifying in case
    // we want to add new diffusion models not in the hard-coded list.
    // Use an integer for diffusionModel to have a distinct unassigned value
    // Set to 0 for "false" and any value other than kUnassignedI for "true"
    int diffusionModel = kUnassignedI;  // don't need to set this.
};

NVAIM_VALIDATE_STRUCT(Audio2FaceCreationParameters)


//! Interface 'Audio2FaceEmotions'
//!
//! {A0BACE84-994D-4B33-848E-1BE7AE4297A9}
struct alignas(8) Audio2FaceEmotions
{
    Audio2FaceEmotions() { };
    NVAIM_UID(UID({ 0xa0bace84, 0x994d, 0x4b33,{0x84, 0x8e, 0x1b, 0xe7, 0xae, 0x42, 0x97, 0xa9} }), kStructVersion1);

    //! v1 members go here, please do NOT break the C ABI compatibility:
    float timeCode = kUnassignedF;
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

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(Audio2FaceEmotions)


//! Interface 'Audio2FaceRuntimeParameters'
//!
//! {B33CC666-25CE-4048-9C39-130EF366D97D}
struct alignas(8) Audio2FaceRuntimeParameters
{
    Audio2FaceRuntimeParameters() { };
    NVAIM_UID(UID({ 0xb33cc666, 0x25ce, 0x4048,{0x9c, 0x39, 0x13, 0x0e, 0xf3, 0x66, 0xd9, 0x7d} }), kStructVersion2);

    //! v1 members go here, please do NOT break the C ABI compatibility:
    // delta time (ms) between frames, if not provided assuming 30fps
    float deltaTimeMs = kUnassignedF;

    float inputStrength = kUnassignedF;
    float lowerFaceSmoothing = kUnassignedF;
    float upperFaceSmoothing = kUnassignedF;
    float lowerFaceStrength = kUnassignedF;
    float upperFaceStrength = kUnassignedF;
    float faceMaskLevel = kUnassignedF;
    float faceMaskSoftness = kUnassignedF;
    float skinStrength = kUnassignedF;
    float blinkStrength = kUnassignedF;
    float eyelidOpenOffset = kUnassignedF;
    float lipOpenOffset = kUnassignedF;
    float blinkOffset = kUnassignedF;

    float tongueStrength = kUnassignedF;
    float tongueHeightOffset = kUnassignedF;
    float tongueDepthOffset = kUnassignedF;
    
    //! Audio2Emotion parameters
    float emotionContrast = kUnassignedF;
    float emotionStrength = kUnassignedF;
    float liveBlendCoef = kUnassignedF;
    float preferredEmotionStrength = kUnassignedF;
    int maxEmotions = kUnassignedI;

    //! v2 members go here, please do NOT break the C ABI compatibility:

    // Use an integer for enablePreferredEmotion to have a distinct unassigned value
    // Set to 0 for "false" and any value other than kUnassignedI for "true"
    int enablePreferredEmotion = kUnassignedI;

    //! v3+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(Audio2FaceRuntimeParameters)


//! Interface 'Audio2FaceBlendShapeParameters'
//!
//! {85026181-9A6C-4335-B028-E2BA18BE3732}
struct alignas(8) Audio2FaceBlendShapeParameters
{
    Audio2FaceBlendShapeParameters() { };
    NVAIM_UID(UID({ 0x85026181, 0x9a6c, 0x4335,{0xb0, 0x28, 0xe2, 0xba, 0x18, 0xbe, 0x37, 0x32} }), kStructVersion2);

    //! v1 members go here, please do NOT break the C ABI compatibility:

    //! Must match model's number of poses (see below Audio2FaceBlendShapesInfo)
    size_t numPoses{};
    //! NOTE: Individual values can be set to nvaim::kUnassignedF to leave defaults in place
    const float* offsets{};
    const float* multipliers{};

    size_t numPosesTongue{};
    const float* offsetsTongue{};
    const float* multipliersTongue{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(Audio2FaceBlendShapeParameters)

//! Interface 'Audio2FaceBlendShapesInfo'
//!
//! {BADD5479-90EB-4E2B-9B90-B63D833D2560}
struct alignas(8) Audio2FaceBlendShapesInfo
{
    Audio2FaceBlendShapesInfo() { };
    NVAIM_UID(UID({ 0xbadd5479, 0x90eb, 0x4e2b,{0x9b, 0x90, 0xb6, 0x3d, 0x83, 0x3d, 0x25, 0x60} }), kStructVersion2);

    // Number of poses and their names per model (GUID) which are enumerated in CommonCapabilitiesAndRequirements
    size_t numPoses{};
    const char** poseNames{};
    const char* modelGUID{};

    size_t numPosesTongue{};
    const char** poseNamesTongue{};

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(Audio2FaceBlendShapesInfo)

//! Interface 'Audio2FaceCapabilitiesAndRequirements'
//! 
// {37F46B38-DFA3-461A-BB9D-33377FEC1BDB}
struct alignas(8) Audio2FaceCapabilitiesAndRequirements {
    Audio2FaceCapabilitiesAndRequirements() {}; 
    NVAIM_UID(UID({ 0x37f46b38, 0xdfa3, 0x461a,{ 0xbb, 0x9d, 0x33, 0x37, 0x7f, 0xec, 0x1b, 0xdb } }), kStructVersion2);
    CommonCapabilitiesAndRequirements* common;
    //! Supported FPS for processing audio
    //! 
    //! Cloud only implementation might limit FPS, like for example to 30hz.
    //! 
    int supportedFPS = -1; // Default value (-1) indicates that any FPS is OK.
    //! Network input requirements
    size_t audioBufferSize{};
    size_t audioBufferOffset{};
    size_t audioSampleRate{};
    //! An array of pointers to model info (common->numSupportedModels determines the size)
    const Audio2FaceBlendShapesInfo** modelInfo{};

    //! Network input requirements
    size_t diffusionAudioBufferSize{};
    size_t diffusionAudioPaddingLeft{};
    size_t diffusionAudioPaddingRight{};
    size_t diffusionAudioSampleRate{};

    // Expose some extra functions for the plug-in
    nvaim::Result(*isDiffusionModel)(const InferenceInstance* a2finstance, bool& isDiffusion) {};
    nvaim::Result(*getDiffusionFrameParameters)(
        const InferenceInstance* a2finstance,
        size_t& numFramesLeftTruncate,
        size_t& numFramesRightTruncate,
        size_t& numFramesCenter
        ) {};
};

NVAIM_VALIDATE_STRUCT(Audio2FaceCapabilitiesAndRequirements)

//! Audio2Face interface
using IAudio2Face = InferenceInterface;

}