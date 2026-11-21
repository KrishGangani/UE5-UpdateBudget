"""P01 UpdateBudget - Phase 7 stability checks on a batch: live switches under load, long runs and large N.

Usage: python -I check_stability.py <batch> [--fps 60]     <batch> may be "latest:<label>": the newest batch with that label
Writes Benchmarks/Summaries/<batch>_stability.md. Reads only the batch's manifest, raw CSVs and agent reports.

Per measured run, from every frame of the capture (each is a pass/fail check unless marked informational):
  repeats    UpdateBudget/RepeatUpdates == 0 on every frame: no client updated twice at one world time, i.e. twice in one
             frame (captures made after Phase 7's review carry this column; older ones show n/a).
  updates    UpdateBudget/UpdatesRun <= UpdateBudget/Clients on every frame. This catches a double update only in a frame
             where every client runs (native frames); with deferred clients it can't, which is what "repeats" is for.
  staleness  every frame's UpdateBudget/MaxGapMs, and every agent's age in the end-of-run report, within the run's own
             bound (CSV metadata ubmaxstaleness + one frame); no agent left without an update.
  switches   with -UBSwitchEvery (metadata ubswitchevery), the capture must hold one ModeSwitch event per switch point
             inside the capture; the mode the run ended in comes from the last event.
  drift      (informational) GameThreadTime and PhysicalUsedMB medians of the first and the last tenth of the capture.
             Not shown for runs that switch modes, whose tenths ran in different modes.
A run that run_bench.py counts as failed, or that can't be checked, is listed with its reason; so is a batch with none.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import statistics
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import analyse_ub  # noqa: E402  (same folder: series, agent_ages)
import csv_stats  # noqa: E402
import run_bench  # noqa: E402  (is_failure)

RAW = analyse_ub.RAW
SUMMARIES = analyse_ub.SUMMARIES


def tenth_medians(values: list[float]) -> tuple[float, float]:
    """Median of the first and of the last tenth of a per-frame series."""
    tenth = max(1, len(values) // 10)
    return statistics.median(values[:tenth]), statistics.median(values[-tenth:])


def expected_switches(record: dict, every: float) -> int:
    """Switch points k * every (k >= 1) strictly inside the capture, whose length is on the run's command line."""
    match = re.search(r"-UBCapture=([0-9.]+)", record.get("command", ""))
    if not match or every <= 0.0:
        return 0
    return max(0, math.ceil(float(match.group(1)) / every) - 1)


def check_run(record: dict, frame_ms: float) -> dict:
    header, data, metadata, finalised = csv_stats.load_capture(os.path.join(RAW, record["capture"]))
    clients = analyse_ub.series(header, data, "UpdateBudget/Clients")
    updates = analyse_ub.series(header, data, "UpdateBudget/UpdatesRun")
    gaps = analyse_ub.series(header, data, "UpdateBudget/MaxGapMs")
    repeats = analyse_ub.series(header, data, "UpdateBudget/RepeatUpdates")
    game_thread = analyse_ub.series(header, data, "GameThreadTime")
    memory = analyse_ub.series(header, data, "PhysicalUsedMB")
    if not (finalised and clients and len(clients) == len(updates) == len(gaps) == len(game_thread)):
        raise ValueError("not a finalised capture with the UpdateBudget columns")
    if "ubmaxstaleness" not in metadata:
        raise ValueError("the capture has no ubmaxstaleness metadata")
    ages = analyse_ub.agent_ages(os.path.join(RAW, record["agents_report"]))
    if not ages:
        raise ValueError("the agent report has no agents")
    bound_ms = float(metadata["ubmaxstaleness"]) * 1000.0 + frame_ms
    switch_events = [event for row in data for event in csv_stats.frame_events(header, row) if "ModeSwitch" in event]
    every = float(metadata.get("ubswitchevery", "-1"))
    expected = expected_switches(record, every)
    switching = bool(switch_events) or expected > 0
    gt_first, gt_last = tenth_medians(game_thread)
    mem_first, mem_last = tenth_medians(memory) if memory else (math.nan, math.nan)
    return {
        "name": record["name"], "config": record["config"], "agents": record["agents"], "frames": len(clients),
        "repeats": sum(repeats) if repeats else None,
        "over_frames": sum(1 for u, c in zip(updates, clients) if u > c),
        "bound_ms": bound_ms, "max_gap_ms": max(gaps), "gap_violations": sum(1 for g in gaps if g > bound_ms),
        "max_age_ms": max(ages), "age_violations": sum(1 for a in ages if a > bound_ms),
        "never_updated": sum(1 for a in ages if a < 0),
        "switches": len(switch_events), "expected": expected,
        "ended_in": switch_events[-1].split()[-1] if switch_events else metadata.get("ubmode", "?"),
        "drift": None if switching else (gt_first, gt_last, mem_first, mem_last),
    }


