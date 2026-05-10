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

        // Phase 1 default visual: engine basic cube scaled to a tile-shaped slab.
        // Designers can still override the StaticMesh in BP/level if desired.
        static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeMeshFinder.Succeeded())
        {
                Visual->SetStaticMesh(CubeMeshFinder.Object);
        }
        Visual->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.1f));
}

bool ACallsignNode::IsOccupied() const
{
        return Occupant.IsValid();
}

bool ACallsignNode::IsAdjacent(const ACallsignNode* Other) const
{
        if (!Other)
        {
                return false;
        }

        return Adjacent.Contains(Other);
}
