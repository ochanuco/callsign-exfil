// Copyright Epic Games, Inc. All Rights Reserved.

#include "CallsignExfilGameMode.h"
#include "Turn/CallsignTurnSystem.h"
#include "Turn/CallsignTurnParticipant.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Pawns/CallsignRifleEnemy.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ACallsignExfilGameMode::ACallsignExfilGameMode()
{
	// Default pawn class is set in derived BP (template default).

	// Phase 1 demo defaults: spawn classes resolved from C++.
	Phase1EnemyClass = ACallsignRifleEnemy::StaticClass();
	Phase1NodeClass = ACallsignNode::StaticClass();
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

	// Spawn the demo grid + enemy first so the GetAllActorsWithInterface scan
	// below picks up the freshly spawned enemy as a turn participant.
	if (bAutoSpawnPhase1Demo)
	{
		SpawnPhase1Demo();
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

	if (bAutoAdvanceTurns)
	{
		World->GetTimerManager().SetTimer(
			AutoAdvanceTimerHandle,
			this,
			&ACallsignExfilGameMode::HandleAutoAdvance,
			TurnAdvanceInterval,
			true);
		UE_LOG(LogTemp, Display, TEXT("[GameMode] Auto-advance timer enabled at %.2fs"), TurnAdvanceInterval);
	}
}

void ACallsignExfilGameMode::HandleAutoAdvance()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>())
	{
		TurnSys->EndCurrentTurn();
	}
}

void ACallsignExfilGameMode::SpawnPhase1Demo()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] Phase1Demo: no player pawn yet, skipping spawn"));
		return;
	}

	const float Spacing = 300.f;
	const FVector Origin = PlayerPawn->GetActorLocation();

	UClass* NodeClass = Phase1NodeClass.Get() ? Phase1NodeClass.Get() : ACallsignNode::StaticClass();
	UClass* EnemyClass = Phase1EnemyClass.Get() ? Phase1EnemyClass.Get() : ACallsignRifleEnemy::StaticClass();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 3x3 grid centered on the player.
	ACallsignNode* Grid[3][3] = {};
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			const FVector Loc = Origin + FVector((i - 1) * Spacing, (j - 1) * Spacing, -90.f);
			ACallsignNode* Node = World->SpawnActor<ACallsignNode>(NodeClass, Loc, FRotator::ZeroRotator, Params);
			Grid[i][j] = Node;
		}
	}

	// Wire 4-connected adjacency between the spawned nodes.
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			ACallsignNode* N = Grid[i][j];
			if (!N)
			{
				continue;
			}
			if (i > 0 && Grid[i - 1][j]) { N->Adjacent.Add(Grid[i - 1][j]); }
			if (i < 2 && Grid[i + 1][j]) { N->Adjacent.Add(Grid[i + 1][j]); }
			if (j > 0 && Grid[i][j - 1]) { N->Adjacent.Add(Grid[i][j - 1]); }
			if (j < 2 && Grid[i][j + 1]) { N->Adjacent.Add(Grid[i][j + 1]); }
		}
	}

	// Place the player on the center node via interface dispatch.
	ACallsignNode* Center = Grid[1][1];
	if (Center && PlayerPawn->GetClass()->ImplementsInterface(UCallsignNodeOccupant::StaticClass()))
	{
		ICallsignNodeOccupant::Execute_MoveToNode(PlayerPawn, Center);
	}

	// Spawn the enemy at the (0,0) corner node.
	ACallsignNode* Corner = Grid[0][0];
	if (Corner)
	{
		ACallsignRifleEnemy* Enemy = World->SpawnActor<ACallsignRifleEnemy>(
			EnemyClass,
			Corner->GetActorLocation(),
			FRotator::ZeroRotator,
			Params);
		if (Enemy && Enemy->GetClass()->ImplementsInterface(UCallsignNodeOccupant::StaticClass()))
		{
			ICallsignNodeOccupant::Execute_MoveToNode(Enemy, Corner);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] Phase1Demo spawned: 9 nodes, 1 enemy at corner, player on center"));
}
