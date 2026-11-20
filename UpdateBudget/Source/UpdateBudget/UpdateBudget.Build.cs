// P01 UpdateBudget - primary game module: demo workload and benchmark harness only.
// The reusable system lives in the UpdateBudgetCore plugin; this module depends on it, never the reverse.

using UnrealBuildTool;

public class UpdateBudget : ModuleRules
{
	public UpdateBudget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UpdateBudgetCore is public: UBDemoBrainComponent.h (a public header) derives from its UBudgetedUpdateComponent.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UpdateBudgetCore" });

		// SignificanceManager is private: only UBBaselineDriverSubsystem.cpp (baseline B3) uses it.
		PrivateDependencyModuleNames.AddRange(new string[] { "SignificanceManager" });
	}
}
