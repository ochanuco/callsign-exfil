// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignTurnSystem.h"
#include "CallsignTurnParticipant.h"
#include "Engine/World.h"
#include "HUD/CallsignMessageBus.h"

namespace
{
        /** Dispatch BeginTurn to the participant if it implements the interface. */
        static void DispatchBeginTurn(AActor* Participant)
        {
                if (!Participant)
                {
                        return;
                }
                if (Participant->GetClass()->ImplementsInterface(UCallsignTurnParticipant::StaticClass()))
                {
                        UE_LOG(LogTemp, Display, TEXT("[Turn] Dispatching BeginTurn to %s"), *GetNameSafe(Participant));
                        ICallsignTurnParticipant::Execute_BeginTurn(Participant);
                }
                else
                {
                        UE_LOG(LogTemp, Warning, TEXT("[Turn] Participant %s does not implement ICallsignTurnParticipant; skipping dispatch"),
                                *GetNameSafe(Participant));
                }
        }
}

void UCallsignTurnSystem::Initialize(FSubsystemCollectionBase& Collection)
{
        Super::Initialize(Collection);
        UE_LOG(LogTemp, Display, TEXT("[Turn] Subsystem initialized"));
}

void UCallsignTurnSystem::Deinitialize()
{
        UE_LOG(LogTemp, Display, TEXT("[Turn] Subsystem deinitialized"));
        TurnQueue.Reset();
        CurrentIndex = INDEX_NONE;
        Super::Deinitialize();
}

void UCallsignTurnSystem::RegisterParticipant(AActor* Participant)
{
        if (!IsValid(Participant))
        {
                return;
        }

        // De-dupe by weak ptr equality.
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

        // TODO Phase 2: keep CurrentIndex consistent across mid-round removals.
}

void UCallsignTurnSystem::BeginRound()
{
        // Drop entries that have been GC'd or destroyed since registration so the
        // first slot is guaranteed to be a live participant before broadcast.
        TurnQueue.RemoveAll([](const TWeakObjectPtr<AActor>& Entry)
        {
                return !Entry.IsValid();
        });

        if (TurnQueue.Num() == 0)
        {
                UE_LOG(LogTemp, Warning, TEXT("[Turn] BeginRound called with empty queue; ignoring"));
                CurrentIndex = INDEX_NONE;
                return;
        }

        CurrentIndex = 0;
        // Phase 1 simplification: phase stays Player while iterating the queue.
        // TODO Phase 2: derive phase per participant kind (Player vs Enemies).
        CurrentPhase = ECallsignTurnPhase::Player;

        ++RoundCounter;
        UE_LOG(LogTemp, Display, TEXT("[Turn] BeginRound, %d participants"), TurnQueue.Num());
        CallsignMsg::PushSystem(GetWorld(), FString::Printf(
                TEXT("ラウンド %d 開始。"), RoundCounter));

        OnPhaseChanged.Broadcast(CurrentPhase);

        if (AActor* Current = GetCurrentParticipant())
        {
                OnTurnBegin.Broadcast(Current);
                DispatchBeginTurn(Current);
        }
}

void UCallsignTurnSystem::EndCurrentTurn()
{
        AActor* Ending = GetCurrentParticipant();
        if (Ending)
        {
                OnTurnEnd.Broadcast(Ending);
        }

        const int32 QueueSize = TurnQueue.Num();
        if (QueueSize == 0)
        {
                CurrentIndex = INDEX_NONE;
                UE_LOG(LogTemp, Display, TEXT("[Turn] EndCurrentTurn -> queue empty"));
                return;
        }

        // Advance, skipping over null / stale weak refs. Bound the scan by queue size.
        AActor* NextActor = nullptr;
        for (int32 Step = 0; Step < QueueSize; ++Step)
        {
                ++CurrentIndex;
                if (CurrentIndex >= TurnQueue.Num())
                {
                        // Round wrap: reset and re-broadcast via BeginRound.
                        BeginRound();
                        UE_LOG(LogTemp, Display, TEXT("[Turn] EndCurrentTurn -> next=%s"), *GetNameSafe(GetCurrentParticipant()));
                        return;
                }

                NextActor = TurnQueue.IsValidIndex(CurrentIndex) ? TurnQueue[CurrentIndex].Get() : nullptr;
                if (NextActor)
                {
                        break;
                }
        }

        if (NextActor)
        {
                OnTurnBegin.Broadcast(NextActor);
                DispatchBeginTurn(NextActor);
        }

        UE_LOG(LogTemp, Display, TEXT("[Turn] EndCurrentTurn -> next=%s"), *GetNameSafe(GetCurrentParticipant()));
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
                DispatchBeginTurn(Current);
        }
}
