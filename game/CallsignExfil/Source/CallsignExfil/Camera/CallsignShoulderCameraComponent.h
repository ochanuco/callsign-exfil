// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "Data/CallsignTypes.h"
#include "CallsignShoulderCameraComponent.generated.h"

/**
 *  Spring arm specialised for the Phase 1 shoulder camera. Holds three sets of
 *  arm-length / socket-offset values (NodeSelect / Aim / Idle) and lerps the
 *  base SpringArm fields toward the active set on tick.
 */
UCLASS(ClassGroup = (Callsign), meta = (BlueprintSpawnableComponent))
class UCallsignShoulderCameraComponent : public USpringArmComponent
{
        GENERATED_BODY()

public:

        /** Constructor. */
        UCallsignShoulderCameraComponent();

        /** Switches the active camera mode. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Camera")
        void SetCameraMode(ECallsignCameraMode Mode);

        virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

        /** Spring arm length when the player is selecting a node. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Camera")
        float NodeSelectArmLength = 600.f;

        /** Socket offset when the player is selecting a node. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Camera")
        FVector NodeSelectSocketOffset = FVector::ZeroVector;

        /** Spring arm length while aiming. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Camera")
        float AimArmLength = 200.f;

        /** Socket offset while aiming. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Camera")
        FVector AimSocketOffset = FVector(0.f, 50.f, 60.f);

        /** Spring arm length when idle (default exploration view). */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Camera")
        float IdleArmLength = 400.f;

        /** Socket offset when idle. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Camera")
        FVector IdleSocketOffset = FVector::ZeroVector;

        /** Interpolation speed for blending toward the active mode's targets. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Camera")
        float InterpSpeed = 8.f;

protected:

        /** Currently selected camera mode. */
        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callsign|Camera")
        ECallsignCameraMode CurrentMode = ECallsignCameraMode::Idle;
};
