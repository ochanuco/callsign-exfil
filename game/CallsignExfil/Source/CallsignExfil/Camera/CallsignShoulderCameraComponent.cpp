// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignShoulderCameraComponent.h"

UCallsignShoulderCameraComponent::UCallsignShoulderCameraComponent()
{
        PrimaryComponentTick.bCanEverTick = true;
        TargetArmLength = IdleArmLength;
        SocketOffset = IdleSocketOffset;
}

void UCallsignShoulderCameraComponent::SetCameraMode(ECallsignCameraMode Mode)
{
        CurrentMode = Mode;
}

void UCallsignShoulderCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
        Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

        float DesiredArmLength = IdleArmLength;
        FVector DesiredSocketOffset = IdleSocketOffset;

        switch (CurrentMode)
        {
        case ECallsignCameraMode::NodeSelect:
                DesiredArmLength = NodeSelectArmLength;
                DesiredSocketOffset = NodeSelectSocketOffset;
                break;
        case ECallsignCameraMode::Aim:
                DesiredArmLength = AimArmLength;
                DesiredSocketOffset = AimSocketOffset;
                break;
        case ECallsignCameraMode::Idle:
        default:
                DesiredArmLength = IdleArmLength;
                DesiredSocketOffset = IdleSocketOffset;
                break;
        }

        TargetArmLength = FMath::FInterpTo(TargetArmLength, DesiredArmLength, DeltaTime, InterpSpeed);
        SocketOffset = FMath::VInterpTo(SocketOffset, DesiredSocketOffset, DeltaTime, InterpSpeed);
}
