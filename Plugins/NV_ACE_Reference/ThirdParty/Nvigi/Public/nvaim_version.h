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

#define NVAIM_VERSION_MAJOR 1
#define NVAIM_VERSION_MINOR 0
#define NVAIM_VERSION_PATCH 0

// Needed for some versions of g++
#undef major
#undef minor

namespace nvaim
{

constexpr uint64_t kSDKVersionMagic = 0xab15;
constexpr uint64_t kSDKVersion = (uint64_t(NVAIM_VERSION_MAJOR) << 48) | (uint64_t(NVAIM_VERSION_MINOR) << 32) | (uint64_t(NVAIM_VERSION_PATCH) << 16) | kSDKVersionMagic;

struct alignas(8) Version
{
    Version() : major(0), minor(0), build(0) {};
    Version(uint32_t v1, uint32_t v2, uint32_t v3) : major(v1), minor(v2), build(v3) {};

    inline operator bool() const { return major != 0 || minor != 0 || build != 0; }

    inline bool operator==(const Version& rhs) const
    {
        return major == rhs.major && minor == rhs.minor && build == rhs.build;
    }
    inline bool operator>(const Version& rhs) const
    {
        if (major < rhs.major) return false;
        else if (major > rhs.major) return true;
        // major version the same
        if (minor < rhs.minor) return false;
        else if (minor > rhs.minor) return true;
        // minor version the same
        if (build < rhs.build) return false;
        else if (build > rhs.build) return true;
        // build version the same
        return false;
    };
    inline bool operator>=(const Version& rhs) const
    {
        return operator>(rhs) || operator==(rhs);
    };
    inline bool operator<(const Version& rhs) const
    {
        if (major > rhs.major) return false;
        else if (major < rhs.major) return true;
        // major version the same
        if (minor > rhs.minor) return false;
        else if (minor < rhs.minor) return true;
        // minor version the same
        if (build > rhs.build) return false;
        else if (build < rhs.build) return true;
        // build version the same
        return false;
    };
    inline bool operator<=(const Version& rhs) const
    {
        return operator<(rhs) || operator==(rhs);
    };

    uint32_t major;
    uint32_t minor;
    uint32_t build;
};

NVAIM_VALIDATE_STRUCT(Version)

}
