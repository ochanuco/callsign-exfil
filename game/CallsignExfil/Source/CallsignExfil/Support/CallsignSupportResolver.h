// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/CallsignSupportTypes.h"
#include "CallsignSupportResolver.generated.h"

/**
 *  ADR-004 §4.2: executes a single support resolution.
 *
 *  Invoked by UCallsignSupportSystem when a queued request reaches
 *  TurnsRemaining=0. Steps (per ADR-004 §7.7-7.9):
 *
 *    1. OverlapMultiByObjectType(ECC_Pawn) inside Definition->RadiusCm
 *    2. ApplyPointDamage to each hit Pawn (when bDealsDamage=true)
 *    3. For each ACallsignNode within TerrainDestructionRadiusCm whose
 *       bDestroyable=true: MarkDestroyed + remove from peers' Adjacent
 *    4. Return FCallsignSupportResolution describing the side effects
 *
 *  Phase 3 always behaves as if bAllowsFriendlyFire=true; the field is
 *  reserved for Phase 4 IFF wiring (ADR-004 §13 Risks).
 */
UCLASS()
class UCallsignSupportResolver : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 *  ADR-004 §4.2 entry point. Returns the resolution snapshot the
	 *  System then re-broadcasts via OnSupportResolved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Support")
	FCallsignSupportResolution Resolve(const FCallsignSupportRequest& Request);
};
