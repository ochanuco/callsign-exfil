// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ACallsignNode::ACallsignNode()
{
        PrimaryActorTick.bCanEverTick = false;

        SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
        RootComponent = SceneRoot;

        Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
        Visual->SetupAttachment(RootComponent);
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // No StaticMesh asset is assigned in C++; designers set it in the BP/level.
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
