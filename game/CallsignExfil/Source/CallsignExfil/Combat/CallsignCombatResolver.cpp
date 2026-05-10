// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignCombatResolver.h"
#include "LineOfSight/CallsignLineOfSightService.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

FCallsignShotResult UCallsignCombatResolver::ResolveShot(const FCallsignShotRequest& Request)
{
        FCallsignShotResult Result;

        UWorld* World = GetWorld();
        if (!World)
        {
                return Result;
        }

        UCallsignLineOfSightService* LosService = World->GetSubsystem<UCallsignLineOfSightService>();
        if (LosService)
        {
                TArray<AActor*> Ignore;
                if (AActor* Inst = Request.Instigator.Get())
                {
                        Ignore.Add(Inst);
                }
                Result.LosResult = LosService->Query(Request.From, Request.To, Ignore);
        }

        const UCallsignWeaponDefinition* Weapon = Request.Weapon;
        const bool bRequiresLos = Weapon ? Weapon->bRequiresLineOfSight : true;
        const float Damage = Weapon ? Weapon->Damage : 0.f;

        if (bRequiresLos && !Result.LosResult.bHasLineOfSight)
        {
                Result.bHit = false;
                Result.DamageApplied = 0.f;
                return Result;
        }

        // TODO Phase 1 impl: pick the actual victim (intended target vs. blocker)
        // once a richer FCallsignShotRequest carries the target reference. For now
        // we fall back to the LoS-reported blocker if any.
        AActor* Victim = Result.LosResult.BlockingActor.Get();
        AActor* Instigator = Request.Instigator.Get();

        if (Victim && Damage > 0.f)
        {
                const FVector HitDir = (Request.To - Request.From).GetSafeNormal();
                AController* InstigatorController = nullptr;
                if (APawn* InstigatorPawn = Cast<APawn>(Instigator))
                {
                        InstigatorController = InstigatorPawn->GetController();
                }

                UGameplayStatics::ApplyPointDamage(
                        Victim,
                        Damage,
                        HitDir,
                        FHitResult(),
                        InstigatorController,
                        Instigator,
                        nullptr);

                Result.bHit = true;
                Result.DamageApplied = Damage;
        }
        else
        {
                Result.bHit = false;
                Result.DamageApplied = 0.f;
        }

        return Result;
}
