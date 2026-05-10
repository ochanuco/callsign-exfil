// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "CallsignWeaponTypes.generated.h"

class UCallsignWeaponDefinition;

/**
 *  Phase 2 ammunition family used by UCallsignWeaponDefinition::AmmoType
 *  and UCallsignInventoryComponent ammo pool.
 *  ADR-003 §3.2 / §11.
 */
UENUM(BlueprintType)
enum class ECallsignAmmoType : uint8
{
        Light           UMETA(DisplayName = "Light"),
        Shell           UMETA(DisplayName = "Shell"),
        Heavy           UMETA(DisplayName = "Heavy")
};

/**
 *  Phase 2 weapon slot. Inventory keeps one weapon per slot.
 *  ADR-003 §3.2 / §4.1.
 */
UENUM(BlueprintType)
enum class ECallsignWeaponSlot : uint8
{
        Main            UMETA(DisplayName = "Main"),
        Secondary       UMETA(DisplayName = "Secondary"),
        Power           UMETA(DisplayName = "Power"),
        Rescue          UMETA(DisplayName = "Rescue")
};

/**
 *  Runtime per-instance weapon state (magazine, durability, wear).
 *  Lives inside UCallsignWeaponInstanceObject. ADR-003 §3.2 / §4.2.
 */
USTRUCT(BlueprintType)
struct FCallsignWeaponInstance
{
        GENERATED_BODY()

        /** Static definition this instance was created from. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Weapon")
        TObjectPtr<UCallsignWeaponDefinition> WeaponDef = nullptr;

        /** Rounds currently loaded in the magazine. Ignored when bHasInfiniteMagazine. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Weapon")
        int32 MagazineCurrent = 0;

        /** Remaining durability points. Ignored when bHasInfiniteDurability. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Weapon")
        int32 DurabilityCurrent = 0;

        /** Phase 3 reserved field (combine/disassemble). Phase 2 keeps it at 0. ADR-003 §12. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Weapon")
        int32 WearLevel = 0;
};

/**
 *  Per-ammo-type pool entry. Inventory holds these as a TMap<ECallsignAmmoType, int32>;
 *  this struct exists to allow Blueprint authoring of starting pools when needed.
 *  ADR-003 §3.2 / §11.
 */
USTRUCT(BlueprintType)
struct FCallsignAmmoPool
{
        GENERATED_BODY()

        /** Which ammo family this pool entry tracks. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Inventory")
        ECallsignAmmoType AmmoType = ECallsignAmmoType::Light;

        /** Reserve count outside of any magazine. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Inventory")
        int32 Count = 0;
};

/**
 *  Outcome of UCallsignInventoryComponent::Reload() / UCallsignWeaponInstanceObject::ReloadFromPool().
 *  bWasTactical is true when MagazineCurrent had remaining rounds that were discarded.
 *  ADR-003 §3.2 / §7.
 */
USTRUCT(BlueprintType)
struct FCallsignReloadResult
{
        GENERATED_BODY()

        /** True when at least one round was loaded into the magazine. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Reload")
        bool bReloaded = false;

        /** True when the reload discarded remaining rounds (tactical reload). */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Reload")
        bool bWasTactical = false;

        /** Rounds discarded from the previously loaded magazine. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Reload")
        int32 DiscardedRounds = 0;

        /** Rounds drawn from the pool and loaded into the magazine. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Reload")
        int32 LoadedRounds = 0;
};
