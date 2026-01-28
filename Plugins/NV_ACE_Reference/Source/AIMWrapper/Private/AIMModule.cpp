/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 - 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "AIMModule.h"

// engine includes
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ReverseIterate.h"

#if PLATFORM_WINDOWS
#include "ID3D12DynamicRHI.h"
#include "nvapi.h"
#endif

// plugin includes
#include "AIMLog.h"
#include "nvaim.h"
#include "nvaim_ai.h"
#include "nvaim_d3d12.h"
#include "nvaim_hwi_cuda.h"

#if PLATFORM_WINDOWS
// nvaim_types.h requires windows headers for LUID definition, only on windows. Strange but true.
#include "Windows/AllowWindowsPlatformTypes.h"
#include "nvaim_types.h"
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include "nvaim_types.h"
#endif


#define LOG_AIM_FEATURE_REQUIREMENTS !UE_BUILD_SHIPPING

#define LOCTEXT_NAMESPACE "FAIMModule"

DEFINE_LOG_CATEGORY(LogACEAimWrapper);


static std::atomic<bool> GShushAIMLog{ false };
static void AIMLogCallback(nvaim::LogType Type, const char* InMessage);

// RAII for AIM core framework loading/initialization and shutdown/unloading
class FAIMCore
{
public:
	FAIMCore(FString InAIMCoreBinaryDirectory, const TSet<FString> InAIMBinaryDirectories, bool bShushAIMLog)
		: bIsAPIStarted(false)
		, AIMBinaryDirectories(InAIMBinaryDirectories.Array())
	{
		// dynamically load AIM core library
		FString AIMFullName = FPaths::Combine(*InAIMCoreBinaryDirectory, AIM_CORE_BINARY_NAME);
		AIMCoreDLL = FPlatformProcess::GetDllHandle(*AIMFullName);

		if (!AIMCoreDLL)
		{
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Cannot load AIM core DLL from %s"), *AIMFullName);
			return;
		}
		else
		{
			UE_LOG(LogACEAimWrapper, Log, TEXT("Loaded AIM core DLL from %s"), *AIMFullName);
		}

		// map library entry points
		Ptr_nvaimInit = (PFun_nvaimInit*)FPlatformProcess::GetDllExport(AIMCoreDLL, TEXT("nvaimInit"));
		Ptr_nvaimShutdown = (PFun_nvaimShutdown*)FPlatformProcess::GetDllExport(AIMCoreDLL, TEXT("nvaimShutdown"));
		Ptr_nvaimLoadInterface = (PFun_nvaimLoadInterface*)FPlatformProcess::GetDllExport(AIMCoreDLL, TEXT("nvaimLoadInterface"));
		Ptr_nvaimUnloadInterface = (PFun_nvaimUnloadInterface*)FPlatformProcess::GetDllExport(AIMCoreDLL, TEXT("nvaimUnloadInterface"));

		if (!(Ptr_nvaimInit && Ptr_nvaimShutdown && Ptr_nvaimLoadInterface && Ptr_nvaimUnloadInterface))
		{
			UE_LOG(LogACEAimWrapper, Error, TEXT("Cannot load AIM core functions"));
			return;
		}

		// initialize AIM framework
		nvaim::Preferences Pref;
#if UE_BUILD_SHIPPING
		Pref.showConsole = false;
#endif

		Pref.logLevel = nvaim::LogLevel::eDefault;

		// convert AIM binary paths to UTF8
		TArray<TArray<UTF8CHAR>> AIMBinaryDirectoriesUTF8;
		for (const FString& AIMBinaryDirectory : AIMBinaryDirectories)
		{
			const auto ConvertedUTF8 = StringCast<UTF8CHAR>(*AIMBinaryDirectory);
			AIMBinaryDirectoriesUTF8.Emplace(ConvertedUTF8.Get(), ConvertedUTF8.Length() + 1);
		}
		TArray<const char*> AIMPluginPaths;
		for (const TArray<UTF8CHAR>& AIMBinaryDirectoryUTF8 : AIMBinaryDirectoriesUTF8)
		{
			// AIM expects its UTF8-encoded strings as const char*
			AIMPluginPaths.Add(reinterpret_cast<const char*>(AIMBinaryDirectoryUTF8.GetData()));
		}
		Pref.utf8PathsToPlugins = AIMPluginPaths.GetData();
		Pref.numPathsToPlugins = AIMPluginPaths.Num();
		// utf8PathToDependencies doesn't actually work in current AIM versions
		//Pref.utf8PathToDependencies = AIMPluginPaths[0];

		Pref.logMessageCallback = AIMLogCallback;


		if (bShushAIMLog)
		{
			GShushAIMLog.store(true);
		}
		nvaim::Result InitResult = Init(Pref, &AIMRequirements, nvaim::kSDKVersion);
		if (bShushAIMLog)
		{
			GShushAIMLog.store(false);
		}

		if (InitResult == nvaim::ResultOk)
		{
			bIsAPIStarted = true;

#if LOG_AIM_FEATURE_REQUIREMENTS
			if (AIMRequirements != nullptr)
			{
				TArrayView<const nvaim::PluginSpec*> AIMPlugins(AIMRequirements->detectedPlugins, AIMRequirements->numDetectedPlugins);
				for (const nvaim::PluginSpec* AIMPlugin : AIMPlugins)
				{
					auto PluginNameUTF8 = StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(AIMPlugin->pluginName));
					FString PluginInfo = FString::Printf(TEXT("Plugin %s requirements: "), PluginNameUTF8.Get());
					if  ((AIMPlugin->requiredAdapterVendor != nvaim::VendorId::eAny) && (AIMPlugin->requiredAdapterVendor != nvaim::VendorId::eNone))
					{
						// if we ever encounter an AIM plugin that require non-NV vendors, we'll need to add logic here to deal with that
						if (ensure(AIMPlugin->requiredAdapterVendor == nvaim::VendorId::eNVDA))
						{
							PluginInfo.Appendf(TEXT("NVIDIA driver %d.%d.%d, "),
								AIMPlugin->requiredAdapterDriverVersion.major, AIMPlugin->requiredAdapterDriverVersion.minor, AIMPlugin->requiredAdapterDriverVersion.build);
							// look for NV_GPU_ARCHITECTURE_* definitions in nvapi.h to decode this number
							PluginInfo.Appendf(TEXT("NVIDIA GPU architecture 0x%x, "), AIMPlugin->requiredAdapterArchitecture);
						}
					}
					PluginInfo.Appendf(TEXT("OS %d.%d.%d"),
						AIMPlugin->requiredOSVersion.major, AIMPlugin->requiredOSVersion.minor, AIMPlugin->requiredOSVersion.build);
					UE_LOG(LogACEAimWrapper, Verbose, TEXT("%s"), *PluginInfo);
				}
			}
#endif
		}
		else
		{
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Unable to initialize AIM (%s)"), *GetAIMStatusString(InitResult));
		}
	}

	~FAIMCore()
	{
		Shutdown();
		if (AIMCoreDLL != nullptr)
		{
			FPlatformProcess::FreeDllHandle(AIMCoreDLL);
		}
	}

