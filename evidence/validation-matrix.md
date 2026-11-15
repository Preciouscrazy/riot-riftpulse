# RiftPulse — cross-platform validation matrix

**Validation date:** 2026-11-15  
**Branch:** `improve/cross-platform-ci-case-study`  
**Verified source and test changes:** commit [`c950cebe1b012a7cda7b256aa66ddacde78b1d66`](https://github.com/Preciouscrazy/riot-riftpulse/commit/c950cebe1b012a7cda7b256aa66ddacde78b1d66)  
**PR:** [#2 — cross-platform CI, CLI safety and recruiter-ready evidence](https://github.com/Preciouscrazy/riot-riftpulse/pull/2)

## Verified GitHub Actions results

Both [the branch push run #43691883857](https://github.com/Preciouscrazy/riot-riftpulse/actions/runs/43691883857) and [the pull-request run #43691891572](https://github.com/Preciouscrazy/riot-riftpulse/actions/runs/43691891572) completed with **success** on the same verified source commit. The workflow runs a Release CMake build and CTest for both OS targets, using Python 3.12 for the CLI tests.

| Check | Ubuntu `ubuntu-latest` | Windows `windows-latest` |
| --- | --- | --- |
| CMake C++20 configuration and Release build | **PASS** | **PASS** |
| `riftpulse_tests` — 6 C++ test functions | **PASS** | **PASS** |
| `riftpulse_telemetry_tests` — 6 C++ test functions | **PASS** | **PASS** |
| `riftpulse_cli_tests` — 11 Python CLI test methods | **PASS** | **PASS** |
| Overall CTest | **3/3 PASS** | **3/3 PASS** |

The CLI suite validates both sample JSON outputs against committed fixtures, verifies direct stdout and explicit output files, checks usage/input failures and partial-row exit codes, confirms deterministic repeated match output, and protects input files from being overwritten via identical input/output paths. The suite does **not** benchmark execution speed or analyze live games.

## Historical evidence preserved

- [Earlier Ubuntu main-branch CI](https://github.com/Preciouscrazy/riot-riftpulse/actions/runs/42587121619) passed the original two C++ test executables, before the new Python CLI suite.
- [Earlier match-only CTest log](test-results.txt) shows only one CTest target and should not be treated as evidence for telemetry or the current test matrix.
- [Match JSON](sample-report.json), [telemetry JSON](telemetry-sample-report.json), and [pre-existing Windows benchmark CSV](benchmark-results.csv) are committed artifacts. The benchmark is a historical synthetic-input measurement, not a newly verified CI performance result.

## Reproduce the test suite

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python3 tests/test_cli.py ./build/riftpulse
```

### Windows PowerShell — Visual Studio 2026

```powershell
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tests/test_cli.py .\build\Release\riftpulse.exe
```

The test executable is located with the CMake `$<TARGET_FILE:riftpulse>` generator expression in the actual CTest registration, making the hosted workflow independent of Windows/Linux executable subdirectories.

## Evidence boundaries

These passing jobs establish that the specified unit and CLI cases succeed on the **GitHub-hosted Windows and Ubuntu environments** for the referenced code. They do not prove successful builds on every local MSVC installation, validate untested parser edge cases, establish production readiness, or measure FPS/network performance in real games. The test and documentation changes do not replace historical benchmark measurements.
