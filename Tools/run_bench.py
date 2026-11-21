"""P01 UpdateBudget - run a benchmark batch on the packaged build and collect every artefact.

Usage (from anywhere):
    python run_bench.py --configs B0,B1 --agents 1000,2000 [--runs N] [--work 128] [--capture 60] [--warmup 10]
                        [--wave-at SEC] [--label NAME] [--cooldown 5] [--no-prime] [--dry-run]

What one batch does:
  1. Records the environment once (CPU, GPU + driver, OS build, power plan, AC power, engine, resolution).
  2. Runs one PRIMING run first (discarded) so file, driver and PSO caches are warm for every measured run.
  3. Runs every (config x agents x work) combination --runs times, INTERLEAVED: the config order rotates every
     round so thermal drift on this laptop spreads over all configs. For the default number of runs see --runs
     (Benchmarks/README.md, measurement rules). On a hybrid CPU the batch process pins itself to the performance
     cores, so every game it starts inherits that mask; the applied mask is recorded in the environment.
  4. Before each run, deletes the build's GameUserSettings.ini (it can override VSync / frame-rate caps).
  5. After each run, moves the CSV capture, the per-agent run report and the run's own log into Benchmarks/Raw as
     <batch>_<Config>[_W<work>]_N<agents>_r<run>.<ext>, where <batch> = YYYY-MM-DD_HHMMSS[_label].
     Existing files are never overwritten. The manifest is rewritten after every run, so a crash loses nothing.
  6. Writes Benchmarks/Summaries/<batch>_summary.md: per combination, the median across runs of each run's
     median / p95 / p99 [min-max across runs]. Wave batches summarise the frames AFTER the wave; pre-wave and
     the wave frame itself are kept in the manifest.

Every run uses: fixed step (-deterministic -FPS=60), real GPU at 1280x720 windowed, no VSync, no sound,
-traceautostart=0, and a capture window started/stopped by the game's BenchStart/BenchEnd events.
"""
from __future__ import annotations

import argparse
import ctypes
import datetime as dt
import glob
import json
import math
import os
import shutil
import statistics
import struct
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)
import csv_stats  # noqa: E402  (same folder)

PROJECT_DIR = os.path.dirname(TOOLS_DIR)
BUILD_DIR = os.path.join(PROJECT_DIR, "Build", "Windows")
# The real game executable, launched directly. (The packaged bootstrap Build\Windows\UpdateBudget.exe only starts
# this one; launching it directly makes the game a direct child, so its exit code and its kill are reliable.)
EXE = os.path.join(BUILD_DIR, "UpdateBudget", "Binaries", "Win64", "UpdateBudget.exe")
SAVED = os.path.join(BUILD_DIR, "UpdateBudget", "Saved")
RAW_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Raw")
SUMMARY_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Summaries")
ENGINE_BUILD_VERSION = r"C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"
MIN_RUNS = 4                    # fewest measured runs per combination (Benchmarks/README.md, measurement rules)
STARTUP_MARGIN_SECONDS = 180.0  # generous allowance for engine start, spawning and shutdown before a run is declared hung

COMMON_ARGS = ("-windowed -ResX=1280 -ResY=720 -nosound -unattended -traceautostart=0 "
               "-deterministic -FPS=60 -novsync -UBExit -csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd")

# Extra command-line arguments per benchmark configuration (02_System_Architecture.md section 9).
CONFIGS = {
    "B0": "",                                             # native tick, every frame
    "B1": '-ExecCmds="tick.AllowBatchedTicks 1"',          # B0 + engine tick batching
    "B2": "-UBBaseline=B2",                                # native tick + distance-band TickInterval
    "B3": "-UBBaseline=B3",                                # native tick + Significance Manager -> TickInterval
    # UpdateBudget (S6). ub.Enabled is read when the world begins play, so it is set through the ini system before
    # start-up; -ExecCmds would run too late (02 section 13). The budget comes from the project settings (2.0 ms).
    "UB": "-ini:Engine:[ConsoleVariables]:ub.Enabled=1",
    # Phase 7 stability: UB with ub.Enabled toggled every 10 s of the capture (live switches under load).
    "UBSW": "-ini:Engine:[ConsoleVariables]:ub.Enabled=1 -UBSwitchEvery=10",
}

