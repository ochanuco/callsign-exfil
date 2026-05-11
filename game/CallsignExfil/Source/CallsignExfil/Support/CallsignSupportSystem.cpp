// Copyright (c) ochanuco 2026. Licensed under MIT.

#include "CallsignSupportSystem.h"
#include "CallsignSupportResolver.h"
#include "Data/CallsignSupportTypes.h"
#include "HUD/CallsignMessageBus.h"
#include "Turn/CallsignTurnSystem.h"
#include "Engine/World.h"

void UCallsignSupportSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Display, TEXT("[Support] Subsystem initialized (subscribe deferred to OnWorldBeginPlay)"));
}

void UCallsignSupportSystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// UWorldSubsystem::Initialize order is not deterministic, so subscribing
	// to UCallsignTurnSystem there sometimes silently fails (TurnSys ptr
	// resolvable but our delegate ends up bound to a not-yet-broadcasting
	// instance, leaving pending support requests stuck at their initial
	// TurnsRemaining). OnWorldBeginPlay runs after every subsystem has
	// completed Initialize, so the subscription always sticks.
	if (UCallsignTurnSystem* TurnSys = InWorld.GetSubsystem<UCallsignTurnSystem>())
	{
		TurnSys->OnTurnEnd.AddUniqueDynamic(this, &UCallsignSupportSystem::HandleTurnEnd);
		UE_LOG(LogTemp, Display, TEXT("[Support] Subscribed to TurnSystem.OnTurnEnd"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Support] OnWorldBeginPlay: no TurnSystem to subscribe to"));
	}
}

void UCallsignSupportSystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UCallsignTurnSystem* TurnSys = World->GetSubsystem<UCallsignTurnSystem>())
		{
			TurnSys->OnTurnEnd.RemoveDynamic(this, &UCallsignSupportSystem::HandleTurnEnd);
		}
	}
	Requests.Reset();
	Definitions.Reset();
	UE_LOG(LogTemp, Display, TEXT("[Support] Subsystem deinitialized"));
	Super::Deinitialize();
}

FGuid UCallsignSupportSystem::RequestSupport(ECallsignSupportType Type, const FVector& TargetLocation, AActor* RequestedBy)
{
	UCallsignSupportDefinition* Def = FindDefinition(Type);
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Support] RequestSupport: no definition registered for type %d"),
			static_cast<int32>(Type));
		return FGuid();
	}

	FCallsignSupportRequest Req;
	Req.RequestId = FGuid::NewGuid();
	Req.RequestedBy = RequestedBy;
	Req.TargetLocation = TargetLocation;
	Req.Definition = Def;
	Req.TurnsRemaining = Def->DelayTurns;
	Req.Phase = ECallsignSupportPhase::Pending;
	Requests.Add(Req);

	UE_LOG(LogTemp, Display, TEXT("[Support] Submitted: id=%s type=%d delay=%d at (%.0f,%.0f,%.0f)"),
		*Req.RequestId.ToString(), static_cast<int32>(Type), Req.TurnsRemaining,
		Req.TargetLocation.X, Req.TargetLocation.Y, Req.TargetLocation.Z);

	OnSupportSubmitted.Broadcast(Req);
	return Req.RequestId;
}

TArray<FCallsignSupportRequest> UCallsignSupportSystem::GetPendingRequests() const
{
	TArray<FCallsignSupportRequest> Out;
	Out.Reserve(Requests.Num());
	for (const FCallsignSupportRequest& R : Requests)
	{
		if (R.Phase == ECallsignSupportPhase::Pending)
		{
			Out.Add(R);
		}
	}
	return Out;
}

bool UCallsignSupportSystem::CancelRequest(const FGuid& RequestId)
{
	for (FCallsignSupportRequest& R : Requests)
	{
		if (R.RequestId == RequestId && R.Phase == ECallsignSupportPhase::Pending)
		{
			R.Phase = ECallsignSupportPhase::Cancelled;
			UE_LOG(LogTemp, Display, TEXT("[Support] Cancelled: id=%s"), *RequestId.ToString());
			return true;
		}
	}
	return false;
}

void UCallsignSupportSystem::RegisterDefinition(ECallsignSupportType Type, UCallsignSupportDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}
	Definitions.Add(Type, Definition);
	UE_LOG(LogTemp, Display, TEXT("[Support] Registered definition for type %d (Delay=%d Radius=%.0f Damage=%d)"),
		static_cast<int32>(Type), Definition->DelayTurns, Definition->RadiusCm, Definition->Damage);
}

UCallsignSupportDefinition* UCallsignSupportSystem::FindDefinition(ECallsignSupportType Type) const
{
	const TObjectPtr<UCallsignSupportDefinition>* Found = Definitions.Find(Type);
	return Found ? Found->Get() : nullptr;
}

void UCallsignSupportSystem::HandleTurnEnd(AActor* /*Participant*/)
{
	UWorld* World = GetWorld();
	UCallsignSupportResolver* Resolver = World ? World->GetSubsystem<UCallsignSupportResolver>() : nullptr;

	// ADR-004 §13 OQ-1 / §Risks: --TurnsRemaining first, then check ==0.
	// Off-by-one matters: a DelayTurns=2 request submitted on turn N must
	// resolve at the end of turn N+2 (two end-of-turn ticks elapsed),
	// not N+1.
	TArray<FCallsignSupportRequest> ToResolve;
	for (FCallsignSupportRequest& R : Requests)
	{
		if (R.Phase != ECallsignSupportPhase::Pending)
		{
			continue;
		}
		--R.TurnsRemaining;
		if (R.TurnsRemaining <= 0)
		{
			R.Phase = ECallsignSupportPhase::Resolving;
			ToResolve.Add(R);
		}
	}

	if (!Resolver)
	{
		if (ToResolve.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Support] %d request(s) ready to resolve but no resolver subsystem"),
				ToResolve.Num());
		}
		return;
	}

	for (const FCallsignSupportRequest& Req : ToResolve)
	{
		const FCallsignSupportResolution Resolution = Resolver->Resolve(Req);

		// Flip the persisted entry to Resolved (Resolve() already mutated
		// our local copy via the snapshot, but the stored copy still says
		// Resolving). Find by RequestId and update.
		for (FCallsignSupportRequest& Stored : Requests)
		{
			if (Stored.RequestId == Req.RequestId)
			{
				Stored.Phase = ECallsignSupportPhase::Resolved;
				break;
			}
		}

		UE_LOG(LogTemp, Display, TEXT("[Support] Resolved: id=%s damageEvents=%d nodesDestroyed=%d"),
			*Req.RequestId.ToString(), Resolution.DamageEventsEmitted, Resolution.DestroyedNodes.Num());

		// Chat-log feedback so the player can see the queued strike went off
		// without having to read floating popups + HP bars to confirm.
		const TCHAR* TypeName = TEXT("支援");
		if (Req.Definition)
		{
			switch (Req.Definition->SupportType)
			{
			case ECallsignSupportType::PrecisionStrike: TypeName = TEXT("精密射撃"); break;
			case ECallsignSupportType::SupplyPod:        TypeName = TEXT("補給投下"); break;
			case ECallsignSupportType::OrbitalBarrage:   TypeName = TEXT("軌道砲撃"); break;
			}
		}
		CallsignMsg::PushSystem(World, FString::Printf(TEXT("%s 解決。"), TypeName));

		OnSupportResolved.Broadcast(Resolution);
	}
}
