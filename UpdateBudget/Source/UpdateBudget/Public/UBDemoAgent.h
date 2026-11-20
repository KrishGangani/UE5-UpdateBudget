// P01 UpdateBudget (demo) - one benchmark agent: a visible mesh plus its brain.

#pragma once

#include "GameFramework/Actor.h"
#include "UBDemoAgent.generated.h"

class UStaticMeshComponent;
class UUBDemoBrainComponent;

/**
 * A minimal agent. The actor itself never ticks: all gameplay work lives in its brain component,
 * which is what the benchmark configurations schedule differently.
 */
UCLASS()
class AUBDemoAgent : public AActor
{
	GENERATED_BODY()

public:
	AUBDemoAgent();

	UUBDemoBrainComponent* GetBrain() const { return Brain; }

private:
	/** A small cube so agents are visible in the demo. No collision: the workload measures gameplay logic, not physics. */
	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<UUBDemoBrainComponent> Brain;
};
