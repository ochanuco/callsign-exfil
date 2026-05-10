// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/ObjectPtr.h"
#include "Data/CallsignWeaponTypes.h"
#include "CallsignInventoryComponent.generated.h"

class UCallsignWeaponInstanceObject;

/** Broadcast on a successful (non-no-op) reload. ADR-003 §7. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignReloadedDelegate, FCallsignReloadResult, Result);

/**
 *  Per-Pawn inventory component.
 *  Owns weapon slots (Main / Secondary / Power / Rescue) and a TMap-backed ammo pool.
 *  Acts as the single entry point for shoot-resource consumption and reload routing.
 *
 *  ADR-003 §4.1. Pawn constructors create one of these via CreateDefaultSubobject.
 */
UCLASS(ClassGroup = ("Callsign"), meta = (BlueprintSpawnableComponent))
class CALLSIGNEXFIL_API UCallsignInventoryComponent : public UActorComponent
{
        GENERATED_BODY()

public:

        UCallsignInventoryComponent();

        /** Equips a weapon instance into a slot. Replaces any existing weapon in that slot. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Inventory")
        void EquipWeapon(ECallsignWeaponSlot Slot, UCallsignWeaponInstanceObject* Weapon);

        /** Sets which slot is currently active (the weapon used by ConsumeShotForCurrentWeapon / Reload). */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Inventory")
        void SetActiveSlot(ECallsignWeaponSlot Slot);

        /** Currently active slot. */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Inventory")
        ECallsignWeaponSlot GetActiveSlot() const { return ActiveSlot; }

        /** Returns the weapon currently in the active slot, or nullptr if none. */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Inventory")
        UCallsignWeaponInstanceObject* GetCurrentWeapon() const;

        /** Pool count for the given ammo type (0 if not present). */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Inventory")
        int32 GetAmmoCount(ECallsignAmmoType AmmoType) const;

        /** Adds Count rounds of AmmoType to the pool. Negative Count is clamped to 0. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Inventory")
        void AddAmmo(ECallsignAmmoType AmmoType, int32 Count);

        /**
         *  ADR-003 §4.3 step 1-7. Called by UCallsignCombatResolver.
         *  Returns true when the shot should proceed (resource was consumed).
         *  Returns false when:
         *   - no current weapon
         *   - bUsesAmmoPool && pool == 0
         *   - weapon broken or empty magazine (delegates to weapon's ConsumeShot)
         */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Inventory")
        bool ConsumeShotForCurrentWeapon();

        /**
         *  ADR-003 §7: routes the reload through the active weapon.
         *  Handles rescue no-op, empty-pool no-op, and full-magazine no-op.
         *  Broadcasts OnReloaded only when Result.bReloaded is true.
         */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Inventory")
        FCallsignReloadResult Reload();

        /** True when the active weapon can usefully reload right now (pool>0, magazine not full). */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Inventory")
        bool IsReloadAvailable() const;

        /** Fired when Reload() actually loaded rounds. */
        UPROPERTY(BlueprintAssignable, Category = "Callsign|Inventory")
        FCallsignReloadedDelegate OnReloaded;

private:

        /** Slot whose weapon is treated as "current" by Combat / PC. */
        UPROPERTY(VisibleAnywhere, Category = "Callsign|Inventory")
        ECallsignWeaponSlot ActiveSlot = ECallsignWeaponSlot::Main;

        /** Weapon instance per slot. */
        UPROPERTY(VisibleAnywhere, Category = "Callsign|Inventory")
        TMap<ECallsignWeaponSlot, TObjectPtr<UCallsignWeaponInstanceObject>> Weapons;

        /** Ammo pool count per ammo family. */
        UPROPERTY(VisibleAnywhere, Category = "Callsign|Inventory")
        TMap<ECallsignAmmoType, int32> AmmoPool;
};
