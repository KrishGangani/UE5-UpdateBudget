// UpdateBudgetCore - UUpdateBudgetSubsystem.

#include "UpdateBudgetSubsystem.h"

#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UpdateBudgetSubsystem)

// CSV category "UpdateBudget", enabled by default so a capture always contains it.
CSV_DEFINE_CATEGORY(UpdateBudget, true);

bool UUpdateBudgetSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UUpdateBudgetSubsystem::Register(UBudgetedUpdateComponent* Client)
{
	check(Client);
	if (Client->RegistryIndex != INDEX_NONE)
	{
		return; // Already registered.
	}
	Client->Subsystem = this;
	Client->RegistryIndex = Clients.Add(Client);
}

void UUpdateBudgetSubsystem::Unregister(UBudgetedUpdateComponent* Client)
{
	check(Client);
	const int32 Index = Client->RegistryIndex;
	if (Clients.IsValidIndex(Index) && Clients[Index] == Client)
	{
		RemoveClientAt(Index);
	}
	Client->RegistryIndex = INDEX_NONE;
	Client->Subsystem = nullptr;
}

void UUpdateBudgetSubsystem::RemoveClientAt(int32 Index)
{
	// O(1): the last entry moves into the freed slot. It may be null (collected without EndPlay).
	Clients.RemoveAtSwap(Index, EAllowShrinking::No);
	if (Clients.IsValidIndex(Index) && Clients[Index])
	{
		Clients[Index]->RegistryIndex = Index;
	}
}

void UUpdateBudgetSubsystem::Deinitialize()
{
	// Normally every client has already unregistered in EndPlay (01b #7). This covers any that haven't.
	for (UBudgetedUpdateComponent* Client : Clients)
	{
		if (Client)
		{
			Client->RegistryIndex = INDEX_NONE;
			Client->Subsystem = nullptr;
		}
	}
	Clients.Empty();

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
