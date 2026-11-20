# 04 · C++ Fundamentals: UpdateBudget

**Rule:** only the C++ ideas this project's code actually relies on, each with a tiny example first and then where it appears in our code. This file grows with every step.

---

## 1. Classes, inheritance, `virtual` and `override`

```cpp
struct Animal            { virtual void Speak() const { /* default */ } virtual ~Animal() = default; };
struct Dog : Animal      { void Speak() const override { /* bark */ } };
Animal* A = new Dog;  A->Speak();   // calls Dog::Speak: chosen at run time through the vtable
```
- `virtual` means "subclasses may replace this, and the call is decided at run time" (dynamic dispatch, through a hidden table of function pointers, the vtable).
- `override` asks the compiler to **check** that you really replace a base function. A typo becomes an error instead of a silent new function.
- **In our code:** `UBudgetedUpdateComponent::OnBudgetedUpdate` is `virtual`, and `UUBDemoBrainComponent` overrides it. One virtual call per update costs a few nanoseconds; that is the price of letting any gameplay plug in.

## 2. Access control: `public`, `protected`, `private` and `friend`

```cpp
class Account { public: int Balance() const; protected: void Audit(); private: int Cents = 0; friend class Bank; };
```
- `public`: anyone. `protected`: the class and its subclasses. `private`: the class only.
- `friend class Bank;` lets **one** named class see the privates.
- **In our code:** `RegistryIndex` and `Subsystem` are private, and only `UUpdateBudgetSubsystem` is a friend. Any other code that wrote them could corrupt the registry. `OnBudgetedUpdate` is protected, so callers must go through `RunUpdate()`, which owns the time contract.

## 3. `const` member functions

```cpp
double GetLastUpdateTime() const { return LastUpdateTime; }   // promises not to modify *this
```
- Callable on `const` objects and references, and documents "read-only".
- **In our code:** every getter, plus `RecordUpdate(const UBudgetedUpdateComponent& Client, …)`, which only *reads* the client.

## 4. Pointers vs references

```cpp
void A(Widget* W) { if (W) W->Go(); }   // may be null: check it
void B(Widget& W) { W.Go(); }           // can't be null: no check needed
```
- **In our code:**
  - `Register(UBudgetedUpdateComponent* Client)` takes a pointer and `check`s it, because callers hold pointers.
  - `RecordUpdate(const UBudgetedUpdateComponent& Client, …)` takes a **reference**, because it is always called with `*this`, which can never be null. One fewer branch in the hottest function.
  - `OnWorldBeginPlay(UWorld& InWorld)`: the engine guarantees a world.

## 5. Inline functions in headers

```cpp
// Header: the body is visible at every call site, so the compiler can paste it in, with no call/return.
void RecordUpdate(...) { ++FrameUpdates; ... }
```
- An out-of-line function in another `.cpp`, or another DLL in editor builds, can't be inlined without link-time optimisation.
- **In our code:** `RecordUpdate` runs once per update, thousands of times a frame, so it lives in the header (efficiency audit).

## 6. `static` locals

```cpp
Widget& Get() { static Widget Instance; return Instance; }   // built once, on first call
```
- **In our code:** `static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(...)` runs the asset lookup **once per class**, not once per constructed agent.

## 7. `constexpr`, namespaces and named constants

```cpp
namespace Grid { constexpr double Spacing = 400.0; }   // compile-time, typed, named
```
- **In our code:** `UBBench::GridSpacing`, `GridJitter` and `AgentHeight`. There are no magic numbers inside the spawn loop, and each one's reason is written next to it. Since S3 also `UBBaseline::NearDistance` / `FarDistance` (the band edges) and `BelowOne`.

## 8. `enum class`

```cpp
enum class EState : uint8 { Patrol, Alert };   // scoped: EState::Patrol; no silent int conversion
```
- **In our code:** `EUBBrainState`, 1 byte, reflected with `UENUM()`. Since S3 also two **private, nested** enums inside the baseline driver, `EMode` and `EBand`: not reflected (nothing outside the class needs them), and a `switch` over `EBand` maps each band to its interval.

## 9. Casts and the ternary operator

```cpp
const float F = static_cast<float>(D);          // explicit, visible narrowing
const double Dt = (Prev < 0.0) ? 0.0 : Now - Prev;
```
- **In our code:** `RunUpdate` converts the double time bookkeeping to the float gameplay delta with `static_cast`. The first-update rule is a ternary.

