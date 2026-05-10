// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignInventoryComponent.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Weapon/CallsignWeaponInstanceObject.h"

UCallsignInventoryComponent::UCallsignInventoryComponent()
{
        PrimaryComponentTick.bCanEverTick = false;
}

void UCallsignInventoryComponent::EquipWeapon(ECallsignWeaponSlot Slot, UCallsignWeaponInstanceObject* Weapon)
{
        Weapons.Add(Slot, Weapon);
        UE_LOG(LogTemp, Display, TEXT("[Inventory] EquipWeapon slot=%d weapon=%s"),
                (int32)Slot, *GetNameSafe(Weapon));
}

void UCallsignInventoryComponent::SetActiveSlot(ECallsignWeaponSlot Slot)
{
        ActiveSlot = Slot;
        UE_LOG(LogTemp, Display, TEXT("[Inventory] SetActiveSlot -> %d"), (int32)Slot);
}

UCallsignWeaponInstanceObject* UCallsignInventoryComponent::GetCurrentWeapon() const
{
        if (const TObjectPtr<UCallsignWeaponInstanceObject>* Found = Weapons.Find(ActiveSlot))
        {
                return Found->Get();
        }
        return nullptr;
}

int32 UCallsignInventoryComponent::GetAmmoCount(ECallsignAmmoType AmmoType) const
{
        return AmmoPool.FindRef(AmmoType);
}

void UCallsignInventoryComponent::AddAmmo(ECallsignAmmoType AmmoType, int32 Count)
{
        if (Count <= 0)
        {
                return;
        }
        int32& Cur = AmmoPool.FindOrAdd(AmmoType);
        Cur += Count;
        UE_LOG(LogTemp, Display, TEXT("[Inventory] AddAmmo type=%d +%d -> %d"),
                (int32)AmmoType, Count, Cur);
}

bool UCallsignInventoryComponent::ConsumeShotForCurrentWeapon()
{
        UCallsignWeaponInstanceObject* Weapon = GetCurrentWeapon();
        if (!Weapon)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Inventory] ConsumeShotForCurrentWeapon rejected: no current weapon"));
                return false;
        }

        const UCallsignWeaponDefinition* Def = Weapon->GetWeaponDefinition();
        if (!Def)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Inventory] ConsumeShotForCurrentWeapon rejected: weapon has no def"));
                return false;
        }

        const ECallsignAmmoType AmmoType = Def->AmmoType;
        const bool bUsesPool = Def->bUsesAmmoPool;

        // Step 2: pool check (if applicable). Rescue handgun (bUsesAmmoPool=false) skips this.
        if (bUsesPool)
        {
                const int32 PoolNow = AmmoPool.FindRef(AmmoType);
                if (PoolNow <= 0)
                {
                        UE_LOG(LogTemp, Display, TEXT("[Inventory] ConsumeShotForCurrentWeapon rejected: empty pool type=%d"),
                                (int32)AmmoType);
                        return false;
                }
        }

        // Steps 3-7 inside the weapon: magazine, durability, broken-broadcast.
        if (!Weapon->ConsumeShot())
        {
                // weapon refused (broken / empty mag)
                return false;
        }

        // Phase 2: pool decrement is per-action (= ShotsPerAction). Phase 3 may revisit.
        if (bUsesPool)
        {
                int32& PoolRef = AmmoPool.FindOrAdd(AmmoType);
                PoolRef -= Def->ShotsPerAction;
                if (PoolRef < 0)
                {
                        PoolRef = 0;
                }
                UE_LOG(LogTemp, Display, TEXT("[Inventory] pool type=%d -%d -> %d"),
                        (int32)AmmoType, Def->ShotsPerAction, PoolRef);
        }

        return true;
}

FCallsignReloadResult UCallsignInventoryComponent::Reload()
{
        FCallsignReloadResult Result;

        UCallsignWeaponInstanceObject* Weapon = GetCurrentWeapon();
        if (!Weapon)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Inventory] Reload rejected: no current weapon"));
                return Result;
        }

        const UCallsignWeaponDefinition* Def = Weapon->GetWeaponDefinition();
        if (!Def)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Inventory] Reload rejected: no weapon def"));
                return Result;
        }

        // ADR-003 §9: rescue handgun has bHasInfiniteMagazine -> no-op.
        if (Def->bHasInfiniteMagazine)
        {
                UE_LOG(LogTemp, Display, TEXT("[Inventory] Reload no-op: infinite magazine"));
                return Result;
        }

        const ECallsignAmmoType AmmoType = Def->AmmoType;

        // Edge case: weapon does not draw from a pool but is also not the rescue special-case.
        // (Should not happen in normal Phase 2 data; rescue handles itself above. Keep no-op for safety.)
        if (!Def->bUsesAmmoPool)
        {
                UE_LOG(LogTemp, Display, TEXT("[Inventory] Reload no-op: weapon does not use ammo pool"));
                return Result;
        }

        const int32 Available = AmmoPool.FindRef(AmmoType);
        if (Available <= 0)
        {
                // ADR-003 §7: empty pool -> no-op, no broadcast, no turn cost.
                UE_LOG(LogTemp, Display, TEXT("[Inventory] Reload no-op: empty pool type=%d"), (int32)AmmoType);
                return Result;
        }

        int32 PoolAfter = Available;
        Result = Weapon->ReloadFromPool(Available, PoolAfter);
        AmmoPool.FindOrAdd(AmmoType) = PoolAfter;

        if (Result.bReloaded)
        {
                UE_LOG(LogTemp, Display, TEXT("[Inventory] Reload tactical=%d discarded=%d loaded=%d pool_after=%d"),
                        Result.bWasTactical ? 1 : 0, Result.DiscardedRounds, Result.LoadedRounds, PoolAfter);
                OnReloaded.Broadcast(Result);
        }

        return Result;
}

bool UCallsignInventoryComponent::IsReloadAvailable() const
{
        const UCallsignWeaponInstanceObject* Weapon = GetCurrentWeapon();
        if (!Weapon)
        {
                return false;
        }
        const UCallsignWeaponDefinition* Def = Weapon->GetWeaponDefinition();
        if (!Def)
        {
                return false;
        }
        if (Def->bHasInfiniteMagazine)
        {
                return false;
        }
        if (Weapon->GetMagazineCurrent() >= Def->MagazineSize)
        {
                return false;
        }
        if (!Def->bUsesAmmoPool)
        {
                return false;
        }
        return AmmoPool.FindRef(Def->AmmoType) > 0;
}
