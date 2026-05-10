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

	/** Default weapon used by this PC when issuing shoot actions in Phase 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Callsign|Weapon")
	TObjectPtr<UCallsignWeaponDefinition> DefaultWeapon;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

public:

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

	/** Ends the player's current turn via the world TurnSystem. */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
	void EndTurn();
};
