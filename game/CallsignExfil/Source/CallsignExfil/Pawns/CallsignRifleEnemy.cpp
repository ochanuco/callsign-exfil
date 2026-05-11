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
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACallsignRifleEnemy::ACallsignRifleEnemy()
{
        // Tick is reserved for the hit-flash decay and death scale-down anim.
        // It's only enabled while one of those is active (see HandleHealthChanged
        // and HandleDied); idle enemies don't tick.
        PrimaryActorTick.bCanEverTick = true;
        PrimaryActorTick.bStartWithTickEnabled = false;

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
                HealthComp->OnHealthChanged.AddDynamic(this, &ACallsignRifleEnemy::HandleHealthChanged);
        }

        // Create a tinted MID so enemies read as "the red team" at a glance.
        // Material may or may not expose a Color parameter; SetVectorParameterValue
        // is a silent no-op when the param doesn't exist (still leaves the MID
        // bound, so we always go through the MID path for future tints).
        if (DebugMesh)
        {
                if (UMaterialInterface* SrcMat = DebugMesh->GetMaterial(0))
                {
                        BodyMID = UMaterialInstanceDynamic::Create(SrcMat, this);
                        if (BodyMID)
                        {
                                const FLinearColor TeamRed(0.85f, 0.18f, 0.18f, 1.0f);
                                BodyMID->SetVectorParameterValue(TEXT("Color"), TeamRed);
                                BodyMID->SetVectorParameterValue(TEXT("BaseColor"), TeamRed);
                                DebugMesh->SetMaterial(0, BodyMID);
                        }
                }
        }
}

void ACallsignRifleEnemy::Tick(float DeltaSeconds)
{
        Super::Tick(DeltaSeconds);

        // Hit flash decay: brightens the MID white for ~0.18s after each hit.
        // Compute the lerp factor from the PRE-decay remaining so the first
        // frame after a hit lands at full white (otherwise the flash never
        // reaches the saturated end of the gradient).
        if (HitFlashRemaining > 0.f && BodyMID)
        {
                const float Pre = HitFlashRemaining;
                HitFlashRemaining = FMath::Max(0.f, Pre - DeltaSeconds);
                const float Norm = FMath::Clamp(Pre / 0.18f, 0.f, 1.f);
                const FLinearColor TeamRed(0.85f, 0.18f, 0.18f, 1.0f);
                const FLinearColor FlashWhite(1.0f, 1.0f, 1.0f, 1.0f);
                const FLinearColor Mix = FMath::Lerp(TeamRed, FlashWhite, Norm);
                BodyMID->SetVectorParameterValue(TEXT("Color"), Mix);
                BodyMID->SetVectorParameterValue(TEXT("BaseColor"), Mix);
        }

        // Death scale-down: shrink + tilt over 0.5s, then hide. Avoids the
        // jarring instant disappearance.
        if (DeathAnimElapsed >= 0.f)
        {
                const float Duration = 0.5f;
                DeathAnimElapsed += DeltaSeconds;
                const float t = FMath::Clamp(DeathAnimElapsed / Duration, 0.f, 1.f);
                const float Eased = 1.f - FMath::Pow(1.f - t, 2.f);
                SetActorScale3D(FVector(1.f - Eased));
                AddActorLocalRotation(FRotator(0.f, 0.f, 240.f * DeltaSeconds));
                if (t >= 1.f)
                {
                        // Capsule collision is already dropped up-front in
                        // HandleDied; here we just hide the mesh.
                        SetActorHiddenInGame(true);
                        DeathAnimElapsed = -1.f;
                }
        }

        // Park Tick once both animations are idle so we're not eating frames
        // on enemies that aren't doing anything.
        if (HitFlashRemaining <= 0.f && DeathAnimElapsed < 0.f)
        {
                SetActorTickEnabled(false);
        }
}

void ACallsignRifleEnemy::HandleHealthChanged(UCallsignHealthComponent* /*Source*/, int32 Delta, AActor* /*Causer*/)
{
        // Damage-only flash. Heals are positive deltas; ignore.
        if (Delta < 0)
        {
                HitFlashRemaining = 0.18f;
                SetActorTickEnabled(true);
        }
}

float ACallsignRifleEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser)
{
        // Return what HealthComp ACTUALLY applied, not what Super accepted —
        // otherwise dead targets / round-down / over-cap clamp desync the
        // caller's view of damage from real HP state.
        if (!HealthComp || !HealthComp->IsAlive() || DamageAmount <= 0.f)
        {
                return 0.f;
        }
        const float Requested = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
        const int32 Applied = HealthComp->ApplyDamage(FMath::FloorToInt32(Requested), DamageCauser);
        return static_cast<float>(Applied);
}

void ACallsignRifleEnemy::HandleDied(UCallsignHealthComponent* /*Comp*/)
{
        UE_LOG(LogTemp, Display, TEXT("[Health] Rifle enemy %s down — starting death anim"), *GetName());
        CallsignMsg::PushSystem(GetWorld(), TEXT("敵が制圧された。"));

        // Vacate node occupancy AND drop capsule collision immediately so
        // movement queries / IsOccupied don't see a stale blocker while the
        // 0.5s visual scale-down plays out.
        if (CurrentNode && CurrentNode->Occupant.Get() == this)
        {
                CurrentNode->Occupant = nullptr;
        }
        if (UCapsuleComponent* Cap = GetCapsuleComponent())
        {
                Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        // Mark turn as finished so the AI controller doesn't stall the round
        // if death lands during this enemy's turn.
        bTurnFinished = true;

        // Kick off the scale-down anim in Tick. The actor stays visible while
        // shrinking; collision is dropped once the anim ends so other pawns
        // can pass through the corpse cell.
        DeathAnimElapsed = 0.f;
        SetActorTickEnabled(true);
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
