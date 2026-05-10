// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CallsignNodeOccupant.generated.h"

class ACallsignNode;

UINTERFACE(MinimalAPI, Blueprintable)
class UCallsignNodeOccupant : public UInterface
{
        GENERATED_BODY()
};

/**
 *  Implemented by actors that can occupy an ACallsignNode.
 *  Used by both player and enemy pawns in Phase 1.
 */
class ICallsignNodeOccupant
{
        GENERATED_BODY()

public:

        /** Returns the node this actor currently occupies, or nullptr. */
        UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Callsign|Node")
        ACallsignNode* GetCurrentNode() const;

        /** Moves this actor to the target node. */
        UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Callsign|Node")
        void MoveToNode(ACallsignNode* TargetNode);
};
