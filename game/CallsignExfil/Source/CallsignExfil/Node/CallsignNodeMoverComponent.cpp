// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNodeMoverComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UCallsignNodeMoverComponent::UCallsignNodeMoverComponent()
{
        PrimaryComponentTick.bCanEverTick = true;
        // Tick is gated; only enabled while a move is in flight to avoid
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

        // Character path: drive CMC walk so the move respects gravity / ground /
        // capsule collision. Plain-actor path: direct lerp.
        ACharacter* Char = Cast<ACharacter>(Owner);
        UCapsuleComponent* Cap = Char ? Char->GetCapsuleComponent() : nullptr;

        // Raise the destination by capsule half-height so feet end on the node,
        // not the capsule center buried in it. Plain actors use the raw target.
        if (Cap)
        {
                EndLoc = TargetLoc + FVector(0.f, 0.f, Cap->GetScaledCapsuleHalfHeight());
        }
        else
        {
                EndLoc = TargetLoc;
        }

        bUsingCharacterWalk = false;
        if (Char)
        {
                if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
                {
                        SavedMaxWalkSpeed = CMC->MaxWalkSpeed;
                        CMC->MaxWalkSpeed = WalkSpeed;
                        bUsingCharacterWalk = true;
                }
        }

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
                StopMovingAndRestore();
                return;
        }

        Elapsed += DeltaTime;

        if (bUsingCharacterWalk)
        {
                ACharacter* Char = Cast<ACharacter>(Owner);
                if (!Char)
                {
                        StopMovingAndRestore();
                        return;
                }

                const FVector CurLoc = Owner->GetActorLocation();
                FVector ToTargetH = EndLoc - CurLoc;
                ToTargetH.Z = 0.f; // walk on horizontal plane; CMC handles vertical
                const float DistH = ToTargetH.Size();

                // Issue #50: distinguish arrival vs timeout. Arrival snaps clean
                // to the node; timeout leaves the pawn where CMC ended up so the
                // failure isn't masked by a fake snap. Either way we stop ticking
                // and restore CMC speed so the round can advance.
                const bool bArrived = DistH < ArriveThreshold;
                const bool bTimedOut = Elapsed >= WalkTimeout;

                if (bArrived)
                {
                        // Snap (with sweep) so the final pose is exactly on the node.
                        Char->SetActorLocation(EndLoc, /*bSweep*/ true);
                        StopMovingAndRestore();
                        return;
                }
                if (bTimedOut)
                {
                        UE_LOG(LogTemp, Warning, TEXT("[NodeMover] %s timed out at %.0fcm from target; leaving in place"),
                                *GetNameSafe(Char), DistH);
                        StopMovingAndRestore();
                        return;
                }

                Char->AddMovementInput(ToTargetH.GetSafeNormal(), 1.0f);
        }
        else
        {
                // Plain-actor fallback: SmoothStep lerp.
                const float RawAlpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
                const float Eased = FMath::SmoothStep(0.f, 1.f, RawAlpha);
                Owner->SetActorLocation(FMath::Lerp(StartLoc, EndLoc, Eased));

                if (RawAlpha >= 1.f)
                {
                        Owner->SetActorLocation(EndLoc);
                        StopMovingAndRestore();
                }
        }
}

void UCallsignNodeMoverComponent::StopMovingAndRestore()
{
        if (bUsingCharacterWalk)
        {
                if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
                {
                        if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
                        {
                                CMC->StopMovementImmediately();
                                CMC->MaxWalkSpeed = SavedMaxWalkSpeed;
                        }
                }
        }
        bMoving = false;
        bUsingCharacterWalk = false;
        SetComponentTickEnabled(false);
}
