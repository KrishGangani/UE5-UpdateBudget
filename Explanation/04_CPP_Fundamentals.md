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
