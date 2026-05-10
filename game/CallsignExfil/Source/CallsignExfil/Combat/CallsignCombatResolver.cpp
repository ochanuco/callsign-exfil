// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignCombatResolver.h"
#include "LineOfSight/CallsignLineOfSightService.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
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

        // Phase 2 path (ADR-003 §4.3): when the request carries an Inventory ref, route the
        // ammo/magazine/durability/broken checks through it BEFORE doing LoS+damage.
        // If consumption fails, short-circuit (bHit=false, no damage). Otherwise fall through
        // to the Phase 1 LoS+damage logic below, which uses Request.Weapon (still required).
        // Phase 1 callers leave Request.Inventory null and skip this entire branch.
        const UCallsignWeaponDefinition* Phase2WeaponDef = nullptr;
        if (UCallsignInventoryComponent* Inv = Request.Inventory.Get())
        {
                if (!Inv->ConsumeShotForCurrentWeapon())
                {
                        UE_LOG(LogTemp, Display, TEXT("[Combat] ResolveShot rejected by inventory (ammo/mag/broken)"));
                        return Result;
                }
                if (UCallsignWeaponInstanceObject* CurWep = Inv->GetCurrentWeapon())
                {
                        Phase2WeaponDef = CurWep->GetWeaponDefinition();
                }
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

        // Phase 2: prefer the inventory's current weapon definition (carries Phase 2 fields).
        // Phase 1: fall back to Request.Weapon as before.
        const UCallsignWeaponDefinition* Weapon = Phase2WeaponDef ? Phase2WeaponDef : Request.Weapon;
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