## 10. `if` with an initialiser-style declaration

```cpp
if (Widget* W = FindWidget()) { W->Go(); }      // W exists only inside the if
```
- **In our code:** `if (UUpdateBudgetSubsystem* Current = Subsystem)` reads the `TObjectPtr` once into a raw local (cheaper in editor builds), and the null check is part of the same line.

## 11. Why a "sink" variable keeps a benchmark honest

```cpp
float Sink = 0; for (...) Sink = f(Sink);   // if Sink were never used, an optimiser may delete the whole loop
Member = Sink;                              // storing to a member makes the result observable, so the loop must run
```
- **In our code:** `ComputeSink` in `UUBDemoBrainComponent`. Without it, an optimised build might measure an empty loop.

## 12. Static member functions

```cpp
class UMy { static float Twice(float X) { return 2.f * X; } };   // no 'this': depends only on its arguments
const float Y = UMy::Twice(3.f);
```
- **Why:** a function that needs no object state says so in its signature. It can't accidentally read or change a member, and it's safe to call from any thread if its arguments are.
- **In our code (S3):** `BandForDistanceSquared`, `BandForSignificance`, `IntervalForBand` and `SignificanceForDistanceSquared` in the baseline driver. That is why the significance lambda, which runs on worker threads, can call them without capturing anything.

## 13. Lambdas and what they capture

```cpp
auto Square = [](float X) { return X * X; };               // captures nothing: a plain function object
auto Count  = [this](int N) { Total += N; };               // captures the object pointer: can use members
```
- **The capture list** (`[]`, `[this]`, `[&]`, `[=]`) decides what the lambda can reach from where it was written. Capturing `this` copies only a pointer, so **the object must outlive every copy of the lambda**.
- **Copies:** storing a lambda in a `TFunction` (as `USignificanceManager::RegisterObject` does) copies it. Defining the lambda once and passing it many times makes one object that is copied per registration.
- **In our code (S3):** the B3 significance function captures nothing: it runs on worker threads and touches only its arguments. The post-significance function captures `this` to call `SetIntervalIfChanged`. That is safe because every registration is removed (in `HandleAgentEndPlay` or `OnWorldEndPlay`) before the driver is destroyed (05 Part D).

## 14. `explicit` constructors and default arguments

```cpp
struct FScheduler { explicit FScheduler(int32 Categories = 1, double Alpha = 0.2); };
FScheduler A;          // defaults: 1 category, alpha 0.2
FScheduler B(4);       // 4 categories, alpha 0.2
// FScheduler C = 4;   // error: explicit forbids the silent int -> FScheduler conversion
```
- **In our code (S4):** `FUpdateBudgetScheduler(int32 NumCategories = 1, double InitialCostMs = 0.005, double SmoothingAlpha = 0.2)`. Tests choose only what they care about, and an `int` can never turn into a scheduler by accident.

## 15. Type aliases with `using`

```cpp
using FRunClient = TFunctionRef<double(int32 ClientIndex)>;   // a readable name for a long type
```
- **In our code (S4):** `FUpdateBudgetScheduler::FRunClient`. It documents what the callback means (run client *i*, return its cost) wherever it's used.

## 16. Aggregate initialisation and `std::initializer_list`

```cpp
struct FState { double Deadline = 0.0; int32 Category = 0; };
FState S{ 2.5, 1 };                                        // fills the members in order
void Make(std::initializer_list<double> Values);           // lets callers write Make({ 1.0, 2.0, 3.0 })
```
- **In our code (S4 tests):** `Clients.Add({ Deadline, 0 })` and `MakeClients({ 5.0, 1.0, 3.0 })` make test inputs short enough to read at a glance. Members with defaults (`= 0.0`) can still be aggregate-initialised since C++14.

## 17. Template specialisation: telling a library about your type

```cpp
template<typename T> struct TStructOpsTypeTraitsBase2 { enum { WithCopy = true /* … */ }; };   // the general rule
template<> struct TStructOpsTypeTraits<FUpdateBudgetTickFunction>                              // the rule for one type
    : public TStructOpsTypeTraitsBase2<FUpdateBudgetTickFunction> { enum { WithCopy = false }; };
```
- **What it is.** `template<>` writes a version of a template for one exact type. Code that asks `TStructOpsTypeTraits<X>::WithCopy` gets the answer for `X`; every other type keeps the general one.
- **Why:** the reflection system has to know whether it may copy a struct. A *trait* answers that at compile time, with no runtime cost and no change to the struct itself.
- **In our code (S5):** `FUpdateBudgetTickFunction` must never be copied (the tick manager holds raw pointers to it), so its trait says `WithCopy = false`, exactly like the engine's `FActorTickFunction`.

