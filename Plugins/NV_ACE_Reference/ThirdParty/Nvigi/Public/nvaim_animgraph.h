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
namespace animgraph
{
    constexpr PluginID kId = {{0x9e497efb, 0xddfa, 0x4f8b,{0x9e, 0x90, 0x81, 0x4b, 0x0d, 0x89, 0x50, 0xdd}}, 0xe9c4a3}; //{9E497EFB-DDFA-4F8B-9E90-814B0D8950DD} [nvaim.plugin.animgraph.cloud.grpc]
}
}

constexpr const uint32_t ANIMGRAPH_DEFAULT_CONNECTION_TIMEOUT_IN_MS{ 100u };
constexpr const uint32_t ANIMGRAPH_MAX_CONNECTION_TIMEOUT_IN_MS{ 10000u };

//! Data slot keys
//! INPUT:
constexpr const char* kAnimgraphDataSlotRequestId = "requestid";
constexpr const char* kAnimgraphDataSlotStreamId = "streamid";
constexpr const char* kAnimgraphDataSlotTargetObjectId = "targetobjectid";

//! OUTPUT:
constexpr const char* kAnimgraphDataSlotBlendshapes = "blendshapes";
constexpr const char* kAnimgraphDataSlotJointTranslations = "jointtranslations";
constexpr const char* kAnimgraphDataSlotJointRotations = "jointrotations";
constexpr const char* kAnimgraphDataSlotJointScales = "jointscales";
constexpr const char* kAnimgraphDataSlotAudio = "audio";
constexpr const char* kAnimgraphDataSlotTimeCodes = "timecodes";
constexpr const char* kAnimgraphStatusCode = "statuscode";

enum class AnimgraphStatusCode : uint64_t
{
    // The first 17 correspond to gRPC status codes
    // https://grpc.io/docs/guides/status-codes/

    eGrpcStatusCodeOK                       =  0u,
    eGrpcStatusCodeCancelled                =  1u,
    eGrpcStatusCodeUnknown                  =  2u,
    eGrpcStatusCodeInvalidArgument          =  3u,
    eGrpcStatusCodeDeadlineExceeded         =  4u,
    eGrpcStatusCodeNotFound                 =  5u,
    eGrpcStatusCodeAlreadyExists            =  6u,
    eGrpcStatusCodePermissionDenied         =  7u,
    eGrpcStatusCodeResourceExhausted        =  8u,
    eGrpcStatusCodeFailedPrecondition       =  9u,
    eGrpcStatusCodeAborted                  = 10u,
    eGrpcStatusCodeOutOfRange               = 11u,
    eGrpcStatusCodeUnimplemented            = 12u,
    eGrpcStatusCodeInternal                 = 13u,
    eGrpcStatusCodeUnavailable              = 14u,
    eGrpcStatusCodeDataLoss                 = 15u,
    eGrpcStatusCodeUnauthenticated          = 16u,

    // The remaining ones represent errors found outside the gRPC status codes
    // and generated within either the ACE Protobuf animation interface, or the
    // AIM plugin implementation

    eAceStatusCodeErrorUnknown              = 17u,
    eAceStatusCodeErrorStreamIdDoesNotExist = 18u,
    eAimUnknown                             = 19u,
    eAimGrpcDataHeapAllocationError         = 20u,
    eAimGrpcConnectionProblem               = 21u,
    eAimReaderThreadCannotStart             = 22u,
    eAimReaderThreadCannotJoin              = 23u
};

//! Interface 'AnimgraphCreationParameters'
//!
//! {E5AFFC28-D262-416A-89E4-BF1EE2B85E99}
struct alignas(8) AnimgraphCreationParameters
{
    AnimgraphCreationParameters() { };
    NVAIM_UID(UID({0xe5affc28, 0xd262, 0x416a,{0x89, 0xe4, 0xbf, 0x1e, 0xe2, 0xb8, 0x5e, 0x99}}), kStructVersion2);
    CommonCreationParameters* common;

    uint32_t connection_timeout_in_ms{ ANIMGRAPH_DEFAULT_CONNECTION_TIMEOUT_IN_MS };
};

NVAIM_VALIDATE_STRUCT(AnimgraphCreationParameters)

//! Interface 'AnimgraphCreationParametersEx'
//!
//! {CAEDE327-7ACE-41C9-84EA-8179C279CE8F}
struct alignas(8) AnimgraphCreationParametersEx
{
    AnimgraphCreationParametersEx() { };
    NVAIM_UID(UID({0xcaede327, 0x7ace, 0x41c9,{0x84, 0xea, 0x81, 0x79, 0xc2, 0x79, 0xce, 0x8f}}), kStructVersion1);
    //! Some extra optional parameters go here
    //! 
    //! This structure is normally chained to AnimgraphCreationParameters using the _next member.
};

