// UpdateBudgetCore - UUpdateBudgetSubsystem.

#include "UpdateBudgetSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UpdateBudgetSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogUpdateBudget, Log, All);

// CSV category "UpdateBudget", enabled by default so a capture always contains it.
CSV_DEFINE_CATEGORY(UpdateBudget, true);

// "stat UpdateBudget" in a Development build.
DECLARE_STATS_GROUP(TEXT("UpdateBudget"), STATGROUP_UpdateBudget, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("RunFrame"), STAT_UpdateBudget_RunFrame, STATGROUP_UpdateBudget);
DECLARE_CYCLE_STAT(TEXT("Deadlines"), STAT_UpdateBudget_Deadlines, STATGROUP_UpdateBudget);

// ECVF_Default, not a cheat cvar: cheat cvars can't be set from ini files (01b #11). To start a run in budgeted
// mode, set it before the world begins play, e.g. -ini:Engine:[ConsoleVariables]:ub.Enabled=1 on the command line.
static TAutoConsoleVariable<int32> CVarUpdateBudgetEnabled(
	TEXT("ub.Enabled"),
	0,
	TEXT("UpdateBudget mode. 0: native ticks (baselines). 1: budgeted (the scheduler runs the updates). Read when a world begins play; a change switches running worlds at the end of the next frame that ticks actors (not while paused)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarUpdateBudgetMs(
	TEXT("ub.BudgetMs"),
	-1.f,
	TEXT("UpdateBudget: game-thread ms per frame for optional updates. Negative: use Project Settings > Update Budget > Budget Ms."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarUpdateBudgetDebug(
	TEXT("ub.Debug"),
	0,
	TEXT("UpdateBudget debug view. 0: off. 1: one on-screen line per world with this frame's stats. 2: also a point above every client, green when just updated, red at the longest allowed staleness."),
	ECVF_Default);

bool UUpdateBudgetSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UUpdateBudgetSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	bBudgeted = CVarUpdateBudgetEnabled.GetValueOnGameThread() != 0;

	// Settings are read once per world, so a run can't change halfway; the cvar can still override the budget.
	const UUpdateBudgetSettings* Settings = GetDefault<UUpdateBudgetSettings>();
	Categories = Settings->Categories;
	if (Categories.IsEmpty())
	{
		Categories.Add(FUpdateBudgetCategory());   // an ini that empties the array still leaves one valid category
	}
	SettingsBudgetMs = Settings->BudgetMs;
	Scheduler = FUpdateBudgetScheduler(Categories.Num());   // one cost prediction per category

	LongestMaxStaleness = 0.f;
	for (const FUpdateBudgetCategory& Category : Categories)
	{
		LongestMaxStaleness = FMath::Max(LongestMaxStaleness, FMath::Max(Category.NearMaxStaleness, Category.FarMaxStaleness));
		UE_CLOG(Category.IsInverted(), LogUpdateBudget, Warning,
			TEXT("Category '%s' is inverted (near %.0f cm / far %.0f cm, near %.3f s / far %.3f s): far clients would be at least as fresh as near ones."),
			*Category.Name.ToString(), Category.NearDistance, Category.FarDistance, Category.NearMaxStaleness, Category.FarMaxStaleness);
	}

	// Normally nobody has registered yet (OnWorldBeginPlay runs before any actor's BeginPlay, 01b #7); if someone has,
	// the mode must reach them too. This also sets the tick function's enable state before it is registered, as Niagara
	// does. Registered in every mode, so a later switch only has to enable it; a disabled tick function costs nothing
	// per frame (the tick manager walks only its enabled set).
	ApplyModeToClients();
	TickFunction.Subsystem = this;
	TickFunction.RegisterTickFunction(InWorld.PersistentLevel);

	// GC nulls the entry of a component collected without EndPlay. Removing such entries right after each collection
	// keeps the registry exact in every mode; otherwise only the scheduler's loop would ever find them.
	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UUpdateBudgetSubsystem::HandlePostGarbageCollect);

	// Live switching (S7): every world's subsystem listens to the cvar itself.
	EnabledChangedHandle = CVarUpdateBudgetEnabled->OnChangedDelegate().AddUObject(this, &UUpdateBudgetSubsystem::HandleEnabledChanged);

	UE_LOG(LogUpdateBudget, Display, TEXT("%s mode (ub.Enabled %d). Budget %.2f ms (settings %.2f, ub.BudgetMs %.2f), %d categories. Update timer resolution: %.1f ns."),
		GetModeName(bBudgeted), CVarUpdateBudgetEnabled.GetValueOnGameThread(), ResolveBudgetMs(), SettingsBudgetMs,
		CVarUpdateBudgetMs.GetValueOnGameThread(), Categories.Num(), FPlatformTime::GetSecondsPerCycle64() * 1.0e9);
	CSV_METADATA(TEXT("UBMode"), GetModeName(bBudgeted));
	// The staleness bound of this run's settings, so an analysis never has to assume one (SC2).
	CSV_METADATA(TEXT("UBMaxStaleness"), *FString::SanitizeFloat(LongestMaxStaleness));
}

void UUpdateBudgetSubsystem::HandleEnabledChanged(IConsoleVariable* Variable)
{
	// Not now: the change may come from inside an update, and switching mid-loop could run a client both natively and
	// from the scheduler in one frame. Bound only while a switch is pending, so the steady state costs nothing.
	if (!PostActorTickHandle.IsValid())
	{
		PostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UUpdateBudgetSubsystem::HandlePostActorTick);
	}
}

