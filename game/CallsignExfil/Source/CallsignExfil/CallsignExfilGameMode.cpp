// Copyright Epic Games, Inc. All Rights Reserved.

#include "CallsignExfilGameMode.h"
#include "CallsignExfilPlayerController.h"
#include "Turn/CallsignTurnSystem.h"
#include "Turn/CallsignTurnParticipant.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Pawns/CallsignRifleEnemy.h"
#include "HUD/CallsignDebugHUD.h"
#include "HUD/CallsignMessageBus.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ACallsignExfilGameMode::ACallsignExfilGameMode()
{
	// Default pawn class is set in derived BP (template default).

	// Phase 1 demo defaults: spawn classes resolved from C++.
	Phase1EnemyClass = ACallsignRifleEnemy::StaticClass();
	Phase1NodeClass = ACallsignNode::StaticClass();

	// Use the Phase 1 debug HUD (turn info / LoS preview / message log).
	// BP subclasses can still override this in the Editor if needed.
	HUDClass = ACallsignDebugHUD::StaticClass();
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

	// Register the player FIRST so the round always starts on the player's turn.
	// (GetAllActorsWithInterface order is engine-defined and often returns enemies
	// first, which made enemies act before the player and felt reactive.)
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	int32 RegisteredCount = 0;
	if (PlayerPawn && PlayerPawn->GetClass()->ImplementsInterface(UCallsignTurnParticipant::StaticClass()))
	{
		TurnSys->RegisterParticipant(PlayerPawn);
		++RegisteredCount;
	}

	// Then register everyone else (enemies, future allies, etc.). Skip the
	// player which we already added.
	TArray<AActor*> Participants;
	UGameplayStatics::GetAllActorsWithInterface(this, UCallsignTurnParticipant::StaticClass(), Participants);
	for (AActor* Actor : Participants)
	{
		if (Actor && Actor != PlayerPawn)
		{
			TurnSys->RegisterParticipant(Actor);
			++RegisteredCount;
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] Registered %d participants (player first)"), RegisteredCount);

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
	UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>();
	if (!TurnSys)
	{
		return;
	}

	// Skip auto-advance while it is the local player's turn — the player
	// must end their turn explicitly (e.g. CsxEndTurn / key `4`).
	// Enemy turns continue to auto-tick so the demo cycle stays observable.
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (TurnSys->GetCurrentParticipant() == PlayerPawn)
		{
			return;
		}
	}

	TurnSys->EndCurrentTurn();
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

	// Drop the voxel floor + cover dressing AFTER nodes are spawned so the
	// dressing pass can read each node's actor location and skip the floor
	// voxel under it (the node's 1m^3 Visual cube stands in for that cell).
	if (bSpawnEnvironmentDressing)
	{
		SpawnEnvironmentDressing(Origin);
	}

	// Place the player on the center node via interface dispatch.
	ACallsignNode* Center = Grid[1][1];
	if (Center && PlayerPawn->GetClass()->ImplementsInterface(UCallsignNodeOccupant::StaticClass()))
	{
		ICallsignNodeOccupant::Execute_MoveToNode(PlayerPawn, Center);
	}

	// Phase 2 demo: equip the player with a transient handgun-shaped weapon and seed Light ammo.
	EquipPhase2DemoLoadout(PlayerPawn, /*bIsEnemy=*/false);

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

		// Phase 2 demo: equip the enemy with a transient rifle (low durability) so the demo
		// observably reaches OnWeaponBroken in a few turns.
		if (Enemy)
		{
			EquipPhase2DemoLoadout(Enemy, /*bIsEnemy=*/true);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] Phase1Demo spawned: 9 nodes, 1 enemy at corner, player on center"));

	// Phase 2 demo: surface the lifecycle event to the on-screen message log
	// once after both pawns are equipped (per-pawn equip logs stay in UE_LOG only).
	CallsignMsg::PushSystem(World, TEXT("作戦区域に降下。装備を確認してください。"));
}