#define TRUST_AIM_BUT_VERIFY 1
	nvaim::Result LoadInterface(nvaim::PluginID Feature, const nvaim::UID& InterfaceType, uint32_t InterfaceVersion, void** Interface)
	{
		if (!(bIsAPIStarted && (Ptr_nvaimLoadInterface != nullptr)))
		{
			return nvaim::ResultInvalidState;
		}

		// Work around an AIM bug by explicitly adding AIM's own binary path since AIM can't find it 🙄
		for (const FString& AIMBinaryDirectory : AIMBinaryDirectories)
		{
			FPlatformProcess::PushDllDirectory(*AIMBinaryDirectory);
		}
		nvaim::Result Result = (*Ptr_nvaimLoadInterface)(Feature, InterfaceType, InterfaceVersion, Interface);
		for (const FString& AIMBinaryDirectory : ReverseIterate(AIMBinaryDirectories))
		{
			FPlatformProcess::PopDllDirectory(*AIMBinaryDirectory);
		}
#if TRUST_AIM_BUT_VERIFY
		bool bAIMFeatureSupported = DoesSystemSupportFeature(Feature);
		if (((Result == nvaim::ResultOk) && !ensureMsgf(bAIMFeatureSupported, TEXT("AIM loaded an unsupported feature, please report this bug with a full log file")))
			|| (Result != nvaim::ResultOk))
		{
			FString PluginDesc;
			if (AIMRequirements != nullptr)
			{
				TArrayView<const nvaim::PluginSpec*> Plugins(AIMRequirements->detectedPlugins, AIMRequirements->numDetectedPlugins);
				const nvaim::PluginSpec** FindResult = Plugins.FindByPredicate([&Feature](const nvaim::PluginSpec* Candidate) { return Candidate->id == Feature; });
				if (FindResult != nullptr)
				{
					const nvaim::PluginSpec* Plugin = *FindResult;
					if (ensure(Plugin != nullptr))
					{
						PluginDesc += FString::Printf(TEXT("%s: driver %d.%d.%d, gpu arch 0x%x, os %d.%d.%d"),
							UTF8_TO_TCHAR(Plugin->pluginName),
							Plugin->requiredAdapterDriverVersion.major,
							Plugin->requiredAdapterDriverVersion.minor,
							Plugin->requiredAdapterDriverVersion.build,
							Plugin->requiredAdapterArchitecture,
							Plugin->requiredOSVersion.major,
							Plugin->requiredOSVersion.minor,
							Plugin->requiredOSVersion.build);
					}

				}
				else
				{
					PluginDesc += TEXT("(unknown AIM feature)");
				}
			}
			else
			{
				PluginDesc = TEXT("(unknown feature requirements)");
			}

			if (Result == nvaim::ResultOk)
			{
				UE_LOG(LogACEAimWrapper, Warning, TEXT("AIM loaded unsupported feature %s"), *PluginDesc);
			}
			else
			{
				FString SystemDesc;
				if (AIMRequirements != nullptr)
				{
					SystemDesc = FString::Printf(TEXT("os %d.%d.%d"),
						AIMRequirements->osVersion.major,
						AIMRequirements->osVersion.minor,
						AIMRequirements->osVersion.build);
					if (AIMRequirements->detectedAdapters != nullptr)
					{
						TArrayView<const nvaim::AdapterSpec*> AIMDetectedAdapters(AIMRequirements->detectedAdapters, AIMRequirements->numDetectedAdapters);
						for (const nvaim::AdapterSpec* AIMDetectedAdapter : AIMDetectedAdapters)
						{
							if (AIMDetectedAdapter != nullptr)
							{
								switch (AIMDetectedAdapter->vendor)
								{
								case nvaim::VendorId::eNVDA:
									SystemDesc += TEXT(", NVIDIA");
									break;
								case nvaim::VendorId::eAMD:
									SystemDesc += TEXT(", AMD");
									break;
								case nvaim::VendorId::eIntel:
									SystemDesc += TEXT(", Intel");
									break;
								case nvaim::VendorId::eMS:
									SystemDesc += TEXT(", MS");
									break;
								default:
									SystemDesc += TEXT(", unknown adapter");
									break;
								}
								SystemDesc += FString::Printf(TEXT(" %d MiB VRAM"), AIMDetectedAdapter->dedicatedMemoryInMB);
								SystemDesc += FString::Printf(TEXT(" %d.%d.%d driver"),
									AIMDetectedAdapter->driverVersion.major,
									AIMDetectedAdapter->driverVersion.minor,
									AIMDetectedAdapter->driverVersion.build);
								SystemDesc += FString::Printf(TEXT(" 0x%x arch"), AIMDetectedAdapter->architecture);
							}
							else
							{
								SystemDesc += TEXT(", unknown adapter");
							}
						}
					}
					else
					{
						SystemDesc += TEXT(", no detected adapters");
					}
				}
				else
				{
					SystemDesc = TEXT("unknown");
				}
				// if AIM couldn't load a feature it might still be interesting to know what AIM detected in the system
				UE_LOG(LogACEAimWrapper, Verbose, TEXT("AIM couldn't load feature %s. System %s"), *PluginDesc, *SystemDesc);
			}
		}
#endif
		return Result;
	}

	nvaim::Result UnloadInterface(nvaim::PluginID feature, void* _interface)
	{
		if (!(bIsAPIStarted && (Ptr_nvaimUnloadInterface != nullptr)))
		{
			return nvaim::ResultInvalidState;
		}
		return (*Ptr_nvaimUnloadInterface)(feature, _interface);
	}

	bool DoesSystemSupportFeature(nvaim::PluginID Feature)
	{
		if (AIMRequirements != nullptr)
		{
			TArrayView<const nvaim::PluginSpec*> AIMPlugins(AIMRequirements->detectedPlugins, AIMRequirements->numDetectedPlugins);
			const nvaim::PluginSpec** FindResult = AIMPlugins.FindByPredicate([&Feature](const nvaim::PluginSpec* Candidate) { return Candidate->id == Feature; });
			if (FindResult != nullptr)
			{
				const nvaim::PluginSpec* Plugin = *FindResult;
				if (Plugin != nullptr)
				{
					// OS version
#if PLATFORM_WINDOWS
					// AIM features report the wrong minimum OS version on Windows
					// https://jirasw.nvidia.com/browse/HBLS-176
					nvaim::Version GlobalAIMMinOSVersion{ 10, 0, 19041 };
					nvaim::Version AIMReportedMinOSVersion = Plugin->requiredOSVersion;
					nvaim::Version ActualMinOSVersion = FMath::Max(GlobalAIMMinOSVersion, AIMReportedMinOSVersion);

					bool bRequiredOSVersionFound = FWindowsPlatformMisc::VerifyWindowsVersion(ActualMinOSVersion.major, ActualMinOSVersion.minor, ActualMinOSVersion.build);
#else
					// TODO: OS version check for Linux
					bool bRequiredOSVersionFound = true;
#endif

					// graphics adapter
					bool bRequiredAdapterFound = false;
					nvaim::VendorId Vendor = Plugin->requiredAdapterVendor;
					if (Vendor == nvaim::VendorId::eAny)
					{
						bRequiredAdapterFound = AIMRequirements->numDetectedAdapters > 0;
					}
					else if (Vendor != nvaim::VendorId::eNone)
					{
						TArrayView<const nvaim::AdapterSpec*> Adapters(AIMRequirements->detectedAdapters, AIMRequirements->numDetectedAdapters);
						for (const nvaim::AdapterSpec* Adapter : Adapters)
						{
							if ((Adapter->vendor == Vendor) && (Adapter->architecture >= Plugin->requiredAdapterArchitecture) && (Adapter->driverVersion >= Plugin->requiredAdapterDriverVersion))
							{
								bRequiredAdapterFound = true;
								break;
							}
						}
					}

					return bRequiredOSVersionFound && bRequiredAdapterFound;
				} // if (Plugin != nullptr)
			}
		} // if (AIMRequirements != nullptr)

		return false;
	}

