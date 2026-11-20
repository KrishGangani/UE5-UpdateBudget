// UpdateBudgetCore - the world-level registry and the mechanism that runs the scheduling policy each frame.

#pragma once

#include "BudgetedUpdateComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "UpdateBudgetScheduler.h"
#include "UpdateBudgetSettings.h"
#include "UpdateBudgetTickFunction.h"
#include "UpdateBudgetSubsystem.generated.h"

#define UE_API UPDATEBUDGETCORE_API

/**
 * Owns the registry of UBudgetedUpdateComponents in one world, runs their updates in budgeted mode,
 * and publishes per-frame telemetry.
 *
 * Mode (cvar ub.Enabled, read once in OnWorldBeginPlay):
 *   native (0)    every client updates from its own native tick, as in the baselines B0-B3;
 *   budgeted (1)  native ticks are off, and FUpdateBudgetTickFunction calls RunFrame() in TG_PrePhysics: every
 *                 client's significance (distance and view from the player camera) gives its max staleness and
 *                 deadline, and FUpdateBudgetScheduler runs the due clients, then the rest while the budget lasts.
 * Settings (UUpdateBudgetSettings) are read once in OnWorldBeginPlay; cvar ub.BudgetMs overrides the budget per frame.
 *
 * Registry: two parallel arrays with the same index. Clients is a UPROPERTY (seen by GC); States is the hot data
 * the per-frame loops read without touching a UObject (H5). While the scheduler runs, adds and removals are
 * deferred, because it reads States through a raw pointer: a removal nulls the slot and an add waits in PendingAdds.
 * Both are applied right after the loop.
 *
 * It is a *tickable* world subsystem because its own Tick runs after TG_PostPhysics (01b section 2): by then every
 * TG_PrePhysics update of the frame has finished, so the frame's totals are complete.
 *
 * Limitation: CSV stats use ECsvCustomStatOp::Set, so if several game worlds tick in one process
 * (multi-client PIE), the capture shows the last world's values. Benchmarks run one standalone world.
 */
