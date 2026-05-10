// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignSupportResolver.h"
#include "Data/CallsignSupportTypes.h"
#include "Node/CallsignNode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "CollisionShape.h"

FCallsignSupportResolution UCallsignSupportResolver::Resolve(const FCallsignSupportRequest& Request)
{
	FCallsignSupportResolution Out;
	Out.RequestId = Request.RequestId;

	UWorld* World = GetWorld();
	UCallsignSupportDefinition* Def = Request.Definition;
	if (!World || !Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SupportResolver] missing world / definition; resolving as no-op"));
		return Out;
	}

	const FVector Center = Request.TargetLocation;
	AActor* Instigator = Request.RequestedBy.Get();
	AController* InstigatorController = nullptr;
	if (APawn* InstigatorPawn = Cast<APawn>(Instigator))
	{
		InstigatorController = InstigatorPawn->GetController();
	}

	// 1. Damage pass — Pawn overlap inside RadiusCm.
	// ADR-004 §9.1: SupplyPod-style supports skip the damage pass via
	// bDealsDamage=false. Phase 3 ignores bAllowsFriendlyFire (always
	// applies, no IFF filter; reserved for Phase 4).
	if (Def->bDealsDamage && Def->Damage > 0 && Def->RadiusCm > 0.f)
	{
		TArray<FOverlapResult> Overlaps;
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(Def->RadiusCm);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CallsignSupportResolveDamage), false);
		FCollisionObjectQueryParams ObjParams(ECC_Pawn);

		if (World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjParams, Sphere, Params))
		{
			TSet<AActor*> Damaged;
			for (const FOverlapResult& O : Overlaps)
			{
				AActor* Hit = O.GetActor();
				if (!Hit || Damaged.Contains(Hit))
				{
					continue;
				}
				Damaged.Add(Hit);

				const FVector HitDir = (Hit->GetActorLocation() - Center).GetSafeNormal();
				UGameplayStatics::ApplyPointDamage(
					Hit,
					static_cast<float>(Def->Damage),
					HitDir,
					FHitResult(),
					InstigatorController,
					Instigator,
					nullptr);
				++Out.DamageEventsEmitted;

				if (Hit == Instigator)
				{
					Out.bFriendlyFireApplied = true;
				}
			}
		}
	}

	// 2. Terrain destruction pass — flip bIsDestroyed on Destroyable Nodes
	// inside TerrainDestructionRadiusCm and remove them from peers'
	// Adjacent arrays (one-way; ADR-004 §13 OQ-1: full bidirectional GC
	// is Phase 4).
	if (Def->TerrainDestructionRadiusCm > 0.f)
	{
		const float DestroyR2 = Def->TerrainDestructionRadiusCm * Def->TerrainDestructionRadiusCm;
		TArray<ACallsignNode*> ToDestroy;
		for (TActorIterator<ACallsignNode> It(World); It; ++It)
		{
			ACallsignNode* N = *It;
			if (!N || N->bIsDestroyed || !N->bDestroyable)
			{
				continue;
			}
			if (FVector::DistSquared(N->GetActorLocation(), Center) <= DestroyR2)
			{
				ToDestroy.Add(N);
			}
		}

		for (ACallsignNode* N : ToDestroy)
		{
			N->MarkDestroyed();
			Out.DestroyedNodes.Add(N);
		}

		// One-way Adjacent removal: for every node in the world, drop any
		// reference to a destroyed node from its Adjacent list. Cheap on
		// the Phase 1 demo's small grid; revisit if grid count balloons.
		if (ToDestroy.Num() > 0)
		{
			TSet<ACallsignNode*> DestroyedSet(ToDestroy);
			for (TActorIterator<ACallsignNode> It(World); It; ++It)
			{
				ACallsignNode* N = *It;
				if (!N)
				{
					continue;
				}
				N->Adjacent.RemoveAll([&DestroyedSet](const TObjectPtr<ACallsignNode>& Adj)
				{
					return Adj.Get() && DestroyedSet.Contains(Adj.Get());
				});
			}
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("[SupportResolver] resolved id=%s pawnsHit=%d nodesDestroyed=%d friendlyFire=%d"),
		*Request.RequestId.ToString(), Out.DamageEventsEmitted, Out.DestroyedNodes.Num(),
		Out.bFriendlyFireApplied ? 1 : 0);

	return Out;
}
