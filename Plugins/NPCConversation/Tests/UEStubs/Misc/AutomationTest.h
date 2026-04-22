// UE stub: Misc/AutomationTest.h
#pragma once
#include "CoreMinimal.h"

#ifndef WITH_DEV_AUTOMATION_TESTS
#  define WITH_DEV_AUTOMATION_TESTS 1
#endif

namespace EAutomationTestFlags {
    enum Type { ApplicationContextMask = 0, ProductFilter = 0 };
}

struct FAutomationTestBase
{
    virtual ~FAutomationTestBase() = default;
    virtual bool RunTest(const FString& Params) = 0;
    void TestNotNull(const char*, const void*) {}
    template<typename T> void TestEqual(const char*, T, T) {}
    void TestFalse(const char*, bool) {}
    void TestTrue(const char*, bool)  {}
};

// The macro declares the test class; the user then provides the RunTest body
// as a regular out-of-class function definition (not inside this macro).
#define IMPLEMENT_SIMPLE_AUTOMATION_TEST(cls, name, flags) \
    struct cls : public FAutomationTestBase {               \
        virtual bool RunTest(const FString& Params) override; \
    };
