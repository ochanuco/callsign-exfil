// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignNodeMovement.h"

#include "CallsignNode.h"
#include "GameFramework/Actor.h"

namespace CallsignNodeMovement
{
        bool TeleportPawnToNode(AActor* Pawn, TObjectPtr<ACallsignNode>& InOutCurrentNode, ACallsignNode* TargetNode)
        {
                if (!Pawn)
                {
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
                        // Phase 1: simple teleport. TODO Phase 2: smooth interpolation/anim.
                        Pawn->SetActorLocation(TargetNode->GetActorLocation());
                        TargetNode->Occupant = Pawn;
                }

                UE_LOG(LogTemp, Display, TEXT("[Pawn] %s MoveToNode -> %s"),
                        *GetNameSafe(Pawn), *GetNameSafe(TargetNode));

                return true;
        }
}
