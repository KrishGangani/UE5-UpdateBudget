// UpdateBudgetCore - FUpdateBudgetTickFunction.

#include "UpdateBudgetTickFunction.h"

#include "UpdateBudgetSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UpdateBudgetTickFunction)

FUpdateBudgetTickFunction::FUpdateBudgetTickFunction()
{
	// TG_PrePhysics is already the FTickFunction default; stated because the fair comparison depends on it.
	// bCanEverTick and bStartWithTickEnabled are not set: RegisterTickFunction reads neither (only actors and
	// components do), so the subsystem enables or disables this function explicitly.
	TickGroup = TG_PrePhysics;
	EndTickGroup = TG_PrePhysics;
}

void FUpdateBudgetTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (ensure(Subsystem))
	{
		Subsystem->RunFrame(DeltaTime);
	}
}

FString FUpdateBudgetTickFunction::DiagnosticMessage()
{
	return TEXT("UUpdateBudgetSubsystem::RunFrame");
}

FName FUpdateBudgetTickFunction::DiagnosticContext(bool bDetailed)
{
	static const FName Context(TEXT("UpdateBudget"));
	return Context;
}
