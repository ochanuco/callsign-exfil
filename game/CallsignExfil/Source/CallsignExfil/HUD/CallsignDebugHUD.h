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

protected:

        virtual void BeginPlay() override;
};
