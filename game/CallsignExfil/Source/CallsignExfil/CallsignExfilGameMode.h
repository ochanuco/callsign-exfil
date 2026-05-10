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

	/**
	 *  When true, SpawnPhase1Demo also drops a floor + scattered cover blocks
	 *  so the playable area looks like a place instead of "9 boards floating
	 *  in a void". Visual only — the cover blocks have collision, so the
	 *  existing CombatResolver line trace will treat them as LoS blockers.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Callsign|Phase1Demo")
	bool bSpawnEnvironmentDressing = true;

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

	/**
	 *  Drops a floor slab + a handful of cover blocks around the grid for
	 *  visual context. Uses Engine's stock Cube + WorldGridMaterial so no
	 *  authored assets are required. Origin is the grid center (player loc).
	 */
	void SpawnEnvironmentDressing(const FVector& Origin);

	/**
	 *  Phase 2 demo helper: builds a transient UCallsignWeaponDefinition + WeaponInstanceObject
	 *  in code (no Editor Data Asset required), equips it into the pawn's inventory, and seeds
	 *  Light ammo for the player. ADR-003 §3.1 / §4.1 / §4.2.
	 */
	void EquipPhase2DemoLoadout(APawn* Pawn, bool bIsEnemy);

	/** Handle for the auto-advance timer. */
	FTimerHandle AutoAdvanceTimerHandle;
};



