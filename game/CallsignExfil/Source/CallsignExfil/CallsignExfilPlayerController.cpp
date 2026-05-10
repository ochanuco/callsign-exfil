// Copyright Epic Games, Inc. All Rights Reserved.


#include "CallsignExfilPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "CallsignExfil.h"
#include "Widgets/Input/SVirtualJoystick.h"

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

	CurrentMode = NewMode;

	// TODO Phase 1 impl: actually swap input mapping contexts. We sketch the
	// hookup below so the references are not stripped, but the full add/remove
	// flow lands when input actions per mode are defined.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (NodeSelectIMC && AimIMC)
		{
			// Placeholder: in Phase 1 follow-up, remove the inactive IMC and add
			// the active one with a stable priority. Swap depends on which
			// mappings exist for each mode.
			(void)Subsystem;
		}
	}

	OnControllerModeChanged.Broadcast(NewMode);
}
