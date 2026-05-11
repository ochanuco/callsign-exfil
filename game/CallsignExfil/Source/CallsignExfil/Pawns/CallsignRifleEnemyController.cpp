// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemyController.h"
#include "CallsignRifleEnemy.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Turn/CallsignTurnSystem.h"
#include "Combat/CallsignCombatResolver.h"
#include "LineOfSight/CallsignLineOfSightService.h"
#include "HUD/CallsignMessageBus.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Data/CallsignTypes.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void ACallsignRifleEnemyController::OnPossess(APawn* InPawn)
{
        Super::OnPossess(InPawn);
        // Issue #53: do NOT register here. ACallsignExfilGameMode::BeginPlay
        // is the single source of truth for turn-participant registration so
        // the player can be added at index 0 before any auto-possessed enemy.
        // OnPossess fires during SpawnPhase1Demo's enemy spawn and would
        // otherwise beat the GameMode loop, putting the enemy first in the
        // turn queue and starting the round on an enemy attack right after
        // PIE launch.
}

void ACallsignRifleEnemyController::BeginTurn_Implementation()
{
        // Don't act immediately. Schedule the action so there is a visible beat
        // between the player ending their turn and the enemy reacting; tracer +
        // hit sphere + message log all align with that beat.
        UWorld* World = GetWorld();
        if (!World)
        {
                PerformQueuedAction();
                return;
        }
        if (ActionDelay <= 0.f)
        {
                PerformQueuedAction();
                return;
        }
        UE_LOG(LogTemp, Display, TEXT("[EnemyAI] BeginTurn: deferring action by %.2fs"), ActionDelay);
        World->GetTimerManager().SetTimer(
                ActionTimerHandle, this,
                &ACallsignRifleEnemyController::PerformQueuedAction,
                ActionDelay, /*bLoop*/ false);
}

