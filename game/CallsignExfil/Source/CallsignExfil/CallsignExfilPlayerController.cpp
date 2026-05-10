// Copyright Epic Games, Inc. All Rights Reserved.


#include "CallsignExfilPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "CallsignExfil.h"
#include "Camera/CallsignShoulderCameraComponent.h"
#include "Combat/CallsignCombatResolver.h"
#include "Data/CallsignTypes.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Data/CallsignWeaponTypes.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Pawns/CallsignRifleEnemy.h"
#include "Turn/CallsignTurnParticipant.h"
#include "Turn/CallsignTurnSystem.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
#include "Widgets/Input/SVirtualJoystick.h"

ACallsignExfilPlayerController::ACallsignExfilPlayerController()
{
	CurrentMode = ECallsignControllerMode::Idle;
}

void ACallsignExfilPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogCallsignExfil, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	// Subscribe to the world turn system so we can drive camera/IMC mode
	// automatically on player turn boundaries.
	if (UWorld* World = GetWorld())
	{
		if (UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>())
		{
			TurnSys->OnTurnBegin.AddDynamic(this, &ACallsignExfilPlayerController::HandleTurnBegin);
			TurnSys->OnTurnEnd.AddDynamic(this, &ACallsignExfilPlayerController::HandleTurnEnd);
			UE_LOG(LogTemp, Display, TEXT("[PC] BeginPlay: subscribed to TurnSystem events"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[PC] BeginPlay: no TurnSystem subsystem to subscribe to"));
		}
	}
}

void ACallsignExfilPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
	{
		if (UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>())
		{
			TurnSys->OnTurnBegin.RemoveDynamic(this, &ACallsignExfilPlayerController::HandleTurnBegin);
			TurnSys->OnTurnEnd.RemoveDynamic(this, &ACallsignExfilPlayerController::HandleTurnEnd);
		}
	}

	Super::EndPlay(Reason);
}

void ACallsignExfilPlayerController::HandleTurnBegin(AActor* Who)
{
	UE_LOG(LogTemp, Display, TEXT("[PC] HandleTurnBegin: %s"), *GetNameSafe(Who));

	if (Who != nullptr && Who == GetPawn())
	{
		SetMode(ECallsignControllerMode::NodeSelect);
	}
}

void ACallsignExfilPlayerController::HandleTurnEnd(AActor* Who)
{
	UE_LOG(LogTemp, Display, TEXT("[PC] HandleTurnEnd: %s"), *GetNameSafe(Who));

	if (Who != nullptr && Who == GetPawn())
	{
		SetMode(ECallsignControllerMode::Idle);
	}
}

void ACallsignExfilPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Direct number-key bindings for the Phase 2 demo (Mac JIS-friendly: no modifiers).
	// Coexists with Enhanced Input — these run via the legacy InputComponent path.
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::One,   IE_Pressed, this, &ACallsignExfilPlayerController::CsxStatus);
		InputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &ACallsignExfilPlayerController::CsxShoot);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACallsignExfilPlayerController::CsxReload);
		InputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &ACallsignExfilPlayerController::CsxEndTurn);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ACallsignExfilPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ACallsignExfilPlayerController::SetMode(ECallsignControllerMode NewMode)
{
	if (NewMode == CurrentMode)
	{
		return;
	}

	// Resolve IMCs for old vs new mode. Idle has no IMC.
	auto ResolveIMC = [this](ECallsignControllerMode Mode) -> UInputMappingContext*
	{
		switch (Mode)
		{
		case ECallsignControllerMode::NodeSelect: return NodeSelectIMC;
		case ECallsignControllerMode::Aim:        return AimIMC;
		case ECallsignControllerMode::Idle:
		default:                                  return nullptr;
		}
	};

	UInputMappingContext* OldIMC = ResolveIMC(CurrentMode);
	UInputMappingContext* NewIMC = ResolveIMC(NewMode);

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (OldIMC)
			{
				Subsystem->RemoveMappingContext(OldIMC);
			}
			if (NewIMC)
			{
				Subsystem->AddMappingContext(NewIMC, /*priority*/ 0);
			}
		}
	}

	CurrentMode = NewMode;

	// Notify the shoulder camera component on the possessed pawn (if any).
	if (APawn* P = GetPawn())
	{
		if (UCallsignShoulderCameraComponent* Cam = P->FindComponentByClass<UCallsignShoulderCameraComponent>())
		{
			ECallsignCameraMode CamMode = ECallsignCameraMode::Idle;
			switch (NewMode)
			{
			case ECallsignControllerMode::NodeSelect: CamMode = ECallsignCameraMode::NodeSelect; break;
			case ECallsignControllerMode::Aim:        CamMode = ECallsignCameraMode::Aim;        break;
			default:                                  CamMode = ECallsignCameraMode::Idle;       break;
			}
			Cam->SetCameraMode(CamMode);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[PC] SetMode -> %d"), (int32)NewMode);

	OnControllerModeChanged.Broadcast(NewMode);
}

bool ACallsignExfilPlayerController::IsMyTurn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>();
	if (!TurnSys)
	{
		return false;
	}

	return TurnSys->GetCurrentParticipant() == GetPawn();
}

