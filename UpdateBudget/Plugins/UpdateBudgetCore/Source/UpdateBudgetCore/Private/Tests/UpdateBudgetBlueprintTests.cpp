// UpdateBudgetCore - S8: the Blueprint event's cost per call, measured on a real Blueprint built in code.
// Editor builds only (it needs the Blueprint compiler). Run: Tools\run_tests.bat UpdateBudget.Blueprint
//
// Numbers come from an editor Development build in a headless editor, timed in a tight loop: they price one call
// against another on this machine and are not a benchmark result (01 section 7).

#include "Tests/UpdateBudgetTestFlags.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "Tests/UpdateBudgetTestTypes.h"
#include "UObject/Stack.h"

namespace UpdateBudgetBlueprintTests
{
	constexpr int32 Calls = 100000;
	constexpr int32 Rounds = 5;

	/**
	 * Mean time per call (ns) of Count calls of Call. Each batch stands for one frame: in editor and development builds the
	 * Blueprint VM counts branch steps since the last reset and aborts script past MaximumLoopIterationCount (1,000,000,
	 * ScriptCore.cpp:107 and 1268-1284), and the world tick resets the count once per frame (LevelTick.cpp:1563). All
	 * rounds in one test frame went over it, so the reset is done here, outside the timed loop, as the world tick does.
	 */
	template <typename FCall>
	double NanosecondsPerCall(int32 Count, FCall&& Call)
	{
		GInitRunaway();
		const uint64 Start = FPlatformTime::Cycles64();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Call();
		}
		return FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Start) * 1.0e6 / Count;
	}

	double Median(TArray<double> Values)
	{
		Values.Sort();
		return Values[Values.Num() / 2];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetBlueprintEventCostTest, "UpdateBudget.Blueprint.EventCost", UpdateBudgetTests::EditorFlags)
bool FUpdateBudgetBlueprintEventCostTest::RunTest(const FString& Parameters)
{
	using namespace UpdateBudgetBlueprintTests;

	// 1. A transient Blueprint subclass of the probe whose Budgeted Update event calls CountCall: event -> one call node.
	//    CreateBlueprint makes it RF_Standalone (Kismet2.cpp:453), which the editor never collects, so the name is unique
	//    per run, and the flag is cleared on the way out so the next collection takes the Blueprint and its class, as the
	//    engine's own test does (ToolsetLibraryTest.cpp:33-43, 1698-1704). Live probes keep the class until the world goes.
	const FName Name = MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("UpdateBudgetProbeBP"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UUpdateBudgetBlueprintProbe::StaticClass(), GetTransientPackage(), Name, BPTYPE_Normal);
	if (!TestNotNull(TEXT("Blueprint"), Blueprint))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		Blueprint->ClearFlags(RF_Standalone);
	};
	if (!TestTrue(TEXT("has an event graph"), Blueprint->UbergraphPages.Num() > 0))
	{
		return false;
	}
	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	int32 NodeY = 0;
	UK2Node_Event* Event = FKismetEditorUtilities::AddDefaultEventNode(Blueprint, Graph, UUpdateBudgetBlueprintProbe::GetEventName(), UBudgetedUpdateComponent::StaticClass(), NodeY);
	if (!TestNotNull(TEXT("event node"), Event))
	{
		return false;
	}
	FGraphNodeCreator<UK2Node_CallFunction> CallCreator(*Graph);
	UK2Node_CallFunction* Call = CallCreator.CreateNode(false);
	Call->SetFromFunction(UUpdateBudgetBlueprintProbe::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UUpdateBudgetBlueprintProbe, CountCall)));
	CallCreator.Finalize();
	TestTrue(TEXT("event wired to the call"), GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then), Call->GetExecPin()));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	UClass* BlueprintClass = Blueprint->GeneratedClass;
	if (!TestTrue(TEXT("compiled without errors"), Blueprint->Status != BS_Error && BlueprintClass != nullptr))
	{
		return false;
	}

	// 2. One native and one Blueprint probe on one actor in a test world. Native mode: only this test calls RunUpdate.
	FTestWorldWrapper Wrapper;
	if (!TestTrue(TEXT("test world"), Wrapper.CreateTestWorld(EWorldType::Game) && Wrapper.BeginPlayInTestWorld()))
	{
		return false;
	}
	AActor* Owner = Wrapper.GetTestWorld()->SpawnActor<AActor>();
	UUpdateBudgetBlueprintProbe* Native = NewObject<UUpdateBudgetBlueprintProbe>(Owner);
	UUpdateBudgetBlueprintProbe* Scripted = NewObject<UUpdateBudgetBlueprintProbe>(Owner, BlueprintClass);
	Native->RegisterComponent();     // the owner has begun play, so each component begins play here (Actor.cpp:6443-6451)
	Scripted->RegisterComponent();
	TestFalse(TEXT("the native class has no Blueprint update"), Native->HasBlueprintUpdate());
	TestTrue(TEXT("the Blueprint class implements the event"), Scripted->HasBlueprintUpdate());

	// 3. Alternating rounds after a warm-up; the median round per kind.
	const auto RunNative = [Native] { Native->RunUpdate(); };
	const auto RunScripted = [Scripted] { Scripted->RunUpdate(); };
	const auto RunUnguarded = [Native] { Native->CallEventUnguarded(); };
	NanosecondsPerCall(1000, RunNative);
	NanosecondsPerCall(1000, RunScripted);
	NanosecondsPerCall(1000, RunUnguarded);
	Scripted->Calls = 0;
	TArray<double> NativeNs, ScriptedNs, UnguardedNs;
	for (int32 Round = 0; Round < Rounds; ++Round)
	{
		NativeNs.Add(NanosecondsPerCall(Calls, RunNative));
		ScriptedNs.Add(NanosecondsPerCall(Calls, RunScripted));
		UnguardedNs.Add(NanosecondsPerCall(Calls, RunUnguarded));
	}
	TestEqual(TEXT("the Blueprint ran once per update"), Scripted->Calls, Calls * Rounds);

	const double NativeMedian = Median(NativeNs);
	const double ScriptedMedian = Median(ScriptedNs);
	// One output path: a Display line logged during a test also lands in the test report (AutomationTest.cpp:231-254).
	UE_LOG(LogTemp, Display, TEXT("UpdateBudget S8: RunUpdate per call, median of %d rounds x %d calls: native %.1f ns, Blueprint %.1f ns, so the Blueprint event costs %.1f ns per update. Unguarded event call on the native class: %.1f ns."),
		Rounds, Calls, NativeMedian, ScriptedMedian, ScriptedMedian - NativeMedian, Median(UnguardedNs));

	Wrapper.ForwardErrorMessages(this);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
