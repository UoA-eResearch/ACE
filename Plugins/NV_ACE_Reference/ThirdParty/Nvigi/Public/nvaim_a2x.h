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

namespace plugin::a2x::pipeline
{
constexpr PluginID kId = { {0x0d84078b, 0xbe54, 0x4915,{0x8a, 0xcd, 0x58, 0x18, 0x74, 0x3a, 0xce, 0xfb}}, 0xadf18b }; //{0D84078B-BE54-4915-8ACD-5818743ACEFB} [nvaim.plugin.a2x.pipeline]
}
namespace plugin::a2x::cloud::grpc
{
constexpr PluginID kId = { {0x1093344b, 0xfa99, 0x4a98,{0xaa, 0xc1, 0xae, 0x66, 0xe7, 0x24, 0x89, 0xfe}}, 0x5a488d }; //{1093344B-FA99-4A98-AAC1-AE66E72489FE} [nvaim.plugin.a2x.cloud.grpc]
}


//! Interface 'A2XCreationParameters'
//!
//! {8A0A8321-D259-4669-A952-20A00AF5BAED}
struct alignas(8) A2XCreationParameters
{
    A2XCreationParameters() { };
    NVAIM_UID(UID({ 0x8a0a8321, 0xd259, 0x4669,{0xa9, 0x52, 0x20, 0xa0, 0x0a, 0xf5, 0xba, 0xed} }), kStructVersion1);

    //! v1 members go here, please do NOT break the C ABI compatibility:

    //! Backenda/APIs to use
    const PluginID* a2f;
    const PluginID* a2e;

    //! Creation parameters
    const NVAIMParameter* a2fCreationParameters;
    const NVAIMParameter* a2eCreationParameters;

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvaim::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(A2XCreationParameters)



//! Interface 'A2XStreamingParameters'
//!
//! {FC96EC49-4FB4-4AD4-8042-1B38752A9C40}
struct alignas(8) A2XStreamingParameters
{
    A2XStreamingParameters() { };
    NVAIM_UID(UID({ 0xfc96ec49, 0x4fb4, 0x4ad4,{0x80, 0x42, 0x1b, 0x38, 0x75, 0x2a, 0x9c, 0x40} }), kStructVersion1);

    //! v1 members go here, please do NOT break the C ABI compatibility:
    bool streaming{ false };

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvaim::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(A2XStreamingParameters)


//! IAudio2X interface
using IAudio2X = InferenceInterface;

}