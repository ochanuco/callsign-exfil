// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignWeaponInstanceObject.h"
#include "Data/CallsignWeaponDefinition.h"

void UCallsignWeaponInstanceObject::InitializeFromDefinition(UCallsignWeaponDefinition* Def)
{
        State.WeaponDef = Def;
        if (Def)
        {
                State.MagazineCurrent = Def->MagazineSize;
                State.DurabilityCurrent = Def->DurabilityMax;
        }
        else
        {
                State.MagazineCurrent = 0;
                State.DurabilityCurrent = 0;
        }
        State.WearLevel = 0;

        UE_LOG(LogTemp, Display, TEXT("[Weapon] InitializeFromDefinition def=%s mag=%d dur=%d"),
                *GetNameSafe(Def), State.MagazineCurrent, State.DurabilityCurrent);
}

bool UCallsignWeaponInstanceObject::IsBroken() const
{
        // ADR-003 §9: rescue handgun / infinite durability never breaks; check flag before arithmetic.
        if (State.WeaponDef && State.WeaponDef->bHasInfiniteDurability)
        {
                return false;
        }
        return State.DurabilityCurrent <= 0;
}

bool UCallsignWeaponInstanceObject::ConsumeShot()
{
        if (!State.WeaponDef)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Weapon] ConsumeShot rejected: no WeaponDef"));
                return false;
        }

        // 1. broken check (respects bHasInfiniteDurability)
        if (IsBroken())
        {
                UE_LOG(LogTemp, Display, TEXT("[Weapon] ConsumeShot rejected: broken"));
                return false;
        }

        const UCallsignWeaponDefinition* Def = State.WeaponDef;
        const bool bInfMag = Def->bHasInfiniteMagazine;
        const bool bInfDur = Def->bHasInfiniteDurability;

        // 2. magazine empty check (ADR-003 §9: branch on flag before arithmetic)
        if (!bInfMag && State.MagazineCurrent <= 0)
        {
                UE_LOG(LogTemp, Display, TEXT("[Weapon] ConsumeShot rejected: empty magazine"));
                return false;
        }

        // 3. magazine consumption
        if (!bInfMag)
        {
                State.MagazineCurrent -= Def->ShotsPerAction;
                if (State.MagazineCurrent < 0)
                {
                        State.MagazineCurrent = 0;
                }
        }

        // 4. durability consumption (rescue / infinite-durability skips arithmetic)
        if (!bInfDur && !Def->bIsRescue)
        {
                State.DurabilityCurrent -= Def->DurabilityCostPerAction;
                if (State.DurabilityCurrent <= 0)
                {
                        State.DurabilityCurrent = 0;
                        UE_LOG(LogTemp, Display, TEXT("[Weapon] OnWeaponBroken broadcast"));
                        OnWeaponBroken.Broadcast(this);
                }
        }

        UE_LOG(LogTemp, Display, TEXT("[Weapon] ConsumeShot ok mag=%d dur=%d"),
                State.MagazineCurrent, State.DurabilityCurrent);

        return true;
}

FCallsignReloadResult UCallsignWeaponInstanceObject::ReloadFromPool(int32 Available, int32& OutPoolCountAfter)
{
        FCallsignReloadResult Result;

        if (!State.WeaponDef)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Weapon] ReloadFromPool rejected: no WeaponDef"));
                OutPoolCountAfter = Available;
                return Result;
        }

        const UCallsignWeaponDefinition* Def = State.WeaponDef;

        // ADR-003 §9: rescue handgun / infinite magazine -> no-op.
        if (Def->bHasInfiniteMagazine)
        {
                UE_LOG(LogTemp, Display, TEXT("[Weapon] ReloadFromPool no-op: infinite magazine"));
                OutPoolCountAfter = Available;
                return Result;
        }

        // ADR-003 §7: empty pool -> no-op.
        if (Available <= 0)
        {
                UE_LOG(LogTemp, Display, TEXT("[Weapon] ReloadFromPool no-op: empty pool"));
                OutPoolCountAfter = 0;
                return Result;
        }

        // Already full magazine -> no-op.
        const int32 MagSize = Def->MagazineSize;
        const int32 InitialFree = MagSize - State.MagazineCurrent;
        if (InitialFree <= 0)
        {
                UE_LOG(LogTemp, Display, TEXT("[Weapon] ReloadFromPool no-op: magazine already full"));
                OutPoolCountAfter = Available;
                return Result;
        }

        // Tactical vs normal reload: discard remaining rounds when present.
        const int32 Discarded = (State.MagazineCurrent > 0) ? State.MagazineCurrent : 0;
        State.MagazineCurrent = 0;

        const int32 Wanted = MagSize;
        const int32 Loaded = FMath::Min(Wanted, Available);
        State.MagazineCurrent += Loaded;
        const int32 PoolAfter = Available - Loaded;

        Result.bReloaded = true;
        Result.bWasTactical = (Discarded > 0);
        Result.DiscardedRounds = Discarded;
        Result.LoadedRounds = Loaded;

        OutPoolCountAfter = PoolAfter;

        UE_LOG(LogTemp, Display, TEXT("[Weapon] ReloadFromPool tactical=%d discarded=%d loaded=%d pool_after=%d"),
                Result.bWasTactical ? 1 : 0, Result.DiscardedRounds, Result.LoadedRounds, OutPoolCountAfter);

        return Result;
}
