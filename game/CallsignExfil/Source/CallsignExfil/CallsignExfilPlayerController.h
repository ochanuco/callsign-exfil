// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ObjectPtr.h"
#include "CallsignExfilPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UCallsignWeaponDefinition;
class ACallsignNode;
enum class ECallsignSupportType : uint8;

/**
 *  Controller-side mode that mirrors ECallsignCameraMode but is owned by the
 *  PlayerController. Drives input mapping context swaps and notifies the
 *  shoulder camera component via delegate.
 */
UENUM(BlueprintType)
enum class ECallsignControllerMode : uint8
{
	NodeSelect      UMETA(DisplayName = "NodeSelect"),
	Aim             UMETA(DisplayName = "Aim"),
	Idle            UMETA(DisplayName = "Idle")
};

/** Broadcast when the controller's mode changes; consumed by camera component. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnControllerModeChanged, ECallsignControllerMode, NewMode);

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ACallsignExfilPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	/** Constructor */
	ACallsignExfilPlayerController();

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** IMC active while selecting a node. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Callsign|Input")
	TObjectPtr<UInputMappingContext> NodeSelectIMC;

	/** IMC active while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Callsign|Input")
	TObjectPtr<UInputMappingContext> AimIMC;

	/** Currently active controller mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Input")
	ECallsignControllerMode CurrentMode = ECallsignControllerMode::Idle;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Gameplay teardown; unbinds turn-system delegates. */
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Bound to UCallsignTurnSystem::OnTurnBegin; swaps to NodeSelect when our turn begins. */
	UFUNCTION()
	void HandleTurnBegin(AActor* Who);

	/** Bound to UCallsignTurnSystem::OnTurnEnd; swaps to Idle when our turn ends. */
	UFUNCTION()
	void HandleTurnEnd(AActor* Who);

public:

	/**
	 *  Default weapon used by this PC when issuing shoot actions in Phase 1.
	 *  Phase 2 (ADR-003 §3.1): GameMode auto-demo writes this directly when
	 *  equipping a transient weapon definition at spawn, so the field is public.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|Weapon")
	TObjectPtr<UCallsignWeaponDefinition> DefaultWeapon;

	/** Switches the controller mode and notifies camera/IMC subsystems. */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Input")
	void SetMode(ECallsignControllerMode NewMode);

	/** Broadcast when the controller mode changes. */
	UPROPERTY(BlueprintAssignable, Category = "Callsign|Input")
	FCallsignOnControllerModeChanged OnControllerModeChanged;

	/** Returns true if it is currently this PC's possessed pawn's turn. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Turn")
	bool IsMyTurn() const;

	/** Returns true if the possessed pawn can move to the given node this turn. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Move")
	bool CanMoveToNode(const ACallsignNode* Target) const;

	/** Attempts to move the possessed pawn to Target. Ends the turn on success. */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Move")
	bool TryMoveToNode(ACallsignNode* Target);

	/** Attempts to shoot Target with the default weapon. Ends the turn whether hit or miss. */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Combat")
	bool TryShootAtActor(AActor* Target);

	/**
	 *  Phase 2 (ADR-003 §7): routes a reload request through the possessed pawn's inventory.
	 *  Ends the turn only when the reload actually loaded rounds; rescue / empty-pool / full-mag
	 *  no-ops do not consume the turn. Returns true when rounds were loaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Reload")
	bool TryReload();

	/** Ends the player's current turn via the world TurnSystem. */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
	void EndTurn();

	/** Console: shoot the nearest enemy in range with the equipped weapon. */
	UFUNCTION(Exec)
	void CsxShoot();

	/** Console: reload the equipped weapon. No turn cost on no-op. */
	UFUNCTION(Exec)
	void CsxReload();

	/** Console: end the player turn (only valid when it's actually the player's turn). */
	UFUNCTION(Exec)
	void CsxEndTurn();

	/** Console: dump inventory + turn state to the log (no game effect). */
	UFUNCTION(Exec)
	void CsxStatus();

	/**
	 *  ADR-004 §4.3: submit a support request targeting AtNode (typically
	 *  the node currently under the cursor). On success the request is
	 *  enqueued in UCallsignSupportSystem and the player's turn ends.
	 *  Returns true when the request was accepted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Support")
	bool TryRequestSupport(ECallsignSupportType Type, ACallsignNode* AtNode);

	/** Phase 3 demo binding: PrecisionStrike at the cursor node (key 5). */
	void CsxSupportPrecisionStrike();
	/** Phase 3 demo binding: SupplyPod at the cursor node (key 6). */
	void CsxSupportSupplyPod();
	/** Phase 3 demo binding: OrbitalBarrage at the cursor node (key 7). */
	void CsxSupportOrbitalBarrage();

	/** Demo binding: reload the current level. Useful after mission ends. */
	void CsxRestart();

	/** Mouse cursor → node click handler. Invokes TryMoveToNode if the hit node is adjacent + free. */
	void HandleLeftClickToMoveNode();

	/** Helper for HUD: returns the ACallsignNode currently under the mouse cursor (or nullptr). */
	ACallsignNode* GetNodeUnderCursor() const;
};
