// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UObject/ObjectPtr.h"
#include "Node/CallsignNodeOccupant.h"
#include "Turn/CallsignTurnParticipant.h"
#include "CallsignRifleEnemy.generated.h"

class ACallsignNode;

/**
 *  Phase 1 sole enemy archetype. Implements both NodeOccupant and TurnParticipant
 *  interfaces directly. Auto-possessed by ACallsignRifleEnemyController.
 */
UCLASS(Blueprintable)
class ACallsignRifleEnemy : public ACharacter, public ICallsignNodeOccupant, public ICallsignTurnParticipant
{
        GENERATED_BODY()

public:

        /** Constructor. */
        ACallsignRifleEnemy();

        /** Node currently occupied by this enemy. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        TObjectPtr<ACallsignNode> CurrentNode;

        /** Tracks whether this enemy has finished its turn. */
        UPROPERTY(BlueprintReadWrite, Category = "Callsign|Turn")
        bool bTurnFinished = false;

        // ICallsignNodeOccupant
        virtual ACallsignNode* GetCurrentNode_Implementation() const override;
        virtual void MoveToNode_Implementation(ACallsignNode* TargetNode) override;

        // ICallsignTurnParticipant
        virtual void BeginTurn_Implementation() override;
        virtual bool IsTurnFinished_Implementation() const override;
};
