// UpdateBudgetCore - the scheduling policy: who runs this frame. Plain C++ with no engine objects, so it is unit-tested.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Templates/Function.h"

#define UE_API UPDATEBUDGETCORE_API

/** The hot per-client data the policy reads. Plain data: the scheduling loop touches no UObject memory (H5). */
struct FBudgetClientState
{
	/** World time (seconds) by which this client must next be updated: its last update + its max staleness. */
	double Deadline = 0.0;

	/** Which cost prediction this client uses (one per settings category). An index out of range means category 0. */
	int32 CategoryIndex = 0;
};

/** What happened in one frame. */
struct FBudgetFrameStats
{
	/** Measured cost of every update run this frame, in milliseconds. */
	double SpentMs = 0.0;

	/** Updates run, mandatory ones included. */
	int32 Runs = 0;

	/** Updates that had to run because waiting one more frame would pass their deadline. */
	int32 MandatoryRuns = 0;

	/** Clients left for a later frame because the next update would not have fitted in the budget. */
	int32 Deferred = 0;

	/** True when SpentMs ended above the budget: mandatory work, or an update that cost more than predicted. */
	bool bOverBudget = false;
};

/**
 * The UpdateBudget policy (02_System_Architecture.md sections 2 and 6), run once per frame:
 *   1. Every client whose deadline is before the next frame runs: waiting would break its staleness bound.
 *      These are mandatory, and they beat the budget (no starvation).
 *   2. The others run in earliest-deadline-first order while the predicted cost of the next one still fits.
 *      The first one that doesn't fit ends the frame, so a later deadline never jumps ahead of an earlier one.
 *   3. Each measured cost updates its category's prediction (an exponentially weighted moving average).
 * Mandatory clients also run in deadline order, most overdue first. Ties are broken by client index, so the
 * order is deterministic.
 */
class FUpdateBudgetScheduler
{
public:
	/**
	 * Runs one client and returns its measured cost in milliseconds. It may change the client states the
	 * scheduler was given (the subsystem sets the new deadline there); the order is fixed before any client runs.
	 */
	using FRunClient = TFunctionRef<double(int32 ClientIndex)>;

	/**
	 * @param NumCategories   number of cost predictions (at least 1)
	 * @param InitialCostMs   starting prediction: about 0.005 ms is one demo update on this machine (09, S2-f)
	 * @param SmoothingAlpha  weight of the newest measurement in the moving average, clamped to [0, 1]
	 */
	UE_API explicit FUpdateBudgetScheduler(int32 NumCategories = 1, double InitialCostMs = 0.005, double SmoothingAlpha = 0.2);

	/**
	 * Decides who runs this frame and runs them through RunClient. NextFrameTime = now + the frame's delta time.
	 * Deadlines must be finite (the subsystem derives them from world time); a NaN deadline would break the sort.
	 */
	UE_API FBudgetFrameStats Run(TConstArrayView<FBudgetClientState> Clients, double NextFrameTime, double BudgetMs, FRunClient RunClient);

	/** The current cost prediction of a category, in milliseconds. */
	UE_API double GetPredictedCostMs(int32 CategoryIndex) const;

private:
	/** Every cost that enters a prediction passes here: negative and non-finite values count as 0. */
	static double SanitiseCostMs(double CostMs);

	int32 ClampCategory(int32 CategoryIndex) const
	{
		return PredictedCostMs.IsValidIndex(CategoryIndex) ? CategoryIndex : 0;
	}

	/** One moving-average cost prediction per category, in milliseconds. */
	TArray<double> PredictedCostMs;

	/** Client indices sorted by deadline. Kept between frames, so the steady state allocates nothing. */
	TArray<int32> Order;

	double Alpha;
};

#undef UE_API
