// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignDebugHUD.h"

#include "CallsignExfilGameMode.h"
#include "CallsignExfilPlayerController.h"
#include "CallsignMessageBus.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/CallsignInventoryComponent.h"
#include "Pawns/CallsignHealthComponent.h"
#include "Pawns/CallsignRifleEnemy.h"
#include "Pawns/CallsignTargeting.h"
#include "Node/CallsignNode.h"
#include "Weapon/CallsignWeaponInstanceObject.h"
#include "Data/CallsignWeaponDefinition.h"
#include "EngineUtils.h"
#include "Turn/CallsignTurnSystem.h"
#include "LineOfSight/CallsignLineOfSightService.h"
#include "Data/CallsignSupportTypes.h"
#include "Data/CallsignTypes.h"
#include "Support/CallsignSupportSystem.h"

void ACallsignDebugHUD::BeginPlay()
{
        Super::BeginPlay();
        UE_LOG(LogTemp, Display, TEXT("[HUD] Initialized"));
}

void ACallsignDebugHUD::HandleHealthChanged(UCallsignHealthComponent* Source, int32 Delta, AActor* /*Causer*/)
{
        // Drop events while popups are disabled so FloatingNumbers can't grow
        // unboundedly mid-session and dump a backlog when re-enabled.
        if (!bShowDamagePopups || !Source || Delta == 0)
        {
                return;
        }
        AActor* Owner = Source->GetOwner();
        UWorld* World = GetWorld();
        if (!Owner || !World)
        {
                return;
        }
        FFloatingNumber Entry;
        Entry.WorldLoc = Owner->GetActorLocation();
        Entry.SignedAmount = Delta;
        Entry.SpawnedAt = World->GetTimeSeconds();
        FloatingNumbers.Add(Entry);
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

        if (!bShowTurnInfo && !bShowLoSPreview && !bShowMessageLog && !bShowKeyHelp
                && !bShowTargetingPreview && !bShowSupportPreview && !bShowHealthOverlay
                && !bShowMissionBanner && !bShowDamagePopups && !bShowWeaponStatus)
        {
                return;
        }

        // Lazy subscribe new pawn HealthComps so freshly-spawned actors get
        // floating damage popups too. AddUniqueDynamic is idempotent so the
        // per-frame scan is safe; Phase 3 demo has ~3 pawns max.
        if (bShowDamagePopups)
        {
                for (TActorIterator<APawn> It(World); It; ++It)
                {
                        APawn* P = *It;
                        if (!P)
                        {
                                continue;
                        }
                        if (UCallsignHealthComponent* HC = P->FindComponentByClass<UCallsignHealthComponent>())
                        {
                                HC->OnHealthChanged.AddUniqueDynamic(this, &ACallsignDebugHUD::HandleHealthChanged);
                        }
                }
        }

        // ---- 0. Mission banner (drawn last conceptually but anchored top-center) ----
        // Once the GameMode flips MissionResult, render a centered banner so the
        // PIE session has a clear visual end state. Drawn early in this method
        // so it sits behind subsequent overlays (which is fine — the banner
        // text is large and stays readable underneath the corner panels).
        if (bShowMissionBanner && Canvas)
        {
                if (const ACallsignExfilGameMode* GM = World->GetAuthGameMode<ACallsignExfilGameMode>())
                {
                        if (GM->MissionResult != ECallsignMissionResult::InProgress)
                        {
                                const bool bVictory = (GM->MissionResult == ECallsignMissionResult::Victory);
                                const TCHAR* Title = bVictory ? TEXT("MISSION COMPLETE") : TEXT("MISSION FAILED");
                                const FLinearColor TitleColor = bVictory
                                        ? FLinearColor(0.4f, 1.0f, 0.5f, 1.0f)
                                        : FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
                                const FLinearColor BgColor(0.f, 0.f, 0.f, 0.7f);

                                const float TitleScale = 4.5f;
                                const float SubScale = 2.0f;
                                const float ApproxCharW = 18.f * TitleScale * 0.55f;
                                const int32 TitleLen = bVictory ? 16 : 14; // "MISSION COMPLETE" / "MISSION FAILED"
                                const float TitleW = ApproxCharW * TitleLen;
                                const float TitleH = 22.f * TitleScale;
                                const float CenterX = Canvas->ClipX * 0.5f;
                                const float TopY = Canvas->ClipY * 0.32f;

                                const float PanelW = TitleW + 200.f;
                                const float PanelH = TitleH + 110.f;
                                const float PanelX = CenterX - (PanelW * 0.5f);
                                const float PanelY = TopY - 40.f;
                                DrawRect(BgColor, PanelX, PanelY, PanelW, PanelH);

                                const float TitleX = CenterX - (TitleW * 0.5f);
                                DrawText(Title, TitleColor, TitleX, TopY, /*Font*/ nullptr, TitleScale, false);
                                const TCHAR* Sub = bVictory
                                        ? TEXT("敵を全滅させた。")
                                        : TEXT("作戦員が戦闘不能。");
                                DrawText(Sub, FLinearColor::White,
                                        CenterX - 180.f, TopY + TitleH + 16.f,
                                        /*Font*/ nullptr, SubScale, false);
                                DrawText(TEXT("[R] 再出撃"),
                                        FLinearColor(1.0f, 0.85f, 0.3f, 1.0f),
                                        CenterX - 100.f, TopY + TitleH + 60.f,
                                        /*Font*/ nullptr, SubScale, false);
                        }
                }
        }

        // ---- 1. Player status panel (left-anchor, consolidated) ----
        // One dark-bg panel that groups: current turn header, HP bar,
        // weapon mag/dur/pool line, and pending support list. Replaces
        // the previous loose stack of debug DrawText calls.
        if (Canvas && (bShowTurnInfo || bShowHealthOverlay || bShowWeaponStatus || bShowSupportPreview))
        {
                UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>();
                UCallsignSupportSystem* SupportSys = World->GetSubsystem<UCallsignSupportSystem>();
                APlayerController* PCH = GetOwningPlayerController();
                APawn* PlayerP = PCH ? PCH->GetPawn() : nullptr;

                TArray<FCallsignSupportRequest> Pending;
                if (bShowSupportPreview && SupportSys)
                {
                        Pending = SupportSys->GetPendingRequests();
                }

                const float PanelX = 24.f;
                const float PanelY = 24.f;
                const float PanelW = 480.f;
                const float Padding = 18.f;
                const float SectionGap = 12.f;
                const float HeaderH = bShowTurnInfo ? 50.f : 0.f;
                const float HpH = bShowHealthOverlay ? 64.f : 0.f;
                const float WeaponH = bShowWeaponStatus ? 30.f : 0.f;
                const float SupportHeaderH = (Pending.Num() > 0) ? 32.f : 0.f;
                const float SupportRowH = 28.f;
                const float SupportH = SupportHeaderH + SupportRowH * Pending.Num();

                int32 SectionsShown = 0;
                if (HeaderH  > 0) { ++SectionsShown; }
                if (HpH      > 0) { ++SectionsShown; }
                if (WeaponH  > 0) { ++SectionsShown; }
                if (SupportH > 0) { ++SectionsShown; }
                const float GapsTotal = SectionGap * FMath::Max(0, SectionsShown - 1);
                const float ContentH = HeaderH + HpH + WeaponH + SupportH + GapsTotal;
                const float PanelH = Padding * 2.f + ContentH;

                // Background plate with a soft accent border. Keeping it cool blue-grey
                // so it reads "instrument panel" rather than "debug overlay".
                DrawRect(FLinearColor(0.04f, 0.07f, 0.11f, 0.82f), PanelX, PanelY, PanelW, PanelH);
                const FLinearColor BorderColor(0.42f, 0.58f, 0.72f, 0.85f);
                DrawLine(PanelX,           PanelY,           PanelX + PanelW, PanelY,           BorderColor, 1.6f);
                DrawLine(PanelX,           PanelY + PanelH,  PanelX + PanelW, PanelY + PanelH,  BorderColor, 1.6f);
                DrawLine(PanelX,           PanelY,           PanelX,          PanelY + PanelH,  BorderColor, 1.6f);
                DrawLine(PanelX + PanelW,  PanelY,           PanelX + PanelW, PanelY + PanelH,  BorderColor, 1.6f);

                float CursorY = PanelY + Padding;

                // Turn header.
                if (HeaderH > 0)
                {
                        AActor* Current = TurnSys ? TurnSys->GetCurrentParticipant() : nullptr;
                        const ACallsignExfilGameMode* GMH = World->GetAuthGameMode<ACallsignExfilGameMode>();
                        const bool bMissionOver = GMH && GMH->MissionResult != ECallsignMissionResult::InProgress;
                        FString HeaderText;
                        FLinearColor HeaderColor;
                        if (bMissionOver)
                        {
                                HeaderText  = TEXT("作戦終了");
                                HeaderColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);
                        }
                        else if (Current && Current == PlayerP)
                        {
                                HeaderText  = TEXT("あなたのターン");
                                HeaderColor = FLinearColor(0.5f, 0.85f, 1.0f, 1.0f);
                        }
                        else if (Current)
                        {
                                HeaderText  = TEXT("敵のターン");
                                HeaderColor = FLinearColor(1.0f, 0.55f, 0.55f, 1.0f);
                        }
                        else
                        {
                                HeaderText  = TEXT("待機");
                                HeaderColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
                        }
                        DrawText(HeaderText, HeaderColor, PanelX + Padding, CursorY,
                                /*Font*/ nullptr, /*Scale*/ 2.4f, false);
                        CursorY += HeaderH + SectionGap;
                }

                // HP block.
                if (HpH > 0 && PlayerP)
                {
                        if (UCallsignHealthComponent* HC = PlayerP->FindComponentByClass<UCallsignHealthComponent>())
                        {
                                const float BarW = PanelW - Padding * 2.f;
                                const float BarH = 22.f;
                                const float Pct = HC->MaxHealth > 0
                                        ? FMath::Clamp(static_cast<float>(HC->CurrentHealth) / HC->MaxHealth, 0.f, 1.f)
                                        : 0.f;
                                const FLinearColor BarColor = (Pct > 0.5f)
                                        ? FLinearColor(0.25f, 0.85f, 0.35f, 0.95f)
                                        : ((Pct > 0.25f)
                                                ? FLinearColor(1.0f, 0.85f, 0.2f, 0.95f)
                                                : FLinearColor(1.0f, 0.32f, 0.32f, 0.95f));
                                DrawText(TEXT("HP"), FLinearColor(0.85f, 0.85f, 0.85f, 1.0f),
                                        PanelX + Padding, CursorY, /*Font*/ nullptr, /*Scale*/ 1.5f, false);
                                const float BarY = CursorY + 26.f;
                                DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.7f),
                                        PanelX + Padding, BarY, BarW, BarH);
                                DrawRect(BarColor, PanelX + Padding, BarY, BarW * Pct, BarH);
                                const FString HpText = FString::Printf(TEXT("%d / %d"),
                                        HC->CurrentHealth, HC->MaxHealth);
                                DrawText(HpText, FLinearColor::White,
                                        PanelX + Padding + 8.f, BarY + 1.f,
                                        /*Font*/ nullptr, /*Scale*/ 1.4f, false);
                                CursorY += HpH + SectionGap;
                        }
                }

                // Weapon block.
                if (WeaponH > 0 && PlayerP)
                {
                        UCallsignInventoryComponent* Inv = PlayerP->FindComponentByClass<UCallsignInventoryComponent>();
                        UCallsignWeaponInstanceObject* WI = Inv ? Inv->GetCurrentWeapon() : nullptr;
                        UCallsignWeaponDefinition* Def = WI ? WI->GetWeaponDefinition() : nullptr;
                        if (WI && Def)
                        {
                                const int32 MagCur = WI->GetMagazineCurrent();
                                const int32 MagMax = Def->MagazineSize;
                                const int32 DurCur = WI->GetDurabilityCurrent();
                                const int32 DurMax = Def->DurabilityMax;
                                const int32 PoolCur = Inv->GetAmmoCount(Def->AmmoType);
                                FString WeaponLine = FString::Printf(TEXT("MAG %d/%d   DUR %d/%d   POOL %d"),
                                        MagCur, MagMax, DurCur, DurMax, PoolCur);
                                FLinearColor WeaponColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
                                if (WI->IsBroken())
                                {
                                        WeaponLine += TEXT("   [BROKEN]");
                                        WeaponColor = FLinearColor(1.0f, 0.4f, 0.4f, 1.0f);
                                }
                                else if (MagCur == 0)
                                {
                                        WeaponColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);
                                }
                                DrawText(WeaponLine, WeaponColor,
                                        PanelX + Padding, CursorY, /*Font*/ nullptr, /*Scale*/ 1.55f, false);
                                CursorY += WeaponH + SectionGap;
                        }
                }

                // Support pending block.
                if (SupportH > 0)
                {
                        DrawText(TEXT("支援要請"), FLinearColor(1.0f, 0.85f, 0.3f, 1.0f),
                                PanelX + Padding, CursorY, /*Font*/ nullptr, /*Scale*/ 1.7f, false);
                        CursorY += SupportHeaderH;
                        for (int32 i = 0; i < Pending.Num(); ++i)
                        {
                                const FCallsignSupportRequest& R = Pending[i];
                                const TCHAR* TypeName = TEXT("?");
                                if (R.Definition)
                                {
                                        switch (R.Definition->SupportType)
                                        {
                                        case ECallsignSupportType::PrecisionStrike: TypeName = TEXT("精密射撃"); break;
                                        case ECallsignSupportType::SupplyPod:        TypeName = TEXT("補給投下"); break;
                                        case ECallsignSupportType::OrbitalBarrage:   TypeName = TEXT("軌道砲撃"); break;
                                        }
                                }
                                const FString Line = FString::Printf(TEXT("[%d] %s  あと %dT"),
                                        i + 1, TypeName, R.TurnsRemaining);
                                const FLinearColor Color = (R.TurnsRemaining <= 0)
                                        ? FLinearColor(1.0f, 0.4f, 0.4f, 1.0f)
                                        : FLinearColor(0.95f, 0.85f, 0.5f, 1.0f);
                                DrawText(Line, Color,
                                        PanelX + Padding + 12.f, CursorY,
                                        /*Font*/ nullptr, /*Scale*/ 1.4f, false);
                                CursorY += SupportRowH;
                        }
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
                        const FVector PLoc = PlayerP->GetActorLocation();
                        AActor* Nearest = CallsignTargeting::FindNearestRifleEnemy(World, PLoc);
                        if (Nearest)
                        {
                                const FColor TargetColor(255, 220, 80); // amber
                                const FVector ELoc = Nearest->GetActorLocation();
                                // Per-frame redraw via tiny lifetime so it appears continuously.
                                DrawDebugLine(World, PLoc, ELoc, TargetColor,
                                        /*bPersistent*/ false, /*Lifetime*/ 0.05f,
                                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 2.5f);
                                // Ring around the target's feet. Use capsule half-height when
                                // the target is an ACharacter so taller / shorter pawns don't
                                // float or sink the ring; fall back to the default 88 cm.
                                float HalfHeight = 88.f;
                                if (ACharacter* TargetChar = Cast<ACharacter>(Nearest))
                                {
                                        if (UCapsuleComponent* Cap = TargetChar->GetCapsuleComponent())
                                        {
                                                HalfHeight = Cap->GetScaledCapsuleHalfHeight();
                                        }
                                }
                                const FVector RingCenter = ELoc + FVector(0.f, 0.f, -HalfHeight);
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

        // ---- 2b''. Support preview (world-space blast radius only) ----
        // Pending list text moved into the consolidated player status panel
        // above. This block keeps the DebugSphere drawn at the target
        // location so the player can read the radius in 3D.
        if (bShowSupportPreview)
        {
                if (UCallsignSupportSystem* SupportSys = World->GetSubsystem<UCallsignSupportSystem>())
                {
                        const TArray<FCallsignSupportRequest> Pending = SupportSys->GetPendingRequests();
                        for (const FCallsignSupportRequest& R : Pending)
                        {
                                if (!R.Definition)
                                {
                                        continue;
                                }
                                const FColor SphereColor = (R.TurnsRemaining <= 0)
                                        ? FColor(255, 80, 80)
                                        : FColor(255, 220, 80);
                                DrawDebugSphere(World, R.TargetLocation, R.Definition->RadiusCm, /*Segments*/ 16,
                                        SphereColor, /*bPersistent*/ false, /*Lifetime*/ 0.05f,
                                        /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 2.0f);
                                if (R.Definition->TerrainDestructionRadiusCm > 0.f)
                                {
                                        const FColor TerrainColor(255, 140, 60);
                                        DrawDebugSphere(World, R.TargetLocation,
                                                R.Definition->TerrainDestructionRadiusCm, /*Segments*/ 16,
                                                TerrainColor, /*bPersistent*/ false, /*Lifetime*/ 0.05f,
                                                /*DepthPriority*/ SDPG_Foreground, /*Thickness*/ 1.5f);
                                }
                        }
                }
        }

        // ---- 2b'''. Enemy floating HP ----
        // The player HP bar moved into the consolidated panel above. Here
        // we only render world-space HP indicators above each living enemy.
        if (bShowHealthOverlay && Canvas)
        {
                for (TActorIterator<ACallsignRifleEnemy> It(World); It; ++It)
                {
                        ACallsignRifleEnemy* Enemy = *It;
                        if (!Enemy || !Enemy->HealthComp || Enemy->HealthComp->bIsDead)
                        {
                                continue;
                        }
                        const FVector Above = Enemy->GetActorLocation() + FVector(0.f, 0.f, 110.f);
                        const FVector Screen = Project(Above);
                        if (Screen.Z <= 0.f)
                        {
                                continue;
                        }
                        const FString EnemyHp = FString::Printf(TEXT("%d/%d"),
                                Enemy->HealthComp->CurrentHealth, Enemy->HealthComp->MaxHealth);
                        DrawText(EnemyHp, FLinearColor(1.0f, 0.4f, 0.4f, 1.0f),
                                Screen.X - 24.f, Screen.Y, /*Font*/ nullptr, /*Scale*/ 1.4f, false);
                }
        }

        // ---- 2b''''. Floating damage / heal popups ----
        // Per-frame: prune expired entries, then render each as a screen-
        // space "-X" (red) or "+X" (green) that drifts up and fades over
        // DamagePopupLifetime seconds.
        if (bShowDamagePopups && !FloatingNumbers.IsEmpty())
        {
                const float Now = World->GetTimeSeconds();
                const float Lifetime = FMath::Max(0.1f, DamagePopupLifetime);
                FloatingNumbers.RemoveAll([Now, Lifetime](const FFloatingNumber& F)
                {
                        return (Now - F.SpawnedAt) > Lifetime;
                });

                for (const FFloatingNumber& F : FloatingNumbers)
                {
                        const float Age = Now - F.SpawnedAt;
                        const float Norm = FMath::Clamp(Age / Lifetime, 0.f, 1.f);
                        const float Alpha = 1.f - Norm;
                        const FVector AnimLoc = F.WorldLoc + FVector(0.f, 0.f, 140.f + 80.f * Norm);
                        const FVector Screen = Project(AnimLoc);
                        if (Screen.Z <= 0.f)
                        {
                                continue;
                        }
                        const FString Text = (F.SignedAmount < 0)
                                ? FString::Printf(TEXT("-%d"), -F.SignedAmount)
                                : FString::Printf(TEXT("+%d"), F.SignedAmount);
                        const FLinearColor Color = (F.SignedAmount < 0)
                                ? FLinearColor(1.0f, 0.3f, 0.3f, Alpha)
                                : FLinearColor(0.4f, 1.0f, 0.4f, Alpha);
                        DrawText(Text, Color, Screen.X - 24.f, Screen.Y,
                                /*Font*/ nullptr, /*Scale*/ 2.2f + 0.6f * Norm, false);
                }
        }

        // The previous standalone weapon status text was consolidated into
        // section 1 (Player status panel). The line below labels the next
        // section so the file structure stays readable.

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

                                // Background panel: sized to the *visible* portion of the
                                // settled message stack. The text-draw loop bails when a line
                                // walks off the top of the viewport, so sizing by the total
                                // active count would grow the panel past the visible area.
                                // Compute how many settled rows fit between BottomMargin and
                                // the top of the viewport, then clamp Count by that.
                                const float UsableHeight = ClipY - BottomMargin - PanelPadding;
                                const int32 MaxVisibleLines = (LineStep > 0.f && UsableHeight > 0.f)
                                        ? FMath::Max(1, FMath::FloorToInt(UsableHeight / LineStep) + 1)
                                        : 1;
                                const int32 VisibleCount = FMath::Min(Count, MaxVisibleLines);

                                // During animation the new (bottom) message draws below the
                                // panel — its own fade-in alpha keeps that brief overshoot
                                // subtle.
                                const float PanelHeight = (VisibleCount * LineStep) + (2.f * PanelPadding);
                                const float PanelX = X - TextLeftInset - PanelPadding;
                                const float PanelY = ClipY - BottomMargin - ((VisibleCount - 1) * LineStep) - PanelPadding;
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
                        { TEXT("[5]"), TEXT("精密射撃 要請") },
                        { TEXT("[6]"), TEXT("補給投下 要請") },
                        { TEXT("[7]"), TEXT("軌道砲撃 要請") },
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
