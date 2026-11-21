// UpdateBudgetCore - automation tests for significance, max staleness and the view test, no world needed.
// Run: Tools\run_tests.bat (editor, headless), filter "UpdateBudget.Significance".

#include "Tests/UpdateBudgetTestFlags.h"
#include "UpdateBudgetSettings.h"
#include "UpdateBudgetSignificance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSignificanceBandsTest, "UpdateBudget.Significance.Bands", UpdateBudgetTests::Flags)
bool FUpdateBudgetSignificanceBandsTest::RunTest(const FString& Parameters)
{
	// The default category: 1 up to 15 m, 0 from 60 m, linear in distance between (02 section 7).
	const FUpdateBudgetCategory Category;
	auto At = [&Category](double DistanceCm, bool bInView) { return Category.GetSignificance(DistanceCm * DistanceCm, bInView); };

	TestEqual(TEXT("at the camera"), At(0.0, true), 1.f);
	TestEqual(TEXT("exactly at the near distance is near"), At(1500.0, true), 1.f);
	TestEqual(TEXT("half way between the bands"), At(3750.0, true), 0.5f, 1e-6f);
	TestEqual(TEXT("a quarter of the way"), At(2625.0, true), 0.75f, 1e-6f);
	TestEqual(TEXT("exactly at the far distance is far"), At(6000.0, true), 0.f);
	TestEqual(TEXT("beyond the far distance"), At(100000.0, true), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSignificanceViewTest, "UpdateBudget.Significance.OutOfView", UpdateBudgetTests::Flags)
bool FUpdateBudgetSignificanceViewTest::RunTest(const FString& Parameters)
{
	// Outside the view cone significance is multiplied by 0.5: staler, never frozen.
	const FUpdateBudgetCategory Category;
	TestEqual(TEXT("near, out of view"), Category.GetSignificance(FMath::Square(1000.0), false), 0.5f);
	TestEqual(TEXT("mid band, out of view"), Category.GetSignificance(FMath::Square(3750.0), false), 0.25f, 1e-6f);
	TestEqual(TEXT("far, out of view"), Category.GetSignificance(FMath::Square(9000.0), false), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetMaxStalenessTest, "UpdateBudget.Significance.MaxStaleness", UpdateBudgetTests::Flags)
bool FUpdateBudgetMaxStalenessTest::RunTest(const FString& Parameters)
{
	// 0 s at significance 1, 0.5 s at 0, linear between.
	const FUpdateBudgetCategory Category;
	TestEqual(TEXT("significance 1"), Category.GetMaxStaleness(1.f), 0.f);
	TestEqual(TEXT("significance 0"), Category.GetMaxStaleness(0.f), 0.5f);
	TestEqual(TEXT("significance 0.5"), Category.GetMaxStaleness(0.5f), 0.25f, 1e-6f);
	TestEqual(TEXT("near but out of view"), Category.GetMaxStaleness(Category.GetSignificance(0.0, false)), 0.25f, 1e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSignificanceDegenerateTest, "UpdateBudget.Significance.FarNotBeyondNear", UpdateBudgetTests::Flags)
bool FUpdateBudgetSignificanceDegenerateTest::RunTest(const FString& Parameters)
{
	// A far distance at or inside the near distance (a settings mistake) gives a step at the near distance,
	// never a division by zero or a NaN.
	FUpdateBudgetCategory Category;
	Category.NearDistance = 2000.f;
	Category.FarDistance = 2000.f;
	TestEqual(TEXT("inside: near"), Category.GetSignificance(FMath::Square(1999.0), true), 1.f);
	TestEqual(TEXT("outside: far"), Category.GetSignificance(FMath::Square(2001.0), true), 0.f);
	Category.FarDistance = 500.f;
	TestEqual(TEXT("far < near, outside: far (a NaN would fail this too)"), Category.GetSignificance(FMath::Square(2500.0), true), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetViewTest, "UpdateBudget.Significance.ViewCone", UpdateBudgetTests::Flags)
bool FUpdateBudgetViewTest::RunTest(const FString& Parameters)
{
	// A camera at the origin looking along +X, 90 degrees horizontal FOV, 16:9. The screen's corner direction is
	// (1, 1, 0.5625): about 48.9 degrees off axis, outside a cone of half the horizontal FOV (45 degrees).
	FMinimalViewInfo Camera;
	Camera.Location = FVector::ZeroVector;
	Camera.Rotation = FRotator::ZeroRotator;
	Camera.FOV = 90.f;
	Camera.AspectRatio = 16.f / 9.f;
	const FUpdateBudgetView View = FUpdateBudgetView::FromCamera(Camera);
	auto InView = [&View](const FVector& Point) { return View.IsInView(Point - View.Location, (Point - View.Location).SizeSquared()); };

	TestTrue(TEXT("straight ahead"), InView(FVector(1000.0, 0.0, 0.0)));
	TestFalse(TEXT("straight behind"), InView(FVector(-1000.0, 0.0, 0.0)));
	TestTrue(TEXT("near the left edge"), InView(FVector(1000.0, -990.0, 0.0)));
	TestTrue(TEXT("near a screen corner (47 degrees off axis)"), InView(FVector(1000.0, 950.0, 530.0)));
	TestFalse(TEXT("beyond the corner (51 degrees off axis)"), InView(FVector(1000.0, 1000.0, 700.0)));
	TestFalse(TEXT("to the side (90 degrees)"), InView(FVector(0.0, 1000.0, 0.0)));
	TestTrue(TEXT("at the camera"), InView(FVector::ZeroVector));
	TestTrue(TEXT("just behind the camera, within the always-in-view radius"), InView(FVector(-150.0, 0.0, 0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetSignificanceCategoryPickTest, "UpdateBudget.Significance.CategoryPick", UpdateBudgetTests::Flags)
bool FUpdateBudgetSignificanceCategoryPickTest::RunTest(const FString& Parameters)
{
	// The rule the scheduler and the B2/B3 baselines share for a client's category, and the list it reads.
	FUpdateBudgetCategory First;
	First.NearDistance = 100.f;
	FUpdateBudgetCategory Second;
	Second.NearDistance = 200.f;
	const TArray<FUpdateBudgetCategory> Categories = { First, Second };

	TestEqual(TEXT("a valid index"), FUpdateBudgetCategory::Pick(Categories, 1).NearDistance, 200.f);
	TestEqual(TEXT("an index past the end means category 0"), FUpdateBudgetCategory::Pick(Categories, 2).NearDistance, 100.f);
	TestEqual(TEXT("a negative index means category 0"), FUpdateBudgetCategory::Pick(Categories, -1).NearDistance, 100.f);

	// An ini can empty the settings' array; the list every reader gets still holds one default category.
	UUpdateBudgetSettings* Settings = NewObject<UUpdateBudgetSettings>();
	Settings->Categories.Empty();
	const TArray<FUpdateBudgetCategory> InEffect = Settings->GetCategories();
	TestEqual(TEXT("an emptied list still has one category"), InEffect.Num(), 1);
	TestEqual(TEXT("...the default one"), InEffect[0].NearDistance, FUpdateBudgetCategory().NearDistance);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
