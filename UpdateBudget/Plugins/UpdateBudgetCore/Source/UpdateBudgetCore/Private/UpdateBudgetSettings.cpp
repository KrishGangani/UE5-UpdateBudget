// UpdateBudgetCore - UUpdateBudgetSettings.

#include "UpdateBudgetSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UpdateBudgetSettings)

UUpdateBudgetSettings::UUpdateBudgetSettings()
{
	// One default category (02 section 7). Any Categories entries in DefaultGame.ini's
	// [/Script/UpdateBudgetCore.UpdateBudgetSettings] section replace the whole array (Obj.cpp:3091-3095).
	Categories.Add(FUpdateBudgetCategory());
}
