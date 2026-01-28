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

#include "nvaim_struct.h"

namespace nvaim
{

//! Interface RPCParameters
//!
//! {908F10BF-9051-494D-BB50-F302BEC11D9A}
struct alignas(8) RPCParameters {
    RPCParameters() {}; 
    NVAIM_UID(UID({ 0x908f10bf, 0x9051, 0x494d,{ 0xbb, 0x50, 0xf3, 0x02, 0xbe, 0xc1, 0x1d, 0x9a } }), kStructVersion1);
    bool useSSL = true;
    const char* url{};
    const char* sslCert{};
    const char* metaData{};

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(RPCParameters)

//! Interface RESTParameters
//!
//! {AA88BBA7-2217-4569-9C95-0A6EA6BE189C}
struct alignas(8) RESTParameters {
    RESTParameters() {}; 
    NVAIM_UID(UID({ 0xaa88bba7, 0x2217, 0x4569,{ 0x9c, 0x95, 0xa, 0x6e, 0xa6, 0xbe, 0x18, 0x9c } }), kStructVersion1);
    const char* url{};
    const char* authenticationToken{};
    bool verboseMode{};

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(RESTParameters)
}
