// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemyController.h"
#include "CallsignRifleEnemy.h"
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
        // TODO Phase 1 impl: actual decision logic (move toward LoS, shoot, etc.).
        if (ACallsignRifleEnemy* Enemy = Cast<ACallsignRifleEnemy>(GetPawn()))
        {
                Enemy->bTurnFinished = true;
        }
}
