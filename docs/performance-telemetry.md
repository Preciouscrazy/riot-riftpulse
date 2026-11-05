# RiftPulse performance telemetry (FrameTrace-style extension)

This extension adds **frame-time and optional application round-trip latency** analysis to RiftPulse while preserving the existing match-event analyzer. It is an independent, offline portfolio prototype and does **not** connect to Riot Games clients, game servers, or APIs.

## Run on Windows (Visual Studio / MSVC)

After installing these files into your RiftPulse repository root:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\riftpulse.exe --telemetry data/performance_samples.csv --output telemetry-report.json
```

For a single-config generator (often Ninja or Unix Makefiles), use `./build/riftpulse` or `./build/riftpulse.exe` as appropriate. The existing match command remains supported:

```powershell
.\build\Release\riftpulse.exe --input data/match_events.csv --output match-report.json
```

Change the stutter threshold in milliseconds when needed:

```powershell
.\build\Release\riftpulse.exe --telemetry data/performance_samples.csv --stutter-ms 50 --output telemetry-report.json
```

The default threshold is **33.3 milliseconds**, including frames at that threshold. `--stutter-ms` is only valid in telemetry mode.

## CSV schema

```csv
timestamp_ms,frame_time_ms,latency_ms
17,16.7,32.1
34,17.0,
50,16.1,34.5
```

- `timestamp_ms` is a strictly increasing, non-negative **monotonic** timestamp relative to capture start, typically when a frame ends. It is not a wall-clock date.
- `frame_time_ms` is a finite, positive elapsed duration between consecutive frame presentation/render markers. Capture semantics depend on where you place the marker.
- `latency_ms` is an **optional** finite, non-negative request/response round-trip time captured by application-controlled instrumentation. Blank means **not observed**, *not* zero, a timed-out packet, or packet loss.
- The header and three-column structure are required. Blank data lines are ignored. Invalid rows are reported to standard error, counted in the JSON, and cause exit code `3` if any valid samples were analyzed. Invalid headers and files with no valid samples cause exit code `1`.
- Timestamp order is strictly increasing among the accepted rows. Malformed rows are skipped. No inference is made about missing intervals or missing latency samples.

The CLI analyzes collected samples; it does not capture another game's frames or probe another company's game servers.

## Definitions

| Output field | Calculation |
|---|---|
| `frame_time.mean_ms` | Arithmetic mean of accepted frame durations |
| `frame_time.median_ms` | Midpoint median of accepted frame durations |
| `frame_time.p95_ms`, `p99_ms` | Nearest-rank percentiles, after sorting ascending |
| `frame_time.mean_fps` | `1000 / mean(frame_time_ms)`; not mean of instantaneous FPS |
| `frame_time.approximate_one_percent_low_fps` | `1000 / mean(slowest max(1, ceil(1% of samples)) frame times)` |
| `frame_time.stutter_frame_count` | Number of frames with duration >= `stutter_threshold_ms` |
| `frame_time.stutter_events` | Contiguous runs of qualifying accepted frame records, with timestamps and peak |
| `round_trip_latency.*` | Mean, median, p95, p99, maximum and count for *observed* round-trip durations only |

Latency summary fields are `null` when there are no observed latency samples. The report never asserts causal linkage between latency spikes and slow frames.

## Optional instrumentation hooks for software you control

`FrameClock::tick()` uses `std::chrono::steady_clock`. Call it at consistent points in your own render loop (e.g., after each completed frame):

```cpp
riftpulse::FrameClock clock;
clock.tick(); // Set reference point; no frame measurement yet.

// At a consistent point after each subsequent frame:
if (auto frame_time_ms = clock.tick()) {
    // Write a row with monotonic timestamp, *frame_time_ms, and optional RTT.
}
```

`RoundTripClock` measures an elapsed interval. Start it **when your own application sends a request** and stop it **when the corresponding acknowledgment arrives**:

```cpp
riftpulse::RoundTripClock request_timer; // Start near request send.
// ... when a matching acknowledgment is received:
double round_trip_ms = request_timer.elapsed_ms();
```

Using `RoundTripClock` without a real network request measures local elapsed time **not network latency**. It also cannot yield one-way delay. Prefer request IDs to match each response to its own send time. Handle loss and timeouts separately (this schema has no packet-loss column).

## Reproducible synthetic evidence

- Input: [`data/performance_samples.csv`](../data/performance_samples.csv), 25 invented frame durations, of which 21 have illustrative RTT values.
- Output: [`evidence/telemetry-sample-report.json`](../evidence/telemetry-sample-report.json), produced by analyzing that file with the default 33.3 ms threshold.
- Tests: `riftpulse_telemetry_tests` registered with CTest; six test cases cover CSV validation, schema rejection, percentiles, stutter sequences, missing latency, invalid settings, and steady-clock timing.

**Illustrative sample result:** 25 frame samples, mean frame time 20.948 ms, mean FPS 47.737, p95 frame time 49 ms, p99 frame time 70 ms, approximate 1% low 14.286 FPS, three stutter frames in two events, and 21 separate RTT samples (p95 46.1 ms). These values are synthetic inputs, **not real game measurements** or benchmark results.

## Limitations

Frame duration alone does not identify whether a slowdown came from the CPU, GPU, display presentation, process scheduling, drivers, or network. This library does not hook into an operating system's graphics API, access protected game process memory, intercept network traffic, or provide a real-game overlay. Its timers measure elapsed intervals at the locations where you instrument them; that may differ from true on-screen presentation latency.

Stutter runs are consecutive accepted CSV records. A missing, rejected, or skipped record cannot be reconstructed and may affect run boundaries. Request/response timing is not equivalent to game-server ping unless the capture code actually obtains matching replies from that server. All report data shown here is fictional; do not claim production deployment or real Riot performance improvements without evidence.
