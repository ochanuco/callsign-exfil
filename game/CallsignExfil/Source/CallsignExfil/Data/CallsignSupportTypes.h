// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CallsignSupportTypes.generated.h"

class AActor;
class ACallsignNode;
class UCallsignSupportDefinition;

/** ADR-004 §3.1: kind of support request. Phase 3 ships three. */
UENUM(BlueprintType)
enum class ECallsignSupportType : uint8
{
	PrecisionStrike,
	SupplyPod,
	OrbitalBarrage,
};

/** ADR-004 §3.1: lifecycle of a queued request. */
UENUM(BlueprintType)
enum class ECallsignSupportPhase : uint8
{
	Pending,
	Resolving,
	Resolved,
	Cancelled,
};

/**
 *  ADR-004 §3.1: one entry in the support queue. POD only, owned by
 *  UCallsignSupportSystem. UPROPERTY is BlueprintReadOnly so HUD code can
 *  read but not mutate the queue out-of-band.
 */
USTRUCT(BlueprintType)
struct FCallsignSupportRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	TWeakObjectPtr<AActor> RequestedBy;

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	TObjectPtr<UCallsignSupportDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	int32 TurnsRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	ECallsignSupportPhase Phase = ECallsignSupportPhase::Pending;
};

/**
 *  ADR-004 §3.1: result returned from Resolver to System. The struct is
 *  the canonical record of "what one strike did" so HUD/log can render it
 *  without re-querying the world.
 */
USTRUCT(BlueprintType)
struct FCallsignSupportResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	int32 DamageEventsEmitted = 0;

	/**
	 *  TObjectPtr (not TWeakObjectPtr) so the array is BP-compatible and can
	 *  ride through the OnSupportResolved dynamic delegate. Resolution is a
	 *  short-lived snapshot; if a node has been GC'd by the time a listener
	 *  reads this, the entry will simply be null.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	TArray<TObjectPtr<ACallsignNode>> DestroyedNodes;

	UPROPERTY(BlueprintReadOnly, Category = "Callsign|Support")
	bool bFriendlyFireApplied = false;
};

/**
 *  ADR-004 §3.1 / §11: static support parameters, edited as a Data Asset.
 *  Mirrors UCallsignWeaponDefinition's role in Phase 2 — runtime instances
 *  reference these by TObjectPtr and never mutate them.
 */
UCLASS(BlueprintType)
class UCallsignSupportDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Support")
	ECallsignSupportType SupportType = ECallsignSupportType::PrecisionStrike;

	/** Turns from submission to detonation. ADR-004 caps at 1..5 for Phase 3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Support",
		meta = (ClampMin = "1", UIMin = "1", ClampMax = "5", UIMax = "5"))
	int32 DelayTurns = 2;

	/** Damage radius in centimeters. Zero means damageless (used by SupplyPod when bDealsDamage=false). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Support",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RadiusCm = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Support")
	int32 Damage = 30;

	/** Node destruction radius. Zero disables terrain destruction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Support",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TerrainDestructionRadiusCm = 0.f;

	/**
	 *  ADR-004 §13 OQ-4 + §Risks: field exists on the Data Asset for Phase 4
	 *  IFF wiring, but Phase 3 must never branch on it (always treats as true).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Support")
	bool bAllowsFriendlyFire = true;

	/** ADR-004 §13 OQ-4: SupplyPod-style supports skip the damage pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Callsign|Support")
	bool bDealsDamage = true;
};
