# RiftPulse
### Match telemetry analysis in modern C++

RiftPulse is a command-line C++20 application that validates match-event CSV files, calculates per-player match summaries, flags unusually dense event windows for human review, and writes a structured JSON report.

The project uses fictional sample data. It is an independent portfolio project and is **not affiliated with, endorsed by, or representative of Riot Games or any game publisher**.

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
  - A recent Visual Studio 2022 toolchain

## Build and run

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Run the sample dataset:

```bash
./build/riftpulse --input data/match_events.csv --output report.json
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
| `--input <path>` | Yes | Path to the input CSV file |
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

RiftPulse has been built and tested on Windows and Ubuntu Linux using CMake, CTest, and GitHub Actions.

### Automated Testing

- **Windows 10:** Release build successful with MSVC.
- **CTest:** 1/1 registered tests passed.
- **Test coverage:** Six test functions covering CSV parsing, event aggregation, burst detection, malformed quotes, JSON escaping, and mixed-match rejection.
- **GitHub Actions:** Configure, build, test, and sample execution all passed on Ubuntu Linux.

[View successful GitHub Actions run](https://github.com/Preciouscrazy/riot-riftpulse/actions/runs/41789204169)

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

| Dataset | Runs | Median execution time |
|---|---:|---:|
| 10,000 events | 5 | 21.98 ms |
| 100,000 events | 5 | 132.96 ms |

**Methodology:** Each dataset contained one fictional match and ten player IDs. Five independent application executions were timed using PowerShell's Stopwatch. Measurements included application startup, CSV reading, match analysis, and JSON report generation.

**Limitations:** Results reflect one local Windows environment and are preliminary. They are not isolated algorithm benchmarks, real-game FPS measurements, or network-latency measurements. Additional workloads, hardware details, warm-up runs, and repeated trials would strengthen the conclusions.

[View all benchmark measurements](evidence/benchmark-results.csv)

### Reproducible Evidence

- [Automated test results](evidence/test-results.txt)
- [Generated sample JSON report](evidence/sample-report.json)

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
- The parser processes one physical CSV line at a time; quoted fields cannot span multiple lines.
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
