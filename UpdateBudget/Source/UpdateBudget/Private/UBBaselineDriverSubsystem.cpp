// P01 UpdateBudget (demo) - UUBBaselineDriverSubsystem.

#include "UBBaselineDriverSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Math/NumericLimits.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SignificanceManager.h"
#include "Stats/Stats.h"
#include "Subsystems/SubsystemCollection.h"
#include "UBBenchmarkSubsystem.h"
#include "UBDemoAgent.h"
#include "UBDemoBrainComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBBaselineDriverSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogUBBaseline, Log, All);

CSV_DEFINE_CATEGORY(UBBaseline, true);

namespace UBBaseline
{
	/** Band edges: the same near/far distances UpdateBudget uses (02_System_Architecture.md section 7). */
	constexpr double NearDistance = 1500.0;   // cm
	constexpr double FarDistance = 6000.0;    // cm
	constexpr float MidInterval = 0.1f;       // s
	constexpr float FarInterval = 0.5f;       // s
	/** The largest float below 1: keeps a Mid-band significance strictly below 1 after the double -> float conversion. */
	constexpr float BelowOne = 0.99999994f;
	const FName SignificanceTag(TEXT("UBAgent"));
}

bool UUBBaselineDriverSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UUBBaselineDriverSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Before Super: tickable world subsystems join the tick list in Initialize (WorldSubsystem.cpp:101), so this makes
	// the benchmark subsystem tick first every frame. Wave agents are then adopted on the frame they spawn.
	Benchmark = Collection.InitializeDependency<UUBBenchmarkSubsystem>();
	Super::Initialize(Collection);
}

UUBBaselineDriverSubsystem::EBand UUBBaselineDriverSubsystem::BandForDistanceSquared(double DistanceSquared)
{
	if (DistanceSquared <= FMath::Square(UBBaseline::NearDistance))
	{
		return EBand::Near;
	}
	return (DistanceSquared < FMath::Square(UBBaseline::FarDistance)) ? EBand::Mid : EBand::Far;
}

UUBBaselineDriverSubsystem::EBand UUBBaselineDriverSubsystem::BandForSignificance(float Significance)
{
	if (Significance >= 1.f)
	{
		return EBand::Near;
	}
	return (Significance > 0.f) ? EBand::Mid : EBand::Far;
}

float UUBBaselineDriverSubsystem::IntervalForBand(EBand Band)
{
	switch (Band)
	{
	case EBand::Near:
		return 0.f;
	case EBand::Mid:
		return UBBaseline::MidInterval;
	default:
		return UBBaseline::FarInterval;
	}
}

float UUBBaselineDriverSubsystem::SignificanceForDistanceSquared(double DistanceSquared)
{
	const EBand Band = BandForDistanceSquared(DistanceSquared);
	if (Band != EBand::Mid)
	{
		return (Band == EBand::Near) ? 1.f : 0.f;
	}
	// Linear falloff across the Mid band. The clamp keeps the value strictly inside (0, 1), so BandForSignificance
	// returns Mid for it: B3's bands equal B2's by construction, not by rounding luck.
	const double Falloff = 1.0 - FMath::GetRangePct(UBBaseline::NearDistance, UBBaseline::FarDistance, FMath::Sqrt(DistanceSquared));
	return FMath::Clamp(static_cast<float>(Falloff), TNumericLimits<float>::Min(), UBBaseline::BelowOne);
}

void UUBBaselineDriverSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	FString Requested;
	FParse::Value(FCommandLine::Get(), TEXT("UBBaseline="), Requested);
	Requested.ToUpperInline();
	if (Requested == TEXT("B2"))
	{
		Mode = EMode::DistanceIntervals;
	}
	else if (Requested == TEXT("B3"))
	{
		// Fetched here, not in Initialize: the Significance Manager is created after world subsystems initialise (01b #10).
		if (USignificanceManager::Get(&InWorld))
		{
			Mode = EMode::SignificanceIntervals;
		}
		else
		{
			UE_LOG(LogUBBaseline, Error, TEXT("-UBBaseline=B3 requested, but there is no Significance Manager in this world."));
		}
	}
	else if (!Requested.IsEmpty())
	{
		UE_LOG(LogUBBaseline, Error, TEXT("Unknown -UBBaseline=%s (expected B2 or B3)."), *Requested);
	}

	if (Mode != EMode::None)
	{
		CSV_METADATA(TEXT("UBBaseline"), *Requested);
		UE_LOG(LogUBBaseline, Display, TEXT("Baseline %s active."), *Requested);
	}
}

void UUBBaselineDriverSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	// Actors' EndPlay has already run (World.cpp:6203-6210), so agents unregistered themselves through
	// HandleAgentEndPlay. This is the safety net, while the Significance Manager still exists.
	if (USignificanceManager* SignificanceManager = GetSignificanceManager())
	{
		SignificanceManager->UnregisterAll(UBBaseline::SignificanceTag);
	}
	Brains.Empty();
	Mode = EMode::None;
	Super::OnWorldEndPlay(InWorld);
}

USignificanceManager* UUBBaselineDriverSubsystem::GetSignificanceManager() const
{
	// Looked up each time rather than cached: the manager is removed during world cleanup.
	return (Mode == EMode::SignificanceIntervals) ? USignificanceManager::Get(GetWorld()) : nullptr;
}

bool UUBBaselineDriverSubsystem::GetViewTransform(FTransform& OutView) const
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr;
	if (!CameraManager)
	{
		return false;
	}
	const FMinimalViewInfo& ViewInfo = CameraManager->GetCameraCacheView();
	OutView = FTransform(ViewInfo.Rotation, ViewInfo.Location);
	return true;
}

