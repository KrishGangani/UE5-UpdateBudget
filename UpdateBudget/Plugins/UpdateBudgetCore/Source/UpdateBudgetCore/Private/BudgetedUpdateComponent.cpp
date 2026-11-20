// UpdateBudgetCore - UBudgetedUpdateComponent.

#include "BudgetedUpdateComponent.h"

#include "Engine/World.h"
#include "UpdateBudgetSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BudgetedUpdateComponent)

UBudgetedUpdateComponent::UBudgetedUpdateComponent()
{
	// Native path (baselines); bStartWithTickEnabled is already true by default.
	// Components default to TG_DuringPhysics; the scheduler runs in TG_PrePhysics, so the native tick
	// is pinned there too, keeping every configuration at the same point in the frame (01b #1).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UBudgetedUpdateComponent::BeginPlay()
{
	Super::BeginPlay();

	// The subsystem exists only in Game and PIE worlds; elsewhere the component simply ticks natively.
	if (UUpdateBudgetSubsystem* Found = UWorld::GetSubsystem<UUpdateBudgetSubsystem>(GetWorld()))
	{
		Found->Register(this);
	}
}

void UBudgetedUpdateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister explicitly: GC only nulls references when it runs (01b #8), so the registry must not rely on it.
	if (UUpdateBudgetSubsystem* Current = Subsystem)
	{
		Current->Unregister(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UBudgetedUpdateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The engine's DeltaTime is deliberately not forwarded: RunUpdate() measures time itself, from one clock.
	RunUpdate();
}

void UBudgetedUpdateComponent::RunUpdate()
{
	const UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return;
	}

	const double PreviousUpdateTime = LastUpdateTime;
	LastUpdateTime = World->GetTimeSeconds();

	// Reported in every mode, at the same cost, so updates and staleness are measured fairly (02 section 9).
	if (UUpdateBudgetSubsystem* Current = Subsystem)
	{
		Current->RecordUpdate(*this, PreviousUpdateTime);
	}

	const double DeltaSeconds = (PreviousUpdateTime < 0.0) ? 0.0 : LastUpdateTime - PreviousUpdateTime;
	OnBudgetedUpdate(static_cast<float>(DeltaSeconds));
}
