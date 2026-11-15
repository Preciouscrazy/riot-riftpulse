# RiftPulse

**C++20 offline match-event and performance-telemetry analysis | CMake | CTest | reproducible JSON evidence**

RiftPulse turns user-supplied CSV data into structured JSON in two independent modes: **match analysis** (per-player statistics and dense event-window signals) and **performance telemetry** (frame-time percentiles, stutter events, and separately observed request/response RTT). The goal is to make measurement rules, parser failures, and generated reports easy to examine and reproduce.

**Engineering focus:** Defensible statistics, deterministic reporting, input validation, and explicit limits on what timing samples can tell us.

[Engineering case study](docs/case-study.md) · [Performance definitions](docs/performance-telemetry.md) · [CI evidence and reproduction steps](evidence/validation-matrix.md)

| Offline workflow | Checked-in evidence |
| --- | --- |
| Match events | [Sample JSON](evidence/sample-report.json): two player summaries over a synthetic 1,800-second match, no invalid rows or burst flags |
| Performance telemetry | [Telemetry JSON](evidence/telemetry-sample-report.json): 25 synthetic frames, p95/p99 frame time 49/70 ms; 3 stutter frames in 2 events at 33.3 ms, 21 observed RTT entries |

**Measurement boundary:** A missing RTT observation is not a zero measurement. Frame-time spikes do not identify CPU, GPU, display, or network causes without independent instrumentation. All bundled fixtures are fictional, and RiftPulse is not affiliated with Riot Games.

## Capabilities

- Parse a documented five-column CSV format, including quoted fields and escaped quotes.
- Reject malformed records while reporting their source row numbers.
- Aggregate kills, deaths, assists, gold, objectives, and tower damage by player.
- Calculate gold earned per minute.
- Detect configurable high-frequency event windows as review signals.
- Export deterministic, machine-readable JSON.
- Build and test with CMake, CTest, and GitHub Actions.

> **Interpretation note:** Event-burst detection is a simple heuristic. A flag is not evidence of cheating or misconduct; it identifies activity that may warrant additional context and investigation.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler:
  - GCC 11+
  - Clang 13+
  - A C++20-capable Visual Studio toolchain (validated Windows runner status is recorded in the [validation matrix](evidence/validation-matrix.md))

## Build and run

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run both sample datasets:

```bash
./build/riftpulse --input data/match_events.csv --output report.json
./build/riftpulse --telemetry data/performance_samples.csv --stutter-ms 33.3 --output telemetry-report.json
```

On Windows with a Visual Studio generator, the executable is commonly located at `build/Release/riftpulse.exe`:

```powershell
.\build\Release\riftpulse.exe --input data/match_events.csv --output report.json
```

Omit `--output` to write the JSON report to standard output:

```bash
./build/riftpulse --input data/match_events.csv
```

### Command-line options

| Option | Required | Description |
|---|---:|---|
| `--input <path>` | Choose one mode | Match-event CSV |
| `--telemetry <path>` | Choose one mode | Performance telemetry CSV |
| `--stutter-ms <threshold>` | No | Telemetry only. Inclusive cutoff, default 33.3 ms |
| `--output <path>` | No | Destination for the JSON report; defaults to standard output |

### Exit codes

| Code | Meaning |
|---:|---|
| `0` | Report generated; all input rows were valid |
| `1` | Input, output, or analysis error |
| `2` | Invalid command-line arguments |
| `3` | Report generated, but one or more rows were rejected |

## Input format

The header must match this exact sequence:

```csv
match_id,timestamp_seconds,player_id,event_type,value
```

| Column | Meaning |
|---|---|
| `match_id` | Non-empty match identifier |
| `timestamp_seconds` | Non-negative event timestamp in seconds |
| `player_id` | Non-empty player identifier |
| `event_type` | One of the supported event names below |
| `value` | Event-specific value; may be empty for kill, death, and assist |

Supported event types:

| Event type | `value` |
|---|---|
| `kill` | Optional; ignored by the current summary |
| `death` | Optional; ignored by the current summary |
| `assist` | Optional; ignored by the current summary |
| `gold` | Required non-negative integer amount |
| `objective` | Required non-empty label, such as `dragon` |
| `tower_damage` | Required non-negative integer amount |

Example:

```csv
match_id,timestamp_seconds,player_id,event_type,value
MATCH-001,12,blue_fox,kill,
MATCH-001,25,blue_fox,gold,350
MATCH-001,42,red_owl,objective,dragon
MATCH-001,55,red_owl,tower_damage,420
```

CSV fields may be quoted. A quote inside a quoted field is represented by two consecutive quotes (`""`). Malformed rows are skipped and included in `invalid_rows`; warnings are written to standard error.

All events in one analysis must belong to the same match. Timestamps are interpreted as seconds from the start of the match, so the reported duration is the greatest event timestamp.

## Output

The JSON report contains:

- `match_id`, `duration_seconds`, and `invalid_rows`
- A sorted `players` array with event totals and gold per minute
- An `event_bursts` array with player, timestamp, event count, and a review explanation

Player summaries are sorted by player ID. Gold per minute uses the match duration, with a one-minute minimum denominator to avoid division by zero.

## Validation & Results

