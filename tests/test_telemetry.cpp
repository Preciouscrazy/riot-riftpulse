#include "riftpulse/telemetry.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void near(double actual, double expected, double eps, const char* message) {
    require(std::abs(actual - expected) <= eps, message);
}

void test_reader_valid_and_invalid_rows() {
    std::istringstream input(
        "timestamp_ms,frame_time_ms,latency_ms\n"
        "100,16.5,25.3\n"
        "120,20,\n"
        "125,-5,10\n"
        "119,17,15\n"
        "130,17,bad\n"
        "150,20,0\n"
        "200,NaN,1\n"
        "220,17,10,extra\n"
        "230,16,\n");
    const auto result = riftpulse::TelemetryCsvReader{}.read(input);
    require(result.samples.size() == 4, "four valid rows expected");
    require(result.invalid_rows == 5, "five invalid rows should be counted");
    require(result.samples[0].latency_ms.has_value(), "latency must be observed");
    require(!result.samples[1].latency_ms.has_value(), "blank latency must be missing");
    near(*result.samples[2].latency_ms, 0.0, 0.00001, "zero latency is an observed value");
}
void test_reader_header_rejection() {
    std::istringstream input("frame_time_ms,latency_ms\n16,20\n");
    bool threw = false;
    try { (void)riftpulse::TelemetryCsvReader{}.read(input); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "wrong schema must fail");
}
void test_telemetry_metrics_and_stutter_sequences() {
    const std::vector<riftpulse::TelemetrySample> samples{
        {100, 16.0, 25.0},
        {120, 17.0, std::nullopt},
        {160, 40.0, 80.0},
        {205, 45.0, 30.0},
        {225, 16.0, std::nullopt},
        {295, 70.0, 100.0}
    };
    const auto report = riftpulse::TelemetryAnalyzer{33.3}.analyze(samples, 2);
    require(report.invalid_rows == 2, "invalid row count should propagate");
    require(report.frame_time.timings.sample_count == 6, "frame count incorrect");
    near(report.frame_time.timings.mean_ms, 34.0, 0.00001, "frame mean wrong");
    near(report.frame_time.timings.median_ms, 28.5, 0.00001, "frame median wrong");
    near(report.frame_time.timings.p95_ms, 70.0, 0.00001, "nearest-rank p95 wrong");
    near(report.frame_time.timings.p99_ms, 70.0, 0.00001, "nearest-rank p99 wrong");
    near(report.frame_time.mean_fps, 1000.0 / 34.0, 0.00001, "mean FPS wrong");
    near(report.frame_time.approximate_one_percent_low_fps, 1000.0 / 70.0, 0.00001,
         "approximate one percent low wrong");
    require(report.frame_time.stutter_frame_count == 3, "three stutter frames expected");
    require(report.frame_time.stutter_events.size() == 2, "two contiguous events expected");
    const auto& first = report.frame_time.stutter_events.front();
    require(first.start_timestamp_ms == 160 && first.end_timestamp_ms == 205 &&
            first.frame_count == 2, "first stutter interval wrong");
    near(first.peak_frame_time_ms, 45.0, 0.00001, "stutter peak incorrect");
    require(report.round_trip_latency.sample_count == 4, "blank latencies should be excluded");
    near(report.round_trip_latency.median_ms, 55.0, 0.00001, "latency median wrong");
    near(report.round_trip_latency.p95_ms, 100.0, 0.00001, "latency p95 wrong");
    std::ostringstream output;
    riftpulse::TelemetryJsonReporter{}.write(report, output);
    require(output.str().find("\"round_trip_latency\"") != std::string::npos,
            "JSON must include separate latency stats");
    require(output.str().find("\"stutter_events\"") != std::string::npos,
            "JSON must include stutter events");
}
void test_absent_latency_is_not_zero() {
    const std::vector<riftpulse::TelemetrySample> samples{{10, 16.0, std::nullopt}};
    const auto report = riftpulse::TelemetryAnalyzer{}.analyze(samples);
    require(report.round_trip_latency.sample_count == 0, "zero latency samples expected");
    std::ostringstream output;
    riftpulse::TelemetryJsonReporter{}.write(report, output);
    require(output.str().find("\"mean_ms\": null") != std::string::npos,
            "absent latency stats must use null, not zero");
}
void test_invalid_samples_and_thresholds() {
    bool threw = false;
    try { (void)riftpulse::TelemetryAnalyzer{0.0}; }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "zero threshold must be rejected");
    threw = false;
    try { (void)riftpulse::TelemetryAnalyzer{}.analyze({}); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "empty telemetry must be rejected");
    threw = false;
    try { (void)riftpulse::TelemetryAnalyzer{}.analyze({{0, 16, -1.0}}); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "negative latency must be rejected");
    threw = false;
    try { (void)riftpulse::TelemetryAnalyzer{}.analyze({{0, 16, 10.0}, {0, 18, 10.0}}); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "duplicate timestamps must be rejected");
}
void test_timing_primitives() {
    using Clock = riftpulse::FrameClock::Clock;
    const Clock::time_point origin{};
    riftpulse::FrameClock frames;
    require(!frames.tick(origin).has_value(), "first mark has no measured interval");
    const auto measured = frames.tick(origin + std::chrono::microseconds(16667));
    require(measured.has_value(), "second mark must return a duration");
    near(*measured, 16.667, 0.000001, "frame clock interval wrong");
    bool threw = false;
    try { (void)frames.tick(origin); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "backwards frame marks should be rejected");
    frames.reset();
    require(!frames.tick(origin).has_value(), "reset should restart sampling");
    riftpulse::RoundTripClock probe(origin);
    near(probe.elapsed_ms(origin + std::chrono::milliseconds(42)), 42.0, 0.000001,
         "round-trip elapsed timing wrong");
    threw = false;
    try { (void)probe.elapsed_ms(origin - std::chrono::milliseconds(1)); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "backwards round-trip timing should be rejected");
}
}

int main() {
    try {
        test_reader_valid_and_invalid_rows();
        test_reader_header_rejection();
        test_telemetry_metrics_and_stutter_sequences();
        test_absent_latency_is_not_zero();
        test_invalid_samples_and_thresholds();
        test_timing_primitives();
        std::cout << "All six telemetry tests passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Telemetry test failure: " << ex.what() << '\n';
        return 1;
    }
}
