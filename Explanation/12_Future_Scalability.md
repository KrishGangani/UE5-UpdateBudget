# 12 · Future Scalability: UpdateBudget

**Status:** the full chapter is written in **Phase 10**: how the system grows into a production game (dedicated servers, per-platform budgets, parallel scoring, behavioural LOD). The starting list is in [02 §11](02_System_Architecture.md). Growth paths decided during implementation are recorded below as they come up, each with what exists in 5.8.4 to build it on.

---

## Growth paths recorded so far

### Refusing a Blueprint client's Event Tick at compile time (owner decision, Phase 6)

**Today (v1):** a client Blueprint that implements Event Tick gets a warning once per class and world, at registration (05 P6e). Event Tick runs only while the component ticks natively, so such a client does more work in native mode than budgeted, and its Event Tick logic silently stops under the scheduler. The owner kept the warning for v1.

**The growth path:** refuse it when the Blueprint compiles, so the problem is shown in the editor rather than at run time.
- **The engine's own mechanism doesn't fit.** A native parent can forbid Blueprint ticking with `meta = (ChildCannotTick)`, and the Blueprint compiler then warns (`KismetCompiler.cpp:5545`, `:5575–5585`). But that whole check runs only when the class can't tick (`if (!TickFunction->bCanEverTick)`, `:5530`), and UpdateBudget's client must be able to tick natively for the baselines (05 P5).
- **What exists to build it on:** a `UBlueprintCompilerExtension` (`Editor/Kismet/Public/BlueprintCompilerExtension.h:28`) receives every compiled Blueprint of a registered type (`ProcessBlueprintCompiled`, `:41`; registered with `FBlueprintCompilationManager::RegisterCompilerExtension`, `BlueprintCompilationManager.h:97`). One that reports an error for a `UBudgetedUpdateComponent` subclass implementing `ReceiveTick` would make the rule a compile error [inferred: how such an extension reports an error is not checked yet]. The Data Validation plugin's `UEditorValidatorBase` (`Plugins/Editor/DataValidation/Source/DataValidation/Public/EditorValidatorBase.h:28`) is the other route, run on save or on demand.
- **Cost:** an editor module, which the plugin doesn't have yet. Phase 9 adds an editor-only test module (owner decision); an editor module for tooling like this would sit beside it.
