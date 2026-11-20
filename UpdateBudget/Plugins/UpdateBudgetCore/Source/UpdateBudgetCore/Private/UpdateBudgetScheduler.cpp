// UpdateBudgetCore - FUpdateBudgetScheduler.

#include "UpdateBudgetScheduler.h"

#include "Math/UnrealMathUtility.h"

double FUpdateBudgetScheduler::SanitiseCostMs(double CostMs)
{
	// Negative or non-finite costs (a clock oddity, a bad argument) count as 0. NaN would otherwise poison the
	// moving average for good: every later "Spent + NaN > Budget" is false, which would switch the budget off.
	return FMath::IsFinite(CostMs) ? FMath::Max(0.0, CostMs) : 0.0;
}

FUpdateBudgetScheduler::FUpdateBudgetScheduler(int32 NumCategories, double InitialCostMs, double SmoothingAlpha)
	: Alpha(FMath::Clamp(SmoothingAlpha, 0.0, 1.0))
{
	PredictedCostMs.Init(SanitiseCostMs(InitialCostMs), FMath::Max(1, NumCategories));
}

double FUpdateBudgetScheduler::GetPredictedCostMs(int32 CategoryIndex) const
{
	return PredictedCostMs[ClampCategory(CategoryIndex)];
}

FBudgetFrameStats FUpdateBudgetScheduler::Run(TConstArrayView<FBudgetClientState> Clients, double NextFrameTime, double BudgetMs, FRunClient RunClient)
{
	FBudgetFrameStats Stats;

	// One sort by deadline gives both sets: the mandatory clients are exactly the prefix due before the next frame.
	Order.SetNumUninitialized(Clients.Num(), EAllowShrinking::No);
	for (int32 Index = 0; Index < Clients.Num(); ++Index)
	{
		Order[Index] = Index;
	}
	// Raw pointer reads: every index is in [0, Num) by construction, so the view's per-access range check (compiled
	// into Development builds) would only add cost to the hottest loop of the frame.
	const FBudgetClientState* const State = Clients.GetData();
	Order.Sort([State](int32 A, int32 B)
	{
		const double DeadlineA = State[A].Deadline;
		const double DeadlineB = State[B].Deadline;
		return DeadlineA < DeadlineB || (DeadlineA == DeadlineB && A < B);
	});

	// Counted before any client runs, because RunClient moves the deadline of the client it updates.
	int32 NumMandatory = 0;
	while (NumMandatory < Order.Num() && State[Order[NumMandatory]].Deadline < NextFrameTime)
	{
		++NumMandatory;
	}

	for (int32 Position = 0; Position < Order.Num(); ++Position)
	{
		const int32 ClientIndex = Order[Position];
		const int32 Category = ClampCategory(State[ClientIndex].CategoryIndex);
		if (Position >= NumMandatory && Stats.SpentMs + PredictedCostMs[Category] > BudgetMs)
		{
			break;   // Don't start work that won't fit; everything after this has a later deadline.
		}

		const double CostMs = SanitiseCostMs(RunClient(ClientIndex));
		PredictedCostMs[Category] = FMath::Lerp(PredictedCostMs[Category], CostMs, Alpha);
		Stats.SpentMs += CostMs;
		++Stats.Runs;
		if (Position < NumMandatory)
		{
			Stats.MandatoryMs = Stats.SpentMs;   // the mandatory clients are the prefix, so this ends at their total
		}
	}

	// The loop only ever stops at an optional client, so every mandatory client has run.
	Stats.MandatoryRuns = NumMandatory;
	Stats.Deferred = Clients.Num() - Stats.Runs;
	Stats.bOverBudget = Stats.SpentMs > BudgetMs;
	return Stats;
}
