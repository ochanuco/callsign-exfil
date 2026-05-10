// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemyController.h"
#include "CallsignRifleEnemy.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Turn/CallsignTurnSystem.h"
#include "Combat/CallsignCombatResolver.h"
#include "LineOfSight/CallsignLineOfSightService.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Data/CallsignTypes.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void ACallsignRifleEnemyController::OnPossess(APawn* InPawn)
{
        Super::OnPossess(InPawn);

        if (!InPawn)
        {
                return;
        }

        if (UWorld* World = GetWorld())
        {
                if (UCallsignTurnSystem* TurnSystem = World->GetSubsystem<UCallsignTurnSystem>())
                {
                        TurnSystem->RegisterParticipant(InPawn);
                }
        }
}

void ACallsignRifleEnemyController::BeginTurn_Implementation()
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
                                        bDidShoot = true;
                                }
                                else
                                {
                                        UE_LOG(LogTemp, Verbose, TEXT("[EnemyAI] skip shoot: los=%d hasWeapon=%d"),
                                                LosRes.bHasLineOfSight ? 1 : 0, bHasUsableWeapon ? 1 : 0);
                                }
                        }
                }
        }

        if (bDidShoot)
        {
                FinishTurn(TEXT("shot"));
                return;
        }

        // Filter adjacent nodes: usable if not occupied or self-occupied.
        ACallsignNode* Pick = nullptr;
        for (const TObjectPtr<ACallsignNode>& Adj : CurrentNode->Adjacent)
        {
                if (!Adj)
                {
                        continue;
                }

                const bool bSelfOccupied = (Adj->Occupant.Get() == Enemy);
                if (!Adj->IsOccupied() || bSelfOccupied)
                {
                        Pick = Adj;
                        break; // Phase 1 simplification: first valid neighbor.
                }
        }

        // TODO Phase 2: weighted choice (high-ground, LoS to player, distance to player).
        if (Pick)
        {
                ICallsignNodeOccupant::Execute_MoveToNode(Enemy, Pick);
                UE_LOG(LogTemp, Display, TEXT("[EnemyAI] %s -> moved to %s"),
                        *GetNameSafe(Enemy), *GetNameSafe(Pick));
        }
        else
        {
                UE_LOG(LogTemp, Display, TEXT("[EnemyAI] %s -> stay (no valid neighbor)"),
                        *GetNameSafe(Enemy));
        }

        FinishTurn(TEXT("done"));
}
