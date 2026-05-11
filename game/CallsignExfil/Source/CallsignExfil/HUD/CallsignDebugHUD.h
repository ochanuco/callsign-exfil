// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CallsignDebugHUD.generated.h"

class UCallsignTurnSystem;
class UCallsignLineOfSightService;
class UCallsignMessageBus;

/**
 *  Phase 1 PIE-time debug HUD.
 *
 *  Draws lightweight on-screen overlays via AHUD::DrawHUD:
 *    - Current turn phase and active participant (top-left text)
 *    - A line-of-sight preview line from the player pawn toward the camera-aim
 *      endpoint, colored by the LineOfSight subsystem result.
 *
 *  Per ADR-002, UMG/HUD widget authoring is Blueprint-side; this class exists
 *  purely as a C++ debug overlay so designers can verify Phase 1 systems are
 *  wired correctly during PIE without spawning UMG assets.
 */
UCLASS()
class CALLSIGNEXFIL_API ACallsignDebugHUD : public AHUD
{
        GENERATED_BODY()

public:

        virtual void DrawHUD() override;

        /** Toggle the top-left turn-info text overlay. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowTurnInfo = true;

        /** Toggle the player-to-camera-aim LoS preview line. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowLoSPreview = true;

        /** Color used for the LoS preview line when LoS is clear. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        FLinearColor LosOkColor = FLinearColor::Green;

        /** Color used for the LoS preview line when LoS is blocked. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        FLinearColor LosBlockedColor = FLinearColor::Red;

        /** Toggle the bottom-right roguelike-style narrative message log. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowMessageLog = true;

        /** Toggle the top-right key-binding help panel. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowKeyHelp = true;

        /** Toggle the player-turn targeting preview (line + ring on the would-be shoot target). */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowTargetingPreview = true;

        /**
         *  ADR-004 §8: toggle the support-request preview overlay
         *  (pending list text + DebugSphere blast radius).
         */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowSupportPreview = true;

        /**
         *  Player HP bar (top-left under turn info) + small floating HP
         *  text above each rifle enemy's head.
         */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowHealthOverlay = true;

        /** Toggle the centered Mission Result banner (Victory / Defeat). */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowMissionBanner = true;

        /**
         *  Toggle floating damage / heal popups above hit pawns. Listens to
         *  each pawn's UCallsignHealthComponent::OnHealthChanged, spawns a
         *  short-lived screen-space "-X" / "+X" that drifts up and fades.
         */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowDamagePopups = true;

        /** Toggle the player weapon status panel (mag / durability / pool). */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD")
        bool bShowWeaponStatus = true;

        /** How long a popup remains visible (seconds). */
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Callsign|HUD",
                meta = (ClampMin = "0.1", UIMin = "0.1"))
        float DamagePopupLifetime = 1.5f;

protected:

        virtual void BeginPlay() override;

private:

        /** One floating number entry currently animating. */
        struct FFloatingNumber
        {
                FVector WorldLoc;
                int32 SignedAmount;
                float SpawnedAt;
        };

        TArray<FFloatingNumber> FloatingNumbers;

        UFUNCTION()
        void HandleHealthChanged(class UCallsignHealthComponent* Source, int32 Delta, AActor* Causer);
};
