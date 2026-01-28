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
//! Avoiding system includes with INT_MAX and FLT_MAX
constexpr float kUnassignedF = 3.402823466e+38F;
constexpr int kUnassignedI = 2147483647;
}

#define NVAIM_ENUM_OPERATORS_64(T)                                                      \
inline bool operator&(T a, T b)                                                         \
{                                                                                       \
    return ((uint64_t)a & (uint64_t)b) != 0;                                            \
}                                                                                       \
                                                                                        \
inline T& operator&=(T& a, T b)                                                         \
{                                                                                       \
    a = (T)((uint64_t)a & (uint64_t)b);                                                 \
    return a;                                                                           \
}                                                                                       \
                                                                                        \
inline T operator|(T a, T b)                                                            \
{                                                                                       \
    return (T)((uint64_t)a | (uint64_t)b);                                              \
}                                                                                       \
                                                                                        \
inline T& operator |= (T& lhs, T rhs)                                                   \
{                                                                                       \
    lhs = (T)((uint64_t)lhs | (uint64_t)rhs);                                           \
    return lhs;                                                                         \
}                                                                                       \
                                                                                        \
inline T operator~(T a)                                                                 \
{                                                                                       \
    return (T)~((uint64_t)a);                                                           \
}

#define NVAIM_ENUM_OPERATORS_32(T)                                                      \
inline bool operator&(T a, T b)                                                         \
{                                                                                       \
    return ((uint32_t)a & (uint32_t)b) != 0;                                            \
}                                                                                       \
                                                                                        \
inline T& operator&=(T& a, T b)                                                         \
{                                                                                       \
    a = (T)((uint32_t)a & (uint32_t)b);                                                 \
    return a;                                                                           \
}                                                                                       \
                                                                                        \
inline T operator|(T a, T b)                                                            \
{                                                                                       \
    return (T)((uint32_t)a | (uint32_t)b);                                              \
}                                                                                       \
                                                                                        \
inline T& operator |= (T& lhs, T rhs)                                                   \
{                                                                                       \
    lhs = (T)((uint32_t)lhs | (uint32_t)rhs);                                           \
    return lhs;                                                                         \
}                                                                                       \
                                                                                        \
inline T operator~(T a)                                                                 \
{                                                                                       \
    return (T)~((uint32_t)a);                                                           \
}

