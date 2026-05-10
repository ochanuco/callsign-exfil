// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignTargeting.h"

#include "CallsignRifleEnemy.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace CallsignTargeting
{
	AActor* FindNearestRifleEnemy(UWorld* World, const FVector& From)
	{
		if (!World)
		{
			return nullptr;
		}

		TArray<AActor*> Enemies;
		UGameplayStatics::GetAllActorsOfClass(World, ACallsignRifleEnemy::StaticClass(), Enemies);

		AActor* Nearest = nullptr;
		float NearestDistSq = TNumericLimits<float>::Max();
		for (AActor* E : Enemies)
		{
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
