// UpdateBudgetCore - Phase 7: the scheduling policy's invariants, on hand-picked edge cases and on random frames.
// Run: Tools\run_tests.bat UpdateBudget.Scheduler (editor, headless). No world needed.

#include "Tests/UpdateBudgetTestFlags.h"
#include "Math/RandomStream.h"
#include "UpdateBudgetScheduler.h"

#include <limits>   // std::numeric_limits<double>: TNumericLimits has no NaN or infinity in 5.8

#if WITH_DEV_AUTOMATION_TESTS

namespace UpdateBudgetInvariantTests
{
	/** Clients whose deadlines are given; all in category 0. */
	TArray<FBudgetClientState> MakeClients(std::initializer_list<double> Deadlines)
	{
		TArray<FBudgetClientState> Clients;
		for (const double Deadline : Deadlines)
		{
			Clients.Add({ Deadline, 0 });
		}
		return Clients;
	}

	/** The clients the policy ran, in the order it ran them. Every update is free and everything fits. */
	TArray<int32> RunOrder(FUpdateBudgetScheduler& Scheduler, const TArray<FBudgetClientState>& Clients)
	{
		TArray<int32> Ran;
		Scheduler.Run(Clients, 0.0, 10.0, [&Ran](int32 Index) { Ran.Add(Index); return 0.0; });
		return Ran;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerBreakNotSkipTest, "UpdateBudget.Scheduler.BreakNotSkip", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerBreakNotSkipTest::RunTest(const FString& Parameters)
{
	// The cut-off ends the frame: the first optional client that doesn't fit stops the loop, and a cheaper client with a
	// later deadline doesn't jump ahead of it (05 P1: `break`, not `continue`). Category 0 predicts 1.0 ms, category 1 is
	// taught 0.01 ms; the earliest client is in category 0 and can't fit 0.5 ms, the later one could.
	FUpdateBudgetScheduler Scheduler(2, 1.0, 1.0);   // alpha 1: a prediction becomes the last measurement
	const TArray<FBudgetClientState> Teach = { { 0.0, 1 } };
	Scheduler.Run(Teach, 1.0, 10.0, [](int32) { return 0.01; });   // mandatory, so it teaches category 1
	TestEqual(TEXT("category 1 learned 0.01 ms"), Scheduler.GetPredictedCostMs(1), 0.01, 1e-12);
	TestEqual(TEXT("category 0 still predicts 1.0 ms"), Scheduler.GetPredictedCostMs(0), 1.0, 1e-12);

	const TArray<FBudgetClientState> Clients = { { 5.0, 0 }, { 6.0, 1 } };
	TArray<int32> Ran;
	const FBudgetFrameStats Stats = Scheduler.Run(Clients, 0.0, 0.5, [&Ran](int32 Index) { Ran.Add(Index); return 0.01; });
	TestEqual(TEXT("nothing ran: the later, cheaper client waited"), Ran.Num(), 0);
	TestEqual(TEXT("Deferred"), Stats.Deferred, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerNonFiniteCostsTest, "UpdateBudget.Scheduler.NonFiniteCosts", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerNonFiniteCostsTest::RunTest(const FString& Parameters)
{
	using namespace UpdateBudgetInvariantTests;
	const double Infinity = std::numeric_limits<double>::infinity();
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const TArray<FBudgetClientState> Due = MakeClients({ 0.0 });

	// Infinite measured costs count as 0, like NaN and negative ones (CostPrediction): both the prediction and the
	// frame's spent time stay finite, and the frame isn't over budget.
	for (const double Bad : { Infinity, -Infinity })
	{
		FUpdateBudgetScheduler Scheduler(1, 0.5, 1.0);
		const FBudgetFrameStats Stats = Scheduler.Run(Due, 1.0, 10.0, [Bad](int32) { return Bad; });
		TestEqual(FString::Printf(TEXT("%f measured: prediction"), Bad), Scheduler.GetPredictedCostMs(0), 0.0, 1e-12);
		TestEqual(FString::Printf(TEXT("%f measured: spent"), Bad), Stats.SpentMs, 0.0, 1e-12);
		TestFalse(FString::Printf(TEXT("%f measured: not over budget"), Bad), Stats.bOverBudget);
	}

	// The starting prediction goes through the same rule (05 P1, SanitiseCostMs).
	for (const double Bad : { NaN, Infinity, -1.0 })
	{
		const FUpdateBudgetScheduler Scheduler(1, Bad, 0.2);
		TestEqual(FString::Printf(TEXT("starting prediction %f"), Bad), Scheduler.GetPredictedCostMs(0), 0.0, 1e-12);
	}

	// The smoothing weight is clamped to [0, 1]: above 1 the prediction becomes the measurement, below 0 it never moves.
	FUpdateBudgetScheduler High(1, 1.0, 5.0);
	High.Run(Due, 1.0, 10.0, [](int32) { return 0.25; });
	TestEqual(TEXT("alpha 5 behaves as 1"), High.GetPredictedCostMs(0), 0.25, 1e-12);
	FUpdateBudgetScheduler Low(1, 1.0, -3.0);
	Low.Run(Due, 1.0, 10.0, [](int32) { return 0.25; });
	TestEqual(TEXT("alpha -3 behaves as 0"), Low.GetPredictedCostMs(0), 1.0, 1e-12);

	// Zero categories still leave one prediction to read.
	const FUpdateBudgetScheduler None(0, 0.3, 0.2);
	TestEqual(TEXT("no categories: category 0 exists"), None.GetPredictedCostMs(0), 0.3, 1e-12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerBufferReuseTest, "UpdateBudget.Scheduler.BufferReuseAcrossSizes", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerBufferReuseTest::RunTest(const FString& Parameters)
{
	using namespace UpdateBudgetInvariantTests;

	// The sort buffer is a member, reused every frame without zeroing (05 P1). Shrinking and growing the client count
	// between frames must never leave a stale index in it.
	FUpdateBudgetScheduler Scheduler(1, 0.0, 0.2);
	TestEqual(TEXT("5 clients"), RunOrder(Scheduler, MakeClients({ 5.0, 4.0, 3.0, 2.0, 1.0 })), TArray<int32>({ 4, 3, 2, 1, 0 }));
	TestEqual(TEXT("then 3"), RunOrder(Scheduler, MakeClients({ 2.0, 3.0, 1.0 })), TArray<int32>({ 2, 0, 1 }));
	TestEqual(TEXT("then 6"), RunOrder(Scheduler, MakeClients({ 6.0, 1.0, 5.0, 2.0, 4.0, 3.0 })), TArray<int32>({ 1, 3, 5, 4, 2, 0 }));
	TestEqual(TEXT("then none"), RunOrder(Scheduler, TArray<FBudgetClientState>()), TArray<int32>());
	TestEqual(TEXT("then 2 again"), RunOrder(Scheduler, MakeClients({ 7.0, 7.0 })), TArray<int32>({ 0, 1 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerRandomFramesTest, "UpdateBudget.Scheduler.RandomFrameInvariants", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerRandomFramesTest::RunTest(const FString& Parameters)
{
	// 2,000 random frames on one scheduler (a fixed seed, so a failure is reproducible): 0-48 clients, deadlines on a
	// coarse grid so ties are common, categories 0-3 against 3 predictions (so 3 is out of range), measured costs that
	// are sometimes negative, random budgets and next-frame times. Predictions carry over from frame to frame. After
	// every frame each rule of the policy (05 P1) must hold; the first broken frame is reported.
	FRandomStream Random(20261121);
	FUpdateBudgetScheduler Scheduler(3, 0.05, 0.2);
	int32 BrokenFrames = 0;
	FString FirstBroken;
	TArray<FBudgetClientState> Clients;
	TArray<double> Costs;
	TArray<int32> Sorted;
	TArray<int32> Ran;

	for (int32 Frame = 0; Frame < 2000; ++Frame)
	{
		const int32 Num = Random.RandRange(0, 48);
		Clients.Reset();
		Costs.Reset();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			Clients.Add({ Random.RandRange(0, 20) * 0.05, Random.RandRange(0, 3) });
			Costs.Add(Random.FRandRange(-0.02, 0.1));
		}
		const double NextFrameTime = Random.FRandRange(0.0, 1.0);
		const double BudgetMs = Random.FRandRange(0.0, 0.6);

		// The reference: earliest deadline first, ties by index; the due clients are a prefix.
		Sorted.Reset();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			Sorted.Add(Index);
		}
		Sorted.Sort([&Clients](int32 A, int32 B)
		{
			return Clients[A].Deadline < Clients[B].Deadline || (Clients[A].Deadline == Clients[B].Deadline && A < B);
		});
		int32 NumDue = 0;
		while (NumDue < Num && Clients[Sorted[NumDue]].Deadline < NextFrameTime)
		{
			++NumDue;
		}
		auto CategoryOf = [&Clients](int32 Index) { return (Clients[Index].CategoryIndex < 3) ? Clients[Index].CategoryIndex : 0; };

		Ran.Reset();
		double Spent = 0.0;
		double DueSpent = 0.0;
		bool bStartedWithoutRoom = false;
		const FBudgetFrameStats Stats = Scheduler.Run(Clients, NextFrameTime, BudgetMs, [&](int32 Index)
		{
			// An optional client may start only if its category's current prediction still fits.
			if (Ran.Num() >= NumDue && Spent + Scheduler.GetPredictedCostMs(CategoryOf(Index)) > BudgetMs)
			{
				bStartedWithoutRoom = true;
			}
			const double Cost = FMath::Max(0.0, Costs[Index]);
			Spent += Cost;
			DueSpent += (Ran.Num() < NumDue) ? Cost : 0.0;
			Ran.Add(Index);
			return Costs[Index];
		});

		TArray<FString> Broken;
		bool bPrefix = true;
		for (int32 Position = 0; Position < Ran.Num(); ++Position)
		{
			bPrefix &= (Ran[Position] == Sorted[Position]);
		}
		if (!bPrefix)
		{
			Broken.Add(TEXT("the run order is not a prefix of earliest-deadline-first order"));
		}
		if (Ran.Num() < NumDue || Stats.MandatoryRuns != NumDue)
		{
			Broken.Add(TEXT("not every due client ran, or the mandatory count is wrong"));
		}
		if (bStartedWithoutRoom)
		{
			Broken.Add(TEXT("an optional client started although its predicted cost didn't fit"));
		}
		if (Ran.Num() < Num && Spent + Scheduler.GetPredictedCostMs(CategoryOf(Sorted[Ran.Num()])) <= BudgetMs)
		{
			Broken.Add(TEXT("the loop stopped although the next client fitted"));
		}
		if (Stats.Runs != Ran.Num() || Stats.Runs + Stats.Deferred != Num)
		{
			Broken.Add(TEXT("Runs + Deferred is not the client count"));
		}
		if (!FMath::IsNearlyEqual(Stats.SpentMs, Spent, 1e-12) || !FMath::IsNearlyEqual(Stats.MandatoryMs, DueSpent, 1e-12))
		{
			Broken.Add(TEXT("SpentMs or MandatoryMs is not the sum of the (non-negative) measured costs"));
		}
		if (Stats.bOverBudget != (Stats.SpentMs > BudgetMs))
		{
			Broken.Add(TEXT("bOverBudget doesn't match SpentMs > BudgetMs"));
		}
		if (!Broken.IsEmpty() && BrokenFrames++ == 0)
		{
			FirstBroken = FString::Printf(TEXT("frame %d (%d clients, %d due, budget %.4f ms): %s"), Frame, Num, NumDue, BudgetMs, *FString::Join(Broken, TEXT("; ")));
		}
	}
	TestEqual(TEXT("frames that broke a rule"), BrokenFrames, 0);
	if (BrokenFrames > 0)
	{
		AddError(FString::Printf(TEXT("First: %s"), *FirstBroken));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
