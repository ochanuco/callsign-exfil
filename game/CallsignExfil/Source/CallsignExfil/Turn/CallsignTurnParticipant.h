// Copyright (c) ochanuco 2026. Licensed under MIT.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CallsignTurnParticipant.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UCallsignTurnParticipant : public UInterface
{
        GENERATED_BODY()
};

/**
 *  Implemented by actors that take part in the turn loop.
 *  BlueprintNativeEvent so subclasses (C++ or BP) can override behaviour
 *  without losing the default implementation.
 */
class ICallsignTurnParticipant
{
        GENERATED_BODY()

public:

        /** Called when this participant's turn becomes active. */
        UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Callsign|Turn")
        void BeginTurn();

        /** Returns true once the participant has finished its turn. */
        UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Callsign|Turn")
        bool IsTurnFinished() const;
};
