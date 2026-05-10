// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "Data/CallsignTypes.h"
#include "CallsignNode.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 *  Single placeable node in the arena graph. Holds attributes, adjacency
 *  to neighbouring nodes, and the actor currently occupying it (if any).
 */
UCLASS(Blueprintable)
class ACallsignNode : public AActor
{
        GENERATED_BODY()

public:

        /** Constructor. */
        ACallsignNode();

        /** Returns true if any actor currently occupies this node. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Node")
        bool IsOccupied() const;

        /** Returns true if Other is in this node's adjacency list. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Node")
        bool IsAdjacent(const ACallsignNode* Other) const;

        /** Per-node attributes (kind, height level). */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        FCallsignNodeAttributes Attributes;

        /** Adjacent nodes for movement queries. Edited in the level. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        TArray<TObjectPtr<ACallsignNode>> Adjacent;

        /** Actor currently occupying this node. Transient, set at runtime. */
        UPROPERTY(Transient, BlueprintReadWrite, Category = "Callsign|Node")
        TWeakObjectPtr<AActor> Occupant;

protected:

        /** Scene root. */
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
        TObjectPtr<USceneComponent> SceneRoot;

        /** Visual marker for the node. No mesh assigned in C++. */
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
        TObjectPtr<UStaticMeshComponent> Visual;
};
