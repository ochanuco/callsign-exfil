// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"

class AActor;
class ACallsignNode;

namespace CallsignNodeMovement
{
        /**
         * Phase 1 teleport-style move. Performs:
         *  - occupancy guard (rejects if target has a different live occupant)
         *  - detach from old node's occupancy if Pawn was there
         *  - assign new node, set actor location, claim new occupancy
         *  - one [Pawn] log line
         * Returns true iff the move was performed; false on rejection / null pawn.
         */
        bool TeleportPawnToNode(AActor* Pawn, TObjectPtr<ACallsignNode>& InOutCurrentNode, ACallsignNode* TargetNode);
}
