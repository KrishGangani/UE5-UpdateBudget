// UpdateBudgetCore - the world-level registry (step S1: registry + per-frame telemetry; scheduling arrives in S5).

#pragma once

#include "BudgetedUpdateComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "UpdateBudgetSubsystem.generated.h"

#define UE_API UPDATEBUDGETCORE_API

/**
 * Owns the registry of UBudgetedUpdateComponents in one world and publishes per-frame telemetry.
 *
 * It is a *tickable* world subsystem because its own Tick runs after TG_PostPhysics (01b section 2):
 * by then every TG_PrePhysics update of the frame has finished, so the frame's totals are complete.
 * Scheduling (step S5) uses a separate tick function in TG_PrePhysics.
 *
 * Limitation: CSV stats use ECsvCustomStatOp::Set, so if several game worlds tick in one process
 * (multi-client PIE), the capture shows the last world's values. Benchmarks run one standalone world.
 */
UCLASS(MinimalAPI)
class UUpdateBudgetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Links a client to this subsystem (sets its Subsystem and RegistryIndex). Called from the client's BeginPlay. */
	UE_API void Register(UBudgetedUpdateComponent* Client);

	/** Removes a client in O(1) and clears its link. Called from the client's EndPlay. */
	UE_API void Unregister(UBudgetedUpdateComponent* Client);

	/**
	 * The single per-update sync point, called in every mode right after the client stamped its new update time.
	 * Inline because it runs once per update (thousands of times per frame) and is only a few instructions.
	 * @param PreviousUpdateTime The client's update time before this one; negative for its first update (counted, no gap).
	 */
	void RecordUpdate(const UBudgetedUpdateComponent& Client, double PreviousUpdateTime)
	{
		++FrameUpdates;
		if (PreviousUpdateTime >= 0.0)
		{
			FrameMaxGapSeconds = FMath::Max(FrameMaxGapSeconds, Client.GetLastUpdateTime() - PreviousUpdateTime);
		}
	}

	int32 GetNumClients() const { return Clients.Num(); }

	//~ Begin USubsystem
	/** Detaches every client still registered, so none keeps a reference to a subsystem that is going away. */
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
	/** Swap-removes the entry at Index and fixes the moved entry's index. Tolerates a null moved entry. */
	void RemoveClientAt(int32 Index);

	/**
	 * Every registered client. A UPROPERTY so the garbage collector sees these references.
	 * An entry can become null if GC collects a component that never reached EndPlay; every access tolerates that.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBudgetedUpdateComponent>> Clients;

	/** This frame's totals, published and reset in Tick(). */
	int32 FrameUpdates = 0;
	double FrameMaxGapSeconds = 0.0;
};

#undef UE_API
