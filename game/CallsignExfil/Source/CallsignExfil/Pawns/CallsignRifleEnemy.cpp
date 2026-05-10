// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemy.h"
#include "CallsignRifleEnemyController.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeMovement.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ACallsignRifleEnemy::ACallsignRifleEnemy()
{
        PrimaryActorTick.bCanEverTick = false;

        AIControllerClass = ACallsignRifleEnemyController::StaticClass();
        AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

        // Phase 1 debug visual: child cube mesh attached to the capsule so the enemy
        // is visible without configuring a skeletal mesh.
        DebugMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugMesh"));
        DebugMesh->SetupAttachment(GetCapsuleComponent());
        static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeMeshFinder.Succeeded())
        {
                DebugMesh->SetStaticMesh(CubeMeshFinder.Object);
        }
        DebugMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.5f));
        DebugMesh->SetRelativeLocation(FVector(0.f, 0.f, -45.f));
        DebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // Phase 2 (ADR-003 §4.1): every pawn owns an inventory component.
        // Phase 2 enemies use bUsesAmmoPool=false data so combat does not draw from a pool;
        // the slot still holds a weapon definition for damage/range purposes (ADR-003 §13).
        Inventory = CreateDefaultSubobject<UCallsignInventoryComponent>(TEXT("Inventory"));
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
        bTurnFinished = false;

        // Delegate to the AIController which actually decides + moves + ends the turn.
        if (ACallsignRifleEnemyController* AIC = Cast<ACallsignRifleEnemyController>(GetController()))
        {
                AIC->BeginTurn();
        }
        else
        {
                UE_LOG(LogTemp, Warning, TEXT("[Pawn] %s BeginTurn: no ACallsignRifleEnemyController possessing this pawn"),
                        *GetNameSafe(this));
                // Fail-close: nothing else will end this turn, so the turn-system
                // would stall on IsTurnFinished. Mark finished so the round advances.
                bTurnFinished = true;
        }
}

bool ACallsignRifleEnemy::IsTurnFinished_Implementation() const
{
        return bTurnFinished;
}
