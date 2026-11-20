// P01 UpdateBudget (demo) - AUBDemoAgent.

#include "UBDemoAgent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UBDemoBrainComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBDemoAgent)

AUBDemoAgent::AUBDemoAgent()
{
	// The actor itself has no per-frame work.
	PrimaryActorTick.bCanEverTick = false;

	// Scene components default to Movable (SceneComponent.cpp:124), which a moving agent needs.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->SetRelativeScale3D(FVector(0.5)); // 50 cm cube.

	// Engine content; cooked through DirectoriesToAlwaysCook in DefaultGame.ini.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	Brain = CreateDefaultSubobject<UUBDemoBrainComponent>(TEXT("Brain"));
}
