#include "riftpulse/analyzer.hpp"
#include "riftpulse/csv_reader.hpp"
#include "riftpulse/json_reporter.hpp"

#include <fstream>
#include <iostream>
#include <string>

namespace {
void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " --input <events.csv> [--output <report.json>]\n";
}
}
int main(int argc, char* argv[]) {
    std::string input_path;
    std::string output_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--input" || arg == "--output") && i + 1 < argc) {
            if (arg == "--input") input_path = argv[++i];
            else output_path = argv[++i];
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }
    if (input_path.empty()) {
        print_usage(argv[0]);
        return 2;
    }

    std::ifstream input(input_path);
    if (!input) {
        std::cerr << "Error: could not open input file: " << input_path << '\n';
        return 1;
    }

    const auto parsed = riftpulse::CsvReader{}.read(input);
    for (const auto& warning : parsed.warnings) std::cerr << "Warning: " << warning << '\n';
    if (parsed.events.empty()) {
        std::cerr << "Error: no valid events found.\n";
        return 1;
    }

    try {
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
            std::cout << "Report written to " << output_path << '\n';
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
    return parsed.invalid_rows == 0 ? 0 : 3;
}