private:

	nvaim::Result Init(const nvaim::Preferences& pref, nvaim::PluginAndSystemInformation** pluginInfo, uint64_t sdkVersion)
	{
		if (!Ptr_nvaimInit)
		{
			return nvaim::ResultInvalidState;
		}
		return (*Ptr_nvaimInit)(pref, pluginInfo, sdkVersion);
	}

	nvaim::Result Shutdown()
	{
		if (!Ptr_nvaimShutdown)
		{
			return nvaim::ResultInvalidState;
		}
		bIsAPIStarted = false;
		return (*Ptr_nvaimShutdown)();
	}

private:

	bool bIsAPIStarted{ false };
	TArray<FString> AIMBinaryDirectories;
	void* AIMCoreDLL{};
	nvaim::PluginAndSystemInformation* AIMRequirements{};

	PFun_nvaimInit* Ptr_nvaimInit{};
	PFun_nvaimShutdown* Ptr_nvaimShutdown{};
	PFun_nvaimLoadInterface* Ptr_nvaimLoadInterface{};
	PFun_nvaimUnloadInterface* Ptr_nvaimUnloadInterface{};
};

////////////////////////
// AIM feature registry
//
// - prevents incompatible AIM features from being loaded at the same time
// - ensures thread safety for the thread-unsafe AIM API
// - reinitializes the AIM framework as necessary when new AIM binary paths are added
// - checks for leaked AIM features on exit
//
class FAIMFeatureRegistry: FNoncopyable
{
public:
	FAIMFeatureRegistry(const FString& InAIMCoreBinaryDirectory) : AIMCoreBinaryDirectory(InAIMCoreBinaryDirectory), AIMBinaryDirectories({ InAIMCoreBinaryDirectory }), CS()
	{
	}

