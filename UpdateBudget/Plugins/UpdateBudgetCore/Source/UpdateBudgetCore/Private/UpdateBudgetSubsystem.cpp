// UpdateBudgetCore - UUpdateBudgetSubsystem.

#include "UpdateBudgetSubsystem.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UpdateBudgetSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogUpdateBudget, Log, All);

// CSV category "UpdateBudget", enabled by default so a capture always contains it.
CSV_DEFINE_CATEGORY(UpdateBudget, true);

// ECVF_Default, not a cheat cvar: cheat cvars can't be set from ini files (01b #11). To start a run in budgeted
// mode, set it before the world begins play, e.g. -ini:Engine:[ConsoleVariables]:ub.Enabled=1 on the command line.
static TAutoConsoleVariable<int32> CVarUpdateBudgetEnabled(
	TEXT("ub.Enabled"),
	0,
	TEXT("UpdateBudget mode, read when a world begins play. 0: native ticks (baselines). 1: budgeted (the scheduler runs the updates)."),
	ECVF_Default);

namespace UpdateBudget
{
	/** The default budget (02 section 7). Settings and the ub.BudgetMs cvar replace it in step S6. */
	constexpr double DefaultBudgetMs = 2.0;

	/**
	 * Max staleness of every client until significance exists (step S6). At 0 every client is due every frame, so the
	 * scheduler path does exactly the native path's work: each client updated once per frame, at the same world times.
	 */
	constexpr double UnscoredMaxStalenessSeconds = 0.0;
}

bool UUpdateBudgetSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UUpdateBudgetSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	bBudgeted = CVarUpdateBudgetEnabled.GetValueOnGameThread() != 0;

	// Normally nobody has registered yet (OnWorldBeginPlay runs before any actor's BeginPlay, 01b #7);
	// if someone has, the mode must reach them too, or they would update natively and from the scheduler.
	for (UBudgetedUpdateComponent* Client : Clients)
	{
		if (Client)
		{
			ApplyMode(*Client);
		}
	}

	// Registered in every mode, so a later switch only has to enable it; a disabled tick function costs nothing per
	// frame (the tick manager walks only its enabled set). The enable state is set before registering, as Niagara does.
	TickFunction.Subsystem = this;
	TickFunction.SetTickFunctionEnable(bBudgeted);
	TickFunction.RegisterTickFunction(InWorld.PersistentLevel);

	// GC nulls the entry of a component collected without EndPlay. Removing such entries right after each collection
	// keeps the registry exact in every mode; otherwise only the scheduler's loop would ever find them.
	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UUpdateBudgetSubsystem::HandlePostGarbageCollect);

	UE_LOG(LogUpdateBudget, Display, TEXT("%s mode (ub.Enabled %d). Update timer resolution: %.1f ns."),
		bBudgeted ? TEXT("Budgeted") : TEXT("Native"), CVarUpdateBudgetEnabled.GetValueOnGameThread(),
		FPlatformTime::GetSecondsPerCycle64() * 1.0e9);
}

void UUpdateBudgetSubsystem::Register(UBudgetedUpdateComponent* Client)
{
	check(Client);
	if (Client->Subsystem)
	{
		ensure(Client->Subsystem == this);
		return; // Already registered (or waiting in PendingAdds).
	}
	Client->Subsystem = this;

	// Applied at once, even when the add itself is deferred: a component spawned during the loop has just had its
	// native tick registered (Actor.cpp:4824), and it must not tick natively in a later group of this frame.
	ApplyMode(*Client);

	if (bRunningScheduler)
	{
		PendingAdds.Add(Client);   // the scheduler reads States through a raw pointer: no reallocation now
	}
	else
	{
		AddClient(Client);
	}
}

void UUpdateBudgetSubsystem::Unregister(UBudgetedUpdateComponent* Client)
{
	check(Client);
	const int32 Index = Client->RegistryIndex;
	if (Index == INDEX_NONE)
	{
		PendingAdds.RemoveSingleSwap(Client, EAllowShrinking::No);   // registered and removed in the same loop
	}
	else if (Clients.IsValidIndex(Index) && Clients[Index] == Client)
	{
		if (bRunningScheduler)
		{
			Clients[Index] = nullptr;   // the loop skips it; removed after the loop with the other invalid entries
			bHasInvalidClients = true;
		}
		else
		{
			RemoveClientAt(Index);
		}
	}
	DetachClient(Client);
}

void UUpdateBudgetSubsystem::ApplyMode(UBudgetedUpdateComponent& Client) const
{
	// Compared, not only set in budgeted mode: a client that was scheduler-driven elsewhere gets its native tick back,
	// and a native client's tick state, which gameplay may have changed, is left alone.
	if (Client.IsDrivenByScheduler() != bBudgeted)
	{
		Client.SetDrivenByScheduler(bBudgeted);
	}
}

