#include "riftpulse/analyzer.hpp"
#include "riftpulse/csv_reader.hpp"
#include "riftpulse/json_reporter.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void test_csv_reader() {
    std::istringstream csv(
        "match_id,timestamp_seconds,player_id,event_type,value\n"
        "M1,10,\"player,one\",kill,\n"
        "M1,-2,p2,death,\n"
        "M1,20,p2,gold,500\n"
        "M1,30,p3,unknown,\n");
    const auto result = riftpulse::CsvReader{}.read(csv);
    require(result.events.size() == 2, "CSV reader should retain two valid events");
    require(result.invalid_rows == 2, "CSV reader should count invalid rows");
    require(result.events.front().player_id == "player,one", "Quoted comma should parse");
}
void test_aggregation_and_duration() {
    std::vector<riftpulse::GameEvent> events{
        {"M2", 0, "alpha", riftpulse::EventType::Kill, ""},
        {"M2", 60, "alpha", riftpulse::EventType::Death, ""},
        {"M2", 120, "alpha", riftpulse::EventType::Gold, "600"},
        {"M2", 120, "alpha", riftpulse::EventType::Objective, "dragon"}
    };
    const auto report = riftpulse::MatchAnalyzer{}.analyze(events);
    require(report.match_id == "M2", "Match id should be preserved");
    require(report.duration_seconds == 120, "Duration should use max event timestamp");
    require(report.players.size() == 1, "One player summary expected");
    const auto& p = report.players.front();
    require(p.kills == 1 && p.deaths == 1 && p.objectives == 1, "Event counters incorrect");
    require(p.gold_earned == 600, "Gold aggregation incorrect");
    require(std::abs(p.gold_per_minute - 300.0) < 0.01, "Gold/minute incorrect");
}
void test_burst_detection() {
    std::vector<riftpulse::GameEvent> events;
    for (int i = 0; i < 8; ++i) {
        events.push_back({"M3", i, "fast_player", riftpulse::EventType::Assist, ""});
    }
    const auto report = riftpulse::MatchAnalyzer(10, 8).analyze(events);
    require(!report.event_bursts.empty(), "Eight events in the window should trigger a review flag");
}

void test_csv_rejects_malformed_quotes() {
    std::istringstream csv(
        "match_id,timestamp_seconds,player_id,event_type,value\n"
        "M1,10,p1,kill,\"bad\"suffix\n"
        "M1,11,p2,kill,bad\"quote\n"
        "M1,12,p3,assist,\n");
    const auto result = riftpulse::CsvReader{}.read(csv);
    require(result.events.size() == 1, "Malformed quoted fields should be rejected");
    require(result.invalid_rows == 2, "Malformed quoted fields should be counted");
}

void test_json_escapes_control_characters() {
    riftpulse::MatchReport report;
    report.match_id = std::string("M") + char(1) + "1";
    std::ostringstream output;
    riftpulse::JsonReporter{}.write(report, output);
    require(output.str().find("\\u0001") != std::string::npos,
            "JSON control characters should use Unicode escapes");
}

void test_mixed_matches_rejected() {
    std::vector<riftpulse::GameEvent> events{
        {"A", 1, "p1", riftpulse::EventType::Kill, ""},
        {"B", 2, "p2", riftpulse::EventType::Kill, ""}
    };
    bool threw = false;
    try { (void)riftpulse::MatchAnalyzer{}.analyze(events); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "Mixed match IDs should be rejected");
}
}
int main() {
    try {
        test_csv_reader();
        test_aggregation_and_duration();
        test_burst_detection();
        test_csv_rejects_malformed_quotes();
        test_json_escapes_control_characters();
        test_mixed_matches_rejected();
        std::cout << "All RiftPulse tests passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failure: " << ex.what() << '\n';
        return 1;
    }
}
