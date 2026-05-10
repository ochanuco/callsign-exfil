// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/CallsignTypes.h"
#include "CallsignLineOfSightService.generated.h"

/**
 *  Per-world subsystem that owns line-of-sight queries.
 *  Phase 1 returns binary CoverLevel (0 = full LoS, 2 = blocked); partial cover
 *  is reserved for Phase 2.
 */
UCLASS()
class UCallsignLineOfSightService : public UWorldSubsystem
{
        GENERATED_BODY()

public:

        /** Performs an ECC_Visibility line trace from From to To and returns the result. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|LineOfSight")
        FCallsignLineOfSightResult Query(const FVector& From, const FVector& To, const TArray<AActor*>& IgnoreActors) const;
};