void UUpdateBudgetSubsystem::AddClient(UBudgetedUpdateComponent* Client)
{
	Client->RegistryIndex = Clients.Add(Client);
	States.AddDefaulted_GetRef().LastUpdateTime = Client->GetLastUpdateTime();
}

void UUpdateBudgetSubsystem::RemoveClientAt(int32 Index)
{
	// O(1): the last entry moves into the freed slot, in both arrays. It may be null (collected without EndPlay).
	Clients.RemoveAtSwap(Index, EAllowShrinking::No);
	States.RemoveAtSwap(Index, EAllowShrinking::No);
	if (Clients.IsValidIndex(Index) && Clients[Index])
	{
		Clients[Index]->RegistryIndex = Index;
	}
}

void UUpdateBudgetSubsystem::RemoveInvalidClients()
{
	// Backwards, so the entry a swap moves into Index has already been checked.
	for (int32 Index = Clients.Num() - 1; Index >= 0; --Index)
	{
		UBudgetedUpdateComponent* Client = Clients[Index];
		if (!IsValid(Client))
		{
			DetachClient(Client);   // invalid but not yet collected: its EndPlay, if it comes, must find no link
			RemoveClientAt(Index);
		}
	}
	bHasInvalidClients = false;
}

void UUpdateBudgetSubsystem::HandlePostGarbageCollect()
{
	// Collections run between frames, so normally the arrays can change now. If an update forced one inside the loop,
	// only mark the entries; the loop removes them when it ends.
	if (bRunningScheduler)
	{
		bHasInvalidClients = true;
	}
	else
	{
		RemoveInvalidClients();
	}
}

void UUpdateBudgetSubsystem::DetachClient(UBudgetedUpdateComponent* Client)
{
	if (Client)
	{
		Client->RegistryIndex = INDEX_NONE;
		Client->Subsystem = nullptr;
	}
}

void UUpdateBudgetSubsystem::RunFrame(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();

	// Deadlines from the hot data only. A client never updated has LastUpdateTime < 0, so it is due at once.
	for (FBudgetClientState& State : States)
	{
		State.Deadline = State.LastUpdateTime + UpdateBudget::UnscoredMaxStalenessSeconds;
	}

	// The last frame's delta estimates when the next frame starts (02 section 6); with a fixed step it is exact.
	bRunningScheduler = true;
	Scheduler.Run(States, Now + DeltaSeconds, UpdateBudget::DefaultBudgetMs, [this](int32 Index) { return RunClient(Index); });
	bRunningScheduler = false;

	// Changes made by the updates themselves, applied now that nothing reads the arrays.
	if (bHasInvalidClients)
	{
		RemoveInvalidClients();
	}
	for (UBudgetedUpdateComponent* Client : PendingAdds)
	{
		if (IsValid(Client))
		{
			AddClient(Client);
		}
		else
		{
			DetachClient(Client);   // became garbage while waiting: it must not look registered
		}
	}
	PendingAdds.Reset();
}

double UUpdateBudgetSubsystem::RunClient(int32 Index)
{
	UBudgetedUpdateComponent* Client = Clients[Index];
	if (!IsValid(Client))
	{
		bHasInvalidClients = true;   // unregistered during this loop, or garbage that never reached EndPlay
		return 0.0;
	}

	// The measured cost is what the budget is spent on. QueryPerformanceCounter on Windows (WindowsPlatformTime.h:39).
	const uint64 StartCycles = FPlatformTime::Cycles64();
	Client->RunUpdate();
	return FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);
}

void UUpdateBudgetSubsystem::Deinitialize()
{
	TickFunction.UnRegisterTickFunction();
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);

	// Normally every client has already unregistered in EndPlay (01b #7). This covers any that haven't.
	for (const TArray<TObjectPtr<UBudgetedUpdateComponent>>* Registry : { &Clients, &PendingAdds })
	{
		for (UBudgetedUpdateComponent* Client : *Registry)
		{
			DetachClient(Client);
		}
	}
	Clients.Empty();
	States.Empty();
	PendingAdds.Empty();

	Super::Deinitialize();
}

void UUpdateBudgetSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// One value per frame per stat. The CSV profiler's own startup benchmark puts CSV_CUSTOM_STAT at about 50 ns,
	// which is why updates are counted in plain fields and published once per frame, not once per update.
	CSV_CUSTOM_STAT(UpdateBudget, Clients, Clients.Num(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(UpdateBudget, UpdatesRun, FrameUpdates, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(UpdateBudget, MaxGapMs, FrameMaxGapSeconds * 1000.0, ECsvCustomStatOp::Set);

	FrameUpdates = 0;
	FrameMaxGapSeconds = 0.0;
}

TStatId UUpdateBudgetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUpdateBudgetSubsystem, STATGROUP_Tickables);
}
