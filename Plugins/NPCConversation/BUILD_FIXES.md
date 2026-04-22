# NPCConversation Plugin - Build Fixes and Testing

## Summary

Fixed build errors in the NPCConversation plugin and added comprehensive testing infrastructure with CI/CD support.

## Build Errors Fixed

### 1. HTTP Callback Parameter Type Declarations

**Problem:**
The header files incorrectly declared HTTP callback parameters using the `class` keyword:
```cpp
void OnWhisperResponse(class FHttpRequestPtr Request, class FHttpResponsePtr Response, bool bWasSuccessful);
```

**Issue:**
`FHttpRequestPtr` and `FHttpResponsePtr` are typedefs (type aliases) for `TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>` and `TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>` respectively. They cannot be forward-declared using the `class` keyword because they are not classes themselves.

**Fix:**
- Removed the `class` keyword from all HTTP callback parameter declarations
- Added `#include "Interfaces/IHttpRequest.h"` to all async node headers to properly include the HTTP type definitions

**Files Modified:**
- `Plugins/NPCConversation/Source/NPCConversation/Public/NPCSTTAsync.h`
- `Plugins/NPCConversation/Source/NPCConversation/Public/NPCLLMAsync.h`
- `Plugins/NPCConversation/Source/NPCConversation/Public/NPCTTSAsync.h`

## Testing Infrastructure Added

### 1. Automation Test Module

Created a new test module `NPCConversationTests` that includes:

**Test Files:**
- `Plugins/NPCConversation/Source/NPCConversationTests/NPCConversationTests.Build.cs` - Build configuration
- `Plugins/NPCConversation/Source/NPCConversationTests/Private/NPCConversationTests.cpp` - Test implementation
- `Plugins/NPCConversation/Source/NPCConversationTests/README.md` - Documentation

**Test Coverage:**
- ✅ STT Node Creation and Factory Methods
- ✅ LLM Node Creation and Factory Methods
- ✅ TTS Node Creation and Factory Methods
- ✅ Plugin Settings Validation
- ✅ Default Values Verification

**Tests Implemented:**
1. `NPCConversation.STT.Creation` - Verify STT node can be instantiated
2. `NPCConversation.STT.StaticFactory` - Test STT static factory method
3. `NPCConversation.LLM.Creation` - Verify LLM node can be instantiated
4. `NPCConversation.LLM.StaticFactory` - Test LLM static factory method
5. `NPCConversation.TTS.Creation` - Verify TTS node can be instantiated
6. `NPCConversation.TTS.StaticFactory` - Test TTS static factory method
7. `NPCConversation.Settings.Defaults` - Validate settings object and defaults

### 2. GitHub Actions CI/CD Workflow

Created `.github/workflows/npc-conversation-ci.yml` with two jobs:

**Job 1: Validate Plugin Structure**
- Validates plugin descriptor JSON syntax
- Checks for required Build.cs files
- Verifies all required source files are present
- Checks header files for proper `#pragma once`
- Validates HTTP type usage (no incorrect forward declarations)

**Job 2: Code Quality Checks**
- Scans for TODO/FIXME comments
- Checks for UTF-8 BOM issues
- Validates line endings consistency

**Triggers:**
- Push to `main` branch or any `claude/**` branch
- Pull requests affecting the plugin
- Manual workflow dispatch

## Running Tests

### In Unreal Editor
1. Window → Developer Tools → Session Frontend
2. Automation tab → NPCConversation
3. Select tests and click "Start Tests"

### Command Line
```bash
UnrealEditor-Cmd.exe "C:\Path\To\KairosSample.uproject" \
    -ExecCmds="Automation RunTests NPCConversation" \
    -unattended -nopause -nullrhi -log
```

## Future Enhancements

To enable full build and test execution in CI:

1. **Set up Self-Hosted Runner** with UE5.6 installed
2. **Add Build Commands** using UnrealBuildTool
3. **Run Full Test Suite** with Unreal's automation framework
4. **Add Integration Tests** with mock HTTP responses
5. **Test Audio Pipeline** with simulated audio capture

## Verification

All changes have been validated:
- ✅ Plugin descriptor JSON is valid
- ✅ No incorrect HTTP type forward declarations
- ✅ All required source files present
- ✅ GitHub Actions workflow configured
- ✅ Tests structured according to UE5 automation framework

## Commit History

1. `a11754c` - Fix HTTP callback parameter types in NPCConversation plugin
2. `660294d` - Add comprehensive tests and CI workflow for NPCConversation plugin
