// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CallsignNodeMoverComponent.generated.h"

/**
 *  Smooth-move helper attached to pawns that traverse the node graph.
 *
 *  CallsignNodeMovement::TeleportPawnToNode looks for this component on the
 *  pawn; if present, it calls StartMove to interpolate the pawn's location
 *  over a short duration instead of snapping. If the component is missing
 *  the helper falls back to the original instant SetActorLocation.
 */
UCLASS(ClassGroup = "Callsign", meta = (BlueprintSpawnableComponent))
class CALLSIGNEXFIL_API UCallsignNodeMoverComponent : public UActorComponent
{
        GENERATED_BODY()

public:

        UCallsignNodeMoverComponent();

        /** Begin a smooth interpolation from the pawn's current location to TargetLoc over Duration seconds. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Move")
        void StartMove(const FVector& TargetLoc, float Duration);

        /** Default interpolation duration when StartMove receives a non-positive value. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Move", meta = (ClampMin = "0.05", UIMin = "0.05"))
        float DefaultDuration = 0.35f;

protected:

        virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

        bool bMoving = false;
        FVector StartLoc = FVector::ZeroVector;
        FVector EndLoc = FVector::ZeroVector;
        float Elapsed = 0.f;
        float Duration = 0.35f;
};
