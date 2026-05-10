// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignRifleEnemy.h"
#include "CallsignRifleEnemyController.h"
#include "CallsignHealthComponent.h"
#include "HUD/CallsignMessageBus.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeMoverComponent.h"
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

        // Smooth node-to-node interpolation. CallsignNodeMovement::TeleportPawnToNode
        // detects this component and replaces instant SetActorLocation with a 0.35s lerp.
        NodeMover = CreateDefaultSubobject<UCallsignNodeMoverComponent>(TEXT("NodeMover"));

        // HP / death tracking. ApplyPointDamage from Combat / SupportResolver
        // flows through TakeDamage -> HealthComp::ApplyDamage.
        HealthComp = CreateDefaultSubobject<UCallsignHealthComponent>(TEXT("HealthComp"));
}

void ACallsignRifleEnemy::BeginPlay()
{
        Super::BeginPlay();
        if (HealthComp)
        {
                HealthComp->OnDied.AddDynamic(this, &ACallsignRifleEnemy::HandleDied);
        }
}

float ACallsignRifleEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser)
{
        const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
        if (HealthComp && Applied > 0.f)
        {
                HealthComp->ApplyDamage(FMath::FloorToInt32(Applied), DamageCauser);
        }
        return Applied;
}

void ACallsignRifleEnemy::HandleDied(UCallsignHealthComponent* /*Comp*/)
{
        UE_LOG(LogTemp, Display, TEXT("[Health] Rifle enemy %s down — hiding"), *GetName());
        CallsignMsg::PushSystem(GetWorld(), TEXT("敵が制圧された。"));

        SetActorHiddenInGame(true);
        if (UCapsuleComponent* Cap = GetCapsuleComponent())
        {
                Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        // Mark turn as finished so the AI controller doesn't stall the round
        // if death lands during this enemy's turn.
        bTurnFinished = true;
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

        // Skip turn entirely if dead. The GameMode auto-advance timer still
        // ticks the round forward; we just don't queue any AI action.
        if (HealthComp && HealthComp->bIsDead)
        {
                bTurnFinished = true;
                return;
        }

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
