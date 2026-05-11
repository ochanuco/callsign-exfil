// Copyright Epic Games, Inc. All Rights Reserved.


#include "CallsignExfilPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "CallsignExfil.h"
#include "CallsignExfilGameMode.h"
#include "Camera/CallsignShoulderCameraComponent.h"
#include "Combat/CallsignCombatResolver.h"
#include "Data/CallsignSupportTypes.h"
#include "Data/CallsignTypes.h"
#include "Data/CallsignWeaponDefinition.h"
#include "Data/CallsignWeaponTypes.h"
#include "Support/CallsignSupportSystem.h"
#include "HUD/CallsignMessageBus.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Pawns/CallsignHealthComponent.h"
#include "Pawns/CallsignTargeting.h"
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

	// Phase 1 demo: turn-based gameplay needs a visible cursor for click-to-move
	// on adjacent nodes. Enable click + mouseover events so we can hit-test the
	// world under the cursor in the LMB handler.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// Mac trackpad / Windows mouse: GameAndUI input mode lets viewport click
	// events flow into our BindKey handler. Without this, the default game-only
	// input mode can silently drop clicks on macOS depending on capture state.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

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
		CallsignMsg::PushPlayer(GetWorld(), TEXT("あなたの番です。"));
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
		// Phase 3 demo (ADR-004 §4.3): support requests on cursor node.
		InputComponent->BindKey(EKeys::Five,  IE_Pressed, this, &ACallsignExfilPlayerController::CsxSupportPrecisionStrike);
		InputComponent->BindKey(EKeys::Six,   IE_Pressed, this, &ACallsignExfilPlayerController::CsxSupportSupplyPod);
		InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ACallsignExfilPlayerController::CsxSupportOrbitalBarrage);
		// WASD: one-tile cardinal move. Coexists with the (intentionally inert)
		// Enhanced Input move action; ACharacter::MaxWalkSpeed=0 keeps the
		// free-movement path a no-op, so only these tile-step handlers act on
		// the keys in practice.
		InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ACallsignExfilPlayerController::CsxMoveNorth);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &ACallsignExfilPlayerController::CsxMoveSouth);
		InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ACallsignExfilPlayerController::CsxMoveEast);
		InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ACallsignExfilPlayerController::CsxMoveWest);
		// R: restart the current PIE level after mission ends.
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACallsignExfilPlayerController::CsxRestart);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this,
			&ACallsignExfilPlayerController::HandleLeftClickToMoveNode);
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

	// Once the mission ends, action inputs (5/6/7, click-to-move, etc.) are
	// no longer meaningful — even though TurnSystem may still report this
	// pawn as the current participant, gating here makes the player wait
	// for [R] restart rather than queue extra strikes after victory.
	if (const ACallsignExfilGameMode* GM = World->GetAuthGameMode<ACallsignExfilGameMode>())
	{
		if (GM->MissionResult != ECallsignMissionResult::InProgress)
		{
			return false;
		}
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
		CallsignMsg::PushPlayer(GetWorld(), TEXT("撃てない。"));
		return false;
	}

	APawn* P = GetPawn();
	if (Target == nullptr || Target == P)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryShootAtActor: invalid target (null or self)"));
		CallsignMsg::PushPlayer(GetWorld(), TEXT("撃てない。"));
		return false;
	}

	if (DefaultWeapon == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryShootAtActor: no DefaultWeapon set"));
		CallsignMsg::PushPlayer(GetWorld(), TEXT("撃てない。"));
		return false;
	}

	UWorld* World = GetWorld();
	UCallsignCombatResolver* Resolver = World ? World->GetSubsystem<UCallsignCombatResolver>() : nullptr;
	if (!Resolver)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC] TryShootAtActor: no CombatResolver subsystem"));
		CallsignMsg::PushPlayer(GetWorld(), TEXT("撃てない。"));
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

	if (Result.bHit)
	{
		CallsignMsg::PushPlayer(GetWorld(), FString::Printf(
			TEXT("敵を撃った。命中、%.0f ダメージ。"), Result.DamageApplied));
	}
	else
	{
		CallsignMsg::PushPlayer(GetWorld(), TEXT("敵を撃った。外れた。"));
	}

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
		CallsignMsg::PushPlayer(GetWorld(), TEXT("リロード不要 / 弾切れ。"));
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("[PC] TryReload: tactical=%d discarded=%d loaded=%d"),
		R.bWasTactical ? 1 : 0, R.DiscardedRounds, R.LoadedRounds);

	if (R.bWasTactical)
	{
		CallsignMsg::PushPlayer(GetWorld(), FString::Printf(
			TEXT("リロード (装填 %d、廃棄 %d)。"), R.LoadedRounds, R.DiscardedRounds));
	}
	else
	{
		CallsignMsg::PushPlayer(GetWorld(), FString::Printf(
			TEXT("リロード (装填 %d)。"), R.LoadedRounds));
	}

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
	CallsignMsg::PushPlayer(World, TEXT("ターン終了。"));
}

