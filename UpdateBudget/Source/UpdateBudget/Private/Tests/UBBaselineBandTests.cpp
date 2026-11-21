// P01 UpdateBudget (demo) - Phase 6: the B2/B3 baselines use exactly UpdateBudget's bands (owner decision).
// Run: Tools\run_tests.bat UpdateBudget.Baselines

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UBBaselineBands.h"
#include "UBBaselineDriverSubsystem.h"
#include "UpdateBudgetSignificance.h"

namespace UBBaselineBandTests
{
	/** The same contexts and filter as the plugin's tests (UpdateBudgetTestFlags.h, private to that module). */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** UpdateBudget's own reading of a distance (its significance, in view), mapped to a band the way B3 maps its own. */
	EUBBaselineBand BandFromUpdateBudget(const FUpdateBudgetCategory& Category, double Distance)
	{
		return FUBBaselineBands::BandForSignificance(Category.GetSignificance(FMath::Square(Distance), /*bInView*/ true));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUBBaselineBandsDefaultsTest, "UpdateBudget.Baselines.DefaultsAreTheS3Bands", UBBaselineBandTests::Flags)
bool FUBBaselineBandsDefaultsTest::RunTest(const FString& Parameters)
{
	// The S3 baselines were captured with constants. The driver's own path (this project's settings, the brain class's
	// category, UpdateBudget's rule) must give exactly those, so the captured batches stay valid. A Categories entry in
	// DefaultGame.ini that changes them makes this test fail on purpose.
	const FUBBaselineBands Bands = UUBBaselineDriverSubsystem::ReadBands();
	TestTrue(TEXT("near edge exactly 15 m"), Bands.NearDistance == 1500.0);
	TestTrue(TEXT("far edge exactly 60 m"), Bands.FarDistance == 6000.0);
	TestTrue(TEXT("near: exactly every frame"), Bands.NearInterval == 0.f);
	TestTrue(TEXT("mid: exactly 0.1 s"), Bands.MidInterval == 0.1f);
	TestTrue(TEXT("far: exactly 0.5 s"), Bands.FarInterval == 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUBBaselineBandsMatchTest, "UpdateBudget.Baselines.SameBandsAsUpdateBudget", UBBaselineBandTests::Flags)
bool FUBBaselineBandsMatchTest::RunTest(const FString& Parameters)
{
	using namespace UBBaselineBandTests;

	// The default category and a changed one: B2's distance bands, B3's significance bands and UpdateBudget's own
	// significance agree at both edges and 1 cm either side of them. (Within a fraction of a millimetre past the near edge,
	// UpdateBudget's float significance can round to exactly 1, a Near reading; the baselines clamp it, 05 G4.)
	FUpdateBudgetCategory Changed;
	Changed.NearDistance = 1000.f;
	Changed.FarDistance = 3000.f;
	Changed.NearMaxStaleness = 0.05f;
	Changed.FarMaxStaleness = 1.f;

	for (const FUpdateBudgetCategory& Category : { FUpdateBudgetCategory(), Changed })
	{
		const FUBBaselineBands Bands = FUBBaselineBands::FromCategory(Category);
		const double Near = Category.NearDistance;
		const double Far = Category.FarDistance;
		for (const double Distance : { 0.0, 0.5 * Near, Near, Near + 1.0, 0.5 * (Near + Far), Far - 1.0, Far, Far + 1.0, 2.0 * Far })
		{
			const EUBBaselineBand Expected = BandFromUpdateBudget(Category, Distance);
			const FString Where = FString::Printf(TEXT("near %.0f / far %.0f, at %.0f cm"), Near, Far, Distance);
			TestTrue(FString::Printf(TEXT("B2 band, %s"), *Where), Bands.BandForDistanceSquared(FMath::Square(Distance)) == Expected);
			TestTrue(FString::Printf(TEXT("B3 band, %s"), *Where), FUBBaselineBands::BandForSignificance(Bands.SignificanceForDistanceSquared(FMath::Square(Distance))) == Expected);
		}

		// The ends of the scale are UpdateBudget's own staleness limits; Mid lies between them.
		TestEqual(TEXT("near interval = near max staleness"), Bands.IntervalForBand(EUBBaselineBand::Near), Category.NearMaxStaleness);
		TestEqual(TEXT("far interval = far max staleness"), Bands.IntervalForBand(EUBBaselineBand::Far), Category.FarMaxStaleness);
		TestTrue(TEXT("mid interval between near and far"), Bands.MidInterval >= Bands.NearInterval && Bands.MidInterval <= Bands.FarInterval);
	}

	// A far limit below the baselines' 0.1 s: Mid follows it down instead of becoming staler than Far.
	FUpdateBudgetCategory Tight;
	Tight.FarMaxStaleness = 0.05f;
	TestTrue(TEXT("mid clamped to the far limit"), FUBBaselineBands::FromCategory(Tight).MidInterval == 0.05f);

	// An inverted category (near staler than far; UpdateBudget warns about it): Mid still lies between the two.
	FUpdateBudgetCategory Inverted;
	Inverted.NearMaxStaleness = 0.6f;
	Inverted.FarMaxStaleness = 0.2f;
	TestTrue(TEXT("inverted: mid clamped up to the far limit"), FUBBaselineBands::FromCategory(Inverted).MidInterval == 0.2f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
