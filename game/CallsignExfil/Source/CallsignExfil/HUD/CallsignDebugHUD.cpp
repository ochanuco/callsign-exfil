// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignDebugHUD.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Turn/CallsignTurnSystem.h"
#include "LineOfSight/CallsignLineOfSightService.h"
#include "Data/CallsignTypes.h"

void ACallsignDebugHUD::BeginPlay()
{
        Super::BeginPlay();
        UE_LOG(LogTemp, Display, TEXT("[HUD] Initialized"));
}

void ACallsignDebugHUD::DrawHUD()
{
        Super::DrawHUD();

        if (!bShowTurnInfo && !bShowLoSPreview)
        {
                return;
        }

        UWorld* World = GetWorld();
        if (!World)
        {
                return;
        }

        // ---- 2a. Turn info block ----
        if (bShowTurnInfo)
        {
                UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>();
                if (TurnSys)
                {
                        const ECallsignTurnPhase Phase = TurnSys->GetCurrentPhase();
                        FString PhaseStr;
                        switch (static_cast<int32>(Phase))
                        {
                        case static_cast<int32>(ECallsignTurnPhase::Player):
                                PhaseStr = TEXT("Player");
                                break;
                        case static_cast<int32>(ECallsignTurnPhase::Enemies):
                                PhaseStr = TEXT("Enemies");
                                break;
                        case static_cast<int32>(ECallsignTurnPhase::Resolving):
                                PhaseStr = TEXT("Resolving");
                                break;
                        default:
                                PhaseStr = TEXT("Unknown");
                                break;
                        }

                        const FString CurrentName = GetNameSafe(TurnSys->GetCurrentParticipant());
                        const FString InfoString = FString::Printf(TEXT("Phase=%s Current=%s"), *PhaseStr, *CurrentName);
                        DrawText(InfoString, FLinearColor::White, 30.f, 30.f, /*Font*/ nullptr, /*Scale*/ 1.2f, false);
                }
                else
                {
                        DrawText(TEXT("[Turn] subsystem not available"), FLinearColor::White, 30.f, 30.f, /*Font*/ nullptr, /*Scale*/ 1.2f, false);
                }
        }

        // ---- 2b. LoS preview block ----
        if (!bShowLoSPreview)
        {
                return;
        }

        APlayerController* PC = GetOwningPlayerController();
        if (!PC)
        {
                return;
        }

        APawn* P = PC->GetPawn();
        if (!P)
        {
                return;
        }

        UCallsignLineOfSightService* Los = World->GetSubsystem<UCallsignLineOfSightService>();
        if (!Los)
        {
                return;
        }

        FVector CamLoc;
        FRotator CamRot;
        PC->GetPlayerViewPoint(CamLoc, CamRot);
        const FVector From = P->GetActorLocation();
        const FVector To = CamLoc + CamRot.Vector() * 5000.f;

        TArray<AActor*> Ignore = { P };
        const FCallsignLineOfSightResult Result = Los->Query(From, To, Ignore);

        // Project world-space endpoints to screen space. AHUD::Project returns
        // FVector where X/Y are screen coords (Z is depth, ignored here).
        const FVector FromScreen = Project(From);
        // TODO Phase 2: use actual hit point if blocked (Result.BlockingActor / hit location)
        const FVector ToScreen = Project(To);

        const FLinearColor Color = Result.bHasLineOfSight ? LosOkColor : LosBlockedColor;
        DrawLine(FromScreen.X, FromScreen.Y, ToScreen.X, ToScreen.Y, Color, 2.0f);
}