void UUpdateBudgetSubsystem::HandlePostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	// The tick manager's frame is over (LevelTick.cpp:1886 before 1906): a tick function enabled now first runs next
	// frame, and no tick group of this frame can pick it up again.
	if (World != GetWorld())
	{
		return;
	}
	FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
	PostActorTickHandle.Reset();

	const bool bWanted = CVarUpdateBudgetEnabled.GetValueOnGameThread() != 0;
	if (bWanted != bBudgeted)
	{
		SetBudgeted(bWanted);
	}
}

void UUpdateBudgetSubsystem::ApplyModeToClients()
{
	for (UBudgetedUpdateComponent* Client : Clients)
	{
		if (Client)
		{
			Client->SetDrivenByScheduler(bBudgeted);
		}
	}
	TickFunction.SetTickFunctionEnable(bBudgeted);
}

void UUpdateBudgetSubsystem::SetBudgeted(bool bInBudgeted)
{
	bBudgeted = bInBudgeted;
	ApplyModeToClients();

	// Every client was updated this frame by the old mode, and is updated from the next frame by the new one. The hot
	// state already holds every client's last update time (RecordUpdate runs in both modes), so the deadlines are right.
	CSV_EVENT(UpdateBudget, TEXT("ModeSwitch %s"), GetModeName(bBudgeted));
	CSV_METADATA(TEXT("UBMode"), TEXT("Mixed"));   // a capture that spans a switch is neither mode: analysis must split it
	UE_LOG(LogUpdateBudget, Display, TEXT("Switched to %s mode at %.3f s, %d clients."),
		GetModeName(bBudgeted), GetWorld()->GetTimeSeconds(), Clients.Num());
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

	// A Blueprint's Event Tick is called from the native tick only (ActorComponent.cpp:1886-1888), so such a client does
	// more work in the native mode than in the budgeted one. Said once per class and world; native classes answer false
	// at once (Class.cpp:6525-6529).
	const UClass* Class = Client->GetClass();
	if (Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UBudgetedUpdateComponent, ReceiveTick)))
	{
		bool bAlreadyWarned = false;
		EventTickClassesWarned.Add(Class->GetClassPathName(), &bAlreadyWarned);
		UE_CLOG(!bAlreadyWarned, LogUpdateBudget, Warning,
			TEXT("%s implements Event Tick, which runs only while the component ticks natively, not when the scheduler drives it. Put the logic in the Budgeted Update event, which runs in every mode."),
			*Class->GetPathName());
	}

	// Applied at once, even when the add itself is deferred: a component spawned during the loop has just had its
	// native tick registered (Actor.cpp:4824), and it must not tick natively in a later group of this frame.
	// Applied in both modes: a client that was scheduler-driven elsewhere gets its native tick back here.
	Client->SetDrivenByScheduler(bBudgeted);

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

