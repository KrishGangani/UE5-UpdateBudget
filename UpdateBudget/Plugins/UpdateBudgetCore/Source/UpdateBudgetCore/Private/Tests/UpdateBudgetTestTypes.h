// UpdateBudgetCore - minimal client and actor types for the lifetime tests (UpdateBudgetLifetimeTests.cpp) and the
// S8 Blueprint test (UpdateBudgetBlueprintTests.cpp). UHT only reads headers, so reflected test types live here.
// Like the engine's own test classes in Engine/Private/Tests, they exist in every configuration and are hidden from
// editor pickers (HideDropdown). All but the S8 probe are also NotBlueprintable: that test needs a Blueprint subclass.

#pragma once

#include "BudgetedUpdateComponent.h"
#include "GameFramework/Actor.h"
#include "Templates/Function.h"
#include "UpdateBudgetTestTypes.generated.h"

/** A client whose update calls a test-provided function, so each test decides what happens inside an update. */
UCLASS(NotBlueprintable, HideDropdown)
class UUpdateBudgetTestComponent : public UBudgetedUpdateComponent
{
	GENERATED_BODY()

public:
	/** Called inside every update of this component. */
	TFunction<void(UUpdateBudgetTestComponent&)> OnUpdate;

	/** The DeltaSeconds the latest update received (05 P5's time contract), readable from OnUpdate. */
	float LastDeltaSeconds = -1.f;

protected:
	virtual void OnBudgetedUpdate(float DeltaSeconds) override
	{
		LastDeltaSeconds = DeltaSeconds;
		if (OnUpdate)
		{
			OnUpdate(*this);
		}
	}
};

/**
 * A client whose Blueprint subclass the S8 test builds in code: its "Budgeted Update" event calls CountCall once, so the
 * test can prove the Blueprint ran and time it against the native class.
 */
UCLASS(Blueprintable, HideDropdown)
class UUpdateBudgetBlueprintProbe : public UBudgetedUpdateComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UpdateBudget Test")
	void CountCall() { ++Calls; }

	/** Calls the event without the guard, as code that didn't check would: on a native class, a ProcessEvent with no script. */
	void CallEventUnguarded() { ReceiveBudgetedUpdate(0.f); }

	/** The event's name, taken where the protected member is accessible. */
	static FName GetEventName() { return GET_FUNCTION_NAME_CHECKED(UUpdateBudgetBlueprintProbe, ReceiveBudgetedUpdate); }

	int32 Calls = 0;
};

/** An actor with one test client and nothing else. */
UCLASS(NotBlueprintable, HideDropdown)
class AUpdateBudgetTestActor : public AActor
{
	GENERATED_BODY()

public:
	AUpdateBudgetTestActor();

	UPROPERTY()
	TObjectPtr<UUpdateBudgetTestComponent> Client;
};