# The first six exist in every capture; the UpdateBudget scheduler columns only in UB captures (csv_stats skips absent ones).
SUMMARY_COLUMNS = ("GameThreadTime", "FrameTime", "RenderThreadTime", "GPUTime", "UpdateBudget/UpdatesRun", "UpdateBudget/MaxGapMs",
                   "UpdateBudget/UpdateMs", "UpdateBudget/MandatoryMs", "UpdateBudget/MandatoryRuns", "UpdateBudget/OverBudget", "UpdateBudget/NearMissed",
                   "UpdateBudget/GameThread/RunFrame", "UpdateBudget/GameThread/Deadlines")


def powershell(command: str) -> str:
    result = subprocess.run(["powershell", "-NoProfile", "-Command", command], capture_output=True, text=True)
    return result.stdout.strip()


KERNEL32 = None
if sys.platform == "win32":
    KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
    KERNEL32.GetCurrentProcess.restype = ctypes.c_void_p
    KERNEL32.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    KERNEL32.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]


def performance_core_mask() -> int | None:
    """Affinity mask of the performance cores on a hybrid CPU; None on a non-hybrid CPU or off Windows.

    Why: on this i7-14650HX (8 P-cores + 8 E-cores) the S3 baseline batch showed runs that sat at two stable levels
    about 1.6x apart, and one run that stepped between them mid-capture: Windows had placed the game thread on an
    E-core (09, step S3). Pinning to the P-cores gives every run what a foreground game normally gets.
    Source of truth: GetLogicalProcessorInformationEx(RelationProcessorCore) reports an EfficiencyClass per physical
    core; the P-cores have the higher class. Only processor group 0 exists on a machine with < 64 logical CPUs.
    """
    if KERNEL32 is None:
        return None
    length = ctypes.c_ulong(0)
    KERNEL32.GetLogicalProcessorInformationEx(0, None, ctypes.byref(length))
    buffer = ctypes.create_string_buffer(length.value)
    if not length.value or not KERNEL32.GetLogicalProcessorInformationEx(0, buffer, ctypes.byref(length)):
        return None
    raw, offset, masks = buffer.raw, 0, {}          # efficiency class -> combined mask of its cores
    while offset < length.value:
        relationship, size = struct.unpack_from("<II", raw, offset)
        if relationship == 0 and size >= 48:                     # RelationProcessorCore
            efficiency_class = raw[offset + 9]                   # PROCESSOR_RELATIONSHIP.EfficiencyClass
            mask, group = struct.unpack_from("<QH", raw, offset + 32)   # GroupMask[0]: KAFFINITY Mask, WORD Group
            if group == 0:
                masks[efficiency_class] = masks.get(efficiency_class, 0) | mask
        offset += size
    return masks[max(masks)] if len(masks) > 1 else None


PERFORMANCE_CORE_MASK = performance_core_mask()


def pin_self(mask: int) -> bool:
    """Restricts this batch process to the CPUs in mask. Every process it starts afterwards inherits the mask from its
    first instruction, so each game runs pinned for its whole life, engine start-up included."""
    return bool(KERNEL32.SetProcessAffinityMask(KERNEL32.GetCurrentProcess(), mask))


def applied_mask() -> str:
    """The affinity this process actually has (read back from Windows, not the one requested)."""
    if KERNEL32 is None:
        return "not Windows"
    process_mask, system_mask = ctypes.c_size_t(0), ctypes.c_size_t(0)
    KERNEL32.GetProcessAffinityMask(KERNEL32.GetCurrentProcess(), ctypes.byref(process_mask), ctypes.byref(system_mask))
    if process_mask.value == system_mask.value:
        return f"0x{process_mask.value:X} (all logical processors)"
    return f"0x{process_mask.value:X} (performance cores only; hybrid CPU)"


