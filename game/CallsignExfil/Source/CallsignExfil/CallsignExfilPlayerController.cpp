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
#include "Node/CallsignNode.h"
#include "Node/CallsignNodeOccupant.h"
#include "Turn/CallsignTurnParticipant.h"
#include "Turn/CallsignTurnSystem.h"
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

	const FCallsignShotResult Result = Resolver->ResolveShot(Request);

	UE_LOG(LogTemp, Display, TEXT("[PC] TryShootAtActor -> hit=%d damage=%.2f"),
		Result.bHit ? 1 : 0, Result.DamageApplied);

	// Action consumed: end the turn whether hit or miss.
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
