#include "riftpulse/telemetry.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <iomanip>
#include <istream>
#include <numeric>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace riftpulse {
namespace {
std::string_view trim(std::string_view text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool parse_timestamp(std::string_view text, std::int64_t& value) {
    text = trim(text);
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value >= 0;
}

bool parse_number(std::string_view text, double& value) {
    text = trim(text);
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(value);
}

// Nearest-rank percentile, matching FrameTrace's documented percentile convention.
double percentile(const std::vector<double>& sorted, double fraction) {
    const auto rank = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(sorted.size())));
    return sorted[std::max<std::size_t>(1, rank) - 1];
}

TimingStats stats_for(std::vector<double> values) {
    TimingStats stats;
    stats.sample_count = values.size();
    if (values.empty()) return stats;
    const double total = std::accumulate(values.begin(), values.end(), 0.0);
    stats.mean_ms = total / static_cast<double>(values.size());
    std::sort(values.begin(), values.end());
    const auto midpoint = values.size() / 2;
    stats.median_ms = (values.size() % 2 == 0)
        ? (values[midpoint - 1] / 2.0 + values[midpoint] / 2.0)
        : values[midpoint];
    stats.p95_ms = percentile(values, 0.95);
    stats.p99_ms = percentile(values, 0.99);
    stats.max_ms = values.back();
    return stats;
}

void write_stats(const TimingStats& stats, std::ostream& output, const std::string& indent) {
    output << indent << "\"sample_count\": " << stats.sample_count;
    const auto metric = [&](const char* key, double value) {
        output << ",\n" << indent << '"' << key << "\": ";
        if (stats.sample_count == 0) output << "null";
        else output << value;
    };
    metric("mean_ms", stats.mean_ms);
    metric("median_ms", stats.median_ms);
    metric("p95_ms", stats.p95_ms);
    metric("p99_ms", stats.p99_ms);
    metric("max_ms", stats.max_ms);
}
} // namespace

std::optional<double> FrameClock::tick(Clock::time_point now) {
    if (!previous_.has_value()) {
        previous_ = now;
        return std::nullopt;
    }
    const double ms = std::chrono::duration<double, std::milli>(now - *previous_).count();
    if (!(ms > 0.0) || !std::isfinite(ms)) {
        throw std::invalid_argument("FrameClock marks must be strictly increasing");
    }
    previous_ = now;
    return ms;
}

double RoundTripClock::elapsed_ms(Clock::time_point stop) const {
    const double ms = std::chrono::duration<double, std::milli>(stop - start_).count();
    if (ms < 0.0 || !std::isfinite(ms)) {
        throw std::invalid_argument("RoundTripClock stop precedes start");
    }
    return ms;
}

TelemetryReadResult TelemetryCsvReader::read(std::istream& input) const {
    TelemetryReadResult result;
    std::string line;
    if (!std::getline(input, line) || trim(line) != "timestamp_ms,frame_time_ms,latency_ms") {
        throw std::invalid_argument("Telemetry CSV must start with timestamp_ms,frame_time_ms,latency_ms");
    }
    std::size_t row = 1;
    std::optional<std::int64_t> last_timestamp;
    while (std::getline(input, line)) {
        ++row;
        if (trim(line).empty()) continue;
        const auto a = line.find(',');
        const auto b = a == std::string::npos ? a : line.find(',', a + 1);
        const bool bad_columns = a == std::string::npos || b == std::string::npos ||
                                 line.find(',', b + 1) != std::string::npos;
        std::int64_t timestamp = 0;
        double frame = 0.0;
        double latency = 0.0;
        std::optional<double> observed_latency;
        if (!bad_columns) {
            const std::string_view fields(line);
            const auto latency_text = trim(fields.substr(b + 1));
            if (!latency_text.empty() && parse_number(latency_text, latency) && latency >= 0.0) {
                observed_latency = latency;
            }
            if (parse_timestamp(fields.substr(0, a), timestamp) &&
                parse_number(fields.substr(a + 1, b - a - 1), frame) && frame > 0.0 &&
                (!last_timestamp || timestamp > *last_timestamp) &&
                (latency_text.empty() || observed_latency.has_value())) {
                result.samples.push_back({timestamp, frame, observed_latency});
                last_timestamp = timestamp;
                continue;
            }
        }
        ++result.invalid_rows;
        result.warnings.push_back("Row " + std::to_string(row) +
            ": expected three columns with strictly increasing non-negative timestamps, "
            "positive finite frame times, and blank or non-negative finite latency");
    }
    return result;
}