void UUBBaselineDriverSubsystem::SetIntervalIfChanged(UUBDemoBrainComponent& Brain, float Interval)
{
	if (Brain.GetComponentTickInterval() != Interval)
	{
		Brain.SetComponentTickIntervalAndCooldown(Interval);
		++FrameIntervalChanges;
	}
}

void UUBBaselineDriverSubsystem::HandleAgentEndPlay(AActor* Actor, EEndPlayReason::Type /*EndPlayReason*/)
{
	const AUBDemoAgent* Agent = Cast<AUBDemoAgent>(Actor);
	UUBDemoBrainComponent* Brain = Agent ? Agent->GetBrain() : nullptr;
	if (!Brain)
	{
		return;
	}
	if (USignificanceManager* SignificanceManager = GetSignificanceManager())
	{
		// The manager holds a raw pointer and would keep calling our callbacks on this object.
		SignificanceManager->UnregisterObject(Brain);
	}
	else
	{
		Brains.RemoveSingleSwap(Brain, EAllowShrinking::No);
	}
}

void UUBBaselineDriverSubsystem::AdoptNewAgents()
{
	if (!Benchmark)
	{
		return;
	}
	const TArray<TObjectPtr<AUBDemoAgent>>& Agents = Benchmark->GetAgents();
	if (Agents.Num() == AdoptedAgents
		|| !ensureMsgf(Agents.Num() > AdoptedAgents, TEXT("UUBBenchmarkSubsystem::GetAgents() must be append-only: agents are adopted by index.")))
	{
		return;
	}

	USignificanceManager* SignificanceManager = GetSignificanceManager();

	// B3 callbacks. The significance function runs on worker threads (Update uses ParallelFor). It only reads
	// transforms, during a part of the frame where nothing writes them (after TG_PostPhysics, before end-of-frame updates).
	auto SignificanceFunction = [](USignificanceManager::FManagedObjectInfo* Info, const FTransform& View) -> float
	{
		const UActorComponent* Component = static_cast<const UActorComponent*>(Info->GetObject());
		return SignificanceForDistanceSquared(FVector::DistSquared(Component->GetOwner()->GetActorLocation(), View.GetLocation()));
	};
	// Sequential: runs on the game thread after the parallel pass, where changing tick intervals is allowed.
	// Old and new significance are both supplied, so only a band change touches the component. That holds from the
	// first call: the manager starts every object at significance 1 (SignificanceManager.h:72) and registration passes
	// Old = 1 (SignificanceManager.cpp:236), the Near band, which matches a new brain's interval of 0.
	// Capturing 'this' is safe: every registration is removed in HandleAgentEndPlay or OnWorldEndPlay, both of which
	// run before this subsystem is destroyed.
	auto PostSignificanceFunction = [this](USignificanceManager::FManagedObjectInfo* Info, float OldSignificance, float NewSignificance, bool bFinal)
	{
		const EBand NewBand = BandForSignificance(NewSignificance);
		if (!bFinal && NewBand != BandForSignificance(OldSignificance))
		{
			SetIntervalIfChanged(*static_cast<UUBDemoBrainComponent*>(Info->GetObject()), IntervalForBand(NewBand));
		}
	};

	if (!SignificanceManager)
	{
		Brains.Reserve(Agents.Num());
	}
	for (int32 Index = AdoptedAgents; Index < Agents.Num(); ++Index)
	{
		AUBDemoAgent* Agent = Agents[Index];
		UUBDemoBrainComponent* Brain = IsValid(Agent) ? Agent->GetBrain() : nullptr;
		if (!Brain)
		{
			continue;
		}
		Agent->OnEndPlay.AddDynamic(this, &UUBBaselineDriverSubsystem::HandleAgentEndPlay);
		if (SignificanceManager)
		{
			SignificanceManager->RegisterObject(Brain, UBBaseline::SignificanceTag, SignificanceFunction,
				USignificanceManager::EPostSignificanceType::Sequential, PostSignificanceFunction);
		}
		else
		{
			Brains.Add(Brain);   // B2 re-tunes these itself, in Retune
		}
	}
	AdoptedAgents = Agents.Num();
}

void UUBBaselineDriverSubsystem::Retune()
{
	FTransform View;
	if (!GetViewTransform(View))
	{
		return;
	}
	if (Mode == EMode::SignificanceIntervals)
	{
		// B3: significance is computed in parallel; band changes are applied in the Sequential post function.
		if (USignificanceManager* SignificanceManager = GetSignificanceManager())
		{
			SignificanceManager->Update(MakeArrayView(&View, 1));
		}
		return;
	}
	// B2. Squared distances: comparing against squared edges needs no square root.
	const FVector ViewLocation = View.GetLocation();
	for (UUBDemoBrainComponent* Brain : Brains)
	{
		if (IsValid(Brain))
		{
			SetIntervalIfChanged(*Brain, IntervalForBand(BandForDistanceSquared(FVector::DistSquared(Brain->GetOwner()->GetActorLocation(), ViewLocation))));
		}
	}
}

void UUBBaselineDriverSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Mode == EMode::None)
	{
		return;
	}

	{
		// One-off setup work (registration) is timed separately from the per-frame re-tuning.
		CSV_SCOPED_TIMING_STAT(UBBaseline, Register);
		AdoptNewAgents();
	}

	FrameIntervalChanges = 0;
	{
		// The re-tuning work is part of these approaches' real cost, so it is timed (hypothesis H6).
		CSV_SCOPED_TIMING_STAT(UBBaseline, Retune);
		Retune();
	}
	CSV_CUSTOM_STAT(UBBaseline, IntervalChanges, FrameIntervalChanges, ECsvCustomStatOp::Set);
}

TStatId UUBBaselineDriverSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUBBaselineDriverSubsystem, STATGROUP_Tickables);
}
