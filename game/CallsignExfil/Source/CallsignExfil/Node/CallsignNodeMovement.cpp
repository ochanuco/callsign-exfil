// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNodeMovement.h"

#include "CallsignNode.h"
#include "CallsignNodeMoverComponent.h"
#include "GameFramework/Actor.h"

namespace CallsignNodeMovement
{
        bool TeleportPawnToNode(AActor* Pawn, TObjectPtr<ACallsignNode>& InOutCurrentNode, ACallsignNode* TargetNode)
        {
                if (!Pawn)
                {
                        return false;
                }

                // ADR-004 §9.2: reject move into a destroyed node. Done here
                // (single movement window) instead of in each pawn class so a
                // future pawn type can't accidentally walk onto a node that
                // was just blown up by an OrbitalBarrage.
                if (TargetNode && TargetNode->bIsDestroyed)
                {
                        UE_LOG(LogTemp, Warning, TEXT("[Pawn] %s rejected MoveToNode -> %s (destroyed)"),
                                *GetNameSafe(Pawn), *GetNameSafe(TargetNode));
                        return false;
                }

                // Reject move into a node already held by another actor.
                if (TargetNode && TargetNode->Occupant.IsValid() && TargetNode->Occupant.Get() != Pawn)
                {
                        UE_LOG(LogTemp, Warning, TEXT("[Pawn] %s rejected MoveToNode -> %s (occupied by %s)"),
                                *GetNameSafe(Pawn), *GetNameSafe(TargetNode), *GetNameSafe(TargetNode->Occupant.Get()));
                        return false;
                }

                // Detach from old node's occupancy.
                if (InOutCurrentNode && InOutCurrentNode->Occupant.Get() == Pawn)
                {
                        InOutCurrentNode->Occupant = nullptr;
                }

                InOutCurrentNode = TargetNode;

                if (TargetNode)
                {
                        const FVector EndLoc = TargetNode->GetActorLocation();
                        // If the pawn carries a UCallsignNodeMoverComponent, smoothly
                        // interpolate over a short duration. Otherwise fall back to
                        // an instant SetActorLocation (Phase 1 default).
                        if (UCallsignNodeMoverComponent* Mover = Pawn->FindComponentByClass<UCallsignNodeMoverComponent>())
                        {
                                Mover->StartMove(EndLoc, /*Duration*/ 0.f); // 0 = use component default
                        }
                        else
                        {
                                Pawn->SetActorLocation(EndLoc);
                        }
                        TargetNode->Occupant = Pawn;
                }

                UE_LOG(LogTemp, Display, TEXT("[Pawn] %s MoveToNode -> %s"),
                        *GetNameSafe(Pawn), *GetNameSafe(TargetNode));

                return true;
        }
}
