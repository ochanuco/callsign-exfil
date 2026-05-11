// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignHealthComponent.h"

UCallsignHealthComponent::UCallsignHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCallsignHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = FMath::Max(1, MaxHealth);
	bIsDead = false;
}

int32 UCallsignHealthComponent::ApplyHeal(int32 Amount)
{
	if (bIsDead || Amount <= 0)
	{
		return 0;
	}
	const int32 Before = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	const int32 Restored = CurrentHealth - Before;
	if (Restored > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[Health] %s +%d (%d/%d)"),
			*GetNameSafe(GetOwner()), Restored, CurrentHealth, MaxHealth);
		OnHealthChanged.Broadcast(this, Restored, /*Causer*/ nullptr);
	}
	return Restored;
}

int32 UCallsignHealthComponent::ApplyDamage(int32 Amount, AActor* Causer)
{
	if (bIsDead || Amount <= 0)
	{
		return 0;
	}
	const int32 Before = CurrentHealth;
	CurrentHealth = FMath::Max(0, CurrentHealth - Amount);
	const int32 Applied = Before - CurrentHealth;

	UE_LOG(LogTemp, Display, TEXT("[Health] %s -%d (%d/%d)"),
		*GetNameSafe(GetOwner()), Applied, CurrentHealth, MaxHealth);

	if (Applied > 0)
	{
		OnHealthChanged.Broadcast(this, -Applied, Causer);
	}

	if (CurrentHealth == 0 && !bIsDead)
	{
		bIsDead = true;
		UE_LOG(LogTemp, Display, TEXT("[Health] %s DIED"), *GetNameSafe(GetOwner()));
		OnDied.Broadcast(this);
	}
	return Applied;
}
