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

#include "nvaim_types.h"

#ifdef _WIN32
#define NVAIM_API extern "C"
#else
#define NVAIM_API extern "C" __attribute__((visibility("default")))
#endif
#define NVAIM_CHECK(f) {auto _r = f; if(_r != nvaim::ResultOk) return _r;}
#define NVAIM_FAILED(r, f) nvaim::Result r = f; r != nvaim::ResultOk
#define NVAIM_SUCCEEDED(r, f) nvaim::Result r = f; r == nvaim::ResultOk
#define NVAIM_FUN_DECL(name) PFun_##name* name{}

namespace nvaim {

//! Different levels for logging
enum class LogLevel : uint32_t
{
    //! No logging
    eOff,
    //! Default logging
    eDefault,
    //! Verbose logging
    eVerbose,
    //! Total count
    eCount
};

//! Log type
enum class LogType : uint32_t
{
    //! Controlled by LogLevel, NVAIM can show more information in eLogLevelVerbose mode
    eInfo,
    //! Always shown regardless of LogLevel
    eWarn,
    eError,
    //! Total count
    eCount
};

//! Logging callback
//!
//! Use these callbacks to track messages posted in the log.
//! If any of the NVAIM methods returns false use eLogTypeError
//! type to track down what went wrong and why.
using PFun_LogMessageCallback = void(LogType type, const char* msg);

//! Optional flags
enum class PreferenceFlags : uint64_t
{
    //! Optional - Enables downloading of Over The Air (OTA) updates for NVAIM
    //!
    //! This will invoke the OTA updater to look for new updates.
    eAllowOTA = 1 << 0,
    //! Optional - Disables automatic process privilege downgrade when calling NVAIM functions
    //!
    //! If host process is running with elevated privileges NVAIM will try to downgrade them as needed.
    //! Setting this flag will override this behavior hence potentially introducing security vulnerability.
    eDisablePrivilegeDowngrade = 1 << 1,
    //! Optional - Disables higher resolution timer frequency
    //!
    //! For optimal timing performance NVAIM adjusts CPU timer resolution frequency. 
    //! Set this flag to opt out and leave it unchanged.
    eDisableCPUTimerResolutionChange = 1 << 2
};

NVAIM_ENUM_OPERATORS_64(PreferenceFlags)

//! Application preferences
//!
//! {1CA10965-BF8E-432B-8DA1-6716D879FB14}
struct alignas(8) Preferences {
    Preferences() {}; 
    NVAIM_UID(UID({ 0x1ca10965, 0xbf8e, 0x432b,{ 0x8d, 0xa1, 0x67, 0x16, 0xd8, 0x79, 0xfb, 0x14 } }), kStructVersion1)
    //! Optional - In non-production builds it is useful to enable debugging console window
    bool showConsole = false;
    //! Optional - Various logging levels
    LogLevel logLevel = LogLevel::eDefault;
    //! Optional - Paths to locations where to look for plugins and their dependencies
    //!
    //! NOTE: Duplicated plugins or dependencies are NOT allowed
    const char** utf8PathsToPlugins{};
    //! Optional - Number of paths to search
    uint32_t numPathsToPlugins = 0;
    //! Optional - Path to the location where logs and other data should be stored
    //! 
    //! NOTE: Set this to nullptr in order to disable logging to a file
    const char* utf8PathToLogsAndData{};
    //! Optional - Allows log message tracking including critical errors if they occur
    PFun_LogMessageCallback* logMessageCallback{};
    //! Optional - Flags used to enable or disable advanced options
    PreferenceFlags flags{};
    //! Optional - Path to the location where to look for plugin dependencies
    //!
    //! NOTE: If not provided NVAIM will assume that dependencies are next to the plugin(s)
    //! and that there are NO shared dependencies since they cannot be duplicated.
    const char* utf8PathToDependencies{};

    //! IMPORTANT: New members go here or if optional can be chained in a new struct, see nvaim_struct.h for details
};

NVAIM_VALIDATE_STRUCT(Preferences)

struct BaseStructure;
}