void ACallsignExfilPlayerController::CsxShoot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC|cmd] CsxShoot: no world"));
		return;
	}

	APawn* P = GetPawn();
	const FVector MyLocation = P ? P->GetActorLocation() : FVector::ZeroVector;

	AActor* NearestEnemy = CallsignTargeting::FindNearestRifleEnemy(World, MyLocation);
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

ACallsignNode* ACallsignExfilPlayerController::GetNodeUnderCursor() const
{
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Hit))
	{
		return nullptr;
	}
	return Cast<ACallsignNode>(Hit.GetActor());
}

bool ACallsignExfilPlayerController::TryRequestSupport(ECallsignSupportType Type, ACallsignNode* AtNode)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (APawn* MyPawn = GetPawn())
	{
		if (UCallsignHealthComponent* HC = MyPawn->FindComponentByClass<UCallsignHealthComponent>())
		{
			if (HC->bIsDead)
			{
				UE_LOG(LogTemp, Display, TEXT("[PC|Support] reject: pawn is dead"));
				return false;
			}
		}
	}
	if (!IsMyTurn())
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|Support] reject: not my turn"));
		CallsignMsg::PushPlayer(World, TEXT("自分のターンではない。"));
		return false;
	}
	if (!AtNode)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|Support] reject: no AtNode (cursor not over a node)"));
		CallsignMsg::PushPlayer(World, TEXT("対象ノードを指定してください。"));
		return false;
	}
	if (AtNode->bIsDestroyed)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|Support] reject: target node already destroyed"));
		CallsignMsg::PushPlayer(World, TEXT("対象ノードは既に破壊されている。"));
		return false;
	}

	UCallsignSupportSystem* SupportSys = World->GetSubsystem<UCallsignSupportSystem>();
	if (!SupportSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC|Support] no UCallsignSupportSystem subsystem"));
		return false;
	}

	const FGuid Id = SupportSys->RequestSupport(Type, AtNode->GetActorLocation(), GetPawn());
	if (!Id.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PC|Support] RequestSupport returned invalid id (no definition?)"));
		CallsignMsg::PushPlayer(World, TEXT("支援要請に失敗した。"));
		return false;
	}

	const TCHAR* TypeName = TEXT("Unknown");
	switch (Type)
	{
	case ECallsignSupportType::PrecisionStrike: TypeName = TEXT("精密射撃"); break;
	case ECallsignSupportType::SupplyPod:        TypeName = TEXT("補給投下"); break;
	case ECallsignSupportType::OrbitalBarrage:   TypeName = TEXT("軌道砲撃"); break;
	}
	CallsignMsg::PushPlayer(World, FString::Printf(TEXT("%s を要請。"), TypeName));

	// Phase 3 (ADR-004 §4.3): submitting the request consumes the player turn.
	if (UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>())
	{
		TurnSys->EndCurrentTurn();
	}
	return true;
}

void ACallsignExfilPlayerController::CsxSupportPrecisionStrike()
{
	TryRequestSupport(ECallsignSupportType::PrecisionStrike, GetNodeUnderCursor());
}

void ACallsignExfilPlayerController::CsxSupportSupplyPod()
{
	TryRequestSupport(ECallsignSupportType::SupplyPod, GetNodeUnderCursor());
}

void ACallsignExfilPlayerController::CsxSupportOrbitalBarrage()
{
	TryRequestSupport(ECallsignSupportType::OrbitalBarrage, GetNodeUnderCursor());
}

void ACallsignExfilPlayerController::CsxRestart()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Only allow restart after mission is over — otherwise a stray R press
	// during play wipes progress. The HUD banner shows "[R] 再出撃" only
	// in that window so this matches the documented UX.
	const ACallsignExfilGameMode* GM = World->GetAuthGameMode<ACallsignExfilGameMode>();
	if (!GM || GM->MissionResult == ECallsignMissionResult::InProgress)
	{
		UE_LOG(LogTemp, Display, TEXT("[PC|cmd] CsxRestart ignored: mission still in progress"));
		return;
	}
	const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString*/ true);
	UE_LOG(LogTemp, Display, TEXT("[PC|cmd] CsxRestart: reloading level %s"), *CurrentLevel);
	UGameplayStatics::OpenLevel(this, FName(*CurrentLevel));
}