	~FAIMFeatureRegistry()
	{
		if (InterfaceCIG != nullptr)
		{
			if (CudaParameters.context != nullptr)
			{
				InterfaceCIG->cudaReleaseSharedContext(CudaParameters.context);
			}
			UnloadFeature(nvaim::plugin::hwi::cuda::kId, InterfaceCIG);
		}

		FScopeLock Lock(&CS);
		if (!LoadedFeatures.IsEmpty())
		{
			// Unreal plugin developer forgot to unload a feature
			UE_LOG(LogACEAimWrapper, Warning, TEXT("FAIMModule shutdown with AIM features still loaded!"));
			for (uint32_t FeatureCrc24 : LoadedFeatures)
			{
				FAIMFeature* Feature = Features.Find(FeatureCrc24);
				if (Feature != nullptr)
				{
					UE_LOG(LogACEAimWrapper, Warning, TEXT("Leaking AIM feature 0x%x with RefCount = %d"), FeatureCrc24, Feature->RefCount);
				}
				else
				{
					UE_LOG(LogACEAimWrapper, Warning, TEXT("Unregistered AIM feature 0x%x is loaded, this shouldn't happen"), FeatureCrc24);
				}
			}
		}
	}

	void RegisterFeature(nvaim::PluginID FeatureID, const TArray<FString>& BinaryPaths, const TArray<nvaim::PluginID>& IncompatibleFeatures)
	{
		FScopeLock Lock(&CS);

		if (Features.Contains(FeatureID.crc24))
		{
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Internal error, registering the same AIM feature twice is unsupported: 0x%x"), FeatureID.crc24);
			return;
		}

		Features.Emplace(FeatureID.crc24);

		// add incompatible features pointing both ways
		for (nvaim::PluginID IncompatibleFeatureID : IncompatibleFeatures)
		{
			TSet<uint32_t>& Ours = IncompatibleFeatureMap.FindOrAdd(FeatureID.crc24);
			Ours.Add(IncompatibleFeatureID.crc24);
			TSet<uint32_t>& Theirs = IncompatibleFeatureMap.FindOrAdd(IncompatibleFeatureID.crc24);
			Theirs.Add(FeatureID.crc24);
		}

		bool bNeedsAIMCoreRecreate = false;
		for (const FString& BinaryPath : BinaryPaths)
		{
			if (!AIMBinaryDirectories.Contains(BinaryPath))
			{
				AIMBinaryDirectories.Add(BinaryPath);
				bNeedsAIMCoreRecreate = true;
			}
		}

		if (bNeedsAIMCoreRecreate)
		{
			if (LoadedFeatures.IsEmpty())
			{
				AIMCore.Reset();
			}
			else
			{
				FString Message{ TEXT("New AIM binary paths added but unable to reinitialize AIM due to loaded features: ") };
				for (uint32_t FeatureCrc24 : LoadedFeatures)
				{
					Message.Appendf(TEXT("0x%x "), FeatureCrc24);
				}
				UE_LOG(LogACEAimWrapper, Warning, TEXT("%s"), *Message);
			}
		}
	}

