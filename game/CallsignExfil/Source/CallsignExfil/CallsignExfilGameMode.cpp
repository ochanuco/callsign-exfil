// Copyright Epic Games, Inc. All Rights Reserved.

#include "CallsignExfilGameMode.h"
#include "Turn/CallsignTurnSystem.h"
#include "Turn/CallsignTurnParticipant.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ACallsignExfilGameMode::ACallsignExfilGameMode()
{
	// Default pawn class is set in derived BP (template default).
}

void ACallsignExfilGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[GameMode] BeginPlay: no World"));
		return;
	}

	UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>();
	if (!TurnSys)
	{
		UE_LOG(LogTemp, Error, TEXT("[GameMode] BeginPlay: UCallsignTurnSystem subsystem missing"));
		return;
	}

	// Discover all turn participants in the level (player pawn, enemies, etc.).
	TArray<AActor*> Participants;
	UGameplayStatics::GetAllActorsWithInterface(this, UCallsignTurnParticipant::StaticClass(), Participants);

	for (AActor* Actor : Participants)
	{
		if (Actor)
		{
			TurnSys->RegisterParticipant(Actor);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] Registered %d participants"), Participants.Num());

	TurnSys->BeginRound();

	UE_LOG(LogTemp, Display, TEXT("[GameMode] BeginRound complete; first=%s"), *GetNameSafe(TurnSys->GetCurrentParticipant()));
}