TelemetryAnalyzer::TelemetryAnalyzer(double stutter_threshold_ms)
    : stutter_threshold_ms_(stutter_threshold_ms) {
    if (!std::isfinite(stutter_threshold_ms_) || stutter_threshold_ms_ <= 0.0) {
        throw std::invalid_argument("Stutter threshold must be finite and greater than zero");
    }
}

TelemetryReport TelemetryAnalyzer::analyze(const std::vector<TelemetrySample>& samples,
                                           std::size_t invalid_rows) const {
    if (samples.empty()) throw std::invalid_argument("No valid telemetry samples were provided");
    TelemetryReport report;
    report.invalid_rows = invalid_rows;
    report.frame_time.stutter_threshold_ms = stutter_threshold_ms_;
    std::vector<double> frames;
    std::vector<double> latencies;
    frames.reserve(samples.size());
    latencies.reserve(samples.size());
    std::optional<std::int64_t> previous_timestamp;
    bool in_stutter = false;
    for (const auto& sample : samples) {
        if (sample.timestamp_ms < 0 ||
            (previous_timestamp && sample.timestamp_ms <= *previous_timestamp) ||
            !std::isfinite(sample.frame_time_ms) || sample.frame_time_ms <= 0.0 ||
            (sample.latency_ms && (!std::isfinite(*sample.latency_ms) || *sample.latency_ms < 0.0))) {
            throw std::invalid_argument("Invalid or unordered telemetry sample");
        }
        previous_timestamp = sample.timestamp_ms;
        frames.push_back(sample.frame_time_ms);
        if (sample.latency_ms) latencies.push_back(*sample.latency_ms);
        if (sample.frame_time_ms >= stutter_threshold_ms_) {
            ++report.frame_time.stutter_frame_count;
            if (!in_stutter) {
                report.frame_time.stutter_events.push_back(
                    {sample.timestamp_ms, sample.timestamp_ms, 0, sample.frame_time_ms});
                in_stutter = true;
            }
            auto& event = report.frame_time.stutter_events.back();
            event.end_timestamp_ms = sample.timestamp_ms;
            ++event.frame_count;
            event.peak_frame_time_ms = std::max(event.peak_frame_time_ms, sample.frame_time_ms);
        } else {
            in_stutter = false;
        }
    }
    report.frame_time.timings = stats_for(frames);
    report.round_trip_latency = stats_for(latencies);
    report.frame_time.mean_fps = 1000.0 / report.frame_time.timings.mean_ms;
    const auto slowest_count = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(static_cast<double>(frames.size()) * 0.01)));
    std::sort(frames.begin(), frames.end(), std::greater<double>());
    const double slowest_total = std::accumulate(frames.begin(), frames.begin() +
        static_cast<std::vector<double>::difference_type>(slowest_count), 0.0);
    report.frame_time.approximate_one_percent_low_fps =
        1000.0 / (slowest_total / static_cast<double>(slowest_count));
    return report;
}

void TelemetryJsonReporter::write(const TelemetryReport& report, std::ostream& output) const {
    output << std::fixed << std::setprecision(3)
           << "{\n  \"mode\": \"offline_telemetry_analysis\",\n"
           << "  \"invalid_rows\": " << report.invalid_rows << ",\n"
           << "  \"frame_time\": {\n";
    write_stats(report.frame_time.timings, output, "    ");
    output << ",\n    \"mean_fps\": " << report.frame_time.mean_fps
           << ",\n    \"approximate_one_percent_low_fps\": "
           << report.frame_time.approximate_one_percent_low_fps
           << ",\n    \"stutter_threshold_ms\": " << report.frame_time.stutter_threshold_ms
           << ",\n    \"stutter_frame_count\": " << report.frame_time.stutter_frame_count
           << ",\n    \"stutter_events\": [";
    for (std::size_t i = 0; i < report.frame_time.stutter_events.size(); ++i) {
        const auto& event = report.frame_time.stutter_events[i];
        output << (i ? ",\n" : "\n")
               << "      {\"start_timestamp_ms\": " << event.start_timestamp_ms
               << ", \"end_timestamp_ms\": " << event.end_timestamp_ms
               << ", \"frame_count\": " << event.frame_count
               << ", \"peak_frame_time_ms\": " << event.peak_frame_time_ms << "}";
    }
    if (!report.frame_time.stutter_events.empty()) output << "\n    ";
    output << "]\n  },\n  \"round_trip_latency\": {\n";
    write_stats(report.round_trip_latency, output, "    ");
    output << "\n  }\n}\n";
}

} // namespace riftpulse
