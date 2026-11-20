// UpdateBudgetCore - minimal client and actor types for the lifetime tests (UpdateBudgetLifetimeTests.cpp).
// UHT only reads headers, so reflected test types live here. Like the engine's own test classes in
// Engine/Private/Tests, they exist in every configuration but are hidden from editor pickers and Blueprints.

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

protected:
	virtual void OnBudgetedUpdate(float DeltaSeconds) override
	{
		if (OnUpdate)
		{
			OnUpdate(*this);
		}
	}
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