def passed(r: dict) -> bool:
    return (r["repeats"] in (None, 0) and r["over_frames"] == 0 and r["gap_violations"] == 0 and r["age_violations"] == 0
            and r["never_updated"] == 0 and r["switches"] == r["expected"])


def resolve_batch(name: str) -> str:
    """A batch name, or "latest:<label>" for the newest batch whose name ends with that label (for chained queue jobs)."""
    if not name.startswith("latest:"):
        return name
    suffix = "_" + name.split(":", 1)[1] + "_manifest.json"
    matches = sorted(f for f in os.listdir(RAW) if f.endswith(suffix))
    if not matches:
        raise SystemExit(f"no batch ends with {suffix}")
    return matches[-1][: -len("_manifest.json")]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("batch")
    parser.add_argument("--fps", type=float, default=60.0, help="the fixed step of the runs")
    args = parser.parse_args(argv)
    args.batch = resolve_batch(args.batch)
    with open(os.path.join(RAW, args.batch + "_manifest.json"), encoding="utf-8") as handle:
        manifest = json.load(handle)

    results, failed = [], []
    for record in manifest["runs"]:
        if record.get("priming"):
            continue
        if run_bench.is_failure(record):
            failed.append((record["name"], "failed in run_bench.py (exit code, log errors or no capture)"))
            continue
        try:
            results.append(check_run(record, 1000.0 / args.fps))
        except (OSError, ValueError, KeyError, IndexError) as error:
            failed.append((record["name"], str(error)))
    if not results and not failed:
        failed.append((args.batch, "the batch has no measured run"))

    lines = [f"# Stability check: {args.batch}", "",
             "Generated by `Tools/check_stability.py` from every frame of each capture. Staleness bound = the run's max "
             "staleness + one frame. One run per row: a check of correctness, not a performance result.", "",
             "| Run | Config | N | Frames | Repeat updates | Frames with more updates than clients | Max MaxGapMs | Frames over the bound | "
             "Max agent age (ms) | Agents over the bound | Never updated | Mode switches (expected) | Ended in | Verdict |",
             "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in results:
        repeats = "n/a" if r["repeats"] is None else f"{r['repeats']:.0f}"
        lines.append(f"| `{r['name']}` | {r['config']} | {r['agents']} | {r['frames']} | {repeats} | {r['over_frames']} | "
                     f"{r['max_gap_ms']:.3f} | {r['gap_violations']} | {r['max_age_ms']:.3f} | {r['age_violations']} | "
                     f"{r['never_updated']} | {r['switches']} ({r['expected']}) | {r['ended_in']} | {'PASS' if passed(r) else 'FAIL'} |")
    lines += ["", "Drift (informational; not shown for runs that switch modes):", "",
              "| Run | GameThreadTime median, first / last tenth (ms) | PhysicalUsedMB median, first / last tenth | Memory change |",
              "|---|---|---|---|"]
    for r in results:
        if r["drift"] is None:
            lines.append(f"| `{r['name']}` | n/a (modes switch) | n/a | n/a |")
        else:
            gt_first, gt_last, mem_first, mem_last = r["drift"]
            lines.append(f"| `{r['name']}` | {gt_first:.3f} / {gt_last:.3f} | {mem_first:.1f} / {mem_last:.1f} | {mem_last - mem_first:+.1f} MB |")
    if failed:
        lines += ["", "**Runs not checked:**", ""] + [f"- `{name}`: {reason}" for name, reason in failed]
    os.makedirs(SUMMARIES, exist_ok=True)
    out = os.path.join(SUMMARIES, args.batch + "_stability.md")
    with open(out, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    print("stability check:", out)
    return 0 if results and all(passed(r) for r in results) and not failed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
