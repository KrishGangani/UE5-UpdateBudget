"""P01 UpdateBudget - check the UB runs of a batch against success criteria SC1-SC3 (01_Problem_Analysis.md section 6.4).

Usage: python -I analyse_ub.py <batch> [--fps 60]
Writes Benchmarks/Summaries/<batch>_ub_check.md. Reads only the batch's manifest, raw CSVs and agent reports.

Per measured UB run:
  SC1 budget adherence  p95 of UpdateBudget/UpdateMs <= 1.10 x the budget (the run's UpdateBudget/BudgetMs).
                        Over-budget frames are split by cause: the mandatory updates alone cost more than the budget
                        (UpdateBudget/MandatoryMs, forced), or optional updates ran and the measured cost came out
                        above the prediction (soft budget).
  SC2 no starvation     every frame's UpdateBudget/MaxGapMs, and every agent's age in the end-of-run report,
                        <= max staleness + one frame; no agent left without an update. Violations must be 0.
                        The max staleness is the run's own (CSV metadata ubmaxstaleness, written by the subsystem).
  SC3 relevance first   share of frames with UpdateBudget/NearMissed == 0 (target >= 99 % at N <= 2,000).
Plus GameThreadTime and the scheduler's own timing scopes, for context (from the manifest: run_bench.py computed them).
One run is a check, not evidence. A run that run_bench.py counts as failed (run_bench.is_failure), or that can't be
analysed, is listed with its reason instead of stopping the check.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import csv_stats  # noqa: E402  (same folder)
import run_bench  # noqa: E402  (same folder; only is_failure is used)

PROJECT = os.path.dirname(TOOLS)
RAW = os.path.join(PROJECT, "Benchmarks", "Raw")
SUMMARIES = os.path.join(PROJECT, "Benchmarks", "Summaries")
SC1_FACTOR = 1.10        # 01 section 6.4
SC3_SHARE = 0.99


def series(header: list[str], data: list[list[str]], name: str) -> list[float]:
    return csv_stats.column_values(header, data, name) if name in header else []


NAN_SUMMARY = {key: float("nan") for key in ("median", "p95", "p99", "max")}


def agent_ages(path: str) -> list[float]:
    """Ages at the end of the run (ms) from the per-agent report; -1 means never updated. Destroyed agents are skipped."""
    with open(path, newline="", encoding="utf-8") as handle:
        rows = [row for row in csv.reader(handle) if row and not row[0].startswith("#")]
    column = rows[0].index("age_at_report_ms")
    return [float(row[column]) for row in rows[1:] if len(row) > column and row[column] != ""]


def check_run(record: dict, frame_ms: float) -> dict:
    header, data, metadata, finalised = csv_stats.load_capture(os.path.join(RAW, record["capture"]))
    update_ms = series(header, data, "UpdateBudget/UpdateMs")
    budget = series(header, data, "UpdateBudget/BudgetMs")
    mandatory_ms = series(header, data, "UpdateBudget/MandatoryMs")
    over = series(header, data, "UpdateBudget/OverBudget")
    gaps = series(header, data, "UpdateBudget/MaxGapMs")
    near_missed = series(header, data, "UpdateBudget/NearMissed")
    if not (finalised and update_ms and gaps and len(update_ms) == len(budget) == len(mandatory_ms) == len(over) == len(near_missed)):
        raise ValueError("not a finalised UB capture with the scheduler columns")
    if "ubmaxstaleness" not in metadata:
        raise ValueError("the capture has no ubmaxstaleness metadata")
    ages = agent_ages(os.path.join(RAW, record["agents_report"]))
    if not ages:
        raise ValueError("the agent report has no agents")

    bound_ms = float(metadata["ubmaxstaleness"]) * 1000.0 + frame_ms
    stats = record["stats"]
    budget_ms = csv_stats.summarize(budget)["median"]
    over_frames = [i for i, flag in enumerate(over) if flag > 0]
    forced = sum(1 for i in over_frames if mandatory_ms[i] > budget[i])
    spent = stats["UpdateBudget/UpdateMs"]
    return {
        "name": record["name"], "agents": record["agents"], "frames": len(update_ms), "budget_ms": budget_ms, "bound_ms": bound_ms,
        "update_ms": spent, "sc1_pass": spent["p95"] <= SC1_FACTOR * budget_ms,
        "over_frames": len(over_frames), "over_forced": forced, "over_soft": len(over_frames) - forced,
        "max_gap_ms": max(gaps), "gap_violations": sum(1 for g in gaps if g > bound_ms),
        "max_age_ms": max(ages), "age_violations": sum(1 for a in ages if a > bound_ms),
        "never_updated": sum(1 for a in ages if a < 0),
        "near_ok_share": sum(1 for m in near_missed if m == 0) / len(near_missed),
        "updates": stats["UpdateBudget/UpdatesRun"], "mandatory": stats["UpdateBudget/MandatoryRuns"],
        "gt": stats["GameThreadTime"],
        "run_frame": stats.get("UpdateBudget/GameThread/RunFrame", NAN_SUMMARY),
        "deadlines": stats.get("UpdateBudget/GameThread/Deadlines", NAN_SUMMARY),
    }


def report(batch: str, results: list[dict], failed: list[tuple[str, str]]) -> list[str]:
    f3 = lambda s: f"{s['median']:.3f} / {s['p95']:.3f} / {s['p99']:.3f}"   # noqa: E731
    bounds = sorted({f"{r['bound_ms']:.3f}" for r in results})
    lines = [f"# UB check: {batch}", "",
             f"Generated by `Tools/analyse_ub.py`. Staleness bound = the run's max staleness + one frame = {', '.join(bounds)} ms. "
             "One run per row; a single run is a check, not evidence (01 section 7).", "",
             "| Run | N | Frames | Budget (ms) | UpdateMs median / p95 / p99 | SC1 (p95 <= 1.10 x budget) | Over-budget frames (forced / soft) |",
             "|---|---|---|---|---|---|---|"]
    for r in results:
        lines.append(f"| `{r['name']}` | {r['agents']} | {r['frames']} | {r['budget_ms']:.2f} | {f3(r['update_ms'])} | "
                     f"{'PASS' if r['sc1_pass'] else 'FAIL'} | {r['over_frames']} ({r['over_forced']} / {r['over_soft']}) |")
    lines += ["", "| Run | Max MaxGapMs | Frames over the bound | Max agent age (ms) | Agents over the bound | Never updated | SC2 | Frames with no near client missed (SC3) |",
              "|---|---|---|---|---|---|---|---|"]
    for r in results:
        sc2 = r["gap_violations"] == 0 and r["age_violations"] == 0 and r["never_updated"] == 0
        lines.append(f"| `{r['name']}` | {r['max_gap_ms']:.3f} | {r['gap_violations']} | {r['max_age_ms']:.3f} | {r['age_violations']} | "
                     f"{r['never_updated']} | {'PASS' if sc2 else 'FAIL'} | {100.0 * r['near_ok_share']:.2f} % |")
    lines += ["", "| Run | GameThreadTime median / p95 / p99 (ms) | RunFrame scope median / p95 / p99 (ms) | Deadlines scope median / p95 / p99 (ms) | Updates per frame median [max] | Mandatory per frame median [max] |",
              "|---|---|---|---|---|---|"]
    for r in results:
        lines.append(f"| `{r['name']}` | {f3(r['gt'])} | {f3(r['run_frame'])} | {f3(r['deadlines'])} | "
                     f"{r['updates']['median']:.0f} [{r['updates']['max']:.0f}] | {r['mandatory']['median']:.0f} [{r['mandatory']['max']:.0f}] |")
    if failed:
        lines += ["", "**Runs not checked:**", ""] + [f"- `{name}`: {reason}" for name, reason in failed]
    return lines


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("batch")
    parser.add_argument("--fps", type=float, default=60.0, help="the fixed step of the runs")
    args = parser.parse_args(argv)

    with open(os.path.join(RAW, args.batch + "_manifest.json"), encoding="utf-8") as handle:
        manifest = json.load(handle)
    frame_ms = 1000.0 / args.fps
    results: list[dict] = []
    failed: list[tuple[str, str]] = []
    for record in manifest["runs"]:
        if record["config"] != "UB" or record.get("priming"):
            continue
        if run_bench.is_failure(record) or "agents_report" not in record:
            failed.append((record["name"], "failed in run_bench.py (exit code, log, capture or analysis), or no agent report"))
            continue
        try:
            results.append(check_run(record, frame_ms))
        except (OSError, ValueError, KeyError, IndexError) as error:
            failed.append((record["name"], str(error)))
    if not results:
        print("no UB run of", args.batch, "could be checked:", failed)
        return 1
    os.makedirs(SUMMARIES, exist_ok=True)
    path = os.path.join(SUMMARIES, args.batch + "_ub_check.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(report(args.batch, results, failed)) + "\n")
    print("ub check:", path)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
