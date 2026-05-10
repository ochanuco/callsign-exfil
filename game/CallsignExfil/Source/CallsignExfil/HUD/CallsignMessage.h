// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "CallsignMessage.generated.h"

/**
 *  Single narrative message intended for on-screen display.
 *
 *  Decoupled from UE_LOG: the Output Log keeps the technical channel,
 *  while FCallsignMessage feeds the player-facing roguelike-style log
 *  drawn by ACallsignDebugHUD.
 *
 *  Phase 2-aware: the message bus is reused by Phase 3 support
 *  announcements (ADR-004 §4.4 HUD extension).
 */
USTRUCT(BlueprintType)
struct CALLSIGNEXFIL_API FCallsignMessage
{
        GENERATED_BODY()

        /** Display text (Japanese in Phase 2, English fallback deferred to Phase 4+). */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Messaging")
        FString Text;

        /** World->GetTimeSeconds() at the moment Push was called. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Messaging")
        float SpawnedAt = 0.f;

        /** Seconds the entry remains visible. -1 means the entry never expires. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Messaging")
        float Lifetime = 8.f;

        /** Tint used by the HUD draw pass. */
        UPROPERTY(BlueprintReadOnly, Category = "Callsign|Messaging")
        FLinearColor Color = FLinearColor::White;
};
