// UpdateBudgetCore - the tick function that runs the scheduler once per frame, in TG_PrePhysics.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "UpdateBudgetTickFunction.generated.h"

#define UE_API UPDATEBUDGETCORE_API

class UUpdateBudgetSubsystem;

/**
 * Calls UUpdateBudgetSubsystem::RunFrame() in TG_PrePhysics, the tick group of the native baseline ticks (01b #1).
 * A tickable world subsystem's own Tick runs after TG_PostPhysics and can't choose a group (01b section 2), so the
 * subsystem owns this tick function and registers it on the persistent level, as Mass and Niagara do.
 * The subsystem registers it in OnWorldBeginPlay and unregisters it in Deinitialize.
 */
USTRUCT()
struct FUpdateBudgetTickFunction : public FTickFunction
{
	GENERATED_BODY()

	UE_API FUpdateBudgetTickFunction();

	/** The subsystem that owns this tick function. Set before registration; it outlives the registration. */
	UUpdateBudgetSubsystem* Subsystem = nullptr;

	//~ Begin FTickFunction
	UE_API virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	UE_API virtual FString DiagnosticMessage() override;
	UE_API virtual FName DiagnosticContext(bool bDetailed) override;
	//~ End FTickFunction
};

/** Tick functions must never be copied: the tick manager holds raw pointers to them (EngineBaseTypes.h:550). */
template<>
struct TStructOpsTypeTraits<FUpdateBudgetTickFunction> : public TStructOpsTypeTraitsBase2<FUpdateBudgetTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

#undef UE_API
