// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CallsignWeaponTypes.h"
#include "CallsignWeaponDefinition.generated.h"

/**
 *  Weapon definition asset.
 *  Phase 1 fields (Damage / Range / bRequiresLineOfSight) remain unchanged.
 *  Phase 2 (ADR-003 §3.1) adds ammo / magazine / durability / slot metadata
 *  alongside infinite-flag escape hatches used by the rescue handgun (§9).
 */
UCLASS(BlueprintType)
class UCallsignWeaponDefinition : public UPrimaryDataAsset
{
        GENERATED_BODY()

public:

        // --- Phase 1 fields (unchanged) ---

        /** Per-shot damage applied on a successful resolution. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon")
        float Damage = 10.f;

        /** Maximum effective range. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon")
        float Range = 1500.f;

        /** When true, the combat resolver requires a clear LoS to apply damage. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon")
        bool bRequiresLineOfSight = true;

        // --- Phase 2 fields (ADR-003 §3.1) ---

        /** Ammo family this weapon consumes from the inventory pool. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Ammo")
        ECallsignAmmoType AmmoType = ECallsignAmmoType::Light;

        /** When true, reloads draw from the inventory ammo pool. Rescue handgun keeps this false. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Ammo")
        bool bUsesAmmoPool = true;

        /** Magazine capacity. Generic safe default; per-weapon Data Assets override. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Ammo")
        int32 MagazineSize = 10;

        /** Rounds consumed from the magazine per shoot action (e.g. 3 for a 3-round burst AR). */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Ammo")
        int32 ShotsPerAction = 1;

        /** Maximum durability. ADR-003 §8 / PLAN.md durability MVP. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Durability")
        int32 DurabilityMax = 20;

        /** Durability consumed per shoot action (per-action, not per-bullet — ADR-003 §8). */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Durability")
        int32 DurabilityCostPerAction = 1;

        /** When true, this weapon is the rescue handgun (no durability cost, no pool draw). */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Rescue")
        bool bIsRescue = false;

        /**
         *  When true, ConsumeShot/IsBroken short-circuit before any DurabilityCurrent arithmetic
         *  (ADR-003 §9 — flag-based infinity to avoid integer overflow).
         */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Rescue")
        bool bHasInfiniteDurability = false;

        /**
         *  When true, Reload is a no-op and ConsumeShot skips magazine arithmetic
         *  (ADR-003 §9 — rescue handgun infinite magazine).
         */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon|Rescue")
        bool bHasInfiniteMagazine = false;

        /** Slot this weapon occupies in the inventory. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon")
        ECallsignWeaponSlot WeaponSlot = ECallsignWeaponSlot::Main;
};
