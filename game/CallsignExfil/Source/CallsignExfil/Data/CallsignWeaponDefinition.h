// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CallsignWeaponDefinition.generated.h"

/**
 *  Phase 1 weapon definition.
 *  Intentionally minimal: damage, range and LoS gating only.
 *  Durability/ammo/magazine fields are reserved for Phase 2.
 */
UCLASS(BlueprintType)
class UCallsignWeaponDefinition : public UPrimaryDataAsset
{
        GENERATED_BODY()

public:

        /** Per-shot damage applied on a successful resolution. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon")
        float Damage = 10.f;

        /** Maximum effective range. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon")
        float Range = 1500.f;

        /** When true, the combat resolver requires a clear LoS to apply damage. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Weapon")
        bool bRequiresLineOfSight = true;
};