void ACallsignExfilGameMode::SpawnEnvironmentDressing(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Engine stock primitives — always available, no asset authoring required.
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] EnvironmentDressing: /Engine/BasicShapes/Cube not found, skipping"));
		return;
	}
	UMaterialInterface* GridMat = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn a single 1m^3 voxel cube at the given world-space center. Floor
	// voxels pass NoCollision so ECC_Visibility traces (cursor pick / LoS)
	// pass through; cover voxels keep QueryAndPhysics so the CombatResolver
	// line trace treats them as natural blockers.
	auto SpawnVoxel = [&](const FVector& CenterLoc, UMaterialInterface* Mat,
		bool bEnableCollision) -> AStaticMeshActor*
	{
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), CenterLoc, FRotator::ZeroRotator, Params);
		if (!A)
		{
			return nullptr;
		}
		// Mobility must be Movable before SetStaticMesh on a runtime-spawned
		// StaticMeshActor (default mobility is Static).
		A->SetMobility(EComponentMobility::Movable);
		if (UStaticMeshComponent* Comp = A->GetStaticMeshComponent())
		{
			Comp->SetStaticMesh(CubeMesh);
			Comp->SetCollisionEnabled(bEnableCollision
				? ECollisionEnabled::QueryAndPhysics
				: ECollisionEnabled::NoCollision);
			if (Mat)
			{
				Comp->SetMaterial(0, Mat);
			}
		}
		return A;
	};

	// Voxel grid: 100 cm cubes (Engine BasicShapes/Cube is 100^3). Aligning
	// to 100 cm gives "3 voxels = 1 tile move" (node spacing is 300 cm), so
	// players can read distances directly off the grid. Floor top sits at
	// Origin.Z - 90 to match the existing Node Z, so each node's 1m^3
	// Visual cube has its top face flush with the surrounding floor.
	constexpr float VoxelSize = 100.f;
	constexpr float HalfVoxel = 50.f;
	const float FloorTopZ = Origin.Z - 90.f;
	const float FloorVoxelCenterZ = FloorTopZ - HalfVoxel;

	// Collect the (gx, gy) cells already occupied by ACallsignNode actors
	// so the floor pass can skip them — the node's own Visual cube renders
	// the cell instead, which is what removes the "board sitting on top"
	// look (it's a colored cell of the floor grid rather than a separate
	// thin slab above it).
	TSet<FIntPoint> NodeCells;
	{
		TArray<AActor*> Nodes;
		UGameplayStatics::GetAllActorsOfClass(World, ACallsignNode::StaticClass(), Nodes);
		for (AActor* N : Nodes)
		{
			if (!N)
			{
				continue;
			}
			const FVector NLoc = N->GetActorLocation();
			const int32 gx = FMath::RoundToInt32((NLoc.X - Origin.X) / VoxelSize);
			const int32 gy = FMath::RoundToInt32((NLoc.Y - Origin.Y) / VoxelSize);
			NodeCells.Add(FIntPoint(gx, gy));
		}
	}

	// Floor: 13x13 voxels (~13 m square) centered on the origin. Generous
	// margin around the 3x3 (±300 cm) playable grid so the world doesn't
	// end abruptly at the outer node ring. Cells under nodes are skipped.
	constexpr int32 FloorRadius = 6;
	int32 FloorCount = 0;
	for (int32 i = -FloorRadius; i <= FloorRadius; ++i)
	{
		for (int32 j = -FloorRadius; j <= FloorRadius; ++j)
		{
			if (NodeCells.Contains(FIntPoint(i, j)))
			{
				continue;
			}
			const FVector Loc(
				Origin.X + i * VoxelSize,
				Origin.Y + j * VoxelSize,
				FloorVoxelCenterZ);
			if (SpawnVoxel(Loc, GridMat, /*bEnableCollision=*/false))
			{
				++FloorCount;
			}
		}
	}

	// Cover stacks. Each entry is a (GX,GY) voxel-grid coord (units of
	// VoxelSize from origin) and a Height in voxels. Crate XY positions are
	// kept off the cardinal corridors at GX/GY in {-3, 0, 3} so the player
	// can still walk between any two adjacent nodes without colliding.
	struct FCoverStack { int32 GX; int32 GY; int32 Height; };
	const FCoverStack Stacks[] = {
		// Mid-tile crates: tucked into the diagonal gaps between nodes.
		{ -1, -2, 1 },
		{  2, -1, 1 },
		{  1,  2, 1 },
		{ -2,  1, 1 },
		// Sandbag-shaped low wall on the west flank, 3 voxels long.
		{ -5, -1, 1 }, { -5, 0, 1 }, { -5, 1, 1 },
		// Two outer pillars (4 voxels tall) at the floor corners (±6) so
		// they sit on a real floor voxel rather than floating in space.
		{ -6, -6, 4 },
		{  6,  6, 4 },
	};

	int32 CoverCount = 0;
	for (const FCoverStack& S : Stacks)
	{
		for (int32 z = 0; z < S.Height; ++z)
		{
			const FVector Loc(
				Origin.X + S.GX * VoxelSize,
				Origin.Y + S.GY * VoxelSize,
				FloorTopZ + HalfVoxel + z * VoxelSize);
			if (SpawnVoxel(Loc, /*Mat*/ nullptr, /*bEnableCollision=*/true))
			{
				++CoverCount;
			}
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] EnvironmentDressing: %d floor voxels (%d node cells skipped) + %d cover voxels"),
		FloorCount, NodeCells.Num(), CoverCount);
}

