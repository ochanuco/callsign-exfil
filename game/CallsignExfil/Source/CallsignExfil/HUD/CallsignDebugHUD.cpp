// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignDebugHUD.h"

#include "CallsignExfilPlayerController.h"
#include "CallsignMessageBus.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Node/CallsignNode.h"
#include "Pawns/CallsignRifleEnemy.h"
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

        // Debug-only overlay: skip outside PIE so it cannot leak into Standalone or Shipping.
        UWorld* World = GetWorld();
        if (!World || World->WorldType != EWorldType::PIE)
        {
                return;
        }

        if (!bShowTurnInfo && !bShowLoSPreview && !bShowMessageLog && !bShowKeyHelp && !bShowTargetingPreview)
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
                        DrawText(InfoString, FLinearColor::White, 30.f, 30.f, /*Font*/ nullptr, /*Scale*/ 2.4f, false);
                }
                else
                {
                        DrawText(TEXT("[Turn] subsystem not available"), FLinearColor::White, 30.f, 30.f, /*Font*/ nullptr, /*Scale*/ 2.4f, false);
                }
        }

        // ---- 2b. LoS preview block ----
        if (bShowLoSPreview)
        {
                APlayerController* PC = GetOwningPlayerController();
                APawn* P = PC ? PC->GetPawn() : nullptr;
                UCallsignLineOfSightService* Los = World->GetSubsystem<UCallsignLineOfSightService>();

                if (PC && P && Los)
                {
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
        }

        // ---- 2b'. Targeting preview ----
        // During the player's turn, draw an amber line from the player to the
        // nearest enemy (the actor that pressing `2` would shoot) plus a ring at
        // the enemy's feet so the player can see who is being targeted.
        if (bShowTargetingPreview)
        {
                UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>();
                APlayerController* PCT = GetOwningPlayerController();
                APawn* PlayerP = PCT ? PCT->GetPawn() : nullptr;
                if (TurnSys && PlayerP && TurnSys->GetCurrentParticipant() == PlayerP)
                {
                        TArray<AActor*> Enemies;
                        UGameplayStatics::GetAllActorsOfClass(World, ACallsignRifleEnemy::StaticClass(), Enemies);
                        AActor* Nearest = nullptr;
                        float NearestDistSq = TNumericLimits<float>::Max();
                        const FVector PLoc = PlayerP->GetActorLocation();
                        for (AActor* E : Enemies)
                        {
                                if (!E)
                                {
                                        continue;
                                }
                                const float DSq = FVector::DistSquared(PLoc, E->GetActorLocation());
                                if (DSq < NearestDistSq)
                                {
                                        NearestDistSq = DSq;
                                        Nearest = E;
                                }
                        }
                        if (Nearest)
                        {
                                const FColor TargetColor(255, 220, 80); // amber
                                const FVector ELoc = Nearest->GetActorLocation();
                                // Per-frame redraw via tiny lifetime so it appears continuously.
                                DrawDebugLine(World, PLoc, ELoc, TargetColor,
                                        /*bPersistent*/ false, /*Lifetime*/ 0.05f,
                                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 2.5f);
                                // Ring around the target's feet (capsule half-height ~88).
                                const FVector RingCenter = ELoc + FVector(0.f, 0.f, -88.f);
                                DrawDebugCircle(World, RingCenter, /*Radius*/ 70.f, /*Segments*/ 24,
                                        TargetColor, /*bPersistent*/ false, /*Lifetime*/ 0.05f,
                                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 2.5f,
                                        /*YAxis*/ FVector(0.f, 1.f, 0.f), /*XAxis*/ FVector(1.f, 0.f, 0.f),
                                        /*bDrawAxis*/ false);
                        }

                        // Hovered-adjacent-node highlight: when the cursor is over an
                        // adjacent unoccupied node, ring it green to indicate clickability.
                        if (ACallsignExfilPlayerController* CallsignPC = Cast<ACallsignExfilPlayerController>(PCT))
                        {
                                if (ACallsignNode* Hovered = CallsignPC->GetNodeUnderCursor())
                                {
                                        const bool bClickable = CallsignPC->CanMoveToNode(Hovered);
                                        const FColor HoverColor = bClickable ? FColor(80, 240, 120) : FColor(220, 80, 80);
                                        const FVector NodeLoc = Hovered->GetActorLocation();
                                        DrawDebugCircle(World, NodeLoc, /*Radius*/ 110.f, /*Segments*/ 32,
                                                HoverColor, /*bPersistent*/ false, /*Lifetime*/ 0.05f,
                                                /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 4.f,
                                                /*YAxis*/ FVector(0.f, 1.f, 0.f), /*XAxis*/ FVector(1.f, 0.f, 0.f),
                                                /*bDrawAxis*/ false);
                                }
                        }
                }
        }

        // ---- 2c. Narrative message log (bottom-right) ----
        // Roguelike-style scrolling overlay. Renders newest at the bottom,
        // older entries stacked above. Each line fades over the last 1.5s of
        // its lifetime. Phase 4+ will replace this with a UMG widget.
        if (bShowMessageLog && Canvas)
        {
                if (UCallsignMessageBus* Bus = World->GetSubsystem<UCallsignMessageBus>())
                {
                        const TArray<FCallsignMessage> Active = Bus->GetActiveMessages();
                        if (Active.Num() > 0)
                        {
                                const float Now = World->GetTimeSeconds();
                                const float TextScale = 2.0f;
                                const float PanelWidth = 900.f;          // wider to fit larger glyphs
                                const float LineStep = 44.f;             // ~22 * TextScale
                                const float BottomMargin = 60.f;
                                const float FadeWindow = 1.5f;
                                const float PanelPadding = 12.f;
                                const float TextLeftInset = 16.f;
                                const float SlideDuration = 0.25f;       // scroll-in animation length
                                const FLinearColor PanelBgColor(0.f, 0.f, 0.f, 0.65f);    // semi-transparent black
                                const FLinearColor PanelBorderColor(0.4f, 0.4f, 0.4f, 0.8f); // subtle gray frame

                                // Newest entries are at the END of the array. Render newest
                                // at the bottom and walk older entries upward.
                                const float ClipX = Canvas->ClipX;
                                const float ClipY = Canvas->ClipY;
                                const float X = ClipX - PanelWidth;
                                const int32 Count = Active.Num();

                                // Slide-in animation: when a new message was just pushed, all
                                // visible messages start one slot lower (Y + LineStep) and ease
                                // up to their settled positions over SlideDuration. The newest
                                // message also fades in. Combined effect: scrollbar-like "にゅっと
                                // 入る" feel without per-message animation state.
                                const float LastPushAt = Bus->GetLastPushAt();
                                const float TimeSincePush = (LastPushAt < 0.f) ? SlideDuration : (Now - LastPushAt);
                                const float SlideProgress = FMath::Clamp(TimeSincePush / SlideDuration, 0.f, 1.f);
                                // SmoothStep ease so the scroll feels less mechanical.
                                const float EasedProgress = FMath::SmoothStep(0.f, 1.f, SlideProgress);
                                const float SlideYOffset = (1.f - EasedProgress) * LineStep;

                                // Background panel: sized to the settled message stack. During
                                // animation the new (bottom) message draws below the panel — its
                                // own fade-in alpha keeps that brief overshoot subtle.
                                const float PanelHeight = (Count * LineStep) + (2.f * PanelPadding);
                                const float PanelX = X - TextLeftInset - PanelPadding;
                                const float PanelY = ClipY - BottomMargin - ((Count - 1) * LineStep) - PanelPadding;
                                const float PanelW = PanelWidth + (2.f * PanelPadding);
                                DrawRect(PanelBgColor, PanelX, PanelY, PanelW, PanelHeight);
                                // Top + bottom border lines for a "window" feel.
                                DrawLine(PanelX, PanelY, PanelX + PanelW, PanelY, PanelBorderColor, 1.5f);
                                DrawLine(PanelX, PanelY + PanelHeight, PanelX + PanelW, PanelY + PanelHeight, PanelBorderColor, 1.5f);

                                for (int32 i = 0; i < Count; ++i)
                                {
                                        const FCallsignMessage& Msg = Active[Count - 1 - i];
                                        const float SettledY = ClipY - BottomMargin - (i * LineStep);
                                        const float Y = SettledY + SlideYOffset; // lower during animation

                                        // Stop when we've walked off the top of the viewport.
                                        if (Y < 0.f)
                                        {
                                                break;
                                        }

                                        // Linear alpha fade across the last FadeWindow seconds of lifetime.
                                        // Lifetime <= 0 means "forever" -> no fade.
                                        float Alpha = 1.f;
                                        if (Msg.Lifetime > 0.f)
                                        {
                                                const float Age = Now - Msg.SpawnedAt;
                                                const float Remaining = Msg.Lifetime - Age;
                                                if (Remaining < FadeWindow)
                                                {
                                                        Alpha = FMath::Clamp(Remaining / FadeWindow, 0.f, 1.f);
                                                }
                                        }

                                        // Newest message also fades in over the slide duration.
                                        if (i == 0 && SlideProgress < 1.f)
                                        {
                                                Alpha *= EasedProgress;
                                        }

                                        FLinearColor LineColor = Msg.Color;
                                        LineColor.A *= Alpha;

                                        DrawText(Msg.Text, LineColor, X, Y, /*Font*/ nullptr, TextScale, false);
                                }
                        }
                }
        }

        // ---- 2d. Key help panel (top-right) ----
        // Static reference of the demo key bindings so the player doesn't have to remember
        // what 1/2/3/4 do. Phase 4+ replaces this with a UMG widget; for now we draw a
        // simple framed panel with the same style as the message log.
        if (bShowKeyHelp && Canvas)
        {
                struct FKeyHint
                {
                        const TCHAR* Key;
                        const TCHAR* Label;
                };
                static const FKeyHint Hints[] = {
                        { TEXT("[1]"), TEXT("ステータス") },
                        { TEXT("[2]"), TEXT("射撃") },
                        { TEXT("[3]"), TEXT("リロード") },
                        { TEXT("[4]"), TEXT("ターン終了") },
                        { TEXT("[LMB]"), TEXT("隣接ノードへ移動") },
                };
                static constexpr int32 HintCount = sizeof(Hints) / sizeof(Hints[0]);

                const float TextScale = 2.0f;
                const float LineStep = 44.f;
                const float PanelWidth = 440.f;
                const float TopMargin = 30.f;
                const float RightMargin = 30.f;
                const float PanelPadding = 12.f;
                const float TextLeftInset = 16.f;
                const FLinearColor PanelBgColor(0.f, 0.f, 0.f, 0.65f);
                const FLinearColor PanelBorderColor(0.4f, 0.4f, 0.4f, 0.8f);
                const FLinearColor KeyColor(1.0f, 0.85f, 0.3f, 1.0f);   // amber for the key bracket
                const FLinearColor LabelColor(1.0f, 1.0f, 1.0f, 1.0f);  // white for the action label

                const float ClipX = Canvas->ClipX;
                const float PanelX = ClipX - RightMargin - PanelWidth - (2.f * PanelPadding);
                const float PanelY = TopMargin;
                const float PanelW = PanelWidth + (2.f * PanelPadding);
                const float PanelH = (HintCount * LineStep) + (2.f * PanelPadding);
                DrawRect(PanelBgColor, PanelX, PanelY, PanelW, PanelH);
                DrawLine(PanelX, PanelY, PanelX + PanelW, PanelY, PanelBorderColor, 1.5f);
                DrawLine(PanelX, PanelY + PanelH, PanelX + PanelW, PanelY + PanelH, PanelBorderColor, 1.5f);

                const float TextX = PanelX + PanelPadding + TextLeftInset;
                for (int32 i = 0; i < HintCount; ++i)
                {
                        const float Y = PanelY + PanelPadding + (i * LineStep);
                        DrawText(Hints[i].Key, KeyColor, TextX, Y, /*Font*/ nullptr, TextScale, false);
                        // Offset the label past the key bracket so they line up in two columns.
                        DrawText(Hints[i].Label, LabelColor, TextX + 110.f, Y, /*Font*/ nullptr, TextScale, false);
                }
        }
}
