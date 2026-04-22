# NPCConversation Plugin Tests

This directory contains automated tests for the NPCConversation plugin.

## Test Structure

The tests are implemented using Unreal Engine's Automation Testing framework.

### Test Categories

1. **Creation Tests** - Verify that async nodes can be instantiated
2. **Static Factory Tests** - Test the Blueprint-callable factory methods
3. **Settings Tests** - Validate plugin settings and defaults

## Running Tests

### In the Unreal Editor

1. Open the Unreal Editor
2. Go to **Window → Developer Tools → Session Frontend**
3. Select the **Automation** tab
4. In the test tree, expand **NPCConversation**
5. Select the tests you want to run
6. Click **Start Tests**

Alternatively, use the command line:

```bash
# From the Unreal Editor installation directory
UnrealEditor.exe "C:\Path\To\KairosSample.uproject" -ExecCmds="Automation RunTests NPCConversation" -unattended -nopause -nullrhi
```

### From Command Line (Headless)

On Windows:
```cmd
cd "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64"
UnrealEditor-Cmd.exe "C:\Path\To\KairosSample.uproject" -ExecCmds="Automation RunTests NPCConversation" -unattended -nopause -nullrhi -log
```

On Linux:
```bash
cd /path/to/UE_5.6/Engine/Binaries/Linux
./UnrealEditor-Cmd "~/KairosSample/KairosSample.uproject" -ExecCmds="Automation RunTests NPCConversation" -unattended -nopause -nullrhi -log
```

## Test Coverage

Current tests cover:

- ✅ Node instantiation (STT, LLM, TTS)
- ✅ Static factory methods
- ✅ Parameter passing
- ✅ Settings access and defaults

### Future Test Additions

The following test scenarios are planned for future implementation:

- ⏳ Mock HTTP responses for API testing
- ⏳ Audio capture simulation
- ⏳ WAV file generation validation
- ⏳ Error handling and fallback paths
- ⏳ Multi-threaded async operation tests
- ⏳ Integration tests with ACE plugin

## Adding New Tests

To add new tests:

1. Open `Private/NPCConversationTests.cpp`
2. Add a new test using the `IMPLEMENT_SIMPLE_AUTOMATION_TEST` macro:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYourTestName,
    "NPCConversation.Category.TestName",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FYourTestName::RunTest(const FString& Parameters)
{
    // Your test code here
    TestTrue(TEXT("Condition description"), SomeCondition);

    return true;
}
```

3. Rebuild the project
4. Run the tests as described above

## Test Requirements

- Unreal Engine 5.6 or later
- NPCConversation plugin enabled
- Editor or standalone build with automation framework

## CI/CD

Tests are automatically validated by GitHub Actions on each commit. See `.github/workflows/npc-conversation-ci.yml` for the CI configuration.

Note: The GitHub Actions workflow currently validates plugin structure and code quality. Full build and test execution requires a self-hosted runner with Unreal Engine installed.
