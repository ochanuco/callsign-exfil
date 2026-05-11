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
#include "Pawns/CallsignHealthComponent.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
#include "Data/CallsignSupportTypes.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Support/CallsignSupportSystem.h"
#include "EngineUtils.h"
#include "HUD/CallsignMessageBus.h"
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

	// Mission win/lose triggers: subscribe to every pawn's HealthComp.OnDied
	// so HandlePawnDied can evaluate whether the round is over.
	{
		TArray<ACallsignRifleEnemy*> SpawnedEnemies;
		for (TActorIterator<ACallsignRifleEnemy> It(World); It; ++It)
		{
			if (ACallsignRifleEnemy* E = *It)
			{
				SpawnedEnemies.Add(E);
			}
		}
		HookMissionTriggers(PlayerPawn, SpawnedEnemies);
	}
}

void ACallsignExfilGameMode::HookMissionTriggers(APawn* PlayerPawn, const TArray<ACallsignRifleEnemy*>& Enemies)
{
	TrackedPlayerPawn = PlayerPawn;
	TrackedEnemies.Reset();

	auto Subscribe = [this](AActor* Pawn)
	{
		if (!Pawn)
		{
			return;
		}
		if (UCallsignHealthComponent* HC = Pawn->FindComponentByClass<UCallsignHealthComponent>())
		{
			HC->OnDied.AddDynamic(this, &ACallsignExfilGameMode::HandlePawnDied);
		}
	};

	Subscribe(PlayerPawn);
	for (ACallsignRifleEnemy* E : Enemies)
	{
		Subscribe(E);
		if (E)
		{
			TrackedEnemies.Add(E);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] Mission triggers hooked: player=%s enemies=%d"),
		*GetNameSafe(PlayerPawn), TrackedEnemies.Num());

	// Edge case: zero (or already-dead) enemies at hook time means no OnDied
	// ever fires, so HandlePawnDied can't transition to Victory. Evaluate the
	// living count once right after subscribing.
	int32 LivingAtHook = 0;
	for (const TWeakObjectPtr<ACallsignRifleEnemy>& EW : TrackedEnemies)
	{
		const ACallsignRifleEnemy* E = EW.Get();
		if (!E)
		{
			continue;
		}
		if (const UCallsignHealthComponent* EHC = E->FindComponentByClass<UCallsignHealthComponent>())
		{
			if (!EHC->bIsDead)
			{
				++LivingAtHook;
			}
		}
	}
	if (LivingAtHook == 0)
	{
		SetMissionResult(ECallsignMissionResult::Victory);
	}
}

