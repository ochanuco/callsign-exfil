// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "Data/CallsignTypes.h"
#include "CallsignTurnSystem.generated.h"

/** Broadcast when a participant's turn becomes active. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnTurnBegin, AActor*, Participant);

/** Broadcast when the active participant ends its turn. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnTurnEnd, AActor*, Participant);

/** Broadcast when the turn phase changes (Player/Enemies/Resolving). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCallsignOnPhaseChanged, ECallsignTurnPhase, NewPhase);

/**
 *  Per-world subsystem that owns the turn queue and exposes the current
 *  participant. Phase 1 implementation is intentionally minimal: it advances
 *  through the queue on EndCurrentTurn and broadcasts events.
 */
UCLASS()
class UCallsignTurnSystem : public UWorldSubsystem
{
        GENERATED_BODY()

public:

        /** Adds a participant to the turn queue. No-op if already registered. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
        void RegisterParticipant(AActor* Participant);

        /** Removes a participant from the turn queue. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
        void UnregisterParticipant(AActor* Participant);

        /** Resets the queue index and starts the round at Player phase. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
        void BeginRound();

        /** Advances to the next participant and broadcasts events. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
        void EndCurrentTurn();

        // UWorldSubsystem
        virtual void Initialize(FSubsystemCollectionBase& Collection) override;
        virtual void Deinitialize() override;

        /** Returns the actor whose turn is currently active, or nullptr. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
        AActor* GetCurrentParticipant() const;

        /** Returns the current turn phase. */
        UFUNCTION(BlueprintCallable, Category = "Callsign|Turn")
        ECallsignTurnPhase GetCurrentPhase() const;

        UPROPERTY(BlueprintAssignable, Category = "Callsign|Turn")
        FCallsignOnTurnBegin OnTurnBegin;

        UPROPERTY(BlueprintAssignable, Category = "Callsign|Turn")
        FCallsignOnTurnEnd OnTurnEnd;

        UPROPERTY(BlueprintAssignable, Category = "Callsign|Turn")
        FCallsignOnPhaseChanged OnPhaseChanged;

protected:

        /** Ordered list of registered participants. */
        UPROPERTY()
        TArray<TWeakObjectPtr<AActor>> TurnQueue;

        /** Index into TurnQueue for the active participant. */
        UPROPERTY()
        int32 CurrentIndex = INDEX_NONE;

        /** Current turn phase. */
        UPROPERTY()
        ECallsignTurnPhase CurrentPhase = ECallsignTurnPhase::Player;

private:

        /** Internal helper to broadcast OnTurnBegin for the current index. */
        void BroadcastBeginCurrent();
};