void UUpdateBudgetSubsystem::AddClient(UBudgetedUpdateComponent* Client)
{
	Client->RegistryIndex = Clients.Add(Client);
	FBudgetClientState& State = States.AddDefaulted_GetRef();
	State.LastUpdateTime = Client->GetLastUpdateTime();
	State.CategoryIndex = Client->GetBudgetCategory();
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

double UUpdateBudgetSubsystem::ResolveBudgetMs() const
{
	const float Override = CVarUpdateBudgetMs.GetValueOnGameThread();
	return (Override >= 0.f) ? Override : SettingsBudgetMs;
}

void UUpdateBudgetSubsystem::CountNearClients(double Now)
{
	// From the outcome, after the loop and after invalid entries are gone: a near client is missed if it wasn't updated
	// at this frame's time. One dense pass over States (SC3).
	Frame.NearClients = 0;
	Frame.NearMissed = 0;
	for (const FBudgetClientState& State : States)
	{
		if (State.Significance >= 1.f)
		{
			++Frame.NearClients;
			Frame.NearMissed += (State.LastUpdateTime < Now) ? 1 : 0;
		}
	}
}

void UUpdateBudgetSubsystem::UpdateDeadlines()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateBudget_Deadlines);
	CSV_SCOPED_TIMING_STAT(UpdateBudget, Deadlines);

	// One view for the whole loop: the player camera's cached view, which the demo brains and the B2/B3 drivers use too.
	// Without one (no local player, e.g. a test world or a server) every client keeps significance 1: nothing goes stale.
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* Camera = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr;
	const FUpdateBudgetView View = Camera ? FUpdateBudgetView::FromCamera(Camera->GetCameraCacheView()) : FUpdateBudgetView();

	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		FBudgetClientState& State = States[Index];
		const FUpdateBudgetCategory& Category = GetCategory(State.CategoryIndex);
		float Significance = 1.f;
		const UBudgetedUpdateComponent* Client = Clients[Index];
		const AActor* Owner = Client ? Client->GetOwner() : nullptr;
		if (Camera && Owner)
		{
			const FVector ToClient = Owner->GetActorLocation() - View.Location;
			const double DistanceSquared = ToClient.SizeSquared();
			Significance = Category.GetSignificance(DistanceSquared, View.IsInView(ToClient, DistanceSquared));
		}
		State.Significance = Significance;
		// A client never updated has LastUpdateTime < 0, so its deadline is in the past: it runs this frame.
		State.Deadline = State.LastUpdateTime + Category.GetMaxStaleness(Significance);
	}
}

