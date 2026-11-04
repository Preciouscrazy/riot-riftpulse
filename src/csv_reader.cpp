#include "riftpulse/csv_reader.hpp"

#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace riftpulse {
namespace {
std::vector<std::string> split_csv(const std::string& line, bool& valid) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    bool quote_closed = false;
    valid = true;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];

        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    quoted = false;
                    quote_closed = true;
                }
            } else {
                field.push_back(c);
            }
            continue;
        }

        if (quote_closed) {
            if (c != ',') {
                valid = false;
                return {};
            }
            fields.push_back(field);
            field.clear();
            quote_closed = false;
            continue;
        }

        if (c == ',') {
            fields.push_back(field);
            field.clear();
        } else if (c == '"' && field.empty()) {
            quoted = true;
        } else if (c == '"') {
            valid = false;
            return {};
        } else {
            field.push_back(c);
        }
    }

    if (quoted) {
        valid = false;
        return {};
    }

    fields.push_back(field);
    return fields;
}

bool parse_nonnegative_int64(std::string_view text, std::int64_t& out) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && out >= 0;
}

bool parse_nonnegative_uint64(const std::string& text, std::uint64_t& out) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool parse_type(const std::string& raw, EventType& type) {
    if (raw == "kill") type = EventType::Kill;
    else if (raw == "death") type = EventType::Death;
    else if (raw == "assist") type = EventType::Assist;
    else if (raw == "gold") type = EventType::Gold;
    else if (raw == "objective") type = EventType::Objective;
    else if (raw == "tower_damage") type = EventType::TowerDamage;
    else return false;
    return true;
}
} // namespace

ParseResult CsvReader::read(std::istream& input) const {
    ParseResult result;
    std::string line;
    if (!std::getline(input, line)) {
        result.warnings.emplace_back("Input is empty; expected a CSV header.");
        return result;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "match_id,timestamp_seconds,player_id,event_type,value") {
        result.warnings.emplace_back("Unexpected header. Expected: match_id,timestamp_seconds,player_id,event_type,value");
        return result;
    }

    std::size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        bool csv_valid = false;
        const auto fields = split_csv(line, csv_valid);
        if (!csv_valid || fields.size() != 5 || fields[0].empty() || fields[2].empty()) {
            ++result.invalid_rows;
            result.warnings.push_back("Skipped malformed CSV row " + std::to_string(line_number) + ".");
            continue;
        }

        GameEvent event;
        event.match_id = fields[0];
        event.player_id = fields[2];
        if (!parse_nonnegative_int64(fields[1], event.timestamp_seconds) ||
            !parse_type(fields[3], event.type)) {
            ++result.invalid_rows;
            result.warnings.push_back("Skipped invalid timestamp or event type on row " + std::to_string(line_number) + ".");
            continue;
        }

        if (event.type == EventType::Gold || event.type == EventType::TowerDamage) {
            std::uint64_t amount = 0;
            if (!parse_nonnegative_uint64(fields[4], amount)) {
                ++result.invalid_rows;
                result.warnings.push_back("Skipped invalid numeric value on row " + std::to_string(line_number) + ".");
                continue;
            }
        } else if (event.type == EventType::Objective && fields[4].empty()) {
            ++result.invalid_rows;
            result.warnings.push_back("Skipped objective without a label on row " + std::to_string(line_number) + ".");
            continue;
        }
        event.value = fields[4];
        result.events.push_back(std::move(event));
    }
    return result;
}
} // namespace riftpulse
