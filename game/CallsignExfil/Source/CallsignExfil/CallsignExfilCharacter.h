// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "UObject/ObjectPtr.h"
#include "Node/CallsignNodeOccupant.h"
#include "Turn/CallsignTurnParticipant.h"
#include "CallsignExfilCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class ACallsignNode;
class UCallsignInventoryComponent;
class UCallsignHealthComponent;
class UCallsignNodeMoverComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ACallsignExfilCharacter : public ACharacter, public ICallsignNodeOccupant, public ICallsignTurnParticipant
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	/** Constructor */
	ACallsignExfilCharacter();

protected:

	virtual void BeginPlay() override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

public:

	/** Phase 2: per-pawn inventory (weapon slots + ammo pool). ADR-003 §4.1. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callsign|Inventory")
	TObjectPtr<UCallsignInventoryComponent> Inventory;

	/** HP / death tracking. Phase 1+ utility; ADR-004 demos use this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callsign|Health")
	TObjectPtr<UCallsignHealthComponent> HealthComp;

	/** Smooth node-to-node interpolation; picked up by CallsignNodeMovement helper. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callsign|Move")
	TObjectPtr<UCallsignNodeMoverComponent> NodeMover;

	/** Node currently occupied by the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
	TObjectPtr<ACallsignNode> CurrentNode;

	/** Tracks whether the player has finished their turn. */
	UPROPERTY(BlueprintReadWrite, Category = "Callsign|Turn")
	bool bTurnFinished = false;

	// ICallsignNodeOccupant
	virtual ACallsignNode* GetCurrentNode_Implementation() const override;
	virtual void MoveToNode_Implementation(ACallsignNode* TargetNode) override;

	// ICallsignTurnParticipant
	virtual void BeginTurn_Implementation() override;
	virtual bool IsTurnFinished_Implementation() const override;

	/** Routes UE damage events through HealthComp. */
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

private:
	UFUNCTION()
	void HandleDied(UCallsignHealthComponent* Comp);
};

