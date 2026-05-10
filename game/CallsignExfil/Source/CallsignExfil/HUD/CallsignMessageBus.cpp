// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignMessageBus.h"

#include "Engine/World.h"

void UCallsignMessageBus::Initialize(FSubsystemCollectionBase& Collection)
{
        Super::Initialize(Collection);
        UE_LOG(LogTemp, Display, TEXT("[Msg] Subsystem initialized"));
}

void UCallsignMessageBus::Deinitialize()
{
        UE_LOG(LogTemp, Display, TEXT("[Msg] Subsystem deinitialized"));
        Messages.Reset();
        Super::Deinitialize();
}

void UCallsignMessageBus::Push(const FString& Text, FLinearColor Color, float Lifetime)
{
        FCallsignMessage Msg;
        Msg.Text = Text;
        Msg.Color = Color;
        Msg.Lifetime = Lifetime;

        if (UWorld* World = GetWorld())
        {
                Msg.SpawnedAt = World->GetTimeSeconds();
        }
        else
        {
                Msg.SpawnedAt = 0.f;
        }

        Messages.Add(MoveTemp(Msg));

        // Drop oldest entries when over the cap. MaxLines is clamped to >=1
        // defensively so a Designer-set 0 doesn't free everything immediately.
        const int32 Cap = FMath::Max(1, MaxLines);
        while (Messages.Num() > Cap)
        {
                Messages.RemoveAt(0);
        }
}

TArray<FCallsignMessage> UCallsignMessageBus::GetActiveMessages() const
{
        TArray<FCallsignMessage> Out;
        Out.Reserve(Messages.Num());

        const UWorld* World = GetWorld();
        const float Now = World ? World->GetTimeSeconds() : 0.f;

        for (const FCallsignMessage& Msg : Messages)
        {
                // Lifetime <= 0 marks "forever". Otherwise drop when aged out.
                if (Msg.Lifetime > 0.f && (Now - Msg.SpawnedAt) > Msg.Lifetime)
                {
                        continue;
                }
                Out.Add(Msg);
        }

        return Out;
}

void UCallsignMessageBus::Clear()
{
        Messages.Reset();
}

namespace CallsignMsg
{
        void Push(UWorld* World, const FString& Text, FLinearColor Color, float Lifetime)
        {
                if (!World)
                {
                        return;
                }
                if (UCallsignMessageBus* Bus = World->GetSubsystem<UCallsignMessageBus>())
                {
                        Bus->Push(Text, Color, Lifetime);
                }
        }

        void PushInfo(UWorld* World, const FString& Text)
        {
                Push(World, Text, FLinearColor::White, 8.f);
        }

        void PushPlayer(UWorld* World, const FString& Text)
        {
                // Light cyan tint for player-attributed messages.
                Push(World, Text, FLinearColor(0.6f, 0.9f, 1.0f, 1.0f), 8.f);
        }

        void PushEnemy(UWorld* World, const FString& Text)
        {
                // Light red tint for enemy-attributed messages.
                Push(World, Text, FLinearColor(1.0f, 0.55f, 0.55f, 1.0f), 8.f);
        }

        void PushSystem(UWorld* World, const FString& Text)
        {
                // Yellow, longer lifetime for lifecycle / system events.
                Push(World, Text, FLinearColor(1.0f, 0.95f, 0.3f, 1.0f), 12.f);
        }
}
