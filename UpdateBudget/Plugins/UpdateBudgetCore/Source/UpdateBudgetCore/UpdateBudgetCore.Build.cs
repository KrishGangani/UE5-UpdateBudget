// UpdateBudgetCore - the reusable runtime module (scheduler, registry, tick function, settings, telemetry).
// It must not depend on any game module.

using UnrealBuildTool;

public class UpdateBudgetCore : ModuleRules
{
	public UpdateBudgetCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