	bool IsAIMFeatureAvailable(nvaim::PluginID FeatureID)
	{
		FScopeLock Lock(&CS);
		FAIMFeature* Feature = Features.Find(FeatureID.crc24);
		if (Feature == nullptr)
		{
			// Unreal plugin developer forgot to register the feature first
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Unreal plugin developer error, requested unregistered AIM feature 0x%x"), FeatureID.crc24);
			return false;
		}
		else if (Feature->RefCount > 0)
		{
			// something already has it loaded, so it's available
			return true;
		}

		// check for incompatible features currently loaded
		if (IsIncompatibleFeatureLoaded(FeatureID.crc24))
		{
			return false;
		}

		// we need to attempt to load the feature to learn if it's available
		if (!AIMCore.IsValid())
		{
			AIMCore = MakeUnique<FAIMCore>(AIMCoreBinaryDirectory, AIMBinaryDirectories, true);
		}

		nvaim::InferenceInterface DummyInterface;
		void* TmpInterface = nullptr;

		// load a temporary feature interface to learn if AIM feature is available
		GShushAIMLog.store(true);
		nvaim::Result Result = AIMCore->LoadInterface(FeatureID, nvaim::InferenceInterface::s_type, DummyInterface.getVersion(), &TmpInterface);
		if (TmpInterface != nullptr)
		{
			AIMCore->UnloadInterface(FeatureID, TmpInterface);
		}
		GShushAIMLog.store(false);

		if (Result != nvaim::ResultOk)
		{
			UE_LOG(LogACEAimWrapper, Log, TEXT("AIM feature 0x%x not available (%s)"), FeatureID.crc24, *GetAIMStatusString(Result));
			return false;
		}
		return true;
	}

	template<class T>
	nvaim::Result LoadFeature(nvaim::PluginID FeatureID, T** Interface, bool bShushAIMLog)
	{
		FScopeLock Lock(&CS);
		FAIMFeature* Feature = Features.Find(FeatureID.crc24);
		if (Feature == nullptr)
		{
			// Unreal plugin developer forgot to register the feature first
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Unreal plugin developer error, requested unregistered AIM feature 0x%x"), FeatureID.crc24);
			return nvaim::ResultInvalidState;
		}

		// check for incompatible features
		if (IsIncompatibleFeatureLoaded(FeatureID.crc24))
		{
			return nvaim::ResultNotReady;
		}

		if (!AIMCore.IsValid())
		{
			AIMCore = MakeUnique<FAIMCore>(AIMCoreBinaryDirectory, AIMBinaryDirectories, bShushAIMLog);
		}

		T DummyInterface;
		if (bShushAIMLog)
		{
			GShushAIMLog.store(true);
		}
		nvaim::Result Result = AIMCore->LoadInterface(FeatureID, T::s_type, DummyInterface.getVersion(), reinterpret_cast<void**>(Interface));
		if (bShushAIMLog)
		{
			GShushAIMLog.store(false);
		}

		if (Result == nvaim::ResultOk)
		{
			Feature->RefCount += 1;
			LoadedFeatures.Add(FeatureID.crc24);
		}
		else
		{
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Failed to load AIM feature 0x%x (%s)"), FeatureID.crc24, *GetAIMStatusString(Result));
		}

		return Result;
	}

