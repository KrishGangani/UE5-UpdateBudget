// UpdateBudgetCore - the client side: one gameplay update that can be driven natively or by the scheduler.

#pragma once

#include "Components/ActorComponent.h"
#include "BudgetedUpdateComponent.generated.h"

#define UE_API UPDATEBUDGETCORE_API

class UUpdateBudgetSubsystem;

/**
 * A gameplay update that runs either from this component's own tick (native baselines B0-B3)
 * or from the UpdateBudget scheduler (UB, added in step S5).
 *
 * Subclasses put their logic in OnBudgetedUpdate(). The same function runs in every mode,
 * so every benchmark configuration does identical gameplay work; only the caller changes.
 *
 * Time contract (owned here, so subclasses cannot get it wrong):
 *   DeltaSeconds = world time since this component's previous update, and 0 on its first update.
 *   It is measured from UWorld::GetTimeSeconds() in every mode; the engine's per-tick DeltaTime is ignored.
 */
UCLASS(MinimalAPI, Abstract, ClassGroup = (UpdateBudget))
class UBudgetedUpdateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API UBudgetedUpdateComponent();

	/** Runs one update now: stamps the update time, reports it to the subsystem, then calls OnBudgetedUpdate(). */
	UE_API void RunUpdate();

	/** World time (seconds) of the last update, or a negative value before the first one. */
	double GetLastUpdateTime() const { return LastUpdateTime; }

	//~ Begin UActorComponent
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

protected:
	/** The gameplay work. Override in subclasses. See the time contract above for DeltaSeconds. */
	virtual void OnBudgetedUpdate(float DeltaSeconds) {}

private:
	// The subsystem owns the link (Subsystem + RegistryIndex): it sets both in Register and clears both in Unregister.
	friend class UUpdateBudgetSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UUpdateBudgetSubsystem> Subsystem;

	int32 RegistryIndex = INDEX_NONE;

	/** World time of the last update; negative until the first update. */
	double LastUpdateTime = -1.0;
};

#undef UE_API
