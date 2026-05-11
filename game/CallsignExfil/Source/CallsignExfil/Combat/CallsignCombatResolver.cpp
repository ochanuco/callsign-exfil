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

        // Issue #40: do not early-return on LoS-blocked. Fall through to the
        // tracer block at the end so misses still draw their duller-tint
        // visualization. Skip the actual ApplyPointDamage when LoS is blocked.
        const bool bLosBlocked = bRequiresLos && !Result.LosResult.bHasLineOfSight;

        if (!bLosBlocked)
        {
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
        }
        // Result.bHit / Result.DamageApplied default to false / 0 for the miss
        // and LoS-blocked paths; nothing else to set.

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
                // Tracer beam: two layered lines (outer glow + inner bright core)
                // read as a beam rather than a debug stroke. SDPG_Foreground bypasses
                // depth so the line is visible through the player capsule.
                FColor GlowColor = TracerColor;
                GlowColor.A = 110;
                DrawDebugLine(World, Request.From, Request.To, GlowColor,
                        /*bPersistent*/ false, /*Lifetime*/ 0.6f,
                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 14.f);
                DrawDebugLine(World, Request.From, Request.To, TracerColor,
                        /*bPersistent*/ false, /*Lifetime*/ 0.6f,
                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 3.f);

                if (Result.bHit)
                {
                        // Impact ring on the floor under the target. Reads as a
                        // ground decal instead of a wireframe sphere floating mid-air.
                        AActor* Victim = Request.TargetActor.Get();
                        if (!Victim)
                        {
                                Victim = Result.LosResult.BlockingActor.Get();
                        }
                        if (Victim)
                        {
                                const FVector ImpactGround = Victim->GetActorLocation() + FVector(0.f, 0.f, -85.f);
                                DrawDebugCircle(World, ImpactGround, /*Radius*/ 60.f, /*Segments*/ 32,
                                        TracerColor, /*bPersistent*/ false, /*Lifetime*/ 0.6f,
                                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 3.5f,
                                        /*YAxis*/ FVector(0.f, 1.f, 0.f), /*XAxis*/ FVector(1.f, 0.f, 0.f),
                                        /*bDrawAxis*/ false);
                        }
                }
        }

        return Result;
}
