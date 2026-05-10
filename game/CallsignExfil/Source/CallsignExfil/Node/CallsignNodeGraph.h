// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectPtr.h"
#include "CallsignNodeGraph.generated.h"

class ACallsignNode;

/**
 *  Aggregates ACallsignNode actors in the level and provides simple queries.
 *  Adjacency itself is owned per-node; this class only collects references.
 */
UCLASS(Blueprintable)
class ACallsignNodeGraph : public AActor
{
        GENERATED_BODY()

public:

        /** Constructor. */
        ACallsignNodeGraph();

        /** Looks up a node by Actor name. Returns nullptr if not found. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Node")
        ACallsignNode* FindNodeByName(FName Name) const;

        /** Returns Node->Adjacent, or empty array if Node is null. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Node")
        TArray<ACallsignNode*> GetNeighbors(ACallsignNode* Node) const;

        /** All nodes belonging to this graph. Auto-collected on BeginPlay if empty. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        TArray<TObjectPtr<ACallsignNode>> Nodes;

protected:

        virtual void BeginPlay() override;
};
