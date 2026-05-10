// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/CallsignSupportTypes.h"
#include "CallsignSupportSystem.generated.h"

class AActor;
class UCallsignSupportDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnSupportSubmitted, const FCallsignSupportRequest&, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnSupportResolved, const FCallsignSupportResolution&, Resolution);

/**
 *  ADR-004 §4.1: turn-coupled queue of pending support requests.
 *
 *  Subscribes to UCallsignTurnSystem::OnTurnEnd at Initialize time. On each
 *  turn end, decrements TurnsRemaining for all Pending requests; any that
 *  hit zero are handed to UCallsignSupportResolver::Resolve and broadcast
 *  via OnSupportResolved.
 */
UCLASS()
class UCallsignSupportSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 *  ADR-004 §7 step 2-3: build a FCallsignSupportRequest, push to the
	 *  queue, broadcast OnSupportSubmitted. Returns the request id (FGuid)
	 *  the caller can stash for cancellation. Returns an invalid Guid if
	 *  no UCallsignSupportDefinition is registered for the given type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Support")
	FGuid RequestSupport(ECallsignSupportType Type, const FVector& TargetLocation, AActor* RequestedBy);

	/** ADR-004 §4.1: snapshot of all queued requests in submission order. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Support")
	TArray<FCallsignSupportRequest> GetPendingRequests() const;

	/**
	 *  ADR-004 §4.1: cancel a still-Pending request. Returns true if a
	 *  matching request was found and flipped to Cancelled. Already-
	 *  resolved or unknown ids return false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Support")
	bool CancelRequest(const FGuid& RequestId);

	/**
	 *  Demo / GameMode helper: maps an ECallsignSupportType enum value to
	 *  a UCallsignSupportDefinition asset. Phase 3 demo calls this from
	 *  ACallsignExfilGameMode after building the three transient
	 *  PrecisionStrike / SupplyPod / OrbitalBarrage definitions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Support")
	void RegisterDefinition(ECallsignSupportType Type, UCallsignSupportDefinition* Definition);

	/** Broadcast immediately after a request is enqueued. */
	UPROPERTY(BlueprintAssignable, Category = "Callsign|Support")
	FCallsignOnSupportSubmitted OnSupportSubmitted;

	/** Broadcast after Resolver returns; one fire per resolved request. */
	UPROPERTY(BlueprintAssignable, Category = "Callsign|Support")
	FCallsignOnSupportResolved OnSupportResolved;

private:
	UFUNCTION()
	void HandleTurnEnd(AActor* Participant);

	/** Look up the definition for a type. Phase 3: registered via demo init. */
	UCallsignSupportDefinition* FindDefinition(ECallsignSupportType Type) const;

	/** Persistent queue of all requests (Pending / Resolving / Resolved / Cancelled). */
	UPROPERTY(Transient)
	TArray<FCallsignSupportRequest> Requests;

	/** Static map from type → definition asset, populated by RegisterDefinition. */
	UPROPERTY(Transient)
	TMap<ECallsignSupportType, TObjectPtr<UCallsignSupportDefinition>> Definitions;
};
