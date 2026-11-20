// UpdateBudgetCore - test types for the lifetime tests.

#include "Tests/UpdateBudgetTestTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UpdateBudgetTestTypes)

AUpdateBudgetTestActor::AUpdateBudgetTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Client = CreateDefaultSubobject<UUpdateBudgetTestComponent>(TEXT("Client"));
}