CMake registers two C++ suites, `riftpulse_tests` and `riftpulse_telemetry_tests`, with six test functions each, plus `riftpulse_cli_tests`, which exercises both workflows, golden JSON reports, exit codes, and input/output file safety. **Cross-platform verification passed:** the [upgrade's Windows + Ubuntu pull-request workflow](https://github.com/Preciouscrazy/riot-riftpulse/actions/runs/43691891572) configured and built successfully and passed **3/3 CTest checks** on both operating systems (two C++ suites with six internal test cases each, plus 11 end-to-end CLI regression cases). The CLI checks compare both sample reports against the checked-in JSON fixtures and exercise invalid input, exit codes, and same-file overwrite rejection. The earlier [historical match-only CTest log](evidence/test-results.txt) is preserved as a one-suite record, not evidence for the current version. See the [validation matrix](evidence/validation-matrix.md) for the exact verified commit and reproducibility commands.

### Sample Analysis Results

The application successfully processed a fictional match-event dataset and generated a structured JSON report.

| Metric | Result |
|---|---|
| Match ID | RIFT-001 |
| Match duration | 1,800 seconds |
| Player summaries | 2 |
| Invalid rows | 0 |
| Event-burst flags | 0 |

The report includes per-player kills, deaths, assists, gold earned, gold per minute, objectives, and tower damage.

### Preliminary Performance Benchmark

RiftPulse was benchmarked on Windows 10 using an MSVC Release build and synthetic match-event CSV files.

| Dataset | Runs | Median execution time | Range |
|---|---:|---:|---:|
| 10,000 events | 5 | 21.98 ms | 19.96–85.72 ms |
| 100,000 events | 5 | 132.96 ms | 128.12–156.62 ms |

The 10,000-event first run (85.72 ms) is included. These measurements predate the performance telemetry extension.

**Methodology:** Each dataset contained one fictional match and ten player IDs. Five independent application executions were timed using PowerShell's Stopwatch. Measurements included application startup, CSV reading, match analysis, and JSON report generation.

**Limitations:** Results reflect one local Windows environment and are preliminary. They are not isolated algorithm benchmarks, real-game FPS measurements, or network-latency measurements. Additional workloads, hardware details, warm-up runs, and repeated trials would strengthen the conclusions.

[View all benchmark measurements](evidence/benchmark-results.csv)

### Reproducible Evidence

- [Historical match-only test results](evidence/test-results.txt)
- [Generated sample match JSON report](evidence/sample-report.json)
- [Generated sample telemetry JSON report](evidence/telemetry-sample-report.json)
- [Cross-platform test and verification record](evidence/validation-matrix.md)
- [Engineering case study](docs/case-study.md)

### Validation Limitations

The tests validate selected parser, analyzer, and serialization behaviors. They do not establish complete correctness, production readiness, real-game compatibility, or network-lag performance.

The event-burst detector provides heuristic review signals, not evidence of player misconduct. All sample data is fictional.

## Architecture

```text
CSV input
   |
   v
CsvReader  ---> row validation and typed GameEvent records
   |
   v
MatchAnalyzer ---> player aggregation and event-window heuristic
   |
   v
JsonReporter ---> JSON output
   |
   v
CLI (argument handling, files, exit codes)
```

| Component | Responsibility |
|---|---|
| `models.hpp` | Shared event and report data structures |
| `CsvReader` | CSV parsing, schema validation, invalid-row accounting |
| `MatchAnalyzer` | Match consistency, aggregation, sorting, burst detection |
| `JsonReporter` | JSON serialization and string escaping |
| `main.cpp` | Command-line interface and process exit status |

## Complexity

Let `n` be the number of valid events and `p` the number of distinct players.

- Aggregation: expected `O(n)` using hash maps.
- Player ordering: `O(p log p)`.
- Burst detection: `O(n log n)` overall due to sorting timestamps.
- Space: `O(n + p)` because events and per-player timestamps are retained during analysis.

## Known limitations

- The input schema is intentionally small and requires one match per file.
- The match parser processes one physical CSV line at a time; quoted fields cannot span multiple lines.
- The CLI refuses to write a report over the input CSV, including equivalent existing paths; it does not provide atomic replacement or backups for distinct output files.
- Burst thresholds are fixed by the `MatchAnalyzer` constructor and are not exposed as CLI options.
- The analyzer is a demonstration of data processing, not a production anti-cheat, moderation, or player-risk system.
- The application retains the input events in memory; very large telemetry streams would need a streaming or partitioned design.

## Suggested extensions

1. Add configurable burst-window and threshold options.
2. Add fuzz and boundary tests for CSV parsing and numeric aggregation.
3. Support multi-match input with independent reports.
4. Add benchmark fixtures for large event datasets.
5. Add a small dashboard that consumes the JSON output.

## Development workflow

Before publishing changes, build from a clean directory, run the full test suite, inspect the generated JSON, and verify the sample input. Make incremental commits that describe the actual changes. Be prepared to explain the parser, aggregation strategy, complexity, and limitations in a technical interview.

## License

MIT. See [LICENSE](LICENSE).

## FrameTrace-style frame-time and latency telemetry

RiftPulse now offers a separate C++20 **offline telemetry-analysis mode** alongside its original match-event CLI. It calculates mean FPS, p95/p99 frame times, an approximate 1% low FPS, configurable stutter events, and **separate** request/response round-trip latency statistics (mean, median, p95 and p99). A small `steady_clock`-based API supports instrumentation of applications you control.

```powershell
.\build\Release\riftpulse.exe --telemetry data/performance_samples.csv --stutter-ms 33.3 --output telemetry-report.json
```

The [sample telemetry input](data/performance_samples.csv) and [resulting JSON report](evidence/telemetry-sample-report.json) are **synthetic**, not real Riot-game FPS or server-ping measurements. The new mode has a dedicated CTest suite. See [performance telemetry documentation](docs/performance-telemetry.md) for definitions, capture hooks, limitations, and test instructions. The existing `--input` match analysis is unchanged.
