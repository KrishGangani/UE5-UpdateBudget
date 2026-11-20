// P01 UpdateBudget (demo) - UUBBenchmarkSubsystem.

#include "UBBenchmarkSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"
#include "UBDemoAgent.h"
#include "UBDemoBrainComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBBenchmarkSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogUBBench, Log, All);

// CSV category for the run's events (BenchStart / Wave / BenchEnd).
CSV_DEFINE_CATEGORY(UBBench, true);

namespace UBBench
{
	/** Distance between grid cells, in cm. Patrol circles are 6 m across (3 m radius), so neighbouring circles overlap;
	 *  agents have no collision, so that only affects visuals. 4 m keeps 4,000 agents within a 250 m square. */
	constexpr double GridSpacing = 400.0;
	/** Random offset applied to each grid cell, in cm, so the layout isn't perfectly regular. */
	constexpr double GridJitter = 100.0;
	/** Height of agent centres: the 50 cm cubes rest on the floor at z = 0. */
	constexpr double AgentHeight = 25.0;
	/** Camera path: a circle at half the arena's half-width (at least 15 m), 4 m up, pitched down 15 degrees. */
	constexpr double CameraHeight = 400.0;
	constexpr double CameraMinRadius = 1500.0;
	constexpr double CameraPitchDegrees = -15.0;
	/** Frames to wait after BenchEnd before quitting, so the CSV profiler finishes writing its file. */
	constexpr int32 ExitDelayFrames = 3;
}

bool UUBBenchmarkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UUBBenchmarkSubsystem::FRunSettings UUBBenchmarkSubsystem::ReadRunSettings()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FRunSettings Read;
	FParse::Value(CommandLine, TEXT("UBAgents="), Read.NumAgents);
	FParse::Value(CommandLine, TEXT("UBSeed="), Read.Seed);
	FParse::Value(CommandLine, TEXT("UBWork="), Read.ComputeIterations);
	FParse::Value(CommandLine, TEXT("UBWarmup="), Read.WarmupSeconds);
	FParse::Value(CommandLine, TEXT("UBCapture="), Read.CaptureSeconds);
	FParse::Value(CommandLine, TEXT("UBWaveAt="), Read.WaveAtSeconds);
	FParse::Value(CommandLine, TEXT("UBRunTag="), Read.RunTag);
	Read.bExitWhenDone = FParse::Param(CommandLine, TEXT("UBExit"));
	return Read;
}

void UUBBenchmarkSubsystem::ValidateRunSettings() const
{
	if (Settings.WarmupSeconds < 0.0 || Settings.CaptureSeconds <= 0.0)
	{
		UE_LOG(LogUBBench, Error, TEXT("Invalid run timing: warm-up %.3f s, capture %.3f s (warm-up must be >= 0, capture > 0)."),
			Settings.WarmupSeconds, Settings.CaptureSeconds);
	}
	if (Settings.WaveAtSeconds >= Settings.CaptureSeconds)
	{
		UE_LOG(LogUBBench, Error, TEXT("-UBWaveAt=%.3f is not inside the %.3f s capture: the wave would never be measured."),
			Settings.WaveAtSeconds, Settings.CaptureSeconds);
	}
}

void UUBBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	Settings = ReadRunSettings();
	if (Settings.NumAgents <= 0)
	{
		return; // No workload requested: the world behaves like a normal empty map.
	}
	ValidateRunSettings();

	const double SpawnStart = FPlatformTime::Seconds();
	const uint32 PositionHash = SpawnAgents(InWorld, Settings.NumAgents, Settings.Seed);
	UE_LOG(LogUBBench, Display, TEXT("Spawned %d agents (seed %d) in %.1f ms; start-position hash 0x%08X"),
		Agents.Num(), Settings.Seed, (FPlatformTime::Seconds() - SpawnStart) * 1000.0, PositionHash);

	// Record the compute-step size actually in effect, not "-1 = default", so recalibrations stay visible.
	for (const AUBDemoAgent* Agent : Agents)
	{
		if (IsValid(Agent))
		{
			EffectiveComputeIterations = Agent->GetBrain()->GetComputeIterations();
			break;
		}
	}

	Camera = InWorld.SpawnActor<ACameraActor>();
	BeginPlayTime = InWorld.GetTimeSeconds();
	Phase = ERunPhase::WarmUp;

	// Run description inside the CSV itself, so every capture is self-describing.
	CSV_METADATA(TEXT("UBRunTag"), *Settings.RunTag);
	CSV_METADATA(TEXT("UBAgents"), *FString::FromInt(Settings.NumAgents));
	CSV_METADATA(TEXT("UBSeed"), *FString::FromInt(Settings.Seed));
	CSV_METADATA(TEXT("UBWork"), *FString::FromInt(EffectiveComputeIterations));
	CSV_METADATA(TEXT("UBWaveAt"), *FString::SanitizeFloat(Settings.WaveAtSeconds));
}