def environment_record() -> dict:
    """Facts that must accompany every result (Benchmarks/README.md, environment record)."""
    with open(ENGINE_BUILD_VERSION, encoding="utf-8") as handle:
        build = json.load(handle)
    return {
        "date": dt.datetime.now().isoformat(timespec="seconds"),
        "engine": f"{build['MajorVersion']}.{build['MinorVersion']}.{build['PatchVersion']} (CL {build['Changelist']})",
        "build_config": "Development (packaged)",
        "compiler": "MSVC 14.44.35229, Windows SDK 10.0.22621.0",
        "cpu": powershell("(Get-CimInstance Win32_Processor).Name"),
        "gpus": powershell("(Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name + ' (driver ' + $_.DriverVersion + ')' }) -join '; '"),
        "ram_gb": powershell("[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)"),
        "os": powershell("$o=Get-CimInstance Win32_OperatingSystem; $o.Caption + ' build ' + $o.BuildNumber"),
        "power_plan": powershell("powercfg /getactivescheme"),
        "on_ac_power": powershell("(Get-CimInstance Win32_Battery | Select-Object -First 1).BatteryStatus") == "2",
        "resolution": "1280x720 windowed, default scalability",
        "common_args": COMMON_ARGS,
        "cpu_affinity": applied_mask(),
    }


def unique_path(path: str) -> str:
    """Raw data is never overwritten (Benchmarks/README.md)."""
    if os.path.exists(path):
        raise FileExistsError(f"refusing to overwrite raw data: {path}")
    return path


def launch(name: str, config: str, agents: int, work: int | None, args) -> tuple[str, int | None, float, float]:
    """Starts one run and waits for it. Returns (command, exit code or None on timeout, start time, wall seconds)."""
    ub_args = f"-UBAgents={agents} -UBSeed={args.seed} -UBWarmup={args.warmup} -UBCapture={args.capture} -UBRunTag={name}"
    if work is not None:
        ub_args += f" -UBWork={work}"
    if args.wave_at is not None:
        ub_args += f" -UBWaveAt={args.wave_at}"
    # One string, so -ExecCmds="..." reaches Unreal exactly as written.
    command = f'"{EXE}" {COMMON_ARGS} {ub_args} {CONFIGS[config]}'.strip()
    if args.dry_run:
        print("DRY:", command)
        return command, 0, time.time(), 0.0

    gus = os.path.join(SAVED, "Config", "Windows", "GameUserSettings.ini")
    if os.path.exists(gus):
        os.remove(gus)
    started = time.time()
    process = subprocess.Popen(command)   # inherits this process's CPU affinity (main: pin_self)
    try:
        exit_code = process.wait(timeout=args.warmup + args.capture + STARTUP_MARGIN_SECONDS)
    except subprocess.TimeoutExpired:
        exit_code = None
    finally:
        if process.poll() is None:   # timeout, Ctrl+C or any other error: never leave a game running into the next run
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True)
    return command, exit_code, started, time.time() - started