void UUpdateBudgetSubsystem::RunFrame(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateBudget_RunFrame);
	CSV_SCOPED_TIMING_STAT(UpdateBudget, RunFrame);

	const double Now = GetWorld()->GetTimeSeconds();
	UpdateDeadlines();

	Frame.BudgetMs = ResolveBudgetMs();

	// The last frame's delta estimates when the next frame starts (02 section 6); with a fixed step it is exact.
	bRunningScheduler = true;
	Frame.Scheduler = Scheduler.Run(States, Now + DeltaSeconds, Frame.BudgetMs, [this](int32 Index) { return RunClient(Index); });
	bRunningScheduler = false;

	// Changes made by the updates themselves, applied now that nothing reads the arrays.
	if (bHasInvalidClients)
	{
		RemoveInvalidClients();
	}
	CountNearClients(Now);   // before the adds: a client added now hasn't had its first frame yet
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
	CVarUpdateBudgetEnabled->OnChangedDelegate().Remove(EnabledChangedHandle);
	FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);

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
	Frame.bBudgeted = bBudgeted;
	Frame.Clients = Clients.Num();

	CSV_CUSTOM_STAT(UpdateBudget, Clients, Frame.Clients, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(UpdateBudget, UpdatesRun, Frame.Updates, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(UpdateBudget, MaxGapMs, Frame.MaxGapSeconds * 1000.0, ECsvCustomStatOp::Set);
	if (bBudgeted)
	{
		// The scheduler's own numbers exist only in budgeted mode; native captures don't get columns of zeros.
		CSV_CUSTOM_STAT(UpdateBudget, UpdateMs, Frame.Scheduler.SpentMs, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(UpdateBudget, BudgetMs, Frame.BudgetMs, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(UpdateBudget, MandatoryRuns, Frame.Scheduler.MandatoryRuns, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(UpdateBudget, MandatoryMs, Frame.Scheduler.MandatoryMs, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(UpdateBudget, Deferred, Frame.Scheduler.Deferred, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(UpdateBudget, OverBudget, Frame.Scheduler.bOverBudget ? 1 : 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(UpdateBudget, NearClients, Frame.NearClients, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(UpdateBudget, NearMissed, Frame.NearMissed, ECsvCustomStatOp::Set);
	}

	OnFrameStats.Broadcast(Frame);
	const int32 DebugLevel = CVarUpdateBudgetDebug.GetValueOnGameThread();
	if (DebugLevel > 0)
	{
		ShowDebugStats();
	}
	if (DebugLevel > 1)
	{
		DrawClientRecency();
	}

	Frame = FUpdateBudgetFrameReport();
}

void UUpdateBudgetSubsystem::ShowDebugStats() const
{
	if (!GEngine)
	{
		return;
	}
	const FString Line = Frame.bBudgeted
		? FString::Printf(TEXT("UpdateBudget: budgeted | %d clients | %d updates (%d mandatory, %d deferred) | %.2f of %.2f ms%s | max gap %.0f ms | near %d, missed %d"),
			Frame.Clients, Frame.Updates, Frame.Scheduler.MandatoryRuns, Frame.Scheduler.Deferred, Frame.Scheduler.SpentMs, Frame.BudgetMs,
			Frame.Scheduler.bOverBudget ? TEXT(" OVER") : TEXT(""), Frame.MaxGapSeconds * 1000.0, Frame.NearClients, Frame.NearMissed)
		: FString::Printf(TEXT("UpdateBudget: native | %d clients | %d updates | max gap %.0f ms"),
			Frame.Clients, Frame.Updates, Frame.MaxGapSeconds * 1000.0);
	// Keyed per subsystem, so the line is replaced every frame; a time of 0 shows it for one frame.
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, Frame.Scheduler.bOverBudget ? FColor::Orange : FColor::Green, Line);
}

void UUpdateBudgetSubsystem::DrawClientRecency() const
{
#if ENABLE_DRAW_DEBUG
	// Age against the longest staleness any category allows, the same scale in both modes: green = updated this frame,
	// red = as stale as the settings allow. One point per client, drawn for one frame.
	const UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	const double Scale = FMath::Max(static_cast<double>(LongestMaxStaleness), 1.0e-3);
	for (int32 Index = 0; Index < Clients.Num(); ++Index)
	{
		const UBudgetedUpdateComponent* Client = Clients[Index];
		const AActor* Owner = Client ? Client->GetOwner() : nullptr;
		const double LastUpdate = States[Index].LastUpdateTime;
		if (!Owner || LastUpdate < 0.0)
		{
			continue;
		}
		// The engine's own green-yellow-red scale (Color.cpp:569-577), which clamps its input.
		const FColor Color = FColor::MakeRedToGreenColorFromScalar(1.f - static_cast<float>((Now - LastUpdate) / Scale));
		DrawDebugPoint(World, Owner->GetActorLocation() + FVector(0.0, 0.0, 60.0), 8.f, Color);
	}
#endif
}

TStatId UUpdateBudgetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUpdateBudgetSubsystem, STATGROUP_Tickables);
}
