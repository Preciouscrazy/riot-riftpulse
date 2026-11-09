# RiftPulse interview notes

## Explain the project

RiftPulse is a C++20 command-line program with two offline workflows. It analyzes match-event CSV files and summarizes per-player activity. Its telemetry mode reports frame-time statistics, groups slow frames into stutter events, and summarizes observed request/response round-trip timings separately.

## Explain one design choice

Missing latency values remain absent. Treating a blank field as zero would incorrectly lower latency statistics. Frame time and round-trip time are summarized separately because the measurements do not establish a causal relationship.

## Explain a limitation

The tool analyzes samples supplied by the user. It does not instrument a commercial game, diagnose CPU or GPU bottlenecks, or establish server ping from arbitrary elapsed time. The checked-in telemetry sample is synthetic.

## A next improvement

Expand CI and evidence capture so both CLI modes are exercised on every change, then add additional telemetry fixtures for invalid rows and longer stutter sequences.

## Evidence to open

- `evidence/telemetry-sample-report.json`
- `evidence/sample-report.json`
- `docs/performance-telemetry.md`
