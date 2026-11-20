// P01 UpdateBudget (demo) - drives the two "smarter native tick" baselines: B2 (distance intervals) and B3 (Significance Manager).

#pragma once

#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UBBaselineDriverSubsystem.generated.h"

class AActor;
class USignificanceManager;
class UUBBenchmarkSubsystem;
class UUBDemoBrainComponent;

/**
 * The common hand-made fixes for "too many ticking actors", implemented the way a team would write them,
 * so UpdateBudget is compared against good native practice, not only against doing nothing.
 *
 *   -UBBaseline=B2  every frame: distance from the camera -> a TickInterval band (0 s / 0.1 s / 0.5 s)
 *   -UBBaseline=B3  every frame: USignificanceManager::Update(camera) -> significance -> the same bands,
 *                   applied in a Sequential post-significance callback (Epic's intended usage)
 *
 * Both use the same band edges as UpdateBudget's near/far distances (15 m / 60 m), through one shared edge test
 * (BandForDistanceSquared), and both call SetComponentTickIntervalAndCooldown only when an agent's band changes,
 * because that setter applies at once (the plain setter waits out the old interval).
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

	/** The three update bands both baselines share. */
	enum class EBand : uint8
	{
		Near,   // within 15 m: every frame
		Mid,    // below 60 m: 0.1 s
		Far     // from 60 m: 0.5 s
	};

	/** The one edge test: Near within 15 m, Mid below 60 m, Far from 60 m (squared distances, no square root). */
	static EBand BandForDistanceSquared(double DistanceSquared);

	/** The same bands read back from a significance: 1 -> Near, (0, 1) -> Mid, 0 -> Far. */
	static EBand BandForSignificance(float Significance);

	/** Tick interval for a band: 0 s (every frame), 0.1 s, 0.5 s. */
	static float IntervalForBand(EBand Band);

	/** B3's significance: exactly 1 in Near, strictly inside (0, 1) in Mid, exactly 0 in Far. */
	static float SignificanceForDistanceSquared(double DistanceSquared);

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
	int32 AdoptedAgents = 0;
	int32 FrameIntervalChanges = 0;

	/** Set in Initialize (InitializeDependency returns it). */
	UPROPERTY(Transient)
	TObjectPtr<UUBBenchmarkSubsystem> Benchmark;

	/** B2 only: the brains it re-tunes. B3's are held by the Significance Manager. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUBDemoBrainComponent>> Brains;
};
