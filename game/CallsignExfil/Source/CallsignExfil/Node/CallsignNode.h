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
class ACallsignNode;

/**
 *  ADR-004 §3.2 / §9.2: broadcast when a node transitions to destroyed.
 *  Listeners (HUD VFX, Adjacent cleanup) react without polling.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignNodeDestroyedSignature, ACallsignNode*, Node);

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

        /**
         *  ADR-004 §3.2 / §9.2: flip this node into the destroyed state.
         *  Sets bIsDestroyed=true (idempotent) and broadcasts OnNodeDestroyed
         *  exactly once. Adjacent cleanup is the caller's responsibility
         *  (Phase 3: handled in UCallsignSupportResolver).
         */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Node")
        void MarkDestroyed();

        /** Per-node attributes (kind, height level). */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        FCallsignNodeAttributes Attributes;

        /**
         *  ADR-004 §3.2: static flag set in Editor at placement time. Only
         *  destroyable nodes are considered for terrain destruction.
         */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        bool bDestroyable = false;

        /**
         *  ADR-004 §3.2: runtime flag flipped by MarkDestroyed. Transient —
         *  no save/restore. Movement code must reject destinations where
         *  this is true (ADR-004 §9.2).
         */
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Callsign|Node")
        bool bIsDestroyed = false;

        /** ADR-004 §9.2: fires once when MarkDestroyed flips the state. */
        UPROPERTY(BlueprintAssignable, Category = "Callsign|Node")
        FCallsignNodeDestroyedSignature OnNodeDestroyed;

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
