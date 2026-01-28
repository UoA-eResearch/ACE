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

#include <stddef.h> // size_t for Linux

#include "nvaim_struct.h"
#include "nvaim_result.h"
#include "nvaim_version.h"
#include "nvaim_helpers.h"

namespace nvaim
{

//! Vendor types
//! 
enum class VendorId : uint32_t
{
    // NOT physical adapter types: Used for indicating a "required adapter" for a component
    eAny = 0, // can be any valid display/compute adapter for the platform (e.g. DXGI on Windows)
    eNone = 1, // no adapter of any kind is needed (e.g. "headless/server")

    // Physical adapter types (based upon DXGI VendorId)
    eMS = 0x1414, // Microsoft Software Render Adapter
    eNVDA = 0x10DE,
    eAMD = 0x1002,
    eIntel = 0x8086,
};

//! Engine types
//! 
enum class EngineType : uint32_t
{
    eCustom,
    eUnreal,
    eUnity,
    eCount
};

//! Application Info
//!
//! OPTIONAL - can be chained with Preferences before calling nvaimInit
//! 
//! {BC5449C4-0096-408D-9C5E-4AE573A27A25}
struct alignas(8) AppInfo {
    AppInfo() {}; 
    NVAIM_UID(UID({ 0xbc5449c4, 0x0096, 0x408d,{ 0x9c, 0x5e, 0x4a, 0xe5, 0x73, 0xa2, 0x7a, 0x25 } }), kStructVersion1);
    //! Optional - Id provided by NVIDIA, if not specified then engine type and version are required
    uint32_t applicationId {};
    //! Optional - Type of the rendering engine used, if not specified then applicationId is required
    EngineType engine = EngineType::eCustom;
    //! Optional - Version of the rendering engine used
    const char* engineVersion{};
    //! Optional - GUID (like for example 'a0f57b54-1daf-4934-90ae-c4035c19df04')
    const char* projectId{};
    
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(AppInfo)

#ifndef _WINNT_
//
// Locally Unique Identifier
//
typedef struct _LUID {
    unsigned long LowPart;
    long HighPart;
} LUID, * PLUID;
#endif

//! Interface 'AdapterSpec'
//!
//! {14F70C3F-9D6A-41E8-ABB2-9D15F7F83E5C}
struct alignas(8) AdapterSpec
{
    AdapterSpec() { };
    NVAIM_UID(UID({ 0x14f70c3f, 0x9d6a, 0x41e8,{0xab, 0xb2, 0x9d, 0x15, 0xf7, 0xf8, 0x3e, 0x5c} }), kStructVersion1);

    LUID id{};
    VendorId vendor{};
    size_t dedicatedMemoryInMB{}; // not shared with CPU

    //! Valid only for VendorId::eNVDA
    Version driverVersion{};
    uint32_t architecture{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(AdapterSpec)

//! Interface 'PluginSpec'
//!
//! {F997FBB5-9862-482E-929C-ADF8974E3645}
struct alignas(8) PluginSpec
{
    PluginSpec() { };
    NVAIM_UID(UID({ 0xf997fbb5, 0x9862, 0x482e,{0x92, 0x9c, 0xad, 0xf8, 0x97, 0x4e, 0x36, 0x45} }), kStructVersion1);

    PluginID id{};
    const char* pluginName{};
    Version pluginVersion{};
    Version pluginAPI{};
    Version requiredOSVersion{};
    Version requiredAdapterDriverVersion{};
    VendorId requiredAdapterVendor{};
    uint32_t requiredAdapterArchitecture{};

    //! ResultOk if supported, specific error otherwise
    Result status{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(PluginSpec)

//! System bit flags
//! 
enum class SystemFlags : uint64_t
{
    eNone = 0x00,
    eHWSchedulingEnabled = 0x01
};

NVAIM_ENUM_OPERATORS_64(SystemFlags);

//! Interface 'PluginAndSystemInformation'
//!
//! NOTE: All allocations are managed by 'nvaim.core.framework'
//! and are valid until nvaimShutdown is called.
//! 
//! {EAFD9312-13FA-4DBD-9C05-1B43FD797F74}
struct alignas(8) PluginAndSystemInformation
{
    PluginAndSystemInformation() { };
    NVAIM_UID(UID({ 0xeafd9312, 0x13fa, 0x4dbd,{0x9c, 0x05, 0x1b, 0x43, 0xfd, 0x79, 0x7f, 0x74} }), kStructVersion1);
        
    size_t numDetectedPlugins{};
    const PluginSpec** detectedPlugins{};
    
    size_t numDetectedAdapters{};
    const AdapterSpec** detectedAdapters{};

    Version osVersion{};
    SystemFlags flags{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVAIM_UID macro!
};

NVAIM_VALIDATE_STRUCT(PluginAndSystemInformation)

}