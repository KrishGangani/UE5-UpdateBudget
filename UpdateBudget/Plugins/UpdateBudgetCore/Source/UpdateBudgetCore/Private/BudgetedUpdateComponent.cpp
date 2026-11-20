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
	JoinBudget();
}

void UBudgetedUpdateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Leave explicitly: GC only nulls references when it runs (01b #8), so the registry must not rely on it.
	LeaveBudget();
	Super::EndPlay(EndPlayReason);
}

void UBudgetedUpdateComponent::OnRegister()
{
	Super::OnRegister();

	// Re-registered while playing (ReregisterComponent): the engine brings the native tick back without a second
	// BeginPlay (Actor.cpp:6443-6451), so the registry link comes back here. A first registration joins in BeginPlay.
	if (HasBegunPlay())
	{
		JoinBudget();
	}
}

void UBudgetedUpdateComponent::OnUnregister()
{
	// A level being streamed out unregisters its components before their EndPlay, possibly frames before
	// (World.cpp:4190-4251), and the native tick function goes at that moment (ActorComponent.cpp:2147).
	LeaveBudget();
	Super::OnUnregister();
}

void UBudgetedUpdateComponent::JoinBudget()
{
	// The subsystem exists only in Game and PIE worlds; elsewhere the component simply ticks natively.
	if (UUpdateBudgetSubsystem* Found = UWorld::GetSubsystem<UUpdateBudgetSubsystem>(GetWorld()))
	{
		Found->Register(this);
	}
}

void UBudgetedUpdateComponent::LeaveBudget()
{
	if (UUpdateBudgetSubsystem* Current = Subsystem)
	{
		Current->Unregister(this);
	}
}

void UBudgetedUpdateComponent::SetDrivenByScheduler(bool bInDrivenByScheduler)
{
	if (bInDrivenByScheduler == bDrivenByScheduler)
	{
		return;
	}
	bDrivenByScheduler = bInDrivenByScheduler;

	// Registering a tick function re-enables it when bStartWithTickEnabled is set (ActorComponent.cpp:1747),
	// so the flag must follow the mode too, or a re-registered component would tick natively again.
	if (bInDrivenByScheduler)
	{
		// The tick may also have been set without going through SetComponentTickEnabled, so read it once here.
		bNativeTickEnabled = IsComponentTickEnabled();
		bNativeStartWithTickEnabled = PrimaryComponentTick.bStartWithTickEnabled;
		PrimaryComponentTick.bStartWithTickEnabled = false;
		Super::SetComponentTickEnabled(false);
	}
	else
	{
		PrimaryComponentTick.bStartWithTickEnabled = bNativeStartWithTickEnabled;
		Super::SetComponentTickEnabled(bNativeTickEnabled);
	}
}

void UBudgetedUpdateComponent::SetComponentTickEnabled(bool bEnabled)
{
	bNativeTickEnabled = bEnabled;
	if (!bDrivenByScheduler)
	{
		Super::SetComponentTickEnabled(bEnabled);
	}
}

void UBudgetedUpdateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Driven by the scheduler, the native tick is off. SetComponentTickEnabled can't turn it on, but code that sets the
	// tick function directly can: switch it off again (without recording that as gameplay's choice) instead of updating
	// twice in one frame.
	if (bDrivenByScheduler)
	{
		Super::SetComponentTickEnabled(false);
		return;
	}

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
