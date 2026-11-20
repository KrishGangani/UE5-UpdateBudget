// P01 UpdateBudget - editor target.

using UnrealBuildTool;

public class UpdateBudgetEditorTarget : TargetRules
{
	public UpdateBudgetEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("UpdateBudget");
	}
}
