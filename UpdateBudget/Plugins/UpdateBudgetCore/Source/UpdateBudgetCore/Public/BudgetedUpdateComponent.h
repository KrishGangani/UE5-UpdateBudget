// UpdateBudgetCore - the client side: one gameplay update that can be driven natively or by the scheduler.

#pragma once

#include "Components/ActorComponent.h"
#include "BudgetedUpdateComponent.generated.h"

#define UE_API UPDATEBUDGETCORE_API

class UUpdateBudgetSubsystem;

/**
 * A gameplay update that runs either from this component's own tick (native baselines B0-B3)
 * or from the UpdateBudget scheduler (UB).
 *
 * Subclasses put their logic in OnBudgetedUpdate(). The same function runs in every mode,
 * so every benchmark configuration does identical gameplay work; only the caller changes.
 *
 * Time contract (owned here, so subclasses cannot get it wrong):
 *   DeltaSeconds = world time since this component's previous update, and 0 on its first update.
 *   It is measured from UWorld::GetTimeSeconds() in every mode; the engine's per-tick DeltaTime is ignored.
 *
 * Registry membership follows the component's life in the world: it joins in BeginPlay (and again if it is
 * re-registered while playing) and leaves in OnUnregister or EndPlay, whichever comes first. The engine registers
 * and unregisters the native tick function at those same moments, so both modes stop updating it together.
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

	/** True while this component is in an UpdateBudget registry. */
	bool IsRegisteredWithBudget() const { return Subsystem != nullptr; }

	/** True when the scheduler runs this component's updates (budgeted mode) and its native tick is off. */
	bool IsDrivenByScheduler() const { return bDrivenByScheduler; }

	/** Index into the settings' significance categories (Project Settings > Update Budget). Read when the component registers. */
	int32 GetBudgetCategory() const { return BudgetCategory; }

	//~ Begin UActorComponent
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Gameplay's switch for the native tick (Activate, Deactivate and Blueprint calls end here). The choice is kept in
	 * every mode; while the scheduler drives the component the native tick stays off, and the choice is applied when
	 * the scheduler hands the component back.
	 */
	UE_API virtual void SetComponentTickEnabled(bool bEnabled) override;
	//~ End UActorComponent

protected:
	/** The gameplay work. Override in subclasses. See the time contract above for DeltaSeconds. */
	virtual void OnBudgetedUpdate(float DeltaSeconds) {}

	/** Which significance category this client uses: an index into the settings' Categories (out of range means 0). */
	UPROPERTY(EditAnywhere, Category = "UpdateBudget", meta = (ClampMin = "0"))
	int32 BudgetCategory = 0;

	//~ Begin UActorComponent
	UE_API virtual void OnRegister() override;
	UE_API virtual void OnUnregister() override;
	//~ End UActorComponent

private:
	// The subsystem owns the link (Subsystem + RegistryIndex) and the mode: it sets both in Register,
	// clears the link in Unregister, and is the only caller of SetDrivenByScheduler.
	friend class UUpdateBudgetSubsystem;

	/** Registers with the world's UpdateBudget subsystem, if the world has one (Game and PIE worlds only). */
	void JoinBudget();

	/** Leaves the registry, if registered. Safe to call more than once. */
	void LeaveBudget();

	/**
	 * Switches between scheduler-driven updates (native tick off) and native ticking. Does nothing if already in that
	 * mode. Coming back to native restores the tick state gameplay chose, so a live switch there and back changes nothing.
	 */
	void SetDrivenByScheduler(bool bInDrivenByScheduler);

	UPROPERTY(Transient)
	TObjectPtr<UUpdateBudgetSubsystem> Subsystem;

	int32 RegistryIndex = INDEX_NONE;

	/** World time of the last update; negative until the first update. */
	double LastUpdateTime = -1.0;

	bool bDrivenByScheduler = false;

	/** Gameplay's tick choice and the start flag, kept while the scheduler drives the component; restored after. */
	bool bNativeTickEnabled = true;
	bool bNativeStartWithTickEnabled = true;
};

#undef UE_API
