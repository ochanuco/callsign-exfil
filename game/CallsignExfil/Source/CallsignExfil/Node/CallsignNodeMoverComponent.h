// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CallsignNodeMoverComponent.generated.h"

/**
 *  Smooth-move helper attached to pawns that traverse the node graph.
 *
 *  Two modes:
 *    - **CharacterMovement walk** (when owner is ACharacter): temporarily
 *      lifts the character's MaxWalkSpeed to WalkSpeed, calls AddMovementInput
 *      toward the target each tick, snaps + restores speed on arrival. This
 *      drives the standard ACharacter movement pipeline so animation, ground
 *      stickiness and gravity behave naturally — visually like normal "WASD"
 *      walking but fully scripted.
 *    - **Direct lerp** (when owner is a plain AActor): SmoothStep ease over
 *      Duration via SetActorLocation. Used as a fallback for non-character
 *      pawns.
 *
 *  Picked up by CallsignNodeMovement::TeleportPawnToNode; if the component
 *  is missing the helper falls back to instant SetActorLocation.
 */
UCLASS(ClassGroup = "Callsign", meta = (BlueprintSpawnableComponent))
class CALLSIGNEXFIL_API UCallsignNodeMoverComponent : public UActorComponent
{
        GENERATED_BODY()

public:

        UCallsignNodeMoverComponent();

        /**
         *  Begin a move toward TargetLoc. For character owners this drives
         *  CharacterMovement (walking); for plain actors this lerps location.
         *  TargetLoc is the node's location — the helper raises it by the
         *  character's capsule half-height so feet end on top of the node
         *  instead of buried inside it.
         */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Move")
        void StartMove(const FVector& TargetLoc, float Duration);

        /** Walk speed used while a character-mode move is in flight. Restored on arrival. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Move", meta = (ClampMin = "1", UIMin = "1"))
        float WalkSpeed = 600.f;

        /** Fallback lerp duration for plain-actor (non-character) owners. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Move", meta = (ClampMin = "0.05", UIMin = "0.05"))
        float DefaultDuration = 0.35f;

        /** Per-move timeout (seconds) that aborts character-walk mode to prevent stuck pawns. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Move", meta = (ClampMin = "0.5", UIMin = "0.5"))
        float WalkTimeout = 3.f;

        /** Distance threshold (cm) below which the move is considered complete. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Move", meta = (ClampMin = "1", UIMin = "1"))
        float ArriveThreshold = 30.f;

protected:

        virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

        void StopMovingAndRestore();

        bool bMoving = false;
        /** True while the move is being driven through the character's CMC. */
        bool bUsingCharacterWalk = false;

        /** Lerp endpoints (used for the plain-actor fallback and as snap target on arrival). */
        FVector StartLoc = FVector::ZeroVector;
        FVector EndLoc = FVector::ZeroVector;

        /** Saved CMC MaxWalkSpeed for restore-on-arrival. */
        float SavedMaxWalkSpeed = 0.f;

        float Elapsed = 0.f;
        float Duration = 0.35f;
};
