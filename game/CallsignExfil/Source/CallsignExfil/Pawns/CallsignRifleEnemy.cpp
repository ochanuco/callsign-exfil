// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemy.h"
#include "CallsignRifleEnemyController.h"
#include "Node/CallsignNode.h"

ACallsignRifleEnemy::ACallsignRifleEnemy()
{
        PrimaryActorTick.bCanEverTick = false;

        AIControllerClass = ACallsignRifleEnemyController::StaticClass();
        AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

ACallsignNode* ACallsignRifleEnemy::GetCurrentNode_Implementation() const
{
        return CurrentNode;
}

void ACallsignRifleEnemy::MoveToNode_Implementation(ACallsignNode* TargetNode)
{
        // Reject move into a node already held by another actor.
        if (TargetNode && TargetNode->Occupant.IsValid() && TargetNode->Occupant.Get() != this)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Pawn] %s rejected MoveToNode -> %s (occupied by %s)"),
                        *GetNameSafe(this), *GetNameSafe(TargetNode), *GetNameSafe(TargetNode->Occupant.Get()));
                return;
        }

        // Detach from old node's occupancy.
        if (CurrentNode && CurrentNode->Occupant.Get() == this)
        {
                CurrentNode->Occupant = nullptr;
        }

        CurrentNode = TargetNode;

        if (TargetNode)
        {
                // Phase 1: simple teleport. TODO Phase 2: smooth interpolation/anim.
                SetActorLocation(TargetNode->GetActorLocation());
                TargetNode->Occupant = this;
        }

        UE_LOG(LogTemp, Display, TEXT("[Pawn] %s MoveToNode -> %s"),
                *GetNameSafe(this), *GetNameSafe(TargetNode));
}

void ACallsignRifleEnemy::BeginTurn_Implementation()
{
        // TODO Phase 1 impl: hook into AAIController-driven decision logic.
        bTurnFinished = false;
}

bool ACallsignRifleEnemy::IsTurnFinished_Implementation() const
{
        return bTurnFinished;
}
