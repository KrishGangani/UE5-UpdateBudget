// P01 UpdateBudget (demo) - the benchmark workload: one agent's "sense -> decide -> act" gameplay update.

#pragma once

#include "BudgetedUpdateComponent.h"
#include "UBDemoBrainComponent.generated.h"

/** What the agent is doing. */
UENUM()
enum class EUBBrainState : uint8
{
	Patrol,
	Alert
};

/**
 * A realistic, deterministic stand-in for AI-style gameplay logic:
 *  - sense:  distance and view-cone check against the player's camera,
 *  - decide: Patrol <-> Alert state machine,
 *  - act:    move along a circular patrol and face the patrol direction or the player,
 *  - plus a calibrated compute step standing in for heavier logic.
 *
 * The patrol position is a closed-form function of the total patrol time. The base class guarantees
 * DeltaSeconds = world time since the previous update, so the patrol time always equals
 * (last update time - first update time), up to float rounding. Any update rate therefore lands on the
 * same circle position for the same last-update time; SC6 compares positions with a stated tolerance.
 */
UCLASS(ClassGroup = (UpdateBudget))
class UUBDemoBrainComponent : public UBudgetedUpdateComponent
{
	GENERATED_BODY()

public:
	/** Sets the patrol circle. Call before the owning actor finishes spawning. */
	void InitPatrol(const FVector& InCenter, double InPhaseRadians);

	/** Sets the size of the compute step (loop iterations per update). */
	void SetComputeIterations(int32 InIterations) { ComputeIterations = FMath::Max(0, InIterations); }

	/** The compute-step size in effect (reported in run metadata, so a later recalibration can't hide behind "default"). */
	int32 GetComputeIterations() const { return ComputeIterations; }

	/** Position on the patrol circle after TotalTime seconds of patrol. */
	FVector GetPatrolPosition(double TotalTime) const;

	/** Total patrol time received so far (the sum of every DeltaSeconds). */
	double GetPatrolTime() const { return PatrolTime; }

	EUBBrainState GetState() const { return State; }
	int32 GetStateChanges() const { return StateChanges; }

protected:
	//~ Begin UBudgetedUpdateComponent
	virtual void OnBudgetedUpdate(float DeltaSeconds) override;
	//~ End UBudgetedUpdateComponent

private:
	/** Angle on the patrol circle, in radians, after TotalTime seconds. */
	double GetPatrolAngle(double TotalTime) const { return PhaseRadians + PatrolAngularSpeed * TotalTime; }

	/** Radius of the patrol circle, in cm. */
	UPROPERTY(EditAnywhere, Category = "Workload", meta = (ClampMin = "0"))
	float PatrolRadius = 300.f;

	/** Patrol speed, in radians per second (0.5 rad/s at 3 m radius = 1.5 m/s walking pace). */
	UPROPERTY(EditAnywhere, Category = "Workload")
	float PatrolAngularSpeed = 0.5f;

	/** The agent notices the player within this distance, in cm. */
	UPROPERTY(EditAnywhere, Category = "Workload", meta = (ClampMin = "0"))
	float AlertRadius = 1500.f;

	/** ...and only if the player is inside this cone around the agent's forward direction (cosine: 0.5 = 60 degrees). */
	UPROPERTY(EditAnywhere, Category = "Workload", meta = (ClampMin = "-1", ClampMax = "1"))
	float AlertConeCos = 0.5f;

	/**
	 * Loop iterations of the stand-in compute step. Calibrated in S2 (batch 2026-11-20_233941_calib, N = 1000, B0):
	 * about 20 ns per iteration per agent. At 128 a whole update costs about 5.2 us, the frame is game-thread bound
	 * (FrameTime tracks GameThreadTime), and the native baseline crosses 16.67 ms between N = 2000 and N = 4000.
	 */
	UPROPERTY(EditAnywhere, Category = "Workload", meta = (ClampMin = "0"))
	int32 ComputeIterations = 128;

	FVector PatrolCenter = FVector::ZeroVector;
	double PhaseRadians = 0.0;
	double PatrolTime = 0.0;
	EUBBrainState State = EUBBrainState::Patrol;
	int32 StateChanges = 0;

	/** Result of the compute step, stored so the optimiser cannot remove the work. */
	float ComputeSink = 0.f;
};
