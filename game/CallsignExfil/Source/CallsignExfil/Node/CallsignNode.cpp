// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
