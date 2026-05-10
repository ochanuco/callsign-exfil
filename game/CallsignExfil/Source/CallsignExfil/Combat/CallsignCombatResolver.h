// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/CallsignTypes.h"
#include "CallsignCombatResolver.generated.h"

/**
 *  Per-world subsystem that resolves shot requests by querying the LoS service
 *  and applying point damage. Phase 1 uses a flat per-weapon damage value.
 */
UCLASS()
class UCallsignCombatResolver : public UWorldSubsystem
{
        GENERATED_BODY()

public:

        /** Resolves a single shot request and returns the outcome. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Combat")
        FCallsignShotResult ResolveShot(const FCallsignShotRequest& Request);
};