	template <class T>
	nvaim::Result UnloadFeature(nvaim::PluginID FeatureID, T* Interface)
	{
		FScopeLock Lock(&CS);
		FAIMFeature* Feature = Features.Find(FeatureID.crc24);
		if (Feature == nullptr)
		{
			// Unreal plugin developer forgot to register the feature first
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Unreal plugin developer error, requested unregistered AIM feature 0x%x"), FeatureID.crc24);
			return nvaim::ResultInvalidState;
		}

		if (Feature->RefCount < 1)
		{
			// nothing to do, we're done!
			return nvaim::ResultOk;
		}

		if (!AIMCore.IsValid())
		{
			AIMCore = MakeUnique<FAIMCore>(AIMCoreBinaryDirectory, AIMBinaryDirectories, true);
		}

		nvaim::Result Result = AIMCore->UnloadInterface(FeatureID, reinterpret_cast<void*>(Interface));
		if (Result == nvaim::ResultOk)
		{
			Feature->RefCount -= 1;
			if (Feature->RefCount < 1)
			{
				LoadedFeatures.Remove(FeatureID.crc24);
			}
		}
		else
		{
			UE_LOG(LogACEAimWrapper, Warning, TEXT("Failed to unload AIM feature 0x%x (%s)"), FeatureID.crc24, *GetAIMStatusString(Result));
		}

		return Result;
	}

	nvaim::CudaParameters* GetCIGCudaParameters()
	{
		TryInitCIG();

		return CudaParameters.context != nullptr ? &CudaParameters : nullptr;
	}

private:
	bool IsIncompatibleFeatureLoaded(uint32_t FeatureCrc24) const
	{
		const TSet<uint32>* IncompatibleFeaturesCrc24 = IncompatibleFeatureMap.Find(FeatureCrc24);
		if (IncompatibleFeaturesCrc24 != nullptr)
		{
			for (uint32_t IncompatibleFeatureCrc24 : *IncompatibleFeaturesCrc24)
			{
				if (LoadedFeatures.Contains(IncompatibleFeatureCrc24))
				{
					UE_LOG(LogACEAimWrapper, Log, TEXT("Can't load AIM feature 0x%x due to incompatible feature 0x%x"), FeatureCrc24, IncompatibleFeatureCrc24);
					return true;
				}
			}
		}
		return false;
	}

