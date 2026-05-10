// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/SubclassOf.h"
#include "CallsignExfilGameMode.generated.h"

class ACallsignNode;
class ACallsignRifleEnemy;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class ACallsignExfilGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	ACallsignExfilGameMode();

	/** When true, BeginPlay spawns a default 3x3 node grid + 1 enemy demo. */
	UPROPERTY(EditDefaultsOnly, Category = "Callsign|Phase1Demo")
	bool bAutoSpawnPhase1Demo = true;

	/** When true, BeginPlay starts a timer that auto-ends the current turn. */
	UPROPERTY(EditDefaultsOnly, Category = "Callsign|Phase1Demo")
	bool bAutoAdvanceTurns = true;

	/** Interval in seconds between automatic EndCurrentTurn calls. */
	UPROPERTY(EditDefaultsOnly, Category = "Callsign|Phase1Demo")
	float TurnAdvanceInterval = 1.5f;

	/** Class spawned for the Phase 1 demo enemy (defaulted in constructor). */
	UPROPERTY(EditDefaultsOnly, Category = "Callsign|Phase1Demo")
	TSubclassOf<ACallsignRifleEnemy> Phase1EnemyClass;

	/** Class spawned for Phase 1 demo nodes (defaulted in constructor). */
	UPROPERTY(EditDefaultsOnly, Category = "Callsign|Phase1Demo")
	TSubclassOf<ACallsignNode> Phase1NodeClass;

protected:

	virtual void BeginPlay() override;

private:

	/** Timer handler that ends the current turn each interval. */
	UFUNCTION()
	void HandleAutoAdvance();

	/** Spawns a 3x3 grid of nodes around the player + one enemy at the corner. */
	void SpawnPhase1Demo();

	/** Handle for the auto-advance timer. */
	FTimerHandle AutoAdvanceTimerHandle;
};



