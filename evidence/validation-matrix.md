# RiftPulse — validation matrix and reproducibility

**Source:** `main` baseline commit [`421cdb2`](https://github.com/Preciouscrazy/riot-riftpulse/commit/421cdb2e0826bdabd9adc7f473e440842e4aa4b0) and proposed cross-platform upgrade branch `improve/cross-platform-ci-case-study`.

## Verified before this upgrade

- [Ubuntu GitHub Actions, successful `main` run #42587121619](https://github.com/Preciouscrazy/riot-riftpulse/actions/runs/42587121619), testing the original two C++ suites and running sample CLI invocations.
- [Historical match-only CTest log](test-results.txt): one CTest suite; **not** evidence for the current two-mode source.
- [Sample match JSON](sample-report.json), [synthetic telemetry JSON](telemetry-sample-report.json), and [historical Windows benchmark measurements](benchmark-results.csv).

## Upgrade checks and status

The new CI workflow specifies a matrix of `windows-latest` and `ubuntu-latest`, builds in Release configuration, and invokes CTest with `-C Release` (required for multi-configuration Visual Studio generators).

| Check | Expected | Hosted result |
| --- | --- | --- |
| Windows C++20 configure/build | Success | Pending first branch CI run |
| Ubuntu C++20 configure/build | Success | Pending first branch CI run |
| `riftpulse_tests` | Pass (6 internal test functions) | Pending |
| `riftpulse_telemetry_tests` | Pass (6 internal test functions) | Pending |
| `riftpulse_cli_tests` | Pass (11 standard-library Python CLI tests) | Pending |
| Sample JSON match | Parsed JSON equals checked-in fixture | Pending |
| Sample telemetry JSON | Parsed JSON equals checked-in fixture | Pending |
| Invalid inputs / exit codes / prevent input overwrite | Expected behavior | Pending |

These are **planned expectations**, not claims of successful v1.2 hosted results. Update this record with direct workflow run links after the branch checks complete successfully.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For Windows with Visual Studio Community 18:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Optional direct CLI checks:

```powershell
python tests/test_cli.py .\\build\\Release\\riftpulse.exe
```

The test script checks structured output against repository fixtures; it does not measure real-game performance or attribute bottlenecks. The synthetic benchmark log remains **historical evidence** and has not been remeasured for this change.
