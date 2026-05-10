// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CallsignMessage.h"
#include "CallsignMessageBus.generated.h"

/**
 *  Per-world message bus that holds a small ring of player-facing narrative
 *  messages. Decoupled from UE_LOG so the Output Log retains the technical
 *  channel while ACallsignDebugHUD draws this channel for the player.
 *
 *  Phase 2 (this ADR companion): consumed by PC actions, enemy AI,
 *  weapon broken, GameMode, and TurnSystem.
 *  Phase 3 (ADR-004): reused by support requests / resolutions.
 */
UCLASS()
class CALLSIGNEXFIL_API UCallsignMessageBus : public UWorldSubsystem
{
        GENERATED_BODY()

public:

        // UWorldSubsystem
        virtual void Initialize(FSubsystemCollectionBase& Collection) override;
        virtual void Deinitialize() override;

        /**
         *  Append a message. When the buffer exceeds MaxLines, the oldest entry
         *  is dropped. SpawnedAt is stamped from World->GetTimeSeconds().
         */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Messaging")
        void Push(const FString& Text, FLinearColor Color = FLinearColor::White, float Lifetime = 8.f);

        /**
         *  Returns the messages still considered active (Lifetime <= 0 means
         *  forever; otherwise filtered by Now - SpawnedAt > Lifetime).
         */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Messaging")
        TArray<FCallsignMessage> GetActiveMessages() const;

        /** Clears all retained messages. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Messaging")
        void Clear();

        /** Maximum retained entries. Older entries are dropped on overflow. */
        UPROPERTY(EditDefaultsOnly, Category = "Callsign|Messaging")
        int32 MaxLines = 12;

        /**
         *  World-time of the most recent Push (or -1 before any push). Read by
         *  ACallsignDebugHUD to drive the slide-in animation: when the elapsed
         *  time since the last push is below the HUD's slide duration, the
         *  message stack is rendered scrolling upward.
         */
        UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Callsign|Messaging")
        float GetLastPushAt() const { return LastPushAt; }

private:

        /** Backing store. Newest entry is at the end of the array. */
        UPROPERTY()
        TArray<FCallsignMessage> Messages;

        /** World time of the most recent Push (or -1 before the first push). */
        float LastPushAt = -1.f;
};

/**
 *  Free-function convenience helpers. Each looks up UCallsignMessageBus on the
 *  given world and forwards to Push. All variants are null-world safe.
 *
 *  PushInfo   -> white, 8s
 *  PushPlayer -> light cyan, 8s
 *  PushEnemy  -> light red, 8s
 *  PushSystem -> yellow, 12s
 */
namespace CallsignMsg
{
        CALLSIGNEXFIL_API void Push(UWorld* World, const FString& Text, FLinearColor Color = FLinearColor::White, float Lifetime = 8.f);
        CALLSIGNEXFIL_API void PushInfo(UWorld* World, const FString& Text);
        CALLSIGNEXFIL_API void PushPlayer(UWorld* World, const FString& Text);
        CALLSIGNEXFIL_API void PushEnemy(UWorld* World, const FString& Text);
        CALLSIGNEXFIL_API void PushSystem(UWorld* World, const FString& Text);
}
