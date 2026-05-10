// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/CallsignWeaponTypes.h"
#include "CallsignWeaponInstanceObject.generated.h"

class UCallsignWeaponDefinition;
class UCallsignWeaponInstanceObject;

/** Broadcast when a weapon transitions to broken state (Durability <= 0). ADR-003 §8. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignWeaponBrokenDelegate, UCallsignWeaponInstanceObject*, BrokenWeapon);

/**
 *  Runtime weapon instance.
 *  Wraps FCallsignWeaponInstance state with behaviour:
 *  - magazine/durability consumption per shoot action
 *  - tactical/normal reload from a pool
 *  - broken state with delegate broadcast
 *
 *  ADR-003 §4.2. UObject base chosen for BlueprintCallable accessors and
 *  future state-machine extension when Phase 3 introduces combine/disassemble.
 */
UCLASS(BlueprintType)
class CALLSIGNEXFIL_API UCallsignWeaponInstanceObject : public UObject
{
        GENERATED_BODY()

public:

        /** Sets WeaponDef and resets MagazineCurrent/DurabilityCurrent to the definition maxima. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Weapon")
        void InitializeFromDefinition(UCallsignWeaponDefinition* Def);

        /** Static definition this instance was built from (read-only accessor). */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Weapon")
        UCallsignWeaponDefinition* GetWeaponDefinition() const { return State.WeaponDef; }

        /** Rounds currently loaded in the magazine. */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Weapon")
        int32 GetMagazineCurrent() const { return State.MagazineCurrent; }

        /** Remaining durability points. */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Weapon")
        int32 GetDurabilityCurrent() const { return State.DurabilityCurrent; }

        /**
         *  ADR-003 §9: returns true only when DurabilityCurrent <= 0 AND
         *  the definition does not flag infinite durability.
         */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Weapon")
        bool IsBroken() const;

        /**
         *  ADR-003 §4.3 step 5+6: consume one shoot action's worth of magazine and durability.
         *  Returns false (no-op) if broken or out of magazine. Branches on bHasInfinite* flags
         *  before any arithmetic on MagazineCurrent / DurabilityCurrent.
         */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Weapon")
        bool ConsumeShot();

        /**
         *  ADR-003 §7: reload from the given pool count.
         *  Returns FCallsignReloadResult and sets OutPoolCountAfter to the residual pool count.
         *  - bHasInfiniteMagazine -> no-op, OutPoolCountAfter == Available
         *  - Available <= 0 -> no-op, OutPoolCountAfter == 0
         *  - magazine already full -> no-op, OutPoolCountAfter == Available
         *  - magazine partially loaded -> tactical reload (discard remaining, refill from pool)
         *  - magazine empty -> normal reload (refill from pool)
         */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Weapon")
        FCallsignReloadResult ReloadFromPool(int32 Available, int32& OutPoolCountAfter);

        /** Fired exactly once when this weapon transitions to broken state. */
        UPROPERTY(BlueprintAssignable, Category = "Callsign|Weapon")
        FCallsignWeaponBrokenDelegate OnWeaponBroken;

private:

        /** Internal POD-style state holder. */
        UPROPERTY(VisibleAnywhere, Category = "Callsign|Weapon")
        FCallsignWeaponInstance State;
};
