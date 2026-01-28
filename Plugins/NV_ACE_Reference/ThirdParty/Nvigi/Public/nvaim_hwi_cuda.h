// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
//
// NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
// property and proprietary rights in and to this material, related
// documentation and any modifications thereto. Any use, reproduction,
// disclosure or distribution of this material and related documentation
// without an express license agreement from NVIDIA CORPORATION or
// its affiliates is strictly prohibited.

#include "nvaim_struct.h"
#include "nvaim_d3d12.h"
#include "nvaim_cuda.h"

namespace nvaim
{
namespace plugin::hwi::cuda
{
    constexpr PluginID kId = { {0xf991d01a, 0x8e38, 0x43f9,{0x96, 0x96, 0x81, 0x7e, 0x5c, 0xae, 0x94, 0xdd}}, 0xf4b3f7 }; //{F991D01A-8E38-43F9-9696-817E5CAE94DD} [nvaim.plugin.hwi.cuda]
}

// {68E08679-28C6-400C-B9E9-8E8FDBB6426B}
struct alignas(8) IHWICuda {
    IHWICuda() {}; 
    NVAIM_UID(UID({ 0x68e08679, 0x28c6, 0x400c,{ 0xb9, 0xe9, 0x8e, 0x8f, 0xdb, 0xb6, 0x42, 0x6b } }), kStructVersion1);
    // The D3D12 device and queue must be set in params
    // If a context exists for the given device and queue, it will be returned.  A new one will not be created
    nvaim::Result(*cudaGetSharedContextForQueue)(const nvaim::D3D12Parameters& params, CUcontext* ctx);

    // Must call this before the D3D12 queue is destroyed
    nvaim::Result(*cudaReleaseSharedContext)(CUcontext ctx);
};

NVAIM_VALIDATE_STRUCT(IHWICuda)

}