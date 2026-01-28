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
namespace gpt
{
namespace ggml::cuda{
constexpr PluginID kId  = { {0x54bbefba, 0x535f, 0x4d77,{0x9c, 0x3f, 0x46, 0x38, 0x39, 0x2d, 0x23, 0xac}}, 0x4b9ee9 };  // {54BBEFBA-535F-4D77-9C3F-4638392D23AC} [nvaim.plugin.gpt.ggml.cuda]
}
namespace ggml::cpu{
constexpr PluginID kId   = { {0x1119fd8b, 0xfc4b, 0x425d,{0xa3, 0x72, 0xcc, 0xe7, 0xd5, 0x27, 0x34, 0x10}}, 0xaae2ed };  // {1119FD8B-FC4B-425D-A372-CCE7D5273410} [nvaim.plugin.gpt.ggml.cpu]
}
namespace cloud::rest
{
constexpr PluginID kId = { {0x3553c9f3, 0x686c, 0x4f08,{0x83, 0x8e, 0xf2, 0xe3, 0xb4, 0x01, 0x9a, 0x72}}, 0xa589b7 }; //{3553C9F3-686C-4F08-838E-F2E3B4019A72} [nvaim.plugin.gpt.cloud.rest]
}

namespace trt::cuda{
constexpr PluginID kId   = { {0xdc6e8755, 0xfc71, 0x4bbb,{0xa2, 0x19, 0xce, 0x03, 0x22, 0x23, 0x9a, 0xaa}}, 0x2917c2 };  // {DC6E8755-FC71-4BBB-A219-CE0322239AAA} [nvaim.plugin.gpt.trt.cuda]
}
}
}

constexpr const char* kGPTDataSlotSystem = "system";
constexpr const char* kGPTDataSlotUser = "text"; // matching ASR output when used in a pipeline
constexpr const char* kGPTDataSlotAssistant = "assistant";
constexpr const char* kGPTDataSlotResponse = "text";
constexpr const char* kGPTDataSlotJSON = "json"; // JSON input for the cloud.rest implementation

// {506C5935-67C6-4136-9550-36BBA83C93BC}
struct alignas(8) GPTCreationParameters {
    GPTCreationParameters() {}; 
    NVAIM_UID(UID({ 0x506c5935, 0x67c6, 0x4136,{ 0x95, 0x50, 0x36, 0xbb, 0xa8, 0x3c, 0x93, 0xbc } }), kStructVersion1);
    CommonCreationParameters* common;
    int32_t maxNumTokensToPredict = 200;
    int32_t contextSize = 512;
    int32_t seed = -1;
};

NVAIM_VALIDATE_STRUCT(GPTCreationParameters)

// {FEB5F4A9-8A02-4864-8757-081F42381160}
struct alignas(8) GPTRuntimeParameters {
    GPTRuntimeParameters() {}; 
    NVAIM_UID(UID({ 0xfeb5f4a9, 0x8a02, 0x4864,{ 0x87, 0x57, 0x8, 0x1f, 0x42, 0x38, 0x11, 0x60 } }), kStructVersion1);
    uint32_t seed = 0xFFFFFFFF;     // RNG seed
    int32_t tokensToPredict = -1;   // new tokens to predict
    int32_t batchSize = 512;        // batch size for prompt processing (must be >=32 to use BLAS)
    int32_t tokensToKeep = 0;       // number of tokens to keep from initial prompt
    int32_t tokensToDraft = 16;     // number of tokens to draft during speculative decoding
    int32_t numChunks = -1;         // max number of chunks to process (-1 = unlimited)
    int32_t numParallel = 1;        // number of parallel sequences to decode
    int32_t numSequences = 1;       // number of sequences to decode
    bool interactive = true;        // chat mode by default
    const char* reversePrompt{};    // reverse prompt for the interactive mode
    const char* prefix{};           // prefix for the user input
    const char* suffix{};           // suffix for the user input
};

NVAIM_VALIDATE_STRUCT(GPTRuntimeParameters)

// {7E0C4D03-CFDD-4B63-BBA7-CF94E0F8370E}
struct alignas(8) GPTCapabilitiesAndRequirements {
    GPTCapabilitiesAndRequirements() {}; 
    NVAIM_UID(UID({ 0x7e0c4d03, 0xcfdd, 0x4b63,{ 0xbb, 0xa7, 0xcf, 0x94, 0xe0, 0xf8, 0x37, 0xe } }), kStructVersion1);
    CommonCapabilitiesAndRequirements* common;
};

NVAIM_VALIDATE_STRUCT(GPTCapabilitiesAndRequirements)

//! General Purpose Transformer (GPT) interface
//! 
using IGeneralPurposeTransformer = InferenceInterface;

}