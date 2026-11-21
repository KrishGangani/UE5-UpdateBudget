// P01 UpdateBudget (demo) - drives the two "smarter native tick" baselines: B2 (distance intervals) and B3 (Significance Manager).

#pragma once

#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UBBaselineBands.h"
#include "UBBaselineDriverSubsystem.generated.h"

class AActor;
class USignificanceManager;
class UUBBenchmarkSubsystem;
class UUBDemoBrainComponent;

/**
 * The common hand-made fixes for "too many ticking actors", implemented the way a team would write them,
 * so UpdateBudget is compared against good native practice, not only against doing nothing.
 *
 *   -UBBaseline=B2  every frame: distance from the camera -> a TickInterval band (0 s / 0.1 s / 0.5 s by default)
 *   -UBBaseline=B3  every frame: USignificanceManager::Update(camera) -> significance -> the same bands,
 *                   applied in a Sequential post-significance callback (Epic's intended usage)
 *
 * Both read their bands from UpdateBudget's settings, the category the brains use (FUBBaselineBands::FromCategory):
 * the same near/far distances (15 m / 60 m by default) and the same staleness limits at either end. Both share one
 * edge test (BandForDistanceSquared), and both call SetComponentTickIntervalAndCooldown only when an agent's band
 * changes, because that setter applies at once (the plain setter waits out the old interval).
 *
 * Cost accounting (hypothesis H6): "UBBaseline/Retune" times the per-frame band work and "UBBaseline/Register"
 * times adoption. A further cost lives inside the engine's tick manager: an agent that has ever had an interval
 * stays on the cooling-down list and is rescheduled every frame (TickTaskManager.cpp:1538), even after returning
 * to 0. That cost is inside GameThreadTime (so comparisons stay fair) and is examined with Unreal Insights.
 *
 * View source: the player camera manager, which is the same source the brains sense from, so every consumer
 * (brains, B2, B3, and later UB) sees the identical view.
 */
UCLASS()
class UUBBaselineDriverSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	/** Initialises the benchmark subsystem first, so it is registered, and therefore ticks, before this one. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem

	//~ Begin UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	/** Unregisters from the Significance Manager while it still exists (it is removed during world cleanup). */
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/**
	 * The bands B2 and B3 use: UpdateBudget's settings category for the brain class's default category, by UpdateBudget's
	 * own rule. Every spawned brain uses the class default; per-instance categories would need one set of bands each.
	 */
	static FUBBaselineBands ReadBands();

protected:
	//~ Begin UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	enum class EMode : uint8
	{
		None,
		DistanceIntervals,     // B2
		SignificanceIntervals  // B3
	};

	/** The Significance Manager in B3 mode; null otherwise. */
	USignificanceManager* GetSignificanceManager() const;

	/** The player camera's transform; false if there is no camera yet. */
	bool GetViewTransform(FTransform& OutView) const;

	/** Picks up agents spawned since the last frame (the initial set, and the wave). */
	void AdoptNewAgents();

	/** The per-frame band work: B2's distance loop, or B3's USignificanceManager::Update. */
	void Retune();

	/** Applies an interval only when it changes. Counts each change for UBBaseline/IntervalChanges. */
	void SetIntervalIfChanged(UUBDemoBrainComponent& Brain, float Interval);

	/** Removes an agent that ended play (destroyed or streamed out) from the baseline's bookkeeping. */
	UFUNCTION()
	void HandleAgentEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	EMode Mode = EMode::None;

	/** Set in OnWorldBeginPlay (ReadBands). */
	FUBBaselineBands Bands;

	int32 AdoptedAgents = 0;
	int32 FrameIntervalChanges = 0;

	/** Set in Initialize (InitializeDependency returns it). */
	UPROPERTY(Transient)
	TObjectPtr<UUBBenchmarkSubsystem> Benchmark;

	/** B2 only: the brains it re-tunes. B3's are held by the Significance Manager. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUBDemoBrainComponent>> Brains;
};
