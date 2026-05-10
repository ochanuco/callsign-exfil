// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignCombatResolver.h"
#include "LineOfSight/CallsignLineOfSightService.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
                // Phase 2 (issue #22): also ignore the intended target so the trace
                // doesn't get blocked by the target's own capsule. Gate on
                // Request.TargetActor presence (issue #24) so future callers that
                // set TargetActor without Inventory still get target-aware LoS.
                // Phase 1 callers leave TargetActor null and the original
                // Instigator-only ignore set is preserved.
                if (AActor* T = Request.TargetActor.Get())
                {
                        Ignore.Add(T);
                        UE_LOG(LogTemp, Verbose, TEXT("[Combat] LoS ignore: Instigator=%s Target=%s"),
                                *GetNameSafe(Request.Instigator.Get()), *GetNameSafe(T));
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

        // Phase 2 (issue #22): when the caller carries an explicit target, that's
        // the victim - the LoS pass already ignored both source and target so the
        // trace's BlockingActor is null on a clear path. Fall back to the
        // LoS-reported blocker when no explicit target is provided (Phase 1).
        AActor* Victim = Request.TargetActor.Get();
        if (!Victim)
        {
                Victim = Result.LosResult.BlockingActor.Get();
        }
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

        // Phase 2 demo: lightweight hit/damage trace so the auto-demo can be observed in
        // the Output Log when the request was routed via Inventory (ADR-003 §4.3 Phase 2 path).
        if (Phase2WeaponDef)
        {
                UE_LOG(LogTemp, Display, TEXT("[Combat] ResolveShot Phase2 hit=%d damage=%.2f"),
                        Result.bHit ? 1 : 0, Result.DamageApplied);
        }

        // Visual feedback: draw a tracer line in the world for ~0.6s so the
        // shot is observable beyond the message log. Player shots = cyan,
        // enemy shots = red. Misses use a duller tint.
        //
        // SDPG_Foreground bypasses depth testing so the tracer is not occluded
        // by the player's own capsule (player shots originate inside the player
        // pawn so a depth-tested line is invisible from the third-person camera).
        if (World)
        {
                const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
                const bool bIsPlayerShot = (Request.Instigator.Get() == PlayerPawn);
                FColor TracerColor;
                if (bIsPlayerShot)
                {
                        TracerColor = Result.bHit ? FColor(80, 200, 255) : FColor(120, 200, 220);
                }
                else
                {
                        TracerColor = Result.bHit ? FColor(255, 80, 80) : FColor(220, 140, 140);
                }
                DrawDebugLine(World, Request.From, Request.To, TracerColor,
                        /*bPersistent*/ false, /*Lifetime*/ 0.6f,
                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 6.f);
                // Origin marker on the shooter so the line clearly anchors at the source.
                DrawDebugSphere(World, Request.From, /*Radius*/ 18.f, /*Segments*/ 10,
                        TracerColor, /*bPersistent*/ false, /*Lifetime*/ 0.6f,
                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 2.f);
                if (Result.bHit)
                {
                        // Impact marker at the destination.
                        DrawDebugSphere(World, Request.To, /*Radius*/ 30.f, /*Segments*/ 12,
                                TracerColor, /*bPersistent*/ false, /*Lifetime*/ 0.6f,
                                /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 2.f);
                }
        }

        return Result;
}