static void GetCameraGroundBasis(const APlayerController* PC, FVector& OutForward, FVector& OutRight)
{
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	FVector Fwd = CamRot.Vector();
	Fwd.Z = 0.f;
	// Camera looking straight down would zero out the ground projection;
	// fall back to world +X so the player still gets a sensible step.
	if (!Fwd.Normalize(0.001f))
	{
		Fwd = FVector::ForwardVector;
	}
	OutForward = Fwd;
	OutRight = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
}

void ACallsignExfilPlayerController::CsxMoveNorth()
{
	FVector Fwd, Right; GetCameraGroundBasis(this, Fwd, Right);
	TryMoveCardinal(Fwd);
}
void ACallsignExfilPlayerController::CsxMoveSouth()
{
	FVector Fwd, Right; GetCameraGroundBasis(this, Fwd, Right);
	TryMoveCardinal(-Fwd);
}
void ACallsignExfilPlayerController::CsxMoveEast()
{
	FVector Fwd, Right; GetCameraGroundBasis(this, Fwd, Right);
	TryMoveCardinal(Right);
}
void ACallsignExfilPlayerController::CsxMoveWest()
{
	FVector Fwd, Right; GetCameraGroundBasis(this, Fwd, Right);
	TryMoveCardinal(-Right);
}

bool ACallsignExfilPlayerController::TryMoveCardinal(const FVector& WorldDir)
{
	if (!IsMyTurn())
	{
		return false;
	}
	APawn* P = GetPawn();
	if (!P)
	{
		return false;
	}

	// Resolve player's current node through the NodeOccupant interface so we
	// don't hard-depend on ACallsignExfilCharacter's concrete type.
	ACallsignNode* CurrentNode = nullptr;
	if (P->GetClass()->ImplementsInterface(UCallsignNodeOccupant::StaticClass()))
	{
		CurrentNode = ICallsignNodeOccupant::Execute_GetCurrentNode(P);
	}
	if (!CurrentNode)
	{
		return false;
	}

	// Pick the adjacent node that most aligns with WorldDir. >0.5 dot ≈ within
	// 60° of the requested direction; lets diagonally-placed neighbors still
	// answer cardinal keys gracefully without snapping to an off-axis pick.
	ACallsignNode* Best = nullptr;
	float BestDot = 0.5f;
	const FVector Here = CurrentNode->GetActorLocation();
	for (const TObjectPtr<ACallsignNode>& Adj : CurrentNode->Adjacent)
	{
		if (!Adj || Adj->bIsDestroyed)
		{
			continue;
		}
		// Filter on movability so a blocked nearest-direction neighbor doesn't
		// preempt other valid candidates and turn the keypress into a no-op.
		if (!CanMoveToNode(Adj))
		{
			continue;
		}
		const FVector ToAdj = (Adj->GetActorLocation() - Here).GetSafeNormal();
		const float Dot = FVector::DotProduct(ToAdj, WorldDir);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = Adj;
		}
	}
	if (!Best)
	{
		return false;
	}
	return TryMoveToNode(Best);
}

void ACallsignExfilPlayerController::HandleLeftClickToMoveNode()
{
	// Diagnostic log: confirms the LMB binding fired regardless of trace result.
	// Useful for verifying Mac trackpad clicks reach the controller. Kept at
	// Verbose so shipping / long-session sessions don't get spammed; the
	// rejection / accepted-decision logs below stay at Display.
	UE_LOG(LogTemp, Verbose, TEXT("[PC] LMB pressed"));

	ACallsignNode* Hovered = GetNodeUnderCursor();
	if (!Hovered)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[PC] click: no node under cursor"));
		// Clicks on empty world or non-node actors are ignored (no-op).
		return;
	}
	if (!CanMoveToNode(Hovered))
	{
		UE_LOG(LogTemp, Display, TEXT("[PC] click rejected on %s (not adjacent / occupied / not my turn)"),
			*GetNameSafe(Hovered));
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("[PC] click → TryMoveToNode %s"), *GetNameSafe(Hovered));
	TryMoveToNode(Hovered);
}