NVAIM_VALIDATE_STRUCT(AnimgraphCreationParametersEx)


//TODO BRINGUP
// {

//! Interface 'AnimgraphEmotions'
//!
//! {E2D0BE28-97A1-46ED-8704-6890E18F0DFD}
struct alignas(8) AnimgraphEmotions
{
    AnimgraphEmotions() { };
    NVAIM_UID(UID({ 0xe2d0be28, 0x97a1, 0x46ed,{0x87, 0x04, 0x68, 0x90, 0xe1, 0x8f, 0x0d, 0xfd} }), kStructVersion1);

    float timeCode{}; // NOTE: ignored for local execution
    float amazement{};
    float anger{};
    float cheekiness{};
    float disgust{};
    float fear{};
    float grief{};
    float joy{};
    float outofbreath{};
    float pain{};
    float sadness{};

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(AnimgraphEmotions)


//! Interface 'AnimgraphRuntimeParameters'
//!
//! {B4458BAE-C002-4F7C-BEE1-DC68EE70A07A}
struct alignas(8) AnimgraphRuntimeParameters
{
    AnimgraphRuntimeParameters() { };
    NVAIM_UID(UID({ 0xb4458bae, 0xc002, 0x4f7c,{0xbe, 0xe1, 0xdc, 0x68, 0xee, 0x70, 0xa0, 0x7a} }), kStructVersion1);

    // delta time (ms) between frames, if not provided assuming 30fps
    float deltaTimeMs{};

    float inputStrength = 0.8f;
    float lowerFaceSmoothing = 0.01f;
    float upperFaceSmoothing = 0.1f;
    float lowerFaceStrength = 1.1f;
    float upperFaceStrength = 1.2f;
    float faceMaskLevel = 0.4f;
    float faceMaskSoftness = 0.07f;
    float skinStrength = 0.9f;
    float blinkStrength = 0.8f;
    float eyelidOpenOffset = 0.1f;
    float lipOpenOffset = 0.1f;
    float blinkOffset = 0.5f;

    float tongueStrength = 1.5f;
    float tongueHeightOffset = 0.2f;
    float tongueDepthOffset = 0.13f;

    //! NOTE: Cloud specific
    float emotionContrast = 1.0f;
    float emotionStrength = 0.6f;
    float liveBlendCoef = 0.7f;
    float preferredEmotionStrength = 0.0f;
    int maxEmotions = 10;

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(AnimgraphRuntimeParameters)


//! Interface 'AnimgraphBlendShapeInfo'
//!
//! {F7B3632B-56CB-4679-AD7B-776C65A63DC5}
struct alignas(8) AnimgraphBlendShapeInfo
{
    AnimgraphBlendShapeInfo() { };
    NVAIM_UID(UID({ 0xf7b3632b, 0x56cb, 0x4679,{0xad, 0x7b, 0x77, 0x6c, 0x65, 0xa6, 0x3d, 0xc5} }), kStructVersion1);

    // Number of poses and their names per model (GUID) which are enumerated in CommonCapabilitiesAndRequirements
    size_t numPoses{};
    const char** poseNames{};
    const char* modelGUID{};

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

// }

//! Interface 'AnimgraphCapabilitiesAndRequirements'
//!
//! {68703567-0967-4CA0-BAC0-A61D26C23FF3}
struct alignas(8) AnimgraphCapabilitiesAndRequirements
{
    AnimgraphCapabilitiesAndRequirements() { };
    NVAIM_UID(UID({ 0x68703567, 0x0967, 0x4ca0,{0xba, 0xc0, 0xa6, 0x1d, 0x26, 0xc2, 0x3f, 0xf3} }), kStructVersion1);
    CommonCapabilitiesAndRequirements* common;

    //TODO BRINGUP
    // {
    //! Supported FPS for processing audio
    //! 
    //! Cloud only implementation might limit FPS, like for example to 30hz.
    //! 
    int supportedFPS = -1; // Default value (-1) indicates that any FPS is OK.
    //! An array of pointers to model info (common->numSupportedModels determines the size)
    const AnimgraphBlendShapeInfo** modelInfo{};
    // }
};

NVAIM_VALIDATE_STRUCT(AnimgraphCapabilitiesAndRequirements)

NVAIM_VALIDATE_STRUCT(AnimgraphBlendShapeInfo)

//! Interface 'IAnimgraph'
using IAnimgraph = InferenceInterface;

}