// P01 UpdateBudget (demo) - the update bands baselines B2 and B3 share, read from UpdateBudget's settings.
// Plain data and pure functions, so the tests check them without a world (Private/Tests/UBBaselineBandTests.cpp).

#pragma once

#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "UpdateBudgetSignificance.h"

/** The three update bands both baselines use. */
enum class EUBBaselineBand : uint8
{
	Near,   // up to NearDistance: the Near interval (every frame by default)
	Mid,    // below FarDistance: the Mid interval
	Far     // from FarDistance: the Far interval
};

/**
 * B2's and B3's bands. FromCategory reads them from the settings category UpdateBudget's scheduler uses for the same
 * agents, so a change to the distances or the staleness limits moves both sides of the comparison together (owner
 * decision, Phase 6). Not shared: the out-of-view multiplier, because B2 and B3 are distance-only techniques. The
 * defaults are the constants the S3 baselines were captured with, which are also the settings' defaults: 15 m and 60 m;
 * every frame, 0.1 s and 0.5 s.
 */
struct FUBBaselineBands
{
	double NearDistance = 1500.0;   // cm
	double FarDistance = 6000.0;    // cm
	float NearInterval = 0.f;       // s
	float MidInterval = 0.1f;       // s: the baselines' own hand-picked rate, as a team would choose one
	float FarInterval = 0.5f;       // s

	/**
	 * Edges from the category's distances. The Near and Far intervals are its staleness limits, so an agent at either end
	 * may go as long without an update as UpdateBudget allows there. Mid keeps 0.1 s, clamped between the two, so it can
	 * never be staler than both or fresher than both, even for an inverted category (UpdateBudget warns about those).
	 */
	static FUBBaselineBands FromCategory(const FUpdateBudgetCategory& Category)
	{
		FUBBaselineBands Bands;
		Bands.NearDistance = Category.NearDistance;
		Bands.FarDistance = Category.FarDistance;
		Bands.NearInterval = Category.NearMaxStaleness;
		Bands.FarInterval = Category.FarMaxStaleness;
		Bands.MidInterval = FMath::Clamp(Bands.MidInterval, FMath::Min(Bands.NearInterval, Bands.FarInterval), FMath::Max(Bands.NearInterval, Bands.FarInterval));
		return Bands;
	}

	/** The one edge test: Near up to NearDistance, Mid below FarDistance, Far from FarDistance (squared: no square root). */
	EUBBaselineBand BandForDistanceSquared(double DistanceSquared) const
	{
		if (DistanceSquared <= FMath::Square(NearDistance))
		{
			return EUBBaselineBand::Near;
		}
		return (DistanceSquared < FMath::Square(FarDistance)) ? EUBBaselineBand::Mid : EUBBaselineBand::Far;
	}

	/** The same bands read back from a significance: 1 -> Near, (0, 1) -> Mid, 0 -> Far. */
	static EUBBaselineBand BandForSignificance(float Significance)
	{
		if (Significance >= 1.f)
		{
			return EUBBaselineBand::Near;
		}
		return (Significance > 0.f) ? EUBBaselineBand::Mid : EUBBaselineBand::Far;
	}

	/** Tick interval (s) for a band. */
	float IntervalForBand(EUBBaselineBand Band) const
	{
		switch (Band)
		{
		case EUBBaselineBand::Near:
			return NearInterval;
		case EUBBaselineBand::Mid:
			return MidInterval;
		default:
			return FarInterval;
		}
	}

	/** B3's significance: exactly 1 in Near, strictly inside (0, 1) in Mid, exactly 0 in Far. */
	float SignificanceForDistanceSquared(double DistanceSquared) const
	{
		const EUBBaselineBand Band = BandForDistanceSquared(DistanceSquared);
		if (Band != EUBBaselineBand::Mid)
		{
			return (Band == EUBBaselineBand::Near) ? 1.f : 0.f;
		}
		// Linear falloff across the Mid band. The clamp keeps the value strictly inside (0, 1), so BandForSignificance
		// returns Mid for it: B3's bands equal B2's by construction, not by rounding luck. 0.99999994f is the largest
		// float below 1.
		const double Falloff = 1.0 - FMath::GetRangePct(NearDistance, FarDistance, FMath::Sqrt(DistanceSquared));
		return FMath::Clamp(static_cast<float>(Falloff), TNumericLimits<float>::Min(), 0.99999994f);
	}
};