	void TryInitCIG()
	{
		if (bCIGTriedToInitialize)
		{
			return;
		}

		bCIGTriedToInitialize = true;

#if PLATFORM_WINDOWS
		int UseCIGforAI = 1; // Use CIG by default;
		FParse::Value(FCommandLine::Get(), TEXT("useCIGforAI="), UseCIGforAI);
		if (UseCIGforAI)
		{
			// Work around AIM bug:
			// AIM only supports CiG on Ada+ architecture, but AIM will claim at runtime the nvaim::plugin::hwi::cuda::kId
			// feature supports Volta+, and it will create the feature even though it is untested and apparently buggy.
			// So we have to manually prevent AIM from shooting itself in the foot.
			bool bRequiredArchFound = false;
			{
				NvU32 NumNVGPUs = 0;
				NvPhysicalGpuHandle NVGPUHandles[NVAPI_MAX_PHYSICAL_GPUS];

				NvAPI_Status NVStatus = NvAPI_EnumPhysicalGPUs(NVGPUHandles, &NumNVGPUs);
				if (NVStatus == NvAPI_Status::NVAPI_OK)
				{
					TArrayView<const NvPhysicalGpuHandle> GPUHandles(NVGPUHandles, NumNVGPUs);
					for (const NvPhysicalGpuHandle& GPUHandle : GPUHandles)
					{
						NV_GPU_ARCH_INFO GPUArchInfo;
						GPUArchInfo.version = NV_GPU_ARCH_INFO_VER;
						NVStatus = NvAPI_GPU_GetArchInfo(GPUHandle, &GPUArchInfo);
						if (NVStatus == NvAPI_Status::NVAPI_OK)
						{
							if (GPUArchInfo.architecture >= NV_GPU_ARCHITECTURE_AD100)
							{
								bRequiredArchFound = true;
								break;
							}
						}
					}
				}
			}

			if (bRequiredArchFound)
			{
				ID3D12DynamicRHI* D3D12RHI = nullptr;
				if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12)
				{
					D3D12RHI = static_cast<ID3D12DynamicRHI*>(GDynamicRHI);
				}

				if (D3D12RHI)
				{
					ID3D12CommandQueue* CmdQ = D3D12RHI->RHIGetCommandQueue();
					int DeviceIndex = 0;
					ID3D12Device* D3D12Device = D3D12RHI->RHIGetDevice(DeviceIndex);

					if (CmdQ && D3D12Device)
					{
						RegisterFeature(nvaim::plugin::hwi::cuda::kId, {}, {});

						nvaim::Result Result = LoadFeature(nvaim::plugin::hwi::cuda::kId, &InterfaceCIG, true);
						if (Result != nvaim::ResultOk)
						{
							UE_LOG(LogACEAimWrapper, Warning, TEXT("Unable to load hwi::cuda feature, CIG will not be available, performance may be affected"));
							InterfaceCIG = nullptr;
						} else
						{
							check(InterfaceCIG != nullptr);
							nvaim::D3D12Parameters D3D12Params;
							D3D12Params.device = D3D12Device;
							D3D12Params.queue = CmdQ;
							Result = InterfaceCIG->cudaGetSharedContextForQueue(D3D12Params, &CudaParameters.context);
							if (Result == nvaim::ResultOk)
							{
								UE_LOG(LogACEAimWrapper, Log, TEXT("Created CIG context %p"), CudaParameters.context);
							} else
							{
								UE_LOG(LogACEAimWrapper, Warning, TEXT("Cannot create CIG context, cudaGetSharedContextForQueue failed (%s)"), *GetAIMStatusString(Result));
							}
						}
					} else
					{
						UE_LOG(LogACEAimWrapper, Warning, TEXT("Cannot create CIG context, CmdQ %p D3D12Device %p icig %p"), CmdQ, D3D12Device, InterfaceCIG);
					}
				}
			}
			else
			{
				UE_LOG(LogACEAimWrapper, Warning, TEXT("CIG not supported for current GPU architecture, performance may be affected"));
			}
		}
		else
		{
			UE_LOG(LogACEAimWrapper, Log, TEXT("Not using CIG. If you'd like to use it, please add -useCIGforAI=1 to the executable's parameters"));
		}
#endif
	}

private:
	struct FAIMFeature
	{
		int32 RefCount = 0;
		// we might add a FName or FString description here for better log messages
	};

private:
	TUniquePtr<class FAIMCore> AIMCore;
	const FString AIMCoreBinaryDirectory;
	TSet<FString> AIMBinaryDirectories;
	TSet<uint32_t> LoadedFeatures;

	FCriticalSection CS;
	TMap<uint32_t, FAIMFeature> Features;
	TMap<uint32_t, TSet<uint32_t>> IncompatibleFeatureMap;

	// CIG
	bool bCIGTriedToInitialize = false;
	nvaim::IHWICuda* InterfaceCIG = nullptr;
	nvaim::CudaParameters CudaParameters{};
};

///////////////////////////////////
// IModuleInterface implementation
void FAIMModule::StartupModule()
{
	const FString PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetBaseDir();

	// Make sure we have the absolute path to the plugin directory
	FString PluginBaseDirAbsolute = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*PluginBaseDir);

	const FString Platform = FPlatformProcess::GetBinariesSubdirectory();
	const FString AIMCoreBinaryDirectory = FPaths::Combine(*PluginBaseDirAbsolute, TEXT("ThirdParty"), TEXT("Nvigi"), TEXT("Binaries"), *Platform);
	AIMFeatures = MakeUnique<FAIMFeatureRegistry>(AIMCoreBinaryDirectory);

	AIMModelDirectory = FPaths::Combine(*PluginBaseDirAbsolute, TEXT("ThirdParty"), TEXT("Nvigi"), TEXT("Models"));
}

void FAIMModule::ShutdownModule()
{
	AIMFeatures.Reset();
}

