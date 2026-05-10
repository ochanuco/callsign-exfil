// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNodeMoverComponent.h"

UCallsignNodeMoverComponent::UCallsignNodeMoverComponent()
{
        PrimaryComponentTick.bCanEverTick = true;
        // Tick is gated; we only enable it while a move is in flight to avoid
        // per-frame work when the pawn is idle on a node.
        PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCallsignNodeMoverComponent::StartMove(const FVector& TargetLoc, float InDuration)
{
        AActor* Owner = GetOwner();
        if (!Owner)
        {
                return;
        }

        StartLoc = Owner->GetActorLocation();
        EndLoc = TargetLoc;
        Elapsed = 0.f;
        Duration = (InDuration > 0.f) ? InDuration : DefaultDuration;
        Duration = FMath::Max(0.05f, Duration);
        bMoving = true;
        SetComponentTickEnabled(true);
}

void UCallsignNodeMoverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
        Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

        if (!bMoving)
        {
                return;
        }

        AActor* Owner = GetOwner();
        if (!Owner)
        {
                bMoving = false;
                SetComponentTickEnabled(false);
                return;
        }

        Elapsed += DeltaTime;
        const float RawAlpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
        // SmoothStep ease-in/out; cheap and feels less robotic than pure linear.
        const float Eased = FMath::SmoothStep(0.f, 1.f, RawAlpha);
        const FVector NewLoc = FMath::Lerp(StartLoc, EndLoc, Eased);
        Owner->SetActorLocation(NewLoc);

        if (RawAlpha >= 1.f)
        {
                // Snap exactly to the target so the pawn rests cleanly on the node.
                Owner->SetActorLocation(EndLoc);
                bMoving = false;
                SetComponentTickEnabled(false);
        }
}
