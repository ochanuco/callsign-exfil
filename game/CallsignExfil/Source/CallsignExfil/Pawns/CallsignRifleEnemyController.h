// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CallsignRifleEnemyController.generated.h"

/**
 *  AAIController for the Phase 1 rifle enemy.
 *  Decision logic is intentionally minimal: BeginTurn ends immediately so the
 *  turn loop can be wired and demoed before AI behaviour lands.
 */
UCLASS(Blueprintable)
class ACallsignRifleEnemyController : public AAIController
{
        GENERATED_BODY()

public:

        virtual void OnPossess(APawn* InPawn) override;

        /**
         *  Decide and execute one turn of behaviour. Stub for Phase 1.
         *  Schedules the actual decision via a timer so there's a visible beat
         *  between the player's turn ending and the enemy reacting; the message
         *  log + tracer + hit sphere then all align with that beat.
         */
        UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Callsign|AI")
        void BeginTurn();
        virtual void BeginTurn_Implementation();

        /** Delay (seconds) between BeginTurn dispatch and the AI actually acting. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|AI", meta = (ClampMin = "0.0", UIMin = "0.0"))
        float ActionDelay = 0.6f;

private:

        /** Body of the AI's chosen action. Invoked on the timer set by BeginTurn. */
        UFUNCTION()
        void PerformQueuedAction();

        /** Timer handle for the BeginTurn -> PerformQueuedAction delay. */
        FTimerHandle ActionTimerHandle;
};
