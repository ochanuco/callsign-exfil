// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CallsignHealthComponent.generated.h"

class AActor;
class UCallsignHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnPawnDied, UCallsignHealthComponent*, HealthComp);

/**
 *  Minimal HP / death tracking for Phase 1+ pawns.
 *
 *  Owners (player + rifle enemy) override AActor::TakeDamage to forward the
 *  amount through ApplyDamage(). When CurrentHealth reaches zero the
 *  component flips bIsDead to true and broadcasts OnDied exactly once;
 *  owners are expected to react by hiding the actor and dropping its
 *  collision so it stops being a click / LoS / overlap target.
 *
 *  Phase 3 (ADR-004) doesn't formally specify a death system — this is a
 *  utility layer so support strikes have an observable consequence
 *  (enemy down) instead of just a log line. Phase 4 will likely replace
 *  this with a richer status / wounds layer.
 */
UCLASS(ClassGroup = (Callsign), meta = (BlueprintSpawnableComponent))
class UCallsignHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCallsignHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Health",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxHealth = 100;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callsign|Health")
	int32 CurrentHealth = 100;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callsign|Health")
	bool bIsDead = false;

	/** Broadcast exactly once when CurrentHealth reaches zero. */
	UPROPERTY(BlueprintAssignable, Category = "Callsign|Health")
	FCallsignOnPawnDied OnDied;

	/**
	 *  Reduce CurrentHealth by Amount (clamped to >=0). Returns the actual
	 *  damage applied (0 when already dead). Emits OnDied on the kill blow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Health")
	int32 ApplyDamage(int32 Amount, AActor* Causer);

	/**
	 *  Heal CurrentHealth by Amount (clamped to MaxHealth). Returns the
	 *  actual HP restored (0 when already dead or already full).
	 */
	UFUNCTION(BlueprintCallable, Category = "Callsign|Health")
	int32 ApplyHeal(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Callsign|Health")
	bool IsAlive() const { return !bIsDead; }

protected:
	virtual void BeginPlay() override;
};
