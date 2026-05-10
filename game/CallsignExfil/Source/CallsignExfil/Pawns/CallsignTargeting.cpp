// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignTargeting.h"

#include "CallsignHealthComponent.h"
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
			ACallsignRifleEnemy* E = *It;
			if (!E)
			{
				continue;
			}
			// Skip dead enemies: targeting preview should jump to the next
			// viable enemy after one goes down rather than stick to the
			// corpse, and CsxShoot should never lock on to a dead actor.
			if (E->HealthComp && E->HealthComp->bIsDead)
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
