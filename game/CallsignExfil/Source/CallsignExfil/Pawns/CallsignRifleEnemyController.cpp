// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemyController.h"
#include "CallsignRifleEnemy.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Turn/CallsignTurnSystem.h"
#include "Engine/World.h"

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
