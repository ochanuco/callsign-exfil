// Copyright Epic Games, Inc. All Rights Reserved.

#include "CallsignExfilGameMode.h"
#include "Turn/CallsignTurnSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

ACallsignExfilGameMode::ACallsignExfilGameMode()
{
	// stub
}

void ACallsignExfilGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UCallsignTurnSystem* TurnSystem = World->GetSubsystem<UCallsignTurnSystem>();
	if (!TurnSystem)
	{
		return;
	}

	// Register the player pawn (if already possessed) so it participates in
	// the turn loop. Enemies register themselves via their AAIController on
	// possession.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			TurnSystem->RegisterParticipant(PlayerPawn);
		}
	}

	// TODO Phase 1 impl: explicitly call BeginRound() once all participants
	// (player + enemies) are registered, and wire up turn flow signals.
}
