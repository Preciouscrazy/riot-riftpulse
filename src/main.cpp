#include "riftpulse/analyzer.hpp"
#include "riftpulse/csv_reader.hpp"
#include "riftpulse/json_reporter.hpp"
#include "riftpulse/telemetry.hpp"

#include <charconv>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void print_usage(const char* program) {
    std::cerr << "Usage:\n  " << program
              << " --input <events.csv> [--output <report.json>]\n  " << program
              << " --telemetry <samples.csv> [--stutter-ms <threshold>] [--output <report.json>]\n";
}

bool positive_double(const std::string& text, double& value) {
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
           std::isfinite(value) && value > 0.0;
}
}

int main(int argc, char* argv[]) {
    std::string input_path;
    std::string telemetry_path;
    std::string output_path;
    double stutter_threshold_ms = 33.3;
    bool threshold_specified = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--input" || arg == "--telemetry" || arg == "--output" || arg == "--stutter-ms") &&
            i + 1 < argc) {
            const std::string value = argv[++i];
            if (arg == "--input") input_path = value;
            else if (arg == "--telemetry") telemetry_path = value;
            else if (arg == "--output") output_path = value;
            else {
                threshold_specified = true;
                if (!positive_double(value, stutter_threshold_ms)) {
                    print_usage(argv[0]);
                    return 2;
                }
            }
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }
    if ((input_path.empty() == telemetry_path.empty()) ||
        (threshold_specified && telemetry_path.empty())) {
        print_usage(argv[0]);
        return 2;
    }

    const std::string& source_path = telemetry_path.empty() ? input_path : telemetry_path;
    std::ifstream input(source_path);
    if (!input) {
        std::cerr << "Error: could not open input file: " << source_path << '\n';
        return 1;
    }

    try {
        if (!telemetry_path.empty()) {
            const auto parsed = riftpulse::TelemetryCsvReader{}.read(input);
            for (const auto& warning : parsed.warnings) std::cerr << "Warning: " << warning << '\n';
            const auto report = riftpulse::TelemetryAnalyzer{stutter_threshold_ms}.analyze(
                parsed.samples, parsed.invalid_rows);
            if (output_path.empty()) {
                riftpulse::TelemetryJsonReporter{}.write(report, std::cout);
            } else {
                std::ofstream output(output_path);
                if (!output) {
                    std::cerr << "Error: could not open output file: " << output_path << '\n';
                    return 1;
                }
                riftpulse::TelemetryJsonReporter{}.write(report, output);
                if (!output) {
                    std::cerr << "Error: failed while writing " << output_path << '\n';
                    return 1;
                }
                std::cout << "Report written to " << output_path << '\n';
            }
            return parsed.invalid_rows == 0 ? 0 : 3;
        }

        // Existing match-event analysis remains unchanged.
        const auto parsed = riftpulse::CsvReader{}.read(input);
        for (const auto& warning : parsed.warnings) std::cerr << "Warning: " << warning << '\n';
        if (parsed.events.empty()) {
            std::cerr << "Error: no valid events found.\n";
            return 1;
        }
        const auto report = riftpulse::MatchAnalyzer{}.analyze(parsed.events, parsed.invalid_rows);
        if (output_path.empty()) {
            riftpulse::JsonReporter{}.write(report, std::cout);
        } else {
            std::ofstream output(output_path);
            if (!output) {
                std::cerr << "Error: could not open output file: " << output_path << '\n';
                return 1;
            }
            riftpulse::JsonReporter{}.write(report, output);
            if (!output) {
                std::cerr << "Error: failed while writing " << output_path << '\n';
                return 1;
            }
            std::cout << "Report written to " << output_path << '\n';
        }
        return parsed.invalid_rows == 0 ? 0 : 3;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