void ACallsignRifleEnemyController::PerformQueuedAction()
{
        ACallsignRifleEnemy* Enemy = Cast<ACallsignRifleEnemy>(GetPawn());
        UWorld* World = GetWorld();
        UCallsignTurnSystem* TurnSystem = World ? World->GetSubsystem<UCallsignTurnSystem>() : nullptr;

        auto FinishTurn = [&](const TCHAR* Reason)
        {
                if (Enemy)
                {
                        Enemy->bTurnFinished = true;
                }
                UE_LOG(LogTemp, Display, TEXT("[EnemyAI] BeginTurn finish (%s)"), Reason);
                if (TurnSystem)
                {
                        TurnSystem->EndCurrentTurn();
                }
        };

        if (!Enemy)
        {
                UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] BeginTurn: pawn is not ACallsignRifleEnemy"));
                FinishTurn(TEXT("no-pawn"));
                return;
        }

        ACallsignNode* CurrentNode = Enemy->CurrentNode;
        if (!CurrentNode)
        {
                UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] BeginTurn: enemy %s has no CurrentNode"), *GetNameSafe(Enemy));
                FinishTurn(TEXT("no-node"));
                return;
        }

        // Issue #53 sub-2: re-check that this controller is still the current
        // turn participant. The 0.6s ActionDelay timer can fire after another
        // path (round wrap, player force-end, future support effects) has
        // already advanced the turn; running AI logic here would call
        // EndCurrentTurn against a participant that's no longer current and
        // double-advance the queue.
        if (TurnSystem && TurnSystem->GetCurrentParticipant() != Enemy)
        {
                UE_LOG(LogTemp, Warning, TEXT("[EnemyAI] PerformQueuedAction: not my turn anymore (current=%s); skipping"),
                        *GetNameSafe(TurnSystem->GetCurrentParticipant()));
                return;
        }

        // Phase 2 demo (ADR-003 §4.3): try to shoot the player first. When LoS is clear and
        // the enemy has a usable (non-broken) weapon, route a shot through the CombatResolver
        // (which handles ammo / magazine / durability / broken via Inventory). On a successful
        // resolution the AI ends its turn without also moving. Otherwise we fall through to
        // the existing move-to-adjacent fallback.
        bool bDidShoot = false;
        if (APawn* TargetPawn = UGameplayStatics::GetPlayerPawn(this, 0))
        {
                if (TargetPawn != Enemy)
                {
                        UCallsignLineOfSightService* Los = World ? World->GetSubsystem<UCallsignLineOfSightService>() : nullptr;
                        UCallsignCombatResolver* Combat = World ? World->GetSubsystem<UCallsignCombatResolver>() : nullptr;
                        UCallsignInventoryComponent* Inv = Enemy->FindComponentByClass<UCallsignInventoryComponent>();

                        if (Los && Combat && Inv)
                        {
                                const FVector From = Enemy->GetActorLocation();
                                const FVector To = TargetPawn->GetActorLocation();

                                // Issue #22: ignore both source (Enemy) and target (TargetPawn)
                                // so the trace from enemy to player isn't misread as "blocked
                                // by the player's own capsule". Mirrors the CombatResolver
                                // Phase 2 LoS semantics so the pre-check matches.
                                TArray<AActor*> Ignore;
                                Ignore.Add(Enemy);
                                Ignore.Add(TargetPawn);
                                const FCallsignLineOfSightResult LosRes = Los->Query(From, To, Ignore);

                                UCallsignWeaponInstanceObject* CurWeapon = Inv->GetCurrentWeapon();
                                UCallsignWeaponDefinition* WeaponDef = CurWeapon ? CurWeapon->GetWeaponDefinition() : nullptr;
                                const bool bHasUsableWeapon = (CurWeapon != nullptr) && (WeaponDef != nullptr) && !CurWeapon->IsBroken();

                                if (LosRes.bHasLineOfSight && bHasUsableWeapon)
                                {
                                        FCallsignShotRequest Req;
                                        Req.Instigator = Enemy;
                                        Req.From = From;
                                        Req.To = To;
                                        Req.Weapon = WeaponDef;
                                        Req.Inventory = Inv;
                                        // Issue #22: target-aware LoS - resolver ignores
                                        // both Instigator and TargetActor when tracing.
                                        Req.TargetActor = TargetPawn;

                                        const FCallsignShotResult Res = Combat->ResolveShot(Req);
                                        UE_LOG(LogTemp, Display, TEXT("[EnemyAI] shot: hit=%d damage=%.2f"),
                                                Res.bHit ? 1 : 0, Res.DamageApplied);
                                        if (Res.bHit)
                                        {
                                                CallsignMsg::PushEnemy(World, FString::Printf(
                                                        TEXT("敵があなたを撃った。命中、%.0f ダメージ。"), Res.DamageApplied));
                                        }
                                        else
                                        {
                                                CallsignMsg::PushEnemy(World, TEXT("敵があなたを撃った。外れた。"));
                                        }
                                        bDidShoot = true;
                                }
                                else
                                {
                                        UE_LOG(LogTemp, Verbose, TEXT("[EnemyAI] skip shoot: los=%d hasWeapon=%d"),
                                                LosRes.bHasLineOfSight ? 1 : 0, bHasUsableWeapon ? 1 : 0);
                                        // ADR-003 §8: when LoS is clear but the weapon isn't usable,
                                        // surface the broken/empty-state failure to the player.
                                        if (LosRes.bHasLineOfSight && !bHasUsableWeapon)
                                        {
                                                CallsignMsg::PushEnemy(World, TEXT("敵が射撃を試みたが失敗した。"));
                                        }
                                }
                        }
                }
        }

        if (bDidShoot)
        {
                FinishTurn(TEXT("shot"));
                return;
        }

        // Pick the adjacent node that gets us closest to the player. Forced
        // movement (no LoS / no weapon) should still advance the engagement
        // rather than wander, so the demo flow has the enemy actually approach.
        // Skips occupied / destroyed neighbors. Falls back to "any valid
        // neighbor" semantics naturally when the player can't be resolved.
        ACallsignNode* Pick = nullptr;
        float PickDistSq = TNumericLimits<float>::Max();
        APawn* PlayerPawnForAI = UGameplayStatics::GetPlayerPawn(this, 0);
        const bool bHasPlayerLoc = PlayerPawnForAI && PlayerPawnForAI != Enemy;
        const FVector PlayerLoc = bHasPlayerLoc ? PlayerPawnForAI->GetActorLocation() : FVector::ZeroVector;
        for (const TObjectPtr<ACallsignNode>& Adj : CurrentNode->Adjacent)
        {
                if (!Adj || Adj->bIsDestroyed)
                {
                        continue;
                }
                const bool bSelfOccupied = (Adj->Occupant.Get() == Enemy);
                if (Adj->IsOccupied() && !bSelfOccupied)
                {
                        continue;
                }
                const float DSq = bHasPlayerLoc
                        ? FVector::DistSquared(Adj->GetActorLocation(), PlayerLoc)
                        : 0.f;
                if (DSq < PickDistSq)
                {
                        PickDistSq = DSq;
                        Pick = Adj;
                }
        }

        // TODO Phase 4+: high-ground / cover weighting in addition to raw distance.
        if (Pick)
        {
                ICallsignNodeOccupant::Execute_MoveToNode(Enemy, Pick);
                UE_LOG(LogTemp, Display, TEXT("[EnemyAI] %s -> moved to %s"),
                        *GetNameSafe(Enemy), *GetNameSafe(Pick));
                CallsignMsg::PushEnemy(World, TEXT("敵が移動した。"));
        }
        else
        {
                UE_LOG(LogTemp, Display, TEXT("[EnemyAI] %s -> stay (no valid neighbor)"),
                        *GetNameSafe(Enemy));
        }

        FinishTurn(TEXT("done"));
}
