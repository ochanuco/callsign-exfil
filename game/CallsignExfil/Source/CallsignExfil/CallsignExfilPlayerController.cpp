// Copyright Epic Games, Inc. All Rights Reserved.


#include "CallsignExfilPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "CallsignExfil.h"
#include "Camera/CallsignShoulderCameraComponent.h"
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
