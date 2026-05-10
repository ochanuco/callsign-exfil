// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "CallsignTypes.generated.h"

class UCallsignWeaponDefinition;
class UCallsignInventoryComponent;

/**
 *  Phase of the round-based turn loop.
 *  Player -> Enemies -> Resolving -> Player ...
 */
UENUM(BlueprintType)
enum class ECallsignTurnPhase : uint8
{
        Player          UMETA(DisplayName = "Player"),
        Enemies         UMETA(DisplayName = "Enemies"),
        Resolving       UMETA(DisplayName = "Resolving")
};

/**
 *  Logical kind of a node, used by movement rules and cover/elevation lookups.
 */
UENUM(BlueprintType)
enum class ECallsignNodeKind : uint8
{
        Normal          UMETA(DisplayName = "Normal"),
        HalfCover       UMETA(DisplayName = "HalfCover"),
        FullCover       UMETA(DisplayName = "FullCover"),
        HighGround      UMETA(DisplayName = "HighGround"),
        LowGround       UMETA(DisplayName = "LowGround"),
        Indoor          UMETA(DisplayName = "Indoor")
};

/**
 *  Camera/input mode used by the PlayerController and the shoulder camera component.
 */
UENUM(BlueprintType)
enum class ECallsignCameraMode : uint8
{
        NodeSelect      UMETA(DisplayName = "NodeSelect"),
        Aim             UMETA(DisplayName = "Aim"),
        Idle            UMETA(DisplayName = "Idle")
};

/**
 *  Per-node attributes set in the level.
 */
USTRUCT(BlueprintType)
struct FCallsignNodeAttributes
{
        GENERATED_BODY()

        /** Logical kind of this node. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        ECallsignNodeKind Kind = ECallsignNodeKind::Normal;

        /** Discrete height index used for high/low ground rules. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Node")
        int32 HeightLevel = 0;
};

/**
 *  Row schema for enemy stats table.
 */
USTRUCT(BlueprintType)
struct FCallsignEnemyStats : public FTableRowBase
{
        GENERATED_BODY()

        /** Hit points. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Enemy")
        float HP = 0.f;

        /** Effective range. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Enemy")
        float Range = 0.f;

        /** Per-shot damage. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Enemy")
        float Damage = 0.f;

        /** Action priority used by the AI when picking enemies for a turn. */
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callsign|Enemy")
        int32 Priority = 0;
};

/**
 *  Result of a line-of-sight query.
 */
USTRUCT(BlueprintType)
struct FCallsignLineOfSightResult
{
        GENERATED_BODY()

        /** True when nothing blocks the trace between From and To. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|LoS")
        bool bHasLineOfSight = false;

        /** 0 = full LoS, 1 = partial cover (Phase 2), 2 = full block. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|LoS")
        uint8 CoverLevel = 0;

        /** Actor that blocked the trace, if any. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|LoS")
        TWeakObjectPtr<AActor> BlockingActor;
};

/**
 *  Request payload for a single shot resolution.
 */
USTRUCT(BlueprintType)
struct FCallsignShotRequest
{
        GENERATED_BODY()

        /** Actor that fired. */
        UPROPERTY(BlueprintReadWrite, Category = "Callsign|Combat")
        TWeakObjectPtr<AActor> Instigator;

        /** Origin of the shot. */
        UPROPERTY(BlueprintReadWrite, Category = "Callsign|Combat")
        FVector From = FVector::ZeroVector;

        /** Aim point of the shot. */
        UPROPERTY(BlueprintReadWrite, Category = "Callsign|Combat")
        FVector To = FVector::ZeroVector;

        /** Weapon definition used to resolve damage and LoS rules. */
        UPROPERTY(BlueprintReadWrite, Category = "Callsign|Combat")
        TObjectPtr<UCallsignWeaponDefinition> Weapon = nullptr;

        /**
         *  Phase 2 (ADR-003 §3.2 / §4.3): optional inventory reference.
         *  When set, UCallsignCombatResolver consumes ammo / magazine / durability
         *  via this inventory before applying damage. Phase 1 callers leave this
         *  null and the resolver falls through to the Phase 1 path.
         */
        UPROPERTY(BlueprintReadWrite, Category = "Callsign|Combat")
        TWeakObjectPtr<UCallsignInventoryComponent> Inventory;

        /**
         *  Phase 2 (issue #22): optional target actor. When set, the Phase 2 branch
         *  in UCallsignCombatResolver adds this actor to the LoS IgnoreActors list
         *  so a trace ending on the intended target is treated as LoS-clear (instead
         *  of being misread as "blocked by the target's own capsule"). Also used as
         *  the victim for damage application when LoS is clear. Phase 1 callers
         *  leave this null and behavior is unchanged.
         */
        UPROPERTY(BlueprintReadWrite, Category = "Callsign|Combat")
        TWeakObjectPtr<AActor> TargetActor;
};

/**
 *  Outcome of a single shot resolution.
 */
USTRUCT(BlueprintType)
struct FCallsignShotResult
{
        GENERATED_BODY()

        /** True if the shot connected. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Combat")
        bool bHit = false;

        /** Damage actually applied. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Combat")
        float DamageApplied = 0.f;

        /** LoS query result captured during resolution. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Combat")
        FCallsignLineOfSightResult LosResult;
};
