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

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;

namespace nvaim
{

// {957FF4D8-BF82-4FE4-B133-4C44764F2F77}
struct alignas(8) D3D12Parameters {
    D3D12Parameters() {}; 
    NVAIM_UID(UID({ 0x957ff4d8, 0xbf82, 0x4fe4,{ 0xb1, 0x33, 0x4c, 0x44, 0x76, 0x4f, 0x2f, 0x77 } }), kStructVersion1);
    ID3D12Device* device{};
    ID3D12CommandQueue* queue{};
};

NVAIM_VALIDATE_STRUCT(D3D12Parameters);

//! Interface D3d12Data
//!
//! {4A51AF62-7C2C-41F6-9AA6-B19419084E0D}
struct alignas(8) D3d12Data {
    D3d12Data() {}; 
    NVAIM_UID(UID({ 0x4a51af62, 0x7c2c, 0x41f6,{ 0x9a, 0xa6, 0xb1, 0x94, 0x19, 0x08, 0x4e, 0x0d } }), kStructVersion1);
    ID3D12Resource* resource {};
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(D3d12Data)

}
