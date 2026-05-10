// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignLineOfSightService.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

FCallsignLineOfSightResult UCallsignLineOfSightService::Query(const FVector& From, const FVector& To, const TArray<AActor*>& IgnoreActors) const
{
        FCallsignLineOfSightResult Result;

        const UWorld* World = GetWorld();
        if (!World)
        {
                Result.bHasLineOfSight = false;
                Result.CoverLevel = 2;
                return Result;
        }

        FCollisionQueryParams Params(SCENE_QUERY_STAT(CallsignLineOfSight), false);
        Params.AddIgnoredActors(IgnoreActors);

        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);

        if (!bHit)
        {
                Result.bHasLineOfSight = true;
                Result.CoverLevel = 0;
        }
        else
        {
                // TODO Phase 1 impl: cover degrees (partial cover = 1) based on
                // hit normal, node-kind tagging, and trace height sampling.
                Result.bHasLineOfSight = false;
                Result.CoverLevel = 2;
                Result.BlockingActor = Hit.GetActor();
        }

        return Result;
}
