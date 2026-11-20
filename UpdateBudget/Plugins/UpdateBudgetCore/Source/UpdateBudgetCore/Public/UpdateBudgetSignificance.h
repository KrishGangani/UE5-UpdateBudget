// UpdateBudgetCore - significance: how far and how visible a client is, and how stale it may therefore get.
// Pure functions on plain data, so they are unit-tested without a world (UpdateBudgetSignificanceTests.cpp).

#pragma once

#include "Camera/CameraTypes.h"
#include "UpdateBudgetSignificance.generated.h"

/**
 * How one kind of client turns distance and view into a staleness limit (02 sections 2 and 7).
 *   significance  1 within NearDistance, 0 beyond FarDistance, linear in distance between;
 *                 multiplied by OutOfViewMultiplier when the client is outside the camera's view.
 *   max staleness FarMaxStaleness at significance 0, NearMaxStaleness at 1, linear between.
 */
USTRUCT()
struct FUpdateBudgetCategory
{
	GENERATED_BODY()

	/** Shown in Project Settings; clients refer to a category by its index. */
	UPROPERTY(EditAnywhere, Category = "Category")
	FName Name = TEXT("Default");

	/** Up to this distance from the camera (cm) a client in view has significance 1. */
	UPROPERTY(EditAnywhere, Category = "Category", meta = (ClampMin = "0"))
	float NearDistance = 1500.f;

	/** From this distance (cm) significance is 0. Should be greater than NearDistance. */
	UPROPERTY(EditAnywhere, Category = "Category", meta = (ClampMin = "0"))
	float FarDistance = 6000.f;

	/** Longest time (s) a client of significance 1 may go without an update. 0: every frame. */
	UPROPERTY(EditAnywhere, Category = "Category", meta = (ClampMin = "0"))
	float NearMaxStaleness = 0.f;

	/** Longest time (s) a client of significance 0 may go without an update. Should not be below NearMaxStaleness. */
	UPROPERTY(EditAnywhere, Category = "Category", meta = (ClampMin = "0"))
	float FarMaxStaleness = 0.5f;

	/** Significance factor for clients outside the camera's view: staler, but never frozen. */
	UPROPERTY(EditAnywhere, Category = "Category", meta = (ClampMin = "0", ClampMax = "1"))
	float OutOfViewMultiplier = 0.5f;

	/** Significance of a client at this squared distance from the camera (cm^2). The square root is taken only between the bands. */
	float GetSignificance(double DistanceSquared, bool bInView) const
	{
		const double Near = NearDistance;
		const double Far = FarDistance;
		float Significance = 0.f;
		if (DistanceSquared <= FMath::Square(Near))
		{
			Significance = 1.f;
		}
		else if (DistanceSquared < FMath::Square(Far))   // only reachable when Far > Near
		{
			Significance = static_cast<float>(1.0 - FMath::GetRangePct(Near, Far, FMath::Sqrt(DistanceSquared)));
		}
		return bInView ? Significance : Significance * OutOfViewMultiplier;
	}

	/** The staleness limit (s) for a significance in [0, 1]. */
	float GetMaxStaleness(float Significance) const
	{
		return FMath::Lerp(FarMaxStaleness, NearMaxStaleness, Significance);
	}

	/** True for settings that make far clients fresher than near ones, or leave no band between near and far. */
	bool IsInverted() const
	{
		return FarDistance <= NearDistance || NearMaxStaleness > FarMaxStaleness;
	}
};

/**
 * The camera view one frame's significance is measured from. "In view" is the cone that circumscribes the
 * view rectangle: its half-angle reaches the screen's corners, so nothing on screen counts as out of view
 * (a little just above and below the screen does). Within AlwaysInViewRadius of the camera everything is in
 * view: the player's own pawn and whatever touches it.
 */
struct FUpdateBudgetView
{
	static constexpr double AlwaysInViewRadius = 200.0;   // cm

	FVector Location = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	double CosHalfAngleSquared = 0.0;

	/** From a camera's view. FOV is horizontal (CameraTypes.h:48); AspectRatio is width / height (default 4:3, CameraTypes.h:179), and a narrower aspect only widens the cone. */
	static FUpdateBudgetView FromCamera(const FMinimalViewInfo& Camera)
	{
		const double TanHalfHorizontal = FMath::Tan(FMath::DegreesToRadians(0.5 * FMath::Clamp(static_cast<double>(Camera.FOV), 1.0, 170.0)));
		const double Aspect = (Camera.AspectRatio > 0.f) ? static_cast<double>(Camera.AspectRatio) : 16.0 / 9.0;
		// tan of the half-diagonal: the corner direction is (1, tanH, tanH / aspect) in camera space.
		const double TanHalfDiagonalSquared = FMath::Square(TanHalfHorizontal) * (1.0 + 1.0 / FMath::Square(Aspect));
		FUpdateBudgetView View;
		View.Location = Camera.Location;
		View.Forward = Camera.Rotation.Vector();
		View.CosHalfAngleSquared = 1.0 / (1.0 + TanHalfDiagonalSquared);   // cos^2 = 1 / (1 + tan^2)
		return View;
	}

	/** In view without a square root: Along > cos(half angle) * |ToClient|, squared once both sides are positive. */
	bool IsInView(const FVector& ToClient, double DistanceSquared) const
	{
		if (DistanceSquared <= FMath::Square(AlwaysInViewRadius))
		{
			return true;
		}
		const double Along = FVector::DotProduct(Forward, ToClient);
		return Along > 0.0 && FMath::Square(Along) > CosHalfAngleSquared * DistanceSquared;
	}
};
