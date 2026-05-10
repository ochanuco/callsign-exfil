// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignTargeting.h"

#include "CallsignRifleEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace CallsignTargeting
{
	AActor* FindNearestRifleEnemy(UWorld* World, const FVector& From)
	{
		if (!World)
		{
			return nullptr;
		}

		// HUD draws targeting preview every frame, so prefer TActorIterator
		// over GetAllActorsOfClass to avoid the per-frame TArray allocation.
		AActor* Nearest = nullptr;
		float NearestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<ACallsignRifleEnemy> It(World); It; ++It)
		{
			AActor* E = *It;
			if (!E)
			{
				continue;
			}
			const float DSq = FVector::DistSquared(From, E->GetActorLocation());
			if (DSq < NearestDistSq)
			{
				NearestDistSq = DSq;
				Nearest = E;
			}
		}
		return Nearest;
	}
}