DEFINE_LOG_CATEGORY_STATIC(LogACEAIMSDK, Log, All);
static void AIMLogCallback(nvaim::LogType Type, const char* InMessage)
{
	bool bShushed = false;
	if (GShushAIMLog.load() && (Type != nvaim::LogType::eInfo))
	{
		bShushed = true;
	}
	FString Message(reinterpret_cast<const UTF8CHAR*>(InMessage));
	// AIM log messages end with newlines, so fix that
	Message.TrimEndInline();
	// Check for nuisance messages
	if (Message.Contains(TEXT("Unable to find adapter supporting plugin")))
	{
		// downgrade nuisance AIM error, this is perfectly normal behavior on our build machines and won't cause any issues
		bShushed = true;
	}

	if (bShushed)
	{
		UE_LOG(LogACEAIMSDK, Log, TEXT("AIM (shushed): %s"), *Message);
	}
	else if (Type == nvaim::LogType::eInfo)
	{
		UE_LOG(LogACEAIMSDK, Log, TEXT("AIM: %s"), *Message);
	}
	else if (Type == nvaim::LogType::eWarn)
	{
		UE_LOG(LogACEAIMSDK, Warning, TEXT("AIM: %s"), *Message);
	}
	else if (Type == nvaim::LogType::eError)
	{
		UE_LOG(LogACEAIMSDK, Error, TEXT("AIM: %s"), *Message);
	}
	else
	{
		UE_LOG(LogACEAIMSDK, Error, TEXT("Received unknown AIM log type %d: %s"), Type, *Message);
	}
}

void FAIMModule::RegisterAIMFeature(nvaim::PluginID Feature, const TArray<FString>& AIMBinaryDirectories, const TArray<nvaim::PluginID>& IncompatibleFeatures)
{
	if (AIMFeatures.IsValid())
	{
		AIMFeatures->RegisterFeature(Feature, AIMBinaryDirectories, IncompatibleFeatures);
	}
}

bool FAIMModule::IsAIMFeatureAvailable(nvaim::PluginID Feature)
{
	if (AIMFeatures.IsValid())
	{
		return AIMFeatures->IsAIMFeatureAvailable(Feature);
	}
	return false;
}

nvaim::Result FAIMModule::LoadAIMFeature(nvaim::PluginID Feature, nvaim::InferenceInterface** Interface, bool bShushAIMLog)
{
	if (AIMFeatures.IsValid())
	{
		return AIMFeatures->LoadFeature(Feature, Interface, bShushAIMLog);
	}
	return nvaim::ResultInvalidState;
}

nvaim::Result FAIMModule::UnloadAIMFeature(nvaim::PluginID Feature, nvaim::InferenceInterface* Interface)
{
	if (AIMFeatures.IsValid())
	{
		return AIMFeatures->UnloadFeature(Feature, Interface);
	}
	return nvaim::ResultInvalidState;
}

nvaim::CudaParameters* FAIMModule::GetCIGCudaParameters()
{
	if (AIMFeatures.IsValid())
	{
		return AIMFeatures->GetCIGCudaParameters();
	}

	return nullptr;
}

FString GetAIMStatusString(nvaim::Result Result)
{
	switch (Result)
	{
	case nvaim::ResultOk:
		return FString(TEXT("Success"));
	case nvaim::ResultDriverOutOfDate:
		return FString(TEXT("Driver out of date"));
	case nvaim::ResultOSOutOfDate:
		return FString(TEXT("OS out of date"));
	case nvaim::ResultNoPluginsFound:
		return FString(TEXT("No plugins found"));
	case nvaim::ResultInvalidParameter:
		return FString(TEXT("Invalid parameter"));
	case nvaim::ResultNoSupportedHardwareFound:
		return FString(TEXT("No supported hardware found"));
	case nvaim::ResultMissingInterface:
		return FString(TEXT("Missing interface"));
	case nvaim::ResultMissingDynamicLibraryDependency:
		return FString(TEXT("Missing dynamic library dependency"));
	case nvaim::ResultInvalidState:
		return FString(TEXT("Invalid state"));
	case nvaim::ResultException:
		return FString(TEXT("Exception"));
	case nvaim::ResultJSONException:
		return FString(TEXT("JSON exception"));
	case nvaim::ResultRPCError:
		return FString(TEXT("RPC error"));
	case nvaim::ResultInsufficientResources:
		return FString(TEXT("Insufficient resources"));
	case nvaim::ResultNotReady:
		return FString(TEXT("Not ready"));
	case nvaim::ResultPluginOutOfDate:
		return FString(TEXT("Plugin out of date"));
	case nvaim::ResultDuplicatedPluginId:
		return FString(TEXT("Duplicate plugin ID"));
	default:
		return FString(TEXT("invalid AIM error code"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAIMModule, AIMWrapper)

