# Benchmark batch 2026-11-21_012231_affinitycheck

## Environment

```json
{
  "date": "2026-11-21T01:22:31",
  "engine": "5.8.4 (CL 58902052)",
  "build_config": "Development (packaged)",
  "compiler": "MSVC 14.44.35229, Windows SDK 10.0.22621.0",
  "cpu": "Intel(R) Core(TM) i7-14650HX",
  "gpus": "Intel(R) UHD Graphics (driver 32.0.101.7088); NVIDIA GeForce RTX 5070 Laptop GPU (driver 32.0.16.1047)",
  "ram_gb": "31.6",
  "os": "Microsoft Windows 11 Home Single Language build 26200",
  "power_plan": "Power Scheme GUID: 6fecc5ae-f350-48a5-b669-b472cb895ccf  (Turbo)",
  "on_ac_power": true,
  "resolution": "1280x720 windowed, default scalability",
  "common_args": "-windowed -ResX=1280 -ResY=720 -nosound -unattended -traceautostart=0 -deterministic -FPS=60 -novsync -UBExit -csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd",
  "cpu_affinity": "0xFFFF (performance cores only; hybrid CPU)"
}
```

## Results over the whole capture

Each cell: median across runs of each run's statistic, then [min-max across runs].

| Config | Work | N | Runs | GameThreadTime median / p95 / p99 (ms) | FrameTime median / p95 / p99 (ms) | UpdatesRun median | PSO events | Log errors |
|---|---|---|---|---|---|---|---|---|
| B0 | default | 2000 | 4 | 9.150 [9.085-9.687] / 10.515 [10.322-11.910] / 11.499 [11.374-13.010] | 9.158 [9.104-9.696] / 10.529 [10.304-11.942] / 11.484 [11.313-13.026] | 2000.0 | 20 | 0 |
| B1 | default | 2000 | 4 | 8.595 [8.137-9.058] / 9.888 [9.182-11.009] / 10.647 [9.763-11.823] | 8.608 [8.166-9.078] / 9.905 [9.174-11.011] / 10.571 [9.609-11.943] | 2000.0 | 20 | 0 |
| B0 | default | 4000 | 4 | 17.107 [16.171-18.002] / 19.752 [18.418-21.984] / 21.327 [19.778-23.584] | 17.126 [16.189-18.011] / 19.708 [18.428-21.880] / 21.382 [19.687-23.741] | 4000.0 | 20 | 0 |
| B1 | default | 4000 | 4 | 15.404 [14.838-15.945] / 17.246 [16.907-18.606] / 18.459 [18.362-19.603] | 15.427 [14.867-15.962] / 17.254 [16.949-18.602] / 18.384 [18.283-19.563] | 4000.0 | 20 | 0 |

Raw files: `Benchmarks/Raw/2026-11-21_012231_affinitycheck_*` (CSV, agent report and log per run) and `2026-11-21_012231_affinitycheck_manifest.json`.