def collect(name: str, started: float, split_event: str | None) -> dict:
    """Moves this run's artefacts into Benchmarks/Raw and returns what was found."""
    record: dict = {}
    os.makedirs(RAW_DIR, exist_ok=True)

    csv_files = [p for p in glob.glob(os.path.join(SAVED, "Profiling", "CSV", "*.csv")) if os.path.getmtime(p) >= started]
    if csv_files:
        destination = unique_path(os.path.join(RAW_DIR, name + ".csv"))
        shutil.move(max(csv_files, key=os.path.getmtime), destination)
        record["capture"] = os.path.basename(destination)
        try:
            analysis = csv_stats.analyse(destination, SUMMARY_COLUMNS, 0, split_event)
            record["stats"] = analysis["columns"]
            record["finalised"] = analysis["finalised"]
            record["events"] = analysis["events"]
            if "split" in analysis:
                record["split"] = analysis["split"]
        except (ValueError, IndexError) as error:
            record["analysis_error"] = str(error)

    report = os.path.join(SAVED, "UBRuns", name + ".csv")
    if os.path.exists(report):
        shutil.move(report, unique_path(os.path.join(RAW_DIR, name + "_agents.csv")))
        record["agents_report"] = name + "_agents.csv"

    log = os.path.join(SAVED, "Logs", "UpdateBudget.log")
    if os.path.exists(log) and os.path.getmtime(log) >= started:   # only this run's log, never a stale one
        with open(log, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        record["log_errors"] = text.count("Error:")
        record["log_warnings"] = text.count("Warning:")
        shutil.copy(log, unique_path(os.path.join(RAW_DIR, name + ".log")))
    else:
        record["log_missing"] = True
    return record


def is_failure(record: dict) -> bool:
    return (record.get("exit_code") != 0 or "stats" not in record or record.get("log_errors")
            or record.get("log_missing") or record.get("finalised") is False or "analysis_error" in record)


def stats_for_summary(record: dict) -> dict:
    """Wave runs are summarised on the frames after the wave; everything else on the whole capture."""
    split = record.get("split")
    if split and split.get("post"):
        return split["post"]
    return record.get("stats", {})


def write_manifest(batch: str, environment: dict, records: list[dict]) -> None:
    os.makedirs(RAW_DIR, exist_ok=True)
    with open(os.path.join(RAW_DIR, batch + "_manifest.json"), "w", encoding="utf-8") as handle:
        json.dump({"environment": environment, "runs": records}, handle, indent=2)


def write_summary(batch: str, environment: dict, records: list[dict], wave: bool) -> str:
    os.makedirs(SUMMARY_DIR, exist_ok=True)
    groups: dict[tuple, list[dict]] = {}
    for record in records:
        if "stats" in record and not record.get("priming"):
            groups.setdefault((record["config"], record["work"], record["agents"]), []).append(record)

    scope = "frames after the wave (pre-wave and the wave frame are in the manifest)" if wave else "the whole capture"
    lines = [f"# Benchmark batch {batch}", "", "## Environment", "", "```json", json.dumps(environment, indent=2), "```", "",
             f"## Results over {scope}", "",
             "Each cell: median across runs of each run's statistic, then [min-max across runs].", ""]
    header = "| Config | Work | N | Runs | " + " | ".join(f"{c} median / p95 / p99 (ms)" for c in SUMMARY_COLUMNS[:2]) + " | UpdatesRun median | PSO events | Log errors |"
    lines += [header, "|" + "---|" * (header.count("|") - 1)]
    for (config, work, agents), group in sorted(groups.items(), key=lambda kv: (kv[0][2], str(kv[0][1]), kv[0][0])):
        cells = []
        for column in SUMMARY_COLUMNS[:2]:
            parts = []
            for stat in ("median", "p95", "p99"):
                values = [stats_for_summary(g)[column][stat] for g in group if column in stats_for_summary(g)]
                parts.append(f"{statistics.median(values):.3f} [{min(values):.3f}-{max(values):.3f}]" if values else "n/a")
            cells.append(" / ".join(parts))
        updates = [stats_for_summary(g)["UpdateBudget/UpdatesRun"]["median"] for g in group if "UpdateBudget/UpdatesRun" in stats_for_summary(g)]
        pso = sum(g.get("events", {}).get("PSO", 0) for g in group)
        errors = sum(g.get("log_errors", 0) for g in group)
        lines.append(f"| {config} | {work if work is not None else 'default'} | {agents} | {len(group)} | " + " | ".join(cells)
                     + f" | {statistics.median(updates) if updates else 'n/a'} | {pso} | {errors} |")
    lines += ["", f"Raw files: `Benchmarks/Raw/{batch}_*` (CSV, agent report and log per run) and `{batch}_manifest.json`."]
    path = os.path.join(SUMMARY_DIR, batch + "_summary.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--configs", default="B0")
    parser.add_argument("--agents", default="1000")
    parser.add_argument("--work", default="", help="comma list of compute iterations; empty = the brain's calibrated default")
    parser.add_argument("--runs", type=int, default=0,
                        help=f"runs per combination; 0 (default) = the smallest multiple of the number of configs that is "
                             f"at least {MIN_RUNS}, so the rotation puts every config in every order position equally often")
    parser.add_argument("--warmup", type=float, default=10.0)
    parser.add_argument("--capture", type=float, default=60.0)
    parser.add_argument("--wave-at", type=float, default=None)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--cooldown", type=float, default=5.0)
    parser.add_argument("--label", default="")
    parser.add_argument("--no-prime", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    configs = [c.strip() for c in args.configs.split(",") if c.strip()]
    unknown = [c for c in configs if c not in CONFIGS]
    if unknown:
        parser.error(f"unknown config(s) {unknown}; available: {sorted(CONFIGS)}")
    if "UBSW" in configs and args.capture <= 10.0:
        parser.error("UBSW switches every 10 s: --capture must be longer than 10 s")
    if args.wave_at is not None and not (0.0 <= args.wave_at < args.capture):
        parser.error("--wave-at must be inside the capture window (0 <= wave-at < capture)")
    agent_counts = [int(a) for a in args.agents.split(",") if a.strip()]
    if args.runs <= 0:
        args.runs = math.ceil(MIN_RUNS / len(configs)) * len(configs)
    elif args.runs % len(configs):
        print(f"WARNING: --runs {args.runs} is not a multiple of {len(configs)} configs; the rotation is unbalanced")
    works = [int(w) for w in args.work.split(",") if w.strip()] or [None]
    if not os.path.exists(EXE) and not args.dry_run:
        parser.error(f"packaged build not found: {EXE} (run Tools\\package.bat first)")

    # Keep the display and the system awake while the batch runs, the way a video player does: an occluded or
    # locked desktop can throttle a windowed game's presentation. It applies only while this process lives and
    # changes no power settings (SetThreadExecutionState: ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED).
    if not args.dry_run and KERNEL32 is not None:
        KERNEL32.SetThreadExecutionState(0x80000000 | 0x00000001 | 0x00000002)
        # Pin once, before anything is recorded or started: every game started below inherits the mask (no start-up
        # window, nothing to undo), and the environment record reads back the mask actually applied.
        if PERFORMANCE_CORE_MASK and not pin_self(PERFORMANCE_CORE_MASK):
            parser.error(f"could not pin the batch to the performance cores (0x{PERFORMANCE_CORE_MASK:X}); refusing to "
                         "measure on a hybrid CPU without it")

    batch = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S") + (f"_{args.label}" if args.label else "")
    environment = environment_record() if not args.dry_run else {}
    split_event = "Wave" if args.wave_at is not None else None
    records: list[dict] = []

    def run(config: str, agents: int, work: int | None, run_index: int, priming: bool = False) -> None:
        name = f"{batch}_{config}" + (f"_W{work}" if work is not None else "") + f"_N{agents}_r{run_index}" + ("_prime" if priming else "")
        command, exit_code, started, wall = launch(name, config, agents, work, args)
        record = {"name": name, "config": config, "agents": agents, "work": work, "run": run_index, "priming": priming,
                  "exit_code": exit_code, "wall_seconds": round(wall, 1), "command": command}
        if not args.dry_run:
            record.update(collect(name, started, split_event))
            records.append(record)
            write_manifest(batch, environment, records)
            gt = stats_for_summary(record).get("GameThreadTime", {})
            print(f"  {name}: exit {exit_code}, {wall:.0f} s, GameThreadTime median {gt.get('median', float('nan')):.3f} ms "
                  f"p99 {gt.get('p99', float('nan')):.3f} ms, PSO events {record.get('events', {}).get('PSO', 0)}, "
                  f"log errors {record.get('log_errors')}" + ("  [PRIMING, discarded]" if priming else ""))
            time.sleep(args.cooldown)

    if not args.no_prime:
        run(configs[0], min(agent_counts), works[0], 0, priming=True)

    for run_index in range(1, args.runs + 1):
        rotation = (run_index - 1) % len(configs)
        round_configs = configs[rotation:] + configs[:rotation]       # interleave: rotate the order every round
        for agents in agent_counts:
            for work in works:
                for config in round_configs:
                    run(config, agents, work, run_index)

    if args.dry_run:
        return 0
    print("summary:", write_summary(batch, environment, records, split_event is not None))
    failures = [r["name"] for r in records if not r.get("priming") and is_failure(r)]
    if failures:
        print("ATTENTION - runs with a non-zero exit, timeout, no/partial capture, missing log or log errors:", failures)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
