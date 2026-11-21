// P01 UpdateBudget (demo) - UUBBaselineDriverSubsystem.

#include "UBBaselineDriverSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SignificanceManager.h"
#include "Stats/Stats.h"
#include "Subsystems/SubsystemCollection.h"
#include "UBBenchmarkSubsystem.h"
#include "UBDemoAgent.h"
#include "UBDemoBrainComponent.h"
#include "UpdateBudgetSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBBaselineDriverSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogUBBaseline, Log, All);

CSV_DEFINE_CATEGORY(UBBaseline, true);

namespace UBBaseline
{
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

FUBBaselineBands UUBBaselineDriverSubsystem::ReadBands()
{
	const TArray<FUpdateBudgetCategory> Categories = GetDefault<UUpdateBudgetSettings>()->GetCategories();
	return FUBBaselineBands::FromCategory(FUpdateBudgetCategory::Pick(Categories, GetDefault<UUBDemoBrainComponent>()->GetBudgetCategory()));
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
		Bands = ReadBands();
		CSV_METADATA(TEXT("UBBaseline"), *Requested);
		// The intervals analyse_baselines.py needs for its H4 lags, so it never assumes them; the log line has the rest.
		CSV_METADATA(TEXT("UBIntervalMid"), *FString::SanitizeFloat(Bands.MidInterval));
		CSV_METADATA(TEXT("UBIntervalFar"), *FString::SanitizeFloat(Bands.FarInterval));
		UE_LOG(LogUBBaseline, Display, TEXT("Baseline %s active: bands %.0f / %.0f cm, intervals %.3f / %.3f / %.3f s."),
			*Requested, Bands.NearDistance, Bands.FarDistance, Bands.NearInterval, Bands.MidInterval, Bands.FarInterval);
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
	// Bands is written only in OnWorldBeginPlay, so the worker threads can read it through 'this'.
	auto SignificanceFunction = [this](USignificanceManager::FManagedObjectInfo* Info, const FTransform& View) -> float
	{
		const UActorComponent* Component = static_cast<const UActorComponent*>(Info->GetObject());
		return Bands.SignificanceForDistanceSquared(FVector::DistSquared(Component->GetOwner()->GetActorLocation(), View.GetLocation()));
	};
	// Sequential: runs on the game thread after the parallel pass, where changing tick intervals is allowed.
	// Old and new significance are both supplied, so only a band change touches the component. That holds from the
	// first call: the manager starts every object at significance 1 (SignificanceManager.h:72) and registration passes
	// Old = 1 (SignificanceManager.cpp:236), the Near band, so each brain starts with the Near interval (set below).
	// Capturing 'this' is safe: every registration is removed in HandleAgentEndPlay or OnWorldEndPlay, both of which
	// run before this subsystem is destroyed.
	auto PostSignificanceFunction = [this](USignificanceManager::FManagedObjectInfo* Info, float OldSignificance, float NewSignificance, bool bFinal)
	{
		const EUBBaselineBand NewBand = FUBBaselineBands::BandForSignificance(NewSignificance);
		if (!bFinal && NewBand != FUBBaselineBands::BandForSignificance(OldSignificance))
		{
			SetIntervalIfChanged(*static_cast<UUBDemoBrainComponent*>(Info->GetObject()), Bands.IntervalForBand(NewBand));
		}
	};

	if (!SignificanceManager)
	{
		Brains.Reserve(Agents.Num());
	}
	// The bands are the brain class's default category (ReadBands); UB uses each brain's own. A brain that differs would
	// make the comparison unfair, so it is an error: the batch fails instead of comparing unlike things.
	const int32 BandsCategory = GetDefault<UUBDemoBrainComponent>()->GetBudgetCategory();
	bool bReportedCategory = false;
	for (int32 Index = AdoptedAgents; Index < Agents.Num(); ++Index)
	{
		AUBDemoAgent* Agent = Agents[Index];
		UUBDemoBrainComponent* Brain = IsValid(Agent) ? Agent->GetBrain() : nullptr;
		if (!Brain)
		{
			continue;
		}
		if (!bReportedCategory && Brain->GetBudgetCategory() != BandsCategory)
		{
			UE_LOG(LogUBBaseline, Error, TEXT("Agent %d's brain uses category %d, but B2/B3 band every brain with category %d."),
				Index, Brain->GetBudgetCategory(), BandsCategory);
			bReportedCategory = true;
		}
		Agent->OnEndPlay.AddDynamic(this, &UUBBaselineDriverSubsystem::HandleAgentEndPlay);
		if (SignificanceManager)
		{
			// The manager's starting band is Near (see above). A no-op with the default settings: a new brain ticks every frame.
			SetIntervalIfChanged(*Brain, Bands.NearInterval);
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
			SetIntervalIfChanged(*Brain, Bands.IntervalForBand(Bands.BandForDistanceSquared(FVector::DistSquared(Brain->GetOwner()->GetActorLocation(), ViewLocation))));
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

	// Reset before adoption, so the interval a B3 brain gets when it is adopted counts in the frame it happens, as B2's
	// first re-tune does.
	FrameIntervalChanges = 0;
	{
		// One-off setup work (registration) is timed separately from the per-frame re-tuning.
		CSV_SCOPED_TIMING_STAT(UBBaseline, Register);
		AdoptNewAgents();
	}

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
