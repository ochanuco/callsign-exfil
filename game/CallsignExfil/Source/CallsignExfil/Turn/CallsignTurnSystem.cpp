// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignTurnSystem.h"

void UCallsignTurnSystem::RegisterParticipant(AActor* Participant)
{
        if (!IsValid(Participant))
        {
                return;
        }

        // De-dupe by raw pointer match.
        for (const TWeakObjectPtr<AActor>& Existing : TurnQueue)
        {
                if (Existing.Get() == Participant)
                {
                        return;
                }
        }

        TurnQueue.Add(Participant);
}

void UCallsignTurnSystem::UnregisterParticipant(AActor* Participant)
{
        if (!Participant)
        {
                return;
        }

        TurnQueue.RemoveAll([Participant](const TWeakObjectPtr<AActor>& Entry)
        {
                return !Entry.IsValid() || Entry.Get() == Participant;
        });

        // TODO Phase 1 impl: keep CurrentIndex consistent across removals.
}

void UCallsignTurnSystem::BeginRound()
{
        // TODO Phase 1 impl: phase ordering by participant kind (player vs enemies).
        CurrentIndex = TurnQueue.Num() > 0 ? 0 : INDEX_NONE;
        CurrentPhase = ECallsignTurnPhase::Player;
        OnPhaseChanged.Broadcast(CurrentPhase);
        BroadcastBeginCurrent();
}

void UCallsignTurnSystem::EndCurrentTurn()
{
        AActor* Ending = GetCurrentParticipant();
        if (Ending)
        {
                OnTurnEnd.Broadcast(Ending);
        }

        if (TurnQueue.Num() == 0)
        {
                CurrentIndex = INDEX_NONE;
                return;
        }

        CurrentIndex = (CurrentIndex + 1) % TurnQueue.Num();

        // TODO Phase 1 impl: derive ECallsignTurnPhase from the current participant
        // (Player vs Enemy) and broadcast OnPhaseChanged when it actually changes.

        BroadcastBeginCurrent();
}

AActor* UCallsignTurnSystem::GetCurrentParticipant() const
{
        if (!TurnQueue.IsValidIndex(CurrentIndex))
        {
                return nullptr;
        }

        return TurnQueue[CurrentIndex].Get();
}

ECallsignTurnPhase UCallsignTurnSystem::GetCurrentPhase() const
{
        return CurrentPhase;
}

void UCallsignTurnSystem::BroadcastBeginCurrent()
{
        if (AActor* Current = GetCurrentParticipant())
        {
                OnTurnBegin.Broadcast(Current);
        }
}
