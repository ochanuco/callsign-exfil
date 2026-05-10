// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignLineOfSightService.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "Node/CallsignNode.h"

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

        // Node Visuals block ECC_Visibility so the click-to-move cursor pick
        // can hit them. LoS traces use the same channel — auto-ignore all
        // node actors here so the shooter's own tile (and any tile along the
        // line) doesn't read as a blocker. Phase 1 only spawns 9 nodes so
        // the per-trace cost is negligible.
        for (TActorIterator<ACallsignNode> It(World); It; ++It)
        {
                Params.AddIgnoredActor(*It);
        }

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
