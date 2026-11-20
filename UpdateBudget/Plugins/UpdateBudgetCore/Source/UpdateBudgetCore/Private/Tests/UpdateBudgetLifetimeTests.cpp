// UpdateBudgetCore - lifetime tests: the registry and the scheduler path in a real game world.
// Run: Tools\run_tests.bat (editor, headless), filter "UpdateBudget.Lifetime".
//
// The world comes from the engine's own FTestWorldWrapper (Engine/Public/Tests/AutomationCommon.h), which Epic's
// tick tests use too: a game world with a game instance and a game mode, so actors really begin play, ticked by hand
// with GFrameCounter advanced after every frame (the tick manager runs each tick function once per frame number).

#include "Tests/AutomationCommon.h"
#include "Tests/UpdateBudgetTestFlags.h"
#include "Tests/UpdateBudgetTestTypes.h"
#include "UpdateBudgetSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UpdateBudgetLifetimeTests
{
	using FAction = TFunction<void(UUpdateBudgetTestComponent&, int32 Update)>;
	constexpr float FrameSeconds = 1.f / 60.f;

	/**
	 * What the clients' updates did, kept outside the components: a destroyed component may be garbage-collected at the
	 * end of any world tick (LevelTick.cpp:1970), so a test must never read a destroyed component afterwards.
	 */
	struct FUpdateLog
	{
		/** Updates per client, indexed by the order the test spawned them. */
		TArray<int32> Updates;

		/** Updates of a component that had already ended play or left the world: a stale registry entry. Must stay 0. */
		int32 UpdatesOutOfPlay = 0;
	};

	/** An engine test world in one UpdateBudget mode. Declare the FUpdateLog before it: the world must go first. */
	struct FTestBudgetWorld
	{
		FTestConsoleVariable Enabled{ TEXT("ub.Enabled") };   // declared first, so it is restored after the world is gone
		FTestWorldWrapper Wrapper;
		UWorld* World = nullptr;
		UUpdateBudgetSubsystem* Subsystem = nullptr;

		explicit FTestBudgetWorld(bool bBudgeted)
		{
			Enabled.Set(bBudgeted ? TEXT("1") : TEXT("0"));   // the subsystem reads it in OnWorldBeginPlay
			if (Wrapper.CreateTestWorld(EWorldType::Game) && Wrapper.BeginPlayInTestWorld())
			{
				World = Wrapper.GetTestWorld();
				Subsystem = World->GetSubsystem<UUpdateBudgetSubsystem>();
			}
		}

		bool IsReady(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("test world"), World) && Test.TestNotNull(TEXT("UpdateBudget subsystem"), Subsystem);
		}

		/**
		 * Spawns one test actor whose client counts its updates in Log, then runs Action (if any) with its update number.
		 * Spawned after begin play, so it begins play (and registers) at once: spawn order is registry order.
		 */
		UUpdateBudgetTestComponent* Spawn(FUpdateLog& Log, FAction Action = nullptr) const
		{
			AUpdateBudgetTestActor* Actor = World->SpawnActor<AUpdateBudgetTestActor>();
			if (!Actor)
			{
				return nullptr;
			}
			const int32 Id = Log.Updates.Add(0);
			Actor->Client->OnUpdate = [&Log, Id, Action = MoveTemp(Action)](UUpdateBudgetTestComponent& Self)
			{
				const int32 Update = ++Log.Updates[Id];
				if (!Self.HasBegunPlay() || !Self.IsRegistered())
				{
					++Log.UpdatesOutOfPlay;
				}
				if (Action)
				{
					Action(Self, Update);
				}
			};
			return Actor->Client;
		}

		void Tick(int32 Frames = 1)
		{
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				Wrapper.TickTestWorld(FrameSeconds);
			}
		}
	};
}

using namespace UpdateBudgetLifetimeTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeModeTest, "UpdateBudget.Lifetime.ModeAtBeginPlay", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeModeTest::RunTest(const FString& Parameters)
{
	// Both modes: every client is registered and updated exactly once per frame; only the caller differs.
	for (const bool bBudgeted : { false, true })
	{
		FUpdateLog Log;
		FTestBudgetWorld Test(bBudgeted);
		if (!Test.IsReady(*this))
		{
			return false;
		}
		TArray<UUpdateBudgetTestComponent*> Spawned;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Spawned.Add(Test.Spawn(Log));
		}

		const FString Mode = bBudgeted ? TEXT("budgeted") : TEXT("native");
		TestEqual(Mode + TEXT(": mode"), Test.Subsystem->IsBudgeted(), bBudgeted);
		TestEqual(Mode + TEXT(": clients registered"), Test.Subsystem->GetNumClients(), 4);
		for (const UUpdateBudgetTestComponent* Client : Spawned)
		{
			TestEqual(Mode + TEXT(": driven by the scheduler"), Client->IsDrivenByScheduler(), bBudgeted);
			TestEqual(Mode + TEXT(": native tick enabled"), Client->IsComponentTickEnabled(), !bBudgeted);
		}

		Test.Tick(3);
		TestEqual(Mode + TEXT(": one update per client per frame"), Log.Updates, TArray<int32>({ 3, 3, 3, 3 }));
		Test.Wrapper.ForwardErrorMessages(this);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeDestroyTest, "UpdateBudget.Lifetime.DestroyDuringUpdate", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeDestroyTest::RunTest(const FString& Parameters)
{
	FUpdateLog Log;
	FTestBudgetWorld Test(true);
	if (!Test.IsReady(*this))
	{
		return false;
	}

	// Six clients with equal deadlines run in registry order (ties broken by index), i.e. spawn order.
	// In its 2nd update, client 2 destroys client 0 (already run this frame), itself, and client 5 (not run yet).
	TArray<AActor*> Owners;
	const FAction Destroyer = [&Owners](UUpdateBudgetTestComponent& Self, int32 Update)
	{
		if (Update == 2)
		{
			Owners[0]->Destroy();
			Self.GetOwner()->Destroy();
			Owners[5]->Destroy();
		}
	};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Owners.Add(Test.Spawn(Log, Index == 2 ? Destroyer : FAction())->GetOwner());
	}

	Test.Tick(2);
	TestEqual(TEXT("clients after the destroying frame"), Test.Subsystem->GetNumClients(), 3);
	TestEqual(TEXT("updates after frame 2 (client 5 was destroyed before its turn)"), Log.Updates, TArray<int32>({ 2, 2, 2, 2, 2, 1 }));

	Test.Tick(2);
	TestEqual(TEXT("clients two frames later"), Test.Subsystem->GetNumClients(), 3);
	TestEqual(TEXT("only the survivors kept updating"), Log.Updates, TArray<int32>({ 2, 4, 2, 4, 4, 1 }));
	TestEqual(TEXT("updates after end of play"), Log.UpdatesOutOfPlay, 0);
	Test.Wrapper.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeSpawnTest, "UpdateBudget.Lifetime.SpawnDuringUpdate", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeSpawnTest::RunTest(const FString& Parameters)
{
	FUpdateLog Log;
	FTestBudgetWorld Test(true);
	if (!Test.IsReady(*this))
	{
		return false;
	}

	// In its 1st update, client 1 spawns client 3 (kept) and client 4, which it destroys at once: one add is applied
	// after the loop, the other is cancelled while still pending.
	UUpdateBudgetTestComponent* Kept = nullptr;
	Test.Spawn(Log);
	Test.Spawn(Log, [&Test, &Log, &Kept](UUpdateBudgetTestComponent& Self, int32 Update)
	{
		if (Update == 1)
		{
			Kept = Test.Spawn(Log);
			Test.Spawn(Log)->GetOwner()->Destroy();
		}
	});
	Test.Spawn(Log);

	Test.Tick();
	if (!TestNotNull(TEXT("spawned client"), Kept))
	{
		return false;
	}
	TestEqual(TEXT("clients after the spawning frame"), Test.Subsystem->GetNumClients(), 4);
	TestTrue(TEXT("the spawned client is registered"), Kept->IsRegisteredWithBudget());
	TestTrue(TEXT("the spawned client is driven by the scheduler"), Kept->IsDrivenByScheduler());
	TestFalse(TEXT("the spawned client's native tick is off"), Kept->IsComponentTickEnabled());
	TestEqual(TEXT("spawned clients wait for the next frame"), Log.Updates, TArray<int32>({ 1, 1, 1, 0, 0 }));

	Test.Tick();
	TestEqual(TEXT("the spawned client updates from the next frame on"), Log.Updates, TArray<int32>({ 2, 2, 2, 1, 0 }));
	TestEqual(TEXT("clients one frame later"), Test.Subsystem->GetNumClients(), 4);
	TestEqual(TEXT("updates after end of play"), Log.UpdatesOutOfPlay, 0);
	Test.Wrapper.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeUnloadTest, "UpdateBudget.Lifetime.LevelUnloadOrder", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeUnloadTest::RunTest(const FString& Parameters)
{
	FUpdateLog Log;
	FTestBudgetWorld Test(true);
	if (!Test.IsReady(*this))
	{
		return false;
	}

	// A level being streamed out first unregisters every actor's components (Level.cpp:2196), and only later routes
	// EndPlay(RemovedFromWorld) (Level.cpp:3939). The client must stop updating at the first step, as its native tick does.
	TArray<AActor*> Owners;
	const FAction Unregisterer = [&Owners](UUpdateBudgetTestComponent& Self, int32 Update)
	{
		if (Update == 3)
		{
			Owners[3]->UnregisterAllComponents();   // the same step from inside an update: the removal is deferred
		}
	};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Owners.Add(Test.Spawn(Log, Index == 0 ? Unregisterer : FAction())->GetOwner());
	}

	Test.Tick();
	Owners[1]->UnregisterAllComponents();
	TestEqual(TEXT("clients right after unregistering one actor's components"), Test.Subsystem->GetNumClients(), 3);

	Test.Tick(2);
	TestEqual(TEXT("clients after one was unregistered from inside an update"), Test.Subsystem->GetNumClients(), 2);
	TestEqual(TEXT("unregistered clients stopped updating"), Log.Updates, TArray<int32>({ 3, 1, 3, 2 }));

	Owners[1]->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
	Owners[3]->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
	Test.Tick();
	TestEqual(TEXT("end play after unregistering changes nothing"), Log.Updates, TArray<int32>({ 4, 1, 4, 2 }));
	TestEqual(TEXT("updates out of play"), Log.UpdatesOutOfPlay, 0);
	Test.Wrapper.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeNoDoubleUpdateTest, "UpdateBudget.Lifetime.NoDoubleUpdate", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeNoDoubleUpdateTest::RunTest(const FString& Parameters)
{
	FUpdateLog Log;
	FTestBudgetWorld Test(true);
	if (!Test.IsReady(*this))
	{
		return false;
	}

	// Ways other code can try to bring a scheduler-driven client's native tick back: re-registering the component (the
	// engine re-registers its tick functions), Activate() (which calls SetComponentTickEnabled), and enabling the tick
	// function directly. None may double-update.
	UUpdateBudgetTestComponent* Reregistered = Test.Spawn(Log);
	UUpdateBudgetTestComponent* Activated = Test.Spawn(Log);
	Test.Tick();

	Reregistered->ReregisterComponent();
	TestTrue(TEXT("re-registered: still in the registry"), Reregistered->IsRegisteredWithBudget());
	TestFalse(TEXT("re-registered: native tick still off"), Reregistered->IsComponentTickEnabled());

	Activated->Activate(true);
	TestFalse(TEXT("Activate() can't turn the native tick on while the scheduler drives the client"), Activated->IsComponentTickEnabled());
	Activated->PrimaryComponentTick.SetTickFunctionEnable(true);   // bypasses SetComponentTickEnabled
	TestTrue(TEXT("the tick function was enabled directly"), Activated->IsComponentTickEnabled());

	Test.Tick(3);
	TestEqual(TEXT("clients"), Test.Subsystem->GetNumClients(), 2);
	TestEqual(TEXT("exactly one update per frame each"), Log.Updates, TArray<int32>({ 4, 4 }));
	TestFalse(TEXT("the guard switched the native tick off again"), Activated->IsComponentTickEnabled());
	Test.Wrapper.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeTeardownTest, "UpdateBudget.Lifetime.WorldTeardown", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeTeardownTest::RunTest(const FString& Parameters)
{
	FUpdateLog Log;
	FTestBudgetWorld Test(true);
	if (!Test.IsReady(*this))
	{
		return false;
	}

	// The engine never ends a world inside a tick group: PIE end and travel are queued and run between frames
	// (PlayLevel.cpp:1404, EditorEngine.cpp:2507; GameEngine.cpp:1893 before 1901). The worst case is therefore a frame
	// whose updates spawned and destroyed clients, followed directly by the world's end.
	TArray<TWeakObjectPtr<UUpdateBudgetTestComponent>> Clients;
	AActor* Doomed = nullptr;
	const FAction Changer = [&Test, &Log, &Clients, &Doomed](UUpdateBudgetTestComponent& Self, int32 Update)
	{
		if (Update == 2)
		{
			Clients.Add(Test.Spawn(Log));
			Doomed->Destroy();
		}
	};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UUpdateBudgetTestComponent* Client = Test.Spawn(Log, Index == 0 ? Changer : FAction());
		Clients.Add(Client);
		Doomed = Client->GetOwner();   // ends as client 3's owner
	}
	TWeakObjectPtr<UUpdateBudgetSubsystem> Subsystem = Test.Subsystem;

	Test.Tick(2);
	TestEqual(TEXT("clients before the world ends"), Test.Subsystem->GetNumClients(), 4);

	Test.Wrapper.EndPlayInTestWorld();
	TestEqual(TEXT("clients after the world's EndPlay"), Test.Subsystem->GetNumClients(), 0);
	for (const TWeakObjectPtr<UUpdateBudgetTestComponent>& Client : Clients)
	{
		if (const UUpdateBudgetTestComponent* Alive = Client.Get())
		{
			TestFalse(TEXT("no client keeps a link to the subsystem"), Alive->IsRegisteredWithBudget());
		}
	}

	// Deinitialize (tick function unregistered) and a full garbage collection: nothing may crash or survive.
	Test.Wrapper.DestroyTestWorld(true);
	TestFalse(TEXT("the subsystem is collected with its world"), Subsystem.IsValid());
	TestEqual(TEXT("updates out of play"), Log.UpdatesOutOfPlay, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeLiveSwitchTest, "UpdateBudget.Lifetime.LiveSwitch", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeLiveSwitchTest::RunTest(const FString& Parameters)
{
	FUpdateLog Log;
	FTestBudgetWorld Test(true);
	if (!Test.IsReady(*this))
	{
		return false;
	}

	// Budgeted -> native -> budgeted in a running world. A change takes effect at the end of the frame it is made in,
	// and every client is updated exactly once in every frame, through both switches. The first change is made from
	// inside an update, while the scheduler is running: the case the deferral exists for.
	TArray<UUpdateBudgetTestComponent*> Spawned;
	auto CheckClientsAre = [this, &Spawned](bool bBudgeted)
	{
		for (const UUpdateBudgetTestComponent* Client : Spawned)
		{
			TestEqual(TEXT("driven by the scheduler"), Client->IsDrivenByScheduler(), bBudgeted);
			TestEqual(TEXT("native tick enabled"), Client->IsComponentTickEnabled(), !bBudgeted);
		}
	};
	const FAction SwitchToNative = [&Test](UUpdateBudgetTestComponent& Self, int32 Update)
	{
		if (Update == 3)
		{
			Test.Enabled.Set(TEXT("0"));
		}
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Spawned.Add(Test.Spawn(Log, Index == 0 ? SwitchToNative : FAction()));
	}
	Test.Tick(2);

	Test.Tick();   // frame 3: client 0 sets ub.Enabled 0 inside its update
	TestFalse(TEXT("native after the frame in which it changed"), Test.Subsystem->IsBudgeted());
	CheckClientsAre(false);
	Test.Tick(2);
	TestEqual(TEXT("one update per frame up to the first switch and after it"), Log.Updates, TArray<int32>({ 5, 5, 5 }));

	Test.Enabled.Set(TEXT("1"));
	Test.Enabled.Set(TEXT("0"));
	Test.Tick();
	TestFalse(TEXT("a change undone in the same frame switches nothing"), Test.Subsystem->IsBudgeted());

	Test.Enabled.Set(TEXT("1"));
	TestFalse(TEXT("no switch before the frame ends"), Test.Subsystem->IsBudgeted());
	Test.Tick();
	TestTrue(TEXT("budgeted again"), Test.Subsystem->IsBudgeted());
	CheckClientsAre(true);
	Test.Tick(2);
	TestEqual(TEXT("one update per frame through both switches"), Log.Updates, TArray<int32>({ 9, 9, 9 }));
	TestEqual(TEXT("updates out of play"), Log.UpdatesOutOfPlay, 0);
	Test.Wrapper.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateBudgetLifetimeSwitchKeepsTickStateTest, "UpdateBudget.Lifetime.LiveSwitchKeepsTickState", UpdateBudgetTests::Flags)
bool FUpdateBudgetLifetimeSwitchKeepsTickStateTest::RunTest(const FString& Parameters)
{
	FUpdateLog Log;
	FTestBudgetWorld Test(false);
	if (!Test.IsReady(*this))
	{
		return false;
	}

	// Native mode; gameplay pauses one client (tick off). A live switch to budgeted and back must keep gameplay's choices,
	// including choices made while budgeted: there it pauses the running client and resumes the paused one. The native
	// ticks stay off while budgeted; the choices take effect after the switch back. (While budgeted, the scheduler
	// updates every client: 02 section 12, item 8.)
	UUpdateBudgetTestComponent* Running = Test.Spawn(Log);
	UUpdateBudgetTestComponent* Paused = Test.Spawn(Log);
	Paused->SetComponentTickEnabled(false);
	Test.Tick();
	TestEqual(TEXT("native: the paused client doesn't update"), Log.Updates, TArray<int32>({ 1, 0 }));

	Test.Enabled.Set(TEXT("1"));
	Test.Tick();
	TestTrue(TEXT("budgeted"), Test.Subsystem->IsBudgeted());
	Running->SetComponentTickEnabled(false);
	Paused->SetComponentTickEnabled(true);
	TestFalse(TEXT("budgeted: gameplay can't turn a native tick on"), Paused->IsComponentTickEnabled());

	Test.Enabled.Set(TEXT("0"));
	Test.Tick();
	TestFalse(TEXT("native again"), Test.Subsystem->IsBudgeted());
	TestFalse(TEXT("the client paused while budgeted is paused"), Running->IsComponentTickEnabled());
	TestTrue(TEXT("the client resumed while budgeted ticks again"), Paused->IsComponentTickEnabled());

	const TArray<int32> Before = Log.Updates;
	Test.Tick(2);
	TestEqual(TEXT("the paused client doesn't update natively"), Log.Updates[0], Before[0]);
	TestEqual(TEXT("the resumed client updates natively, once per frame"), Log.Updates[1], Before[1] + 2);
	Test.Wrapper.ForwardErrorMessages(this);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
