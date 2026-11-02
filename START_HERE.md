# Getting Started

This checklist takes you from the source archive to a verified local build.

## 1. Prepare your environment

Install Git, CMake 3.20+, and a C++20 compiler. Open the `riot-riftpulse` directory in your IDE or terminal.

## 2. Configure, build, and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## 3. Run the sample

```bash
./build/riftpulse --input data/match_events.csv --output report.json
```

On Windows with Visual Studio, use `build/Release/riftpulse.exe` if that is where CMake places the executable.

## 4. Trace the data flow

1. Start with `include/riftpulse/models.hpp` to understand the shared data structures.
2. Follow CSV input through `src/csv_reader.cpp`.
3. Review aggregation and event-window analysis in `src/analyzer.cpp`.
4. Inspect JSON serialization in `src/json_reporter.cpp`.
5. Read `src/main.cpp` for argument handling and exit codes.

## 5. Make a small, verifiable change

Try adding a test for a malformed CSV row, changing the sample data, or extending one player metric. Rebuild and run the tests after each change.

## 6. Publish responsibly

Create a repository such as `riftpulse-game-telemetry`, review every file, and push only code you understand and have tested. Describe the project accurately as an independent portfolio project. Do not present the heuristic as a validated cheating detector or imply affiliation with Riot Games.

## Example commit sequence

- `docs: clarify project scope and input schema`
- `feat: validate CSV event records`
- `feat: calculate player match summaries`
- `feat: flag dense event windows for review`
- `test: cover parser and analyzer edge cases`
- `ci: build and run tests with GitHub Actions`
