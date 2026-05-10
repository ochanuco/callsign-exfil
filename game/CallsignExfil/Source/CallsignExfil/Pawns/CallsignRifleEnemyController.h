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

        /** Decide and execute one turn of behaviour. Stub for Phase 1. */
        UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Callsign|AI")
        void BeginTurn();
        virtual void BeginTurn_Implementation();
};