bool ACallsignExfilPlayerController::CanMoveToNode(const ACallsignNode* Target) const
{
	if (!IsMyTurn())
	{
		return false;
	}

	if (Target == nullptr)
	{
		return false;
	}

	APawn* P = GetPawn();
	if (!P)
	{
		return false;
	}

	// Pawn must implement ICallsignNodeOccupant; route through Execute_ for BP overrides.
	if (!P->GetClass()->ImplementsInterface(UCallsignNodeOccupant::StaticClass()))
	{
		return false;
	}

	ACallsignNode* CurrentNode = ICallsignNodeOccupant::Execute_GetCurrentNode(P);
	if (CurrentNode == nullptr)
	{
		return false;
	}

	if (CurrentNode == Target)
	{
		return false;
	}

	return CurrentNode->IsAdjacent(Target) && !Target->IsOccupied();
}

bool ACallsignExfilPlayerController::TryMoveToNode(ACallsignNode* Target)
{
	if (!CanMoveToNode(Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryMoveToNode rejected (not my turn / null target / not adjacent / occupied)"));
		return false;
	}

	APawn* P = GetPawn();
	// CanMoveToNode already validated the interface and pawn liveness.
	ICallsignNodeOccupant::Execute_MoveToNode(P, Target);

	UE_LOG(LogTemp, Display, TEXT("[PC] TryMoveToNode -> %s"), *GetNameSafe(Target));

	// Phase 1 simplification: any move ends the player turn.
	EndTurn();

	return true;
}

bool ACallsignExfilPlayerController::TryShootAtActor(AActor* Target)
{
	if (!IsMyTurn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryShootAtActor: not my turn"));
		return false;
	}

	APawn* P = GetPawn();
	if (Target == nullptr || Target == P)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryShootAtActor: invalid target (null or self)"));
		return false;
	}

	if (DefaultWeapon == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryShootAtActor: no DefaultWeapon set"));
		return false;
	}

	UWorld* World = GetWorld();
	UCallsignCombatResolver* Resolver = World ? World->GetSubsystem<UCallsignCombatResolver>() : nullptr;
	if (!Resolver)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryShootAtActor: no CombatResolver subsystem"));
		return false;
	}

	FCallsignShotRequest Request;
	Request.Instigator = P;
	Request.From = P ? P->GetActorLocation() : FVector::ZeroVector;
	Request.To = Target->GetActorLocation();
	Request.Weapon = DefaultWeapon;
	// Issue #22: target-aware LoS - resolver ignores both Instigator and
	// TargetActor when tracing, so a clear shot to Target isn't blocked
	// by Target's own capsule.
	Request.TargetActor = Target;

	// Phase 2 (ADR-003 §4.3): route through pawn's Inventory so the resolver
	// consumes ammo/durability via the Phase 2 path. Phase 1 callers that need
	// the legacy path can leave Request.Inventory null themselves.
	if (P)
	{
		if (UCallsignInventoryComponent* Inv = P->FindComponentByClass<UCallsignInventoryComponent>())
		{
			Request.Inventory = Inv;
		}
	}

	const FCallsignShotResult Result = Resolver->ResolveShot(Request);

	UE_LOG(LogTemp, Display, TEXT("[PC] TryShootAtActor -> hit=%d damage=%.2f"),
		Result.bHit ? 1 : 0, Result.DamageApplied);

	// Action consumed: end the turn whether hit or miss.
	EndTurn();

	return true;
}

bool ACallsignExfilPlayerController::TryReload()
{
	if (!IsMyTurn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryReload rejected (not my turn)"));
		return false;
	}

	APawn* P = GetPawn();
	if (!P)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryReload: no possessed pawn"));
		return false;
	}

	UCallsignInventoryComponent* Inv = P->FindComponentByClass<UCallsignInventoryComponent>();
	if (!Inv)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryReload: no Inventory on pawn"));
		return false;
	}

	const FCallsignReloadResult R = Inv->Reload();
	if (!R.bReloaded)
	{
		// ADR-003 §7: rescue / empty pool / full magazine -> no-op, no turn cost.
		UE_LOG(LogTemp, Display, TEXT("[PC] TryReload: no-op (rescue/empty pool/full mag)"));
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("[PC] TryReload: tactical=%d discarded=%d loaded=%d"),
		R.bWasTactical ? 1 : 0, R.DiscardedRounds, R.LoadedRounds);

	EndTurn();
	return true;
}