//! AI Inference Manager (NVAIM) core API functions (check feature specific headers for additional APIs)
//! 
using PFun_nvaimInit = nvaim::Result(const nvaim::Preferences& pref, nvaim::PluginAndSystemInformation** pluginInfo, uint64_t sdkVersion);
using PFun_nvaimShutdown = nvaim::Result();
using PFun_nvaimLoadInterface = nvaim::Result(nvaim::PluginID feature, const nvaim::UID& interfaceType, uint32_t interfaceVersion, void** _interface);
using PFun_nvaimUnloadInterface = nvaim::Result(nvaim::PluginID feature, void* _interface);

//! Initializes the NVAIM framework
//!
//! Call this method when your application is initializing
//!
//! @param pref Specifies preferred behavior for the NVAIM framework (NVAIM will keep a copy)
//! @param pluginInfo Optional pointer to data structure containing information about plugins, user system
//! @param sdkVersion Current SDK version
//! @returns nvaim::kResultOk if successful, error code otherwise (see nvaim_result.h for details)
//!
//! This method is NOT thread safe.
NVAIM_API nvaim::Result nvaimInit(const nvaim::Preferences &pref, nvaim::PluginAndSystemInformation** pluginInfo = nullptr, uint64_t sdkVersion = nvaim::kSDKVersion);

//! Shuts down the NVAIM module
//!
//! Call this method when your application is shutting down. 
//!
//! @returns nvaim::kResultOk if successful, error code otherwise (see nvaim_result.h for details)
//!
//! This method is NOT thread safe.
NVAIM_API nvaim::Result nvaimShutdown();

//! Loads an interface for a specific NVAIM feature
//!
//! Call this method when specific interface is needed.
//!
//! NOTE: Interfaces are reference counted so they all must be released before underlying plugin is released.
//!
//! @param feature Specifies feature which needs to provide the requested interface
//! @param interfaceType Type of the interface to obtain
//! @param interfaceVersion Minimal version of the interface to obtain
//! @param interface Pointer to the interface
//! @returns nvaim::kResultOk if successful, error code otherwise (see nvaim_result.h for details)
//!
//! NOTE: It is recommended to use the templated helpers `nvaimGetInterface` or `nvaimGetInterfaceDynamic` (see below in this header)
//! 
//! For example:
//! 
//! nvaim::IGeneralPurposeTransformer* igpt{};
//! 
//! // Static linking
//! nvaimGetInterface(nvaim::plugin::gpt::ggml::cuda::kId, &igpt);
//! 
//! // Dynamic `nvaim.core.framework` loading
//! nvaimGetInterfaceDynamic(nvaim::plugin::gpt::ggml::cuda::kId, &igpt, nvaimLoadInterfaceFunction);
//! 
//! This method is NOT thread safe.
NVAIM_API nvaim::Result nvaimLoadInterface(nvaim::PluginID feature, const nvaim::UID& interfaceType, uint32_t interfaceVersion, void** _interface);

//! Unloads an interface for a specific NVAIM feature
//!
//! Call this method when specific interface is no longer needed
//!
//! NOTE: Interfaces are reference counted so they all must be released before underlying plugin is released.
//!
//! @param feature Specifies feature which provided the interface
//! @param interface Pointer to the interface
//! @returns nvaim::kResultOk if successful, error code otherwise (see nvaim_result.h for details)
//!
//! This method is NOT thread safe.
NVAIM_API nvaim::Result nvaimUnloadInterface(nvaim::PluginID feature, void* _interface);

//! Helper method when statically linking NVAIM framework
//! 
template<typename T>
inline nvaim::Result nvaimGetInterface(nvaim::PluginID feature, T** _interface)
{
    void* tmp{};
    T t{};
    if (NVAIM_FAILED(result, nvaimLoadInterface(feature, T::s_type, t.getVersion(), & tmp)))
    {
        return result;
    }
    *_interface = static_cast<T*>(tmp);
    return nvaim::ResultOk;
}

//! Helper method when dynamically loading NVAIM framework
//! 
template<typename T>
inline nvaim::Result nvaimGetInterfaceDynamic(nvaim::PluginID feature, T** _interface, PFun_nvaimLoadInterface* func)
{
    void* tmp{};
    T t{};
    if (NVAIM_FAILED(result, func(feature, T::s_type, t.getVersion(), &tmp)))
    {
        return result;
    }
    *_interface = static_cast<T*>(tmp);
    return nvaim::ResultOk;
}