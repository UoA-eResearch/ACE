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

namespace nvaim
{

using Result = uint32_t;

//! Common results
//!
//! IMPORTANT: Interfaces should define custom error codes in their respective headers
//! 
//! The following formatting should be used:
//! 
//!     32bit RESULT VALUE
//! | 31 ... 24  | 23 ....  0 |
//! | ERROR CODE | FEATURE ID |
//! 
//! constexpr uint32_t ResultMyResult = (myErrorCode << 24) | nvaim::plugin::$NAME::$BACKEND::$API::kId.crc24; // crc24 is auto generated and part of the PluginID
//!
//! Here is an example from "source\plugins\nvaim.net\nvaim_net.h"
//! 
//! constexpr uint32_t ResultNetMissingAuthentication = 1 << 24 | plugin::net::kId.crc24;

//! Up to 256 error codes, bottom 24bits are reserved and must be 0 since these are common error codes
//! 
constexpr uint32_t ResultOk = 0;
constexpr uint32_t ResultDriverOutOfDate = 1 << 24;
constexpr uint32_t ResultOSOutOfDate = 2 << 24;
constexpr uint32_t ResultNoPluginsFound = 3 << 24;
constexpr uint32_t ResultInvalidParameter = 4 << 24;
constexpr uint32_t ResultNoSupportedHardwareFound = 5 << 24;
constexpr uint32_t ResultMissingInterface = 6 << 24;
constexpr uint32_t ResultMissingDynamicLibraryDependency = 7 << 24;
constexpr uint32_t ResultInvalidState = 8 << 24;
constexpr uint32_t ResultException = 9 << 24;
constexpr uint32_t ResultJSONException = 10 << 24;
constexpr uint32_t ResultRPCError = 11 << 24;
constexpr uint32_t ResultInsufficientResources = 12 << 24;
constexpr uint32_t ResultNotReady = 13 << 24;
constexpr uint32_t ResultPluginOutOfDate = 14 << 24;
constexpr uint32_t ResultDuplicatedPluginId = 15 << 24;

}
