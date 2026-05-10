// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;

/**
 *  Shared targeting helpers (Issue #43).
 *
 *  HUD's targeting preview and PlayerController's CsxShoot both need
 *  "the rifle enemy nearest to the player" using identical math. This
 *  namespace owns that one definition so the two call sites can't drift.
 */
namespace CallsignTargeting
{
	/**
	 *  Return the ACallsignRifleEnemy closest to From (by squared distance),
	 *  or nullptr if none exist in World. nullptr-safe on World.
	 */
	AActor* FindNearestRifleEnemy(UWorld* World, const FVector& From);
}