void ACallsignExfilPlayerController::EndTurn()
{
	if (!IsMyTurn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] EndTurn called outside player turn"));
		return;
	}

	UWorld* World = GetWorld();
	UCallsignTurnSystem* TurnSys = World ? World->GetSubsystem<UCallsignTurnSystem>() : nullptr;
	if (!TurnSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] EndTurn: no TurnSystem subsystem"));
		return;
	}

	TurnSys->EndCurrentTurn();
	UE_LOG(LogTemp, Display, TEXT("[PC] EndTurn"));
}

void ACallsignExfilPlayerController::CsxShoot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC|cmd] CsxShoot: no world"));
		return;
	}

	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(World, ACallsignRifleEnemy::StaticClass(), FoundEnemies);

	if (FoundEnemies.Num() == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] CsxShoot: no enemy in world"));
		return;
	}

	APawn* P = GetPawn();
	const FVector MyLocation = P ? P->GetActorLocation() : FVector::ZeroVector;

	AActor* NearestEnemy = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	for (AActor* Enemy : FoundEnemies)
	{
		if (!Enemy)
		{
			continue;
		}
		const float DistSq = (MyLocation - Enemy->GetActorLocation()).SizeSquared();
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestEnemy = Enemy;
		}
	}

	if (!NearestEnemy)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] CsxShoot: no enemy in world"));
		return;
	}

	const bool bResult = TryShootAtActor(NearestEnemy);
	UE_LOG(LogTemp, Display, TEXT("[PC|cmd] CsxShoot -> %s (returned=%d)"),
		*GetNameSafe(NearestEnemy), bResult ? 1 : 0);
}

void ACallsignExfilPlayerController::CsxReload()
{
	const bool bResult = TryReload();
	UE_LOG(LogTemp, Display, TEXT("[PC|cmd] CsxReload -> %d"), bResult ? 1 : 0);
}

void ACallsignExfilPlayerController::CsxEndTurn()
{
	EndTurn();
	UE_LOG(LogTemp, Display, TEXT("[PC|cmd] CsxEndTurn invoked"));
}

void ACallsignExfilPlayerController::CsxStatus()
{
	UE_LOG(LogTemp, Display, TEXT("[PC|cmd] === Status ==="));
	UE_LOG(LogTemp, Display, TEXT("[PC|cmd] IsMyTurn = %d"), IsMyTurn() ? 1 : 0);

	APawn* P = GetPawn();
	if (!P)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Weapon = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Magazine = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Durability = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] IsBroken = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Ammo = <unavailable>"));
		return;
	}

	UCallsignInventoryComponent* Inv = P->FindComponentByClass<UCallsignInventoryComponent>();
	if (!Inv)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Weapon = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Magazine = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Durability = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] IsBroken = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Ammo = <unavailable>"));
		return;
	}

	UCallsignWeaponInstanceObject* Weapon = Inv->GetCurrentWeapon();
	if (!Weapon)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Weapon = null"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Magazine = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Durability = <unavailable>"));
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] IsBroken = <unavailable>"));
	}
	else
	{
		UCallsignWeaponDefinition* Def = Weapon->GetWeaponDefinition();
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Weapon = %s"), *GetNameSafe(Weapon));
		if (Def)
		{
			UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Magazine = %d / %d"),
				Weapon->GetMagazineCurrent(), Def->MagazineSize);
			UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Durability = %d / %d"),
				Weapon->GetDurabilityCurrent(), Def->DurabilityMax);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Magazine = %d / <unavailable>"),
				Weapon->GetMagazineCurrent());
			UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Durability = %d / <unavailable>"),
				Weapon->GetDurabilityCurrent());
		}
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] IsBroken = %d"), Weapon->IsBroken() ? 1 : 0);
	}

	UE_LOG(LogTemp, Display, TEXT("[PC|cmd] Ammo Light = %d, Shell = %d, Heavy = %d"),
		Inv->GetAmmoCount(ECallsignAmmoType::Light),
		Inv->GetAmmoCount(ECallsignAmmoType::Shell),
		Inv->GetAmmoCount(ECallsignAmmoType::Heavy));
}
