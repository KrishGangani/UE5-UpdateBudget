// UpdateBudgetCore - the automation-test flags shared by every UpdateBudget test file.

#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UpdateBudgetTests
{
	/** Editor and game contexts; one filter (ProductFilter), as the macro requires (AutomationTest.h:4127-4133). */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Editor context only, for tests that need editor code such as the Blueprint compiler. */
	constexpr EAutomationTestFlags EditorFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

#endif // WITH_DEV_AUTOMATION_TESTS
