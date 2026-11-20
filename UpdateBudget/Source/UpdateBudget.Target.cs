// P01 UpdateBudget - game target (standalone/packaged builds).

using UnrealBuildTool;

public class UpdateBudgetTarget : TargetRules
{
	public UpdateBudgetTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("UpdateBudget");
	}
}
