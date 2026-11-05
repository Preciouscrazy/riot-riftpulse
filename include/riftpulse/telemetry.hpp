#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace riftpulse {

// Timing primitives for integration into an application you control.
// FrameClock measures elapsed wall-clock time between consecutive frame marks;
// RoundTripClock measures a request/response interval, not one-way network delay.
class FrameClock {
public:
    using Clock = std::chrono::steady_clock;
    std::optional<double> tick(Clock::time_point now = Clock::now());
    void reset() noexcept { previous_.reset(); }
private:
    std::optional<Clock::time_point> previous_;
};

class RoundTripClock {
public:
    using Clock = std::chrono::steady_clock;
    explicit RoundTripClock(Clock::time_point start = Clock::now()) : start_(start) {}
    double elapsed_ms(Clock::time_point stop = Clock::now()) const;
private:
    Clock::time_point start_;
};

struct TelemetrySample {
    std::int64_t timestamp_ms{};
    double frame_time_ms{};
    std::optional<double> latency_ms;
};

struct TelemetryReadResult {
    std::vector<TelemetrySample> samples;
    std::size_t invalid_rows{};
    std::vector<std::string> warnings;
};

class TelemetryCsvReader {
public:
    // Strict three-column schema: timestamp_ms,frame_time_ms,latency_ms
    // A blank latency means "not observed", not zero latency or packet loss.
    TelemetryReadResult read(std::istream& input) const;
};

struct TimingStats {
    std::size_t sample_count{};
    double mean_ms{};
    double median_ms{};
    double p95_ms{};
    double p99_ms{};
    double max_ms{};
};

struct StutterEvent {
    std::int64_t start_timestamp_ms{};
    std::int64_t end_timestamp_ms{};
    std::size_t frame_count{};
    double peak_frame_time_ms{};
};

struct FrameTimeStats {
    TimingStats timings;
    double mean_fps{};
    double approximate_one_percent_low_fps{};
    double stutter_threshold_ms{};
    std::size_t stutter_frame_count{};
    std::vector<StutterEvent> stutter_events;
};

struct TelemetryReport {
    std::size_t invalid_rows{};
    FrameTimeStats frame_time;
    TimingStats round_trip_latency;
};

class TelemetryAnalyzer {
public:
    explicit TelemetryAnalyzer(double stutter_threshold_ms = 33.3);
    TelemetryReport analyze(const std::vector<TelemetrySample>& samples,
                            std::size_t invalid_rows = 0) const;
private:
    double stutter_threshold_ms_;
};

class TelemetryJsonReporter {
public:
    void write(const TelemetryReport& report, std::ostream& output) const;
};

} // namespace riftpulse