void ACallsignExfilGameMode::HandlePawnDied(UCallsignHealthComponent* HealthComp)
{
	if (MissionResult != ECallsignMissionResult::InProgress || !HealthComp)
	{
		return;
	}

	AActor* Owner = HealthComp->GetOwner();
	APawn* Player = TrackedPlayerPawn.Get();

	if (Owner && Owner == Player)
	{
		SetMissionResult(ECallsignMissionResult::Defeat);
		return;
	}

	// Enemy died — recount living enemies; victory when zero remain.
	int32 Living = 0;
	for (const TWeakObjectPtr<ACallsignRifleEnemy>& EW : TrackedEnemies)
	{
		ACallsignRifleEnemy* E = EW.Get();
		if (!E)
		{
			continue;
		}
		if (UCallsignHealthComponent* EHC = E->FindComponentByClass<UCallsignHealthComponent>())
		{
			if (!EHC->bIsDead)
			{
				++Living;
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[GameMode] Enemy down — %d living enemies remain"), Living);
	if (Living == 0)
	{
		SetMissionResult(ECallsignMissionResult::Victory);
	}
}

void ACallsignExfilGameMode::SetMissionResult(ECallsignMissionResult NewResult)
{
	if (MissionResult != ECallsignMissionResult::InProgress || NewResult == ECallsignMissionResult::InProgress)
	{
		return;
	}
	MissionResult = NewResult;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}

	const TCHAR* Label = (NewResult == ECallsignMissionResult::Victory) ? TEXT("VICTORY") : TEXT("DEFEAT");
	UE_LOG(LogTemp, Display, TEXT("[GameMode] Mission result: %s"), Label);
	CallsignMsg::PushSystem(World, NewResult == ECallsignMissionResult::Victory
		? TEXT("作戦完了。敵を全滅。")
		: TEXT("作戦失敗。撤収できなかった。"));

	OnMissionResultChanged.Broadcast(NewResult);
}

void ACallsignExfilGameMode::HandleAutoAdvance()
{
	if (MissionResult != ECallsignMissionResult::InProgress)
	{
		return;
	}
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
			// ADR-004 §3.2: every outer node is destroyable so OrbitalBarrage
			// has something to bite on. The center stays non-destroyable so
			// the player's spawn cell can't disappear under them.
			if (Node && !(i == 1 && j == 1))
			{
				Node->bDestroyable = true;
			}
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

	// Spawn enemies at multiple corners so support strikes have several
	// targets to pick from and a single PrecisionStrike doesn't end the
	// demo. Corners chosen so the spawn cells are not on the cardinal
	// move corridors (i=0/j=0 etc.) the player walks along.
	ACallsignNode* EnemySpawnNodes[] = {
		Grid[0][0],  // SW corner
		Grid[2][2],  // NE corner
	};
	int32 EnemyCount = 0;
	for (ACallsignNode* SpawnNode : EnemySpawnNodes)
	{
		if (!SpawnNode)
		{
			continue;
		}
		ACallsignRifleEnemy* Enemy = World->SpawnActor<ACallsignRifleEnemy>(
			EnemyClass,
			SpawnNode->GetActorLocation(),
			FRotator::ZeroRotator,
			Params);
		if (!Enemy)
		{
			continue;
		}
		if (Enemy->GetClass()->ImplementsInterface(UCallsignNodeOccupant::StaticClass()))
		{
			ICallsignNodeOccupant::Execute_MoveToNode(Enemy, SpawnNode);
		}
		// Phase 2 demo: rifle + low durability so the demo can observably
		// reach OnWeaponBroken.
		EquipPhase2DemoLoadout(Enemy, /*bIsEnemy=*/true);

		// Phase 3 demo tuning: drop enemy HP so kills are observable in a
		// few turns (1 PrecisionStrike = 40dmg one-shots when MaxHealth=30;
		// normal shooting = 3 hits at 10dmg).
		if (UCallsignHealthComponent* HC = Enemy->FindComponentByClass<UCallsignHealthComponent>())
		{
			HC->MaxHealth = 30;
			HC->CurrentHealth = 30;
		}
		++EnemyCount;
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] Phase1Demo spawned: 9 nodes, %d enemies, player on center"),
		EnemyCount);

	// Phase 2 demo: surface the lifecycle event to the on-screen message log
	// once after both pawns are equipped (per-pawn equip logs stay in UE_LOG only).
	CallsignMsg::PushSystem(World, TEXT("作戦区域に降下。装備を確認してください。"));

	// ADR-004 §3.1 / §11: register the three Phase 3 support defs so the
	// player keys 5/6/7 work without Editor asset authoring.
	if (bAutoInitPhase3SupportDemo)
	{
		InitPhase3SupportDemo();
	}
}

void ACallsignExfilGameMode::InitPhase3SupportDemo()
{
	UWorld* World = GetWorld();
	UCallsignSupportSystem* SupportSys = World ? World->GetSubsystem<UCallsignSupportSystem>() : nullptr;
	if (!SupportSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] Phase3Demo: no UCallsignSupportSystem subsystem"));
		return;
	}

	// PrecisionStrike: short delay, narrow lethal radius, no terrain damage.
	{
		UCallsignSupportDefinition* Def = NewObject<UCallsignSupportDefinition>(this);
		Def->SupportType = ECallsignSupportType::PrecisionStrike;
		Def->DelayTurns = 2;
		Def->RadiusCm = 200.f;
		Def->Damage = 40;
		Def->TerrainDestructionRadiusCm = 0.f;
		Def->bAllowsFriendlyFire = true;
		Def->bDealsDamage = true;
		SupportSys->RegisterDefinition(ECallsignSupportType::PrecisionStrike, Def);
	}

	// SupplyPod: medium delay, no damage. ADR-004 §13 OQ-4: bDealsDamage=
	// false makes it a "wait then nothing visible" demo for the queue/UI
	// path. Phase 4 will hang actual supply effects off the resolved event.
	{
		UCallsignSupportDefinition* Def = NewObject<UCallsignSupportDefinition>(this);
		Def->SupportType = ECallsignSupportType::SupplyPod;
		Def->DelayTurns = 2;
		Def->RadiusCm = 150.f;
		Def->Damage = 0;
		Def->TerrainDestructionRadiusCm = 0.f;
		Def->bAllowsFriendlyFire = true;
		Def->bDealsDamage = false;
		SupportSys->RegisterDefinition(ECallsignSupportType::SupplyPod, Def);
	}

	// OrbitalBarrage: long delay, wide AoE, terrain destruction.
	{
		UCallsignSupportDefinition* Def = NewObject<UCallsignSupportDefinition>(this);
		Def->SupportType = ECallsignSupportType::OrbitalBarrage;
		Def->DelayTurns = 4;
		Def->RadiusCm = 500.f;
		Def->Damage = 20;
		Def->TerrainDestructionRadiusCm = 300.f;
		Def->bAllowsFriendlyFire = true;
		Def->bDealsDamage = true;
		SupportSys->RegisterDefinition(ECallsignSupportType::OrbitalBarrage, Def);
	}

	UE_LOG(LogTemp, Display, TEXT("[GameMode] Phase3Demo: 3 support definitions registered"));
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
		// Dressing voxels are spawned once at BeginPlay and never moved
		// after. Set Mobility back to Static so the engine can fold them
		// into static-rendering paths instead of dynamic-transform ones.
		A->SetMobility(EComponentMobility::Static);
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
		// Sandbag-shaped wall on the west flank: 3 voxels long x 2 deep
		// x 2 tall (12 voxels). Substantial enough to read as a real wall
		// rather than a thin row.
		{ -5, -1, 2 }, { -5, 0, 2 }, { -5, 1, 2 },
		{ -6, -1, 2 }, { -6, 0, 2 }, { -6, 1, 2 },
		// Two outer pillars at opposite floor corners. 2x2 footprint x 4
		// voxels tall (16 voxels each) so they read as solid pillars
		// instead of poles.
		{ -6, -6, 4 }, { -5, -6, 4 }, { -6, -5, 4 }, { -5, -5, 4 },
		{  6,  6, 4 }, {  5,  6, 4 }, {  6,  5, 4 }, {  5,  5, 4 },
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
