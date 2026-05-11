// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/SubclassOf.h"
#include "CallsignExfilGameMode.generated.h"

class ACallsignNode;
class ACallsignRifleEnemy;
class UCallsignHealthComponent;

/** Outcome state of the demo round. */
UENUM(BlueprintType)
enum class ECallsignMissionResult : uint8
{
	InProgress,
	Victory,
	Defeat,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnMissionResultChanged, ECallsignMissionResult, NewResult);

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

	/**
	 *  ADR-004: when true, BeginPlay also builds three transient
	 *  UCallsignSupportDefinition objects (PrecisionStrike / SupplyPod /
	 *  OrbitalBarrage) and registers them with UCallsignSupportSystem so
	 *  the player keys 5/6/7 work in the demo without authoring assets.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Callsign|Phase3Demo")
	bool bAutoInitPhase3SupportDemo = true;

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

	/** Current mission outcome. Transitions only forward (InProgress→Victory/Defeat). */
	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Mission")
	ECallsignMissionResult MissionResult = ECallsignMissionResult::InProgress;

	/** Broadcast once when MissionResult leaves InProgress. */
	UPROPERTY(BlueprintAssignable, Category = "Callsign|Mission")
	FCallsignOnMissionResultChanged OnMissionResultChanged;

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
	 *  ADR-004: builds 3 transient UCallsignSupportDefinition objects and
	 *  registers them with UCallsignSupportSystem so the demo can issue
	 *  PrecisionStrike / SupplyPod / OrbitalBarrage requests without any
	 *  Editor Data Asset authoring. Mirrors EquipPhase2DemoLoadout's
	 *  "transient defs owned by GameMode" pattern.
	 */
	void InitPhase3SupportDemo();

	/**
	 *  Phase 2 demo helper: builds a transient UCallsignWeaponDefinition + WeaponInstanceObject
	 *  in code (no Editor Data Asset required), equips it into the pawn's inventory, and seeds
	 *  Light ammo for the player. ADR-003 §3.1 / §4.1 / §4.2.
	 */
	void EquipPhase2DemoLoadout(APawn* Pawn, bool bIsEnemy);

	/** Subscribes to the player's + each enemy's HealthComp.OnDied. */
	void HookMissionTriggers(APawn* PlayerPawn, const TArray<ACallsignRifleEnemy*>& Enemies);

	/** Bound to every tracked pawn's HealthComp.OnDied to evaluate win/lose. */
	UFUNCTION()
	void HandlePawnDied(UCallsignHealthComponent* HealthComp);

	/** Sets MissionResult and stops the auto-advance timer; broadcasts the change. */
	void SetMissionResult(ECallsignMissionResult NewResult);

	/** Handle for the auto-advance timer. */
	FTimerHandle AutoAdvanceTimerHandle;

	/** Cached so HandlePawnDied can tell "player died vs enemy died" without re-resolving. */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> TrackedPlayerPawn;

	/** Cached enemy list for live-count checks. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ACallsignRifleEnemy>> TrackedEnemies;
};



