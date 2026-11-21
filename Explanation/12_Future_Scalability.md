# 12 · Future Scalability: UpdateBudget

How the system grows into a production game. Each item says what it would need; none of it is built. The starting list is in [02 §11](02_System_Architecture.md).

## Growth paths

- **Multiple viewpoints (split screen, dedicated server):** significance from the nearest of several views, as `USignificanceManager::Update` takes an array of transforms (06 §17). The deadline loop would take a view array instead of one camera.
- **Per-platform budgets:** device profiles and scalability groups set `ub.BudgetMs`; the settings default stays the design value.
- **One budget with animation:** feed the scheduler's spent time to the Animation Budget Allocator (`AddExternalAnimationTimeSpentThisFrame`, 02 §11), so the two share a frame. Measure first.
- **Parallel significance:** `FBudgetClientState` is plain data, so the scoring step can use `ParallelFor` once owner locations are cached on the game thread.
- **Cheaper sort at large N:** at N = 10,000 the scheduler's own cost was about 1.4 ms per frame in one run (09 P7-d); a partial sort or a heap of the clients that can fit is the first candidate (11).
- **Behavioural LOD:** categories choose a cheaper update function per significance band, not just a lower rate.
- **Find the budget that beats B2/B3:** the planned 1 / 2 / 4 ms sweep (03), run before any of the above.

---

## Recorded during implementation

### Refusing a Blueprint client's Event Tick at compile time (owner decision, Phase 6)

**Today (v1):** a client Blueprint that implements Event Tick gets a warning once per class and world, at registration (05 P6e). Event Tick runs only while the component ticks natively, so such a client does more work in native mode than budgeted, and its Event Tick logic silently stops under the scheduler. The owner kept the warning for v1.

**The growth path:** refuse it when the Blueprint compiles, so the problem is shown in the editor rather than at run time.
- **The engine's own mechanism doesn't fit.** A native parent can forbid Blueprint ticking with `meta = (ChildCannotTick)`, and the Blueprint compiler then warns (`KismetCompiler.cpp:5545`, `:5575–5585`). But that whole check runs only when the class can't tick (`if (!TickFunction->bCanEverTick)`, `:5530`), and UpdateBudget's client must be able to tick natively for the baselines (05 P5).
- **What exists to build it on:** a `UBlueprintCompilerExtension` (`Editor/Kismet/Public/BlueprintCompilerExtension.h:28`) receives every compiled Blueprint of a registered type (`ProcessBlueprintCompiled`, `:41`; registered with `FBlueprintCompilationManager::RegisterCompilerExtension`, `BlueprintCompilationManager.h:97`). One that reports an error for a `UBudgetedUpdateComponent` subclass implementing `ReceiveTick` would make the rule a compile error [inferred: how such an extension reports an error is not checked yet]. The Data Validation plugin's `UEditorValidatorBase` (`Plugins/Editor/DataValidation/Source/DataValidation/Public/EditorValidatorBase.h:28`) is the other route, run on save or on demand.
- **Cost:** an editor module, which the plugin doesn't have yet. Phase 9 adds an editor-only test module (owner decision); an editor module for tooling like this would sit beside it.