UCLASS(MinimalAPI)
class UUpdateBudgetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Links a client to this subsystem and applies the mode to it. Called from the client's BeginPlay (or OnRegister). */
	UE_API void Register(UBudgetedUpdateComponent* Client);

	/** Removes a client in O(1) (deferred while the scheduler runs) and clears its link. Called from EndPlay or OnUnregister. */
	UE_API void Unregister(UBudgetedUpdateComponent* Client);

	/**
	 * The single per-update sync point, called in every mode right after the client stamped its new update time.
	 * It mirrors that time into the client's hot state, so the deadline loop never reads the component.
	 * Inline because it runs once per update (thousands of times per frame) and is only a few instructions.
	 * @param PreviousUpdateTime The client's update time before this one; negative for its first update (counted, no gap).
	 */
	void RecordUpdate(const UBudgetedUpdateComponent& Client, double PreviousUpdateTime)
	{
		++FrameUpdates;
		const double UpdateTime = Client.GetLastUpdateTime();
		if (PreviousUpdateTime >= 0.0)
		{
			FrameMaxGapSeconds = FMath::Max(FrameMaxGapSeconds, UpdateTime - PreviousUpdateTime);
		}
		// INDEX_NONE while the client waits in PendingAdds; it is copied from the client when the add is applied.
		if (Client.RegistryIndex != INDEX_NONE)
		{
			States[Client.RegistryIndex].LastUpdateTime = UpdateTime;
		}
	}

	/** Registered clients. Between frames this counts no removed or collected ones. */
	int32 GetNumClients() const { return Clients.Num(); }

	/** True in budgeted mode: the scheduler runs the updates and native ticks are off. */
	bool IsBudgeted() const { return bBudgeted; }

	//~ Begin UWorldSubsystem
	/** Reads the mode, applies it, and registers the scheduler's tick function (01b #2: never in Initialize). */
	UE_API virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem

	//~ Begin USubsystem
	/** Unregisters the tick function and the GC hook, and detaches every client still registered. */
	UE_API virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin FTickableGameObject
	UE_API virtual void Tick(float DeltaTime) override;
	UE_API virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only: never editor or preview worlds (01b #2). */
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	friend struct FUpdateBudgetTickFunction;

	/** One frame of budgeted updates: deadlines, the policy, then the adds and removals deferred during the loop. */
	void RunFrame(float DeltaSeconds);

	/** Step 1: every client's significance and deadline, from the dense States and each owner's location. */
	void UpdateDeadlines();

	/** The category a client uses; an index out of range means category 0 (never empty). */
	const FUpdateBudgetCategory& GetCategory(int32 Index) const
	{
		return Categories.IsValidIndex(Index) ? Categories[Index] : Categories[0];
	}

	/** The budget in effect: ub.BudgetMs when it is >= 0, otherwise the settings value. */
	double ResolveBudgetMs() const;

	/** After the loop: how many clients were near (significance 1) and how many of them weren't updated this frame (SC3). */
	void CountNearClients(double Now);

	/** ub.Debug 1: one on-screen line with this frame's numbers. */
	void ShowDebugStats() const;

	/** Runs one client for the policy and returns its measured cost in ms (0 for an entry that is no longer valid). */
	double RunClient(int32 Index);

	/** Makes the client scheduler-driven in budgeted mode and native otherwise; does nothing if it already is. */
	void ApplyMode(UBudgetedUpdateComponent& Client) const;

	/** Appends a client to both arrays; its hot state starts from the client's own last update time. */
	void AddClient(UBudgetedUpdateComponent* Client);

	/** Swap-removes the entry at Index from both arrays and fixes the moved entry's index. Tolerates a null moved entry. */
	void RemoveClientAt(int32 Index);

	/** Removes every entry that is null or no longer valid (nulled during the loop, or collected without EndPlay). */
	void RemoveInvalidClients();

	/** After each garbage collection: removes the entries GC nulled (deferred if a collection runs inside the loop). */
	void HandlePostGarbageCollect();

	/** Clears a client's link to this subsystem. Accepts null (an entry GC has already cleared). */
	static void DetachClient(UBudgetedUpdateComponent* Client);

	/**
	 * Every registered client. A UPROPERTY so the garbage collector sees these references.
	 * An entry can become null if GC collects a component that never reached EndPlay; every access tolerates that.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBudgetedUpdateComponent>> Clients;

	/** Hot per-client data, same index as Clients. */
	TArray<FBudgetClientState> States;

	/** Clients registered while the scheduler was running; added after the loop. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBudgetedUpdateComponent>> PendingAdds;

	FUpdateBudgetScheduler Scheduler;
	FUpdateBudgetTickFunction TickFunction;
	FDelegateHandle PostGarbageCollectHandle;

	/** The settings' categories, copied in OnWorldBeginPlay (at least one). */
	TArray<FUpdateBudgetCategory> Categories;

	/** The settings' budget, copied in OnWorldBeginPlay; ub.BudgetMs >= 0 overrides it. */
	double SettingsBudgetMs = 2.0;

	bool bBudgeted = false;

	/** True while the scheduler is running clients: registry changes are deferred. */
	bool bRunningScheduler = false;

	/** Set when the loop met a null or invalid entry (or GC ran inside it); RunFrame removes them after the loop. */
	bool bHasInvalidClients = false;

	/** This frame's totals, published and reset in Tick(). The last four exist only in budgeted mode. */
	int32 FrameUpdates = 0;
	double FrameMaxGapSeconds = 0.0;
	FBudgetFrameStats FrameStats;
	double FrameBudgetMs = 0.0;
	int32 FrameNearClients = 0;     // significance 1 (near and in view) when the deadlines were set
	int32 FrameNearMissed = 0;      // ...and not updated this frame: SC3's "near client missed"
};

#undef UE_API
