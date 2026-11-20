// UpdateBudgetCore - Project Settings: the budget and the significance categories.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "UpdateBudgetSignificance.h"
#include "UpdateBudgetSettings.generated.h"

#define UE_API UPDATEBUDGETCORE_API

/**
 * Project Settings > Game > Update Budget, saved in Config/DefaultGame.ini. Read when a world begins play;
 * the ub.BudgetMs cvar overrides the budget at any time.
 */
UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "Update Budget"))
class UUpdateBudgetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UE_API UUpdateBudgetSettings();

	/** Game-thread time per frame (ms) for optional updates. Mandatory updates may exceed it (02 section 12). */
	UPROPERTY(config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0"))
	float BudgetMs = 2.f;

	/** Index 0 is the default; a client's BudgetCategory picks one, and an index out of range means 0. */
	UPROPERTY(config, EditAnywhere, Category = "Categories")
	TArray<FUpdateBudgetCategory> Categories;
};

#undef UE_API