void ACallsignExfilGameMode::EquipPhase2DemoLoadout(APawn* Pawn, bool bIsEnemy)
{
	if (!Pawn)
	{
		return;
	}

	UCallsignInventoryComponent* Inv = Pawn->FindComponentByClass<UCallsignInventoryComponent>();
	if (!Inv)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] Phase2Demo: pawn %s has no UCallsignInventoryComponent"),
			*GetNameSafe(Pawn));
		return;
	}

	// Build a transient UCallsignWeaponDefinition. Outer = GameMode so the lifetime is
	// bounded by the demo run and no Editor Data Asset authoring is required.
	UCallsignWeaponDefinition* Def = NewObject<UCallsignWeaponDefinition>(this);
	if (!Def)
	{
		UE_LOG(LogTemp, Error, TEXT("[GameMode] Phase2Demo: failed to NewObject<UCallsignWeaponDefinition>"));
		return;
	}

	if (bIsEnemy)
	{
		// Enemy rifle: low DurabilityMax so the demo can show OnWeaponBroken after ~12 shots.
		// bUsesAmmoPool=false / bHasInfiniteMagazine=true keeps enemy bookkeeping minimal
		// (ADR-003 §13: 敵側は弾薬無限扱い).
		Def->Damage = 5.f;
		Def->Range = 2500.f;
		Def->bRequiresLineOfSight = true;
		Def->AmmoType = ECallsignAmmoType::Light;
		Def->bUsesAmmoPool = false;
		Def->MagazineSize = 8;
		Def->ShotsPerAction = 1;
		Def->DurabilityMax = 12;
		Def->DurabilityCostPerAction = 1;
		Def->bIsRescue = false;
		Def->bHasInfiniteDurability = false;
		Def->bHasInfiniteMagazine = true;
		Def->WeaponSlot = ECallsignWeaponSlot::Main;
	}
	else
	{
		// Player handgun-shaped weapon: small magazine + short durability so the cycle
		// (shoot -> mag empty -> reload -> durability ticks -> eventually broken) is observable.
		Def->Damage = 10.f;
		Def->Range = 2500.f;
		Def->bRequiresLineOfSight = true;
		Def->AmmoType = ECallsignAmmoType::Light;
		Def->bUsesAmmoPool = true;
		Def->MagazineSize = 6;
		Def->ShotsPerAction = 1;
		Def->DurabilityMax = 20;
		Def->DurabilityCostPerAction = 2;
		Def->bIsRescue = false;
		Def->bHasInfiniteDurability = false;
		Def->bHasInfiniteMagazine = false;
		Def->WeaponSlot = ECallsignWeaponSlot::Main;
	}

	// Build the runtime weapon instance. Outer = Inventory so its lifetime tracks the pawn.
	UCallsignWeaponInstanceObject* WI = NewObject<UCallsignWeaponInstanceObject>(Inv);
	if (!WI)
	{
		UE_LOG(LogTemp, Error, TEXT("[GameMode] Phase2Demo: failed to NewObject<UCallsignWeaponInstanceObject>"));
		return;
	}
	WI->InitializeFromDefinition(Def);

	Inv->EquipWeapon(ECallsignWeaponSlot::Main, WI);
	Inv->SetActiveSlot(ECallsignWeaponSlot::Main);

	if (bIsEnemy)
	{
		UE_LOG(LogTemp, Display, TEXT("[GameMode] Phase2Demo: equipped enemy with rifle (Dur=%d will break after ~%d shots)"),
			Def->DurabilityMax, Def->DurabilityMax / FMath::Max(1, Def->DurabilityCostPerAction));
	}
	else
	{
		// Seed enough Light ammo so the player can demo reload at least a few times.
		Inv->AddAmmo(ECallsignAmmoType::Light, 30);

		// Wire the PC's DefaultWeapon so existing TryShootAtActor (which reads DefaultWeapon
		// for Request.Weapon) has a fallback weapon ref aligned with what is in the inventory.
		if (ACallsignExfilPlayerController* PC = Cast<ACallsignExfilPlayerController>(Pawn->GetController()))
		{
			PC->DefaultWeapon = Def;
		}

		UE_LOG(LogTemp, Display, TEXT("[GameMode] Phase2Demo: equipped player with handgun (Mag=%d Dur=%d, +30 Light ammo)"),
			Def->MagazineSize, Def->DurabilityMax);
	}
}
