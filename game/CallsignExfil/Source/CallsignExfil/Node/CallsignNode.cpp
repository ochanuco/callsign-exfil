// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACallsignNode::ACallsignNode()
{
        PrimaryActorTick.bCanEverTick = false;

        SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
        RootComponent = SceneRoot;

        Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
        Visual->SetupAttachment(RootComponent);
        // QueryOnly + block on Visibility so the mouse cursor's
        // GetHitResultUnderCursor trace can find the node, while staying out
        // of physics simulation. Pawns teleport between nodes (no walking
        // collision needed), so blocking only the Visibility channel is
        // sufficient.
        Visual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Visual->SetCollisionResponseToAllChannels(ECR_Ignore);
        Visual->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

        // Phase 1 default visual: engine basic cube. The Visual represents a
        // single cell of the voxel grid laid down by SpawnEnvironmentDressing,
        // not a thin "board" sitting on top of it — full 1m^3, with its top
        // face aligned to the node actor's Z so it reads as the same surface
        // as the surrounding floor voxels (FloorTopZ = Origin.Z - 90).
        static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeMeshFinder.Succeeded())
        {
                Visual->SetStaticMesh(CubeMeshFinder.Object);
        }
        Visual->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
        Visual->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

        // Tile MID: lifts the playable cells out of the default-gray cube
        // look so they read as deck plates against the surrounding floor
        // grid. Both Color and BaseColor are set since the engine's basic
        // shape material may expose either.
        if (UMaterialInstanceDynamic* TileMID = Visual->CreateDynamicMaterialInstance(0))
        {
                const FLinearColor TileTeal(0.22f, 0.50f, 0.62f, 1.0f);
                TileMID->SetVectorParameterValue(TEXT("Color"), TileTeal);
                TileMID->SetVectorParameterValue(TEXT("BaseColor"), TileTeal);
        }
}

bool ACallsignNode::IsOccupied() const
{
        return Occupant.IsValid();
}

void ACallsignNode::MarkDestroyed()
{
        if (bIsDestroyed)
        {
                return;
        }
        bIsDestroyed = true;
        // ADR-004 §9.2 says destroyed-node visuals are a BP concern. For the
        // Phase 3 PIE demo without a BP override, hide the cell ourselves so
        // destruction is observable without authoring extra assets. A BP
        // listener can override by re-enabling visibility from OnNodeDestroyed.
        if (Visual)
        {
                Visual->SetVisibility(false);
                Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        OnNodeDestroyed.Broadcast(this);
}

bool ACallsignNode::IsAdjacent(const ACallsignNode* Other) const
{
        if (!Other)
        {
                return false;
        }

        return Adjacent.Contains(Other);
}
