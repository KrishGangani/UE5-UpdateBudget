// P01 UpdateBudget (demo) - benchmark director: builds the workload and runs one timed, repeatable benchmark.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "UBBenchmarkSubsystem.generated.h"

class ACameraActor;
class AUBDemoAgent;

/**
 * Runs the benchmark for one game world. A world subsystem instead of a placed actor because the project
 * has no hand-authored map: nothing would place a director actor, and the subsystem already has exactly
 * the world's lifetime. Tickable, because it drives the camera and the run timeline every frame.
 *
 * Timeline (world time since begin play):
 *   [0, Warmup)            warm-up: agents and camera already moving, nothing measured
 *   Warmup                 CSV_EVENT "BenchStart"  -> capture starts with -csvStartOnEvent=BenchStart
 *   Warmup + WaveAt        optional wave: N more agents (the load doubles), CSV_EVENT "Wave"
 *   Warmup + k * Switch    with -UBSwitchEvery, ub.Enabled is toggled (k = 1, 2, ... while inside the capture)
 *   Warmup + Capture       CSV_EVENT "BenchEnd"    -> capture stops at the end of this frame (-csvStopOnEvent=BenchEnd)
 *   next frame             the per-agent run report is written (outside the capture, so it never costs a measured frame)
 *   a few frames later     with -UBExit the game quits, after the CSV profiler has written its file
 *
 * The camera path is timed from the *planned* BenchStart, so the captured path is the same for any warm-up length.
 *
 * Command line (all optional):
 *   -UBAgents=N     agents to spawn (default 0: no workload, normal empty map)
 *   -UBSeed=S       placement seed (default 1)
 *   -UBWork=K       compute-step iterations per update (default: the brain's own default)
 *   -UBWarmup=SEC   warm-up seconds (default 10)
 *   -UBCapture=SEC  measured seconds (default 60)
 *   -UBWaveAt=SEC   seconds after BenchStart to double the agents (default: no wave)
 *   -UBRunTag=NAME  name of the run report file and CSV metadata tag (default "run")
 *   -UBSwitchEvery=SEC  toggle ub.Enabled every SEC seconds of the capture (a stability test of live switching)
 *   -UBExit         quit when the run is finished
 */
UCLASS()
class UUBBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/**
	 * The spawned agents, in spawn order. Append-only: an index stays valid for the world's lifetime (the baseline driver
	 * adopts new agents by index). Entries can be destroyed (or nulled by GC) during a run: check IsValid() before use.
	 */
	const TArray<TObjectPtr<AUBDemoAgent>>& GetAgents() const { return Agents; }

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	/** Values read once from the command line. */
	struct FRunSettings
	{
		int32 NumAgents = 0;
		int32 Seed = 1;
		int32 ComputeIterations = -1;
		double WarmupSeconds = 10.0;
		double CaptureSeconds = 60.0;
		double WaveAtSeconds = -1.0;
		double SwitchEverySeconds = -1.0;
		bool bSwitchGiven = false;
		FString RunTag = TEXT("run");
		bool bExitWhenDone = false;
	};

	enum class ERunPhase : uint8
	{
		Idle,    // no workload requested
		WarmUp,
		Capture,
		Done
	};

	static FRunSettings ReadRunSettings();

	/** Spawns Count agents on a jittered grid (same seed, same placement) and appends them. Returns a start-position hash. */
	uint32 SpawnAgents(UWorld& World, int32 Count, int32 Seed);

	/** Places the camera on its fixed path (time measured from the planned BenchStart; negative during warm-up). */
	void UpdateCamera(double SecondsFromBenchStart);

	/** Warns about settings that would silently produce a misleading run. */
	void ValidateRunSettings() const;

	/** Writes one line per agent (final position, patrol time, last update, age at end) to Saved/UBRuns/<RunTag>.csv. */
	void WriteRunReport() const;

	FRunSettings Settings;
	int32 EffectiveComputeIterations = -1;
	ERunPhase Phase = ERunPhase::Idle;
	double BeginPlayTime = 0.0;
	double BenchStartTime = 0.0;
	double ArenaHalfExtent = 0.0;
	bool bWaveSpawned = false;
	int32 SwitchesRequested = 0;
	int32 FramesSinceEnd = 0;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AUBDemoAgent>> Agents;
};
