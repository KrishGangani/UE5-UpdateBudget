// UpdateBudgetCore - automation tests for the scheduling policy, with fake clients and fake costs (no world needed).
// Run: Tools\run_tests.bat (editor, headless), filter "UpdateBudget.Scheduler".

#include "Misc/AutomationTest.h"
#include "UpdateBudgetScheduler.h"

#include <limits>   // std::numeric_limits<double>::quiet_NaN (TNumericLimits has no NaN in 5.8)

#if WITH_DEV_AUTOMATION_TESTS

namespace UpdateBudgetTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerNoClientsTest, "UpdateBudget.Scheduler.NoClients", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerNoClientsTest::RunTest(const FString& Parameters)
{
	FUpdateBudgetScheduler Scheduler;
	int32 Calls = 0;
	const FBudgetFrameStats Stats = Scheduler.Run({}, 1.0, 2.0, [&Calls](int32) { ++Calls; return 0.0; });

	TestEqual(TEXT("nothing runs"), Calls, 0);
	TestEqual(TEXT("Runs"), Stats.Runs, 0);
	TestEqual(TEXT("Deferred"), Stats.Deferred, 0);
	TestFalse(TEXT("not over budget"), Stats.bOverBudget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerDeadlineOrderTest, "UpdateBudget.Scheduler.DeadlineOrder", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerDeadlineOrderTest::RunTest(const FString& Parameters)
{
	// Nothing is due before the next frame (0.0), and the budget is large: everyone runs, earliest deadline first.
	const TArray<FBudgetClientState> Clients = UpdateBudgetTests::MakeClients({ 5.0, 1.0, 3.0, 1.0, 2.0 });
	FUpdateBudgetScheduler Scheduler;
	TArray<int32> RunOrder;
	const FBudgetFrameStats Stats = Scheduler.Run(Clients, 0.0, 100.0, [&RunOrder](int32 Index) { RunOrder.Add(Index); return 0.01; });

	// Deadlines 1.0 tie between clients 1 and 3: the lower index goes first, so the order is deterministic.
	TestEqual(TEXT("run order"), RunOrder, TArray<int32>({ 1, 3, 4, 2, 0 }));
	TestEqual(TEXT("MandatoryRuns"), Stats.MandatoryRuns, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerBudgetCutOffTest, "UpdateBudget.Scheduler.BudgetCutOff", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerBudgetCutOffTest::RunTest(const FString& Parameters)
{
	// Ten optional clients at 0.1 ms each, predicted exactly; a 0.45 ms budget fits four.
	const TArray<FBudgetClientState> Clients = UpdateBudgetTests::MakeClients({ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 });
	FUpdateBudgetScheduler Scheduler(1, 0.1);
	TArray<int32> RunOrder;
	const FBudgetFrameStats Stats = Scheduler.Run(Clients, 0.0, 0.45, [&RunOrder](int32 Index) { RunOrder.Add(Index); return 0.1; });

	TestEqual(TEXT("Runs"), Stats.Runs, 4);
	TestEqual(TEXT("Deferred"), Stats.Deferred, 6);
	TestEqual(TEXT("SpentMs"), Stats.SpentMs, 0.4, 1e-9);
	TestFalse(TEXT("not over budget"), Stats.bOverBudget);
	TestEqual(TEXT("the four earliest deadlines ran"), RunOrder, TArray<int32>({ 9, 8, 7, 6 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerMandatoryBeatsBudgetTest, "UpdateBudget.Scheduler.MandatoryBeatsBudget", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerMandatoryBeatsBudgetTest::RunTest(const FString& Parameters)
{
	// Every deadline is before the next frame (1.0): all five must run although the budget fits one.
	const TArray<FBudgetClientState> Clients = UpdateBudgetTests::MakeClients({ 0.5, 0.2, 0.9, 0.1, 0.0 });
	FUpdateBudgetScheduler Scheduler(1, 0.1);
	const FBudgetFrameStats Stats = Scheduler.Run(Clients, 1.0, 0.1, [](int32) { return 0.1; });

	TestEqual(TEXT("Runs"), Stats.Runs, 5);
	TestEqual(TEXT("MandatoryRuns"), Stats.MandatoryRuns, 5);
	TestEqual(TEXT("Deferred"), Stats.Deferred, 0);
	TestTrue(TEXT("over budget is reported"), Stats.bOverBudget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerZeroBudgetTest, "UpdateBudget.Scheduler.ZeroBudget", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerZeroBudgetTest::RunTest(const FString& Parameters)
{
	// Three clients due before the next frame (1.0) and three not: a zero budget runs only the mandatory three.
	const TArray<FBudgetClientState> Clients = UpdateBudgetTests::MakeClients({ 0.3, 2.0, 0.1, 3.0, 0.2, 4.0 });
	FUpdateBudgetScheduler Scheduler;
	TArray<int32> RunOrder;
	const FBudgetFrameStats Stats = Scheduler.Run(Clients, 1.0, 0.0, [&RunOrder](int32 Index) { RunOrder.Add(Index); return 0.01; });

	TestEqual(TEXT("only the mandatory clients ran, most overdue first"), RunOrder, TArray<int32>({ 2, 4, 0 }));
	TestEqual(TEXT("Deferred"), Stats.Deferred, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerCostPredictionTest, "UpdateBudget.Scheduler.CostPrediction", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerCostPredictionTest::RunTest(const FString& Parameters)
{
	const TArray<FBudgetClientState> Clients = UpdateBudgetTests::MakeClients({ 0.0 });
	FUpdateBudgetScheduler Scheduler(1, 1.0, 0.2);

	// One measurement of 0 ms: 1.0 + 0.2 * (0 - 1.0) = 0.8.
	Scheduler.Run(Clients, 1.0, 10.0, [](int32) { return 0.0; });
	TestEqual(TEXT("one moving-average step"), Scheduler.GetPredictedCostMs(0), 0.8, 1e-12);

	// A steady 0.25 ms converges: after 100 more frames the remaining error is 0.55 * 0.8^100, far below 1e-9.
	for (int32 Frame = 0; Frame < 100; ++Frame)
	{
		Scheduler.Run(Clients, 1.0, 10.0, [](int32) { return 0.25; });
	}
	TestEqual(TEXT("converges to the measured cost"), Scheduler.GetPredictedCostMs(0), 0.25, 1e-9);

	// A negative measurement (a clock hiccup) counts as 0, never as negative cost.
	FUpdateBudgetScheduler Fresh(1, 1.0, 1.0);
	Fresh.Run(Clients, 1.0, 10.0, [](int32) { return -5.0; });
	TestEqual(TEXT("negative cost clamped to 0"), Fresh.GetPredictedCostMs(0), 0.0, 1e-12);

	// A NaN measurement counts as 0 too: the prediction stays finite and the budget keeps working afterwards.
	FUpdateBudgetScheduler Guarded(1, 0.1, 1.0);
	const FBudgetFrameStats NaNFrame = Guarded.Run(Clients, 1.0, 10.0, [](int32) { return std::numeric_limits<double>::quiet_NaN(); });
	TestTrue(TEXT("prediction stays finite after a NaN cost"), FMath::IsFinite(Guarded.GetPredictedCostMs(0)));
	TestTrue(TEXT("spent time stays finite after a NaN cost"), FMath::IsFinite(NaNFrame.SpentMs));
	const TArray<FBudgetClientState> Optional = UpdateBudgetTests::MakeClients({ 5.0, 6.0, 7.0 });
	Guarded.Run(Optional, 0.0, 0.25, [](int32) { return 0.1; });   // re-learns 0.1 ms
	const FBudgetFrameStats After = Guarded.Run(Optional, 0.0, 0.25, [](int32) { return 0.1; });
	TestEqual(TEXT("the budget cut-off still works after a NaN cost"), After.Runs, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerCategoryFallbackTest, "UpdateBudget.Scheduler.CategoryFallback", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerCategoryFallbackTest::RunTest(const FString& Parameters)
{
	// A client with a category index the scheduler doesn't have uses category 0, and nothing crashes.
	TArray<FBudgetClientState> Clients = UpdateBudgetTests::MakeClients({ 0.0 });
	Clients[0].CategoryIndex = 7;
	FUpdateBudgetScheduler Scheduler(2, 1.0, 1.0);
	Scheduler.Run(Clients, 1.0, 10.0, [](int32) { return 0.5; });

	TestEqual(TEXT("category 0 learned the cost"), Scheduler.GetPredictedCostMs(0), 0.5, 1e-12);
	TestEqual(TEXT("category 1 untouched"), Scheduler.GetPredictedCostMs(1), 1.0, 1e-12);
	TestEqual(TEXT("an unknown category reads category 0"), Scheduler.GetPredictedCostMs(7), 0.5, 1e-12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSchedulerNoStarvationTest, "UpdateBudget.Scheduler.NoStarvation", UpdateBudgetTests::Flags)
bool FUpdateBudgetSchedulerNoStarvationTest::RunTest(const FString& Parameters)
{
	// 200 clients for 600 fixed 60 Hz frames: 20 near (max staleness 0 s), 80 mid (0.1 s), 100 far (0.5 s).
	// Each update costs 0.01 ms and the budget is 0.3 ms. The near clients alone take 0.2 ms, leaving room for 10
	// optional updates per frame, while the others need about 16.7 per frame (80 / 6 + 100 / 30): a real overload,
	// so the budget runs out and clients become mandatory. The guarantee under test: no client ever waits longer
	// than its max staleness, or one frame when that is shorter (02 section 2).
	constexpr int32 NumClients = 200;
	constexpr int32 NumNear = 20;
	constexpr int32 NumFrames = 600;
	constexpr double FrameSeconds = 1.0 / 60.0;
	constexpr double CostMs = 0.01;
	constexpr double BudgetMs = 0.3;

	TArray<FBudgetClientState> States;
	States.Init(FBudgetClientState{}, NumClients);   // deadline 0: every client is due in the first frame
	TArray<double> LastUpdate;
	LastUpdate.Init(-1.0, NumClients);
	TArray<double> WorstGap;
	WorstGap.Init(0.0, NumClients);
	TArray<double> MaxStaleness;
	for (int32 Index = 0; Index < NumClients; ++Index)
	{
		MaxStaleness.Add(Index < NumNear ? 0.0 : (Index < 100 ? 0.1 : 0.5));
	}

	FUpdateBudgetScheduler Scheduler(1, CostMs);
	bool bBudgetHeldWhenOptionalWorkRan = true;
	int32 FramesWithForcedRuns = 0;   // frames after the first where a mid/far client had become mandatory
	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		const double Now = Frame * FrameSeconds;
		const FBudgetFrameStats Stats = Scheduler.Run(States, Now + FrameSeconds, BudgetMs, [&](int32 Index)
		{
			if (LastUpdate[Index] >= 0.0)
			{
				WorstGap[Index] = FMath::Max(WorstGap[Index], Now - LastUpdate[Index]);
			}
			LastUpdate[Index] = Now;
			States[Index].Deadline = Now + MaxStaleness[Index];
			return CostMs;
		});
		// Optional clients run only while the next one fits, so if any ran, the frame stayed inside the budget.
		if (Stats.Runs > Stats.MandatoryRuns && Stats.SpentMs > BudgetMs + 1e-9)
		{
			bBudgetHeldWhenOptionalWorkRan = false;
		}
		FramesWithForcedRuns += (Frame > 0 && Stats.MandatoryRuns > NumNear) ? 1 : 0;
	}

	int32 Violations = 0;
	int32 NeverUpdated = 0;
	for (int32 Index = 0; Index < NumClients; ++Index)
	{
		Violations += (WorstGap[Index] > FMath::Max(MaxStaleness[Index], FrameSeconds) + 1e-9) ? 1 : 0;
		NeverUpdated += (LastUpdate[Index] < 0.0) ? 1 : 0;
	}
	TestTrue(TEXT("the overload really happened (mid/far clients were forced to run)"), FramesWithForcedRuns > 0);
	TestEqual(TEXT("staleness-bound violations"), Violations, 0);
	TestEqual(TEXT("clients never updated"), NeverUpdated, 0);
	TestTrue(TEXT("budget held on every frame that ran optional work"), bBudgetHeldWhenOptionalWorkRan);
	TestEqual(TEXT("near clients ran every frame"), WorstGap[0], FrameSeconds, 1e-9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
