#include "riftpulse/json_reporter.hpp"

#include <iomanip>
#include <ostream>
#include <string>

namespace riftpulse {
namespace {
std::string escape_json(const std::string& input) {
    std::string output;
    for (const unsigned char c : input) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20) {
                    constexpr char hex[] = "0123456789abcdef";
                    output += "\\u00";
                    output += hex[(c >> 4) & 0x0f];
                    output += hex[c & 0x0f];
                } else {
                    output += static_cast<char>(c);
                }
        }
    }
    return output;
}
} // namespace

void JsonReporter::write(const MatchReport& report, std::ostream& output) const {
    output << "{\n"
           << "  \"match_id\": \"" << escape_json(report.match_id) << "\",\n"
           << "  \"duration_seconds\": " << report.duration_seconds << ",\n"
           << "  \"invalid_rows\": " << report.invalid_rows << ",\n"
           << "  \"players\": [";
    for (std::size_t i = 0; i < report.players.size(); ++i) {
        const auto& p = report.players[i];
        output << (i == 0 ? "\n" : ",\n")
               << "    {\"player_id\": \"" << escape_json(p.player_id) << "\", "
               << "\"kills\": " << p.kills << ", "
               << "\"deaths\": " << p.deaths << ", "
               << "\"assists\": " << p.assists << ", "
               << "\"gold_earned\": " << p.gold_earned << ", "
               << "\"gold_per_minute\": " << std::fixed << std::setprecision(2) << p.gold_per_minute << ", "
               << "\"objectives\": " << p.objectives << ", "
               << "\"tower_damage\": " << p.tower_damage << "}";
    }
    if (!report.players.empty()) output << "\n  ";
    output << "],\n  \"event_bursts\": [";
    for (std::size_t i = 0; i < report.event_bursts.size(); ++i) {
        const auto& b = report.event_bursts[i];
        output << (i == 0 ? "\n" : ",\n")
               << "    {\"player_id\": \"" << escape_json(b.player_id) << "\", "
               << "\"timestamp_seconds\": " << b.timestamp_seconds << ", "
               << "\"events_in_window\": " << b.events_in_window << ", "
               << "\"explanation\": \"" << escape_json(b.explanation) << "\"}";
    }
    if (!report.event_bursts.empty()) output << "\n  ";
    output << "]\n}\n";
}
} // namespace riftpulse
