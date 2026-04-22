// UE stub: Modules/ModuleManager.h
#pragma once
#include "Modules/ModuleInterface.h"

#define IMPLEMENT_MODULE(cls, mod) static cls s_module_##mod;