## 18. Destruction order and RAII test fixtures

```cpp
struct FTestBudgetWorld
{
    FTestConsoleVariable Enabled{ TEXT("ub.Enabled") };   // constructed 1st, destroyed LAST
    FTestWorldWrapper Wrapper;                             // constructed 2nd, destroyed FIRST
};
```
- **The rule.** Members are constructed in the order they are **declared** and destroyed in the **reverse** order. Local variables work the same way.
- **RAII** ("resource acquisition is initialisation") puts cleanup in destructors, so it happens on every exit path, early `return` included.
- **In our code (S5):** the world must be destroyed before the cvar is restored, so the cvar is declared first. In each test the `FUpdateLog` is declared before the world, because the components' lambdas write into it until the world is gone.

## 19. Lambda init-captures and references that must outlive the lambda

```cpp
TFunction<void(int32)> Action = …;
auto OnUpdate = [&Log, Id, Action = MoveTemp(Action)](UUpdateBudgetTestComponent& Self) { ++Log.Updates[Id]; Action(…); };
```
- **Init-capture** (`Name = expression`, C++14) creates a new member of the lambda from any expression. `Action = MoveTemp(Action)` moves the function object in instead of copying it.
- **Capture by reference** (`&Log`) stores only an address: the lambda is valid only while `Log` lives. Stored lambdas (in a `TFunction` member) make that rule easy to break, which is why the test declares `Log` before the world that owns the components.
- **In our code (S5):** the lifetime tests' `Spawn` helper builds each client's update hook this way (05 F6).

## 20. Never change a container while something iterates it

```cpp
const FBudgetClientState* State = States.GetData();   // a raw pointer into the array's memory
States.Add({});                                       // may move the whole array: State now points at freed memory
```
- **Why it breaks.** A `TArray` (like `std::vector`) keeps its elements in one block. When it grows past its capacity it allocates a new block, copies the elements and frees the old one. Every pointer, reference, view and iterator into the old block is now dangling. Removing an element shifts or swaps elements, so saved indices change meaning too.
- **The fix.** Defer the change: record it during the loop and apply it after. Replacing an element in place (`Clients[i] = nullptr`) is safe, because nothing moves.
- **In our code (S5):** the scheduler reads `States` through `GetData()` (05 E2), and an update may spawn or destroy other clients. So `Register` queues into `PendingAdds` and `Unregister` nulls the slot while the scheduler runs (05 F4).

## 21. `static` factory functions and `static constexpr` members

```cpp
struct FView
{
    static constexpr double AlwaysInViewRadius = 200.0;        // one constant for the type, no storage per object
    static FView FromCamera(const FMinimalViewInfo& Camera);   // builds a ready-to-use object
};
const FView View = FView::FromCamera(Camera);
```
- **A static factory** is a named constructor: `FromCamera` says what it builds from, which a constructor overload can't. It returns the object by value; the compiler builds it directly in the caller's variable (copy elision).
- **`static constexpr` data members** are inline variables since C++17: defined in the header, one value for the whole program, usable at compile time.
- **In our code (S6):** `FUpdateBudgetView::FromCamera` and `AlwaysInViewRadius` (05 G1).

## 22. Compare squares instead of taking square roots

```cpp
// Is the angle between Forward and V below A?   dot(F, V) > cos(A) * |V|
const double Along = FVector::DotProduct(Forward, V);
const bool bInside = Along > 0.0 && Along * Along > CosASquared * V.SizeSquared();   // no Sqrt, no Normalize
```
- **Why it works.** For two non-negative numbers, x > y exactly when x² > y². So once `Along > 0` is known (and the right side is never negative), squaring both sides keeps the answer and removes the square root of `|V|`.
- **Why it matters.** It runs for every client every frame. Distances are compared the same way: `DistanceSquared <= Square(NearDistance)`.
- **In our code (S6):** `FUpdateBudgetView::IsInView` and `FUpdateBudgetCategory::GetSignificance` (05 G1); the brain's cone test (B4) uses the same idea.