uint32 UUBBenchmarkSubsystem::SpawnAgents(UWorld& World, int32 Count, int32 Seed)
{
	FRandomStream Random(Seed);
	const int32 Side = FMath::CeilToInt32(FMath::Sqrt(static_cast<double>(Count)));
	const double HalfExtent = 0.5 * UBBench::GridSpacing * (Side - 1);
	ArenaHalfExtent = FMath::Max(ArenaHalfExtent, HalfExtent);

	uint32 PositionHash = 0;
	Agents.Reserve(Agents.Num() + Count);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Column = Index % Side;
		const int32 Row = Index / Side;
		const FVector Center(
			Column * UBBench::GridSpacing - HalfExtent + Random.FRandRange(-UBBench::GridJitter, UBBench::GridJitter),
			Row * UBBench::GridSpacing - HalfExtent + Random.FRandRange(-UBBench::GridJitter, UBBench::GridJitter),
			UBBench::AgentHeight);
		const double Phase0 = Random.FRandRange(0.0, 2.0 * UE_DOUBLE_PI);

		// Deferred spawn: the brain's patrol is set *before* BeginPlay runs.
		AUBDemoAgent* Agent = World.SpawnActorDeferred<AUBDemoAgent>(AUBDemoAgent::StaticClass(), FTransform(Center),
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Agent)
		{
			UE_LOG(LogUBBench, Error, TEXT("Spawning agent %d failed; workload is incomplete."), Index);
			continue;
		}

		UUBDemoBrainComponent* Brain = Agent->GetBrain();
		Brain->InitPatrol(Center, Phase0);
		if (Settings.ComputeIterations >= 0)
		{
			Brain->SetComputeIterations(Settings.ComputeIterations);
		}
		const FVector StartPosition = Brain->GetPatrolPosition(0.0);
		Agent->FinishSpawning(FTransform(StartPosition));
		Agents.Add(Agent);

		// Running CRC: chaining equals one CRC over all positions, without buffering them.
		PositionHash = FCrc::TypeCrc32(FVector3f(StartPosition), PositionHash);
	}

	return PositionHash;
}

void UUBBenchmarkSubsystem::UpdateCamera(double SecondsFromBenchStart)
{
	if (!Camera)
	{
		return;
	}

	// One lap per capture window, angle 0 at the planned BenchStart: the captured path never depends on warm-up length.
	const double Radius = FMath::Max(UBBench::CameraMinRadius, 0.5 * ArenaHalfExtent);
	const double LapSeconds = FMath::Max(1.0, Settings.CaptureSeconds);
	const double Angle = 2.0 * UE_DOUBLE_PI * SecondsFromBenchStart / LapSeconds;
	const FVector Location(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), UBBench::CameraHeight);
	const FVector Forward(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0);
	FRotator Rotation = Forward.Rotation();
	Rotation.Pitch = UBBench::CameraPitchDegrees;
	Camera->SetActorLocationAndRotation(Location, Rotation);

	// Re-applied every frame: possessing a pawn later would otherwise reset the view target.
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController && PlayerController->GetViewTarget() != Camera)
	{
		PlayerController->SetViewTarget(Camera);
	}
}

void UUBBenchmarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Phase == ERunPhase::Idle)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const double Elapsed = Now - BeginPlayTime;
	UpdateCamera(Elapsed - Settings.WarmupSeconds);

	switch (Phase)
	{
	case ERunPhase::WarmUp:
		if (Elapsed >= Settings.WarmupSeconds)
		{
			BenchStartTime = Now;
			Phase = ERunPhase::Capture;
			CSV_EVENT(UBBench, TEXT("BenchStart"));
			UE_LOG(LogUBBench, Display, TEXT("BenchStart at %.3f s (warm-up %.1f s)"), Now, Settings.WarmupSeconds);
		}
		break;

	case ERunPhase::Capture:
	{
		const double InCapture = Now - BenchStartTime;
		if (!bWaveSpawned && Settings.WaveAtSeconds >= 0.0 && InCapture >= Settings.WaveAtSeconds)
		{
			bWaveSpawned = true;
			CSV_EVENT(UBBench, TEXT("Wave"));
			const uint32 WaveHash = SpawnAgents(*GetWorld(), Settings.NumAgents, Settings.Seed + 1);
			UE_LOG(LogUBBench, Display, TEXT("Wave: %d agents now (hash 0x%08X)"), Agents.Num(), WaveHash);
		}
		if (InCapture >= Settings.CaptureSeconds)
		{
			// The stop request is processed at the end of this frame; the report is written next frame, outside the capture.
			CSV_EVENT(UBBench, TEXT("BenchEnd"));
			Phase = ERunPhase::Done;
			FramesSinceEnd = 0;
			UE_LOG(LogUBBench, Display, TEXT("BenchEnd after %.3f s of capture"), InCapture);
		}
		break;
	}

	case ERunPhase::Done:
		++FramesSinceEnd;
		if (FramesSinceEnd == 1)
		{
			WriteRunReport();
		}
		if (Settings.bExitWhenDone && FramesSinceEnd >= UBBench::ExitDelayFrames)
		{
			Settings.bExitWhenDone = false; // Request once.
			FPlatformMisc::RequestExit(false, TEXT("UUBBenchmarkSubsystem"));
		}
		break;

	default:
		break;
	}
}

void UUBBenchmarkSubsystem::WriteRunReport() const
{
	const double Now = GetWorld()->GetTimeSeconds();
	FString Report;
	Report.Reserve(Agents.Num() * 96);
	Report += FString::Printf(TEXT("# run=%s agents=%d seed=%d work=%d warmup=%.3f capture=%.3f waveat=%.3f benchstart=%.6f reporttime=%.6f\n"),
		*Settings.RunTag, Agents.Num(), Settings.Seed, EffectiveComputeIterations,
		Settings.WarmupSeconds, Settings.CaptureSeconds, Settings.WaveAtSeconds, BenchStartTime, Now);
	Report += TEXT("index,x,y,z,patrol_time,last_update,age_at_report_ms,state_changes\n");

	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		const AUBDemoAgent* Agent = Agents[Index];
		if (!IsValid(Agent))
		{
			Report += FString::Printf(TEXT("%d,,,,,,,\n"), Index);
			continue;
		}
		const UUBDemoBrainComponent* Brain = Agent->GetBrain();
		const FVector Position = Agent->GetActorLocation();
		const double LastUpdate = Brain->GetLastUpdateTime();
		const double AgeMs = (LastUpdate >= 0.0) ? (Now - LastUpdate) * 1000.0 : -1.0; // -1: never updated
		Report += FString::Printf(TEXT("%d,%.4f,%.4f,%.4f,%.6f,%.6f,%.3f,%d\n"),
			Index, Position.X, Position.Y, Position.Z, Brain->GetPatrolTime(), LastUpdate, AgeMs, Brain->GetStateChanges());
	}

	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UBRuns"), Settings.RunTag + TEXT(".csv"));
	if (FFileHelper::SaveStringToFile(Report, *Path))
	{
		UE_LOG(LogUBBench, Display, TEXT("Run report written: %s"), *FPaths::ConvertRelativePathToFull(Path));
	}
	else
	{
		UE_LOG(LogUBBench, Error, TEXT("Could not write the run report: %s"), *Path);
	}
}

TStatId UUBBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUBBenchmarkSubsystem, STATGROUP_Tickables);
}
