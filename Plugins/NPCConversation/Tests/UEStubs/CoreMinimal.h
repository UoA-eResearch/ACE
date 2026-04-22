// Minimal Unreal Engine stub for syntax-checking plugin headers without a
// full UE installation.  This file is used by CI only and is NOT part of
// the shipping plugin.
#pragma once

#include <cstdint>

// ── Integer type aliases ─────────────────────────────────────────────────────
using int8   = int8_t;   using int16  = int16_t;
using int32  = int32_t;  using int64  = int64_t;
using uint8  = uint8_t;  using uint16 = uint16_t;
using uint32 = uint32_t; using uint64 = uint64_t;

// ── Common value types ───────────────────────────────────────────────────────
struct FString
{
    FString() = default;
    FString(const char*) {}
    bool IsEmpty() const { return false; }
};
struct FText  {};
struct FName  {};

template<typename T> struct TSharedPtr
{
    T* operator->() const { return nullptr; }
    bool IsValid()  const { return false;   }
};

template<typename T> struct TArray
{
    const T* GetData()   const { return nullptr; }
    int32    Num()       const { return 0;        }
    bool     IsEmpty()   const { return true;     }
    void     Append(const T*, int32) {}
};

template<typename T, typename U = T> struct TSubclassOf {};

// ── UObject base ─────────────────────────────────────────────────────────────
class UObject { public: virtual ~UObject() = default; };

template<typename T> T*       NewObject(UObject* = nullptr) { return new T(); }
template<typename T> const T* GetDefault() { static T inst; return &inst; }

// ── Utility macros ───────────────────────────────────────────────────────────
#define TEXT(x)      x
#define FORCEINLINE  inline
#define check(...)
#define ensure(x)    (x)

// ── UE reflection macros (no-ops so class bodies parse) ─────────────────────
#define UCLASS(...)
#define GENERATED_BODY()  public:
#define UFUNCTION(...)
#define UPROPERTY(...)
#define UENUM(...)
#define UMETA(...)
#define NPCCONVERSATION_API
#define DECLARE_LOG_CATEGORY_EXTERN(name, def, all)
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(name, t1, n1, t2, n2) \
    struct name { void Broadcast(t1, t2) {} };
