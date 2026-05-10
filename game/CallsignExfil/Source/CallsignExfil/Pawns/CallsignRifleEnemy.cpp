// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemy.h"
#include "CallsignRifleEnemyController.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeMovement.h"

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
        CallsignNodeMovement::TeleportPawnToNode(this, CurrentNode, TargetNode);
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
