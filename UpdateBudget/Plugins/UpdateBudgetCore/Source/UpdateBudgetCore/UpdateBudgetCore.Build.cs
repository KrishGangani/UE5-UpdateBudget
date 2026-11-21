// UpdateBudgetCore - the reusable runtime module (scheduler, registry, tick function, settings, telemetry).
// It must not depend on any game module.

using UnrealBuildTool;

public class UpdateBudgetCore : ModuleRules
{
	public UpdateBudgetCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// DeveloperSettings is public: the public header UpdateBudgetSettings.h derives from UDeveloperSettings.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });

		// Editor builds only: the S8 test builds a Blueprint in code (UpdateBudgetBlueprintTests.cpp, inside WITH_EDITOR).
		// A packaged game never links these.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph" });
		}
	}
}
