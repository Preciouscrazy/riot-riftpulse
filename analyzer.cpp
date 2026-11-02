#include "riftpulse/analyzer.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace riftpulse {
namespace {
std::uint64_t numeric_value(const std::string& value) {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return 0;
    return parsed;
}
} // namespace

MatchAnalyzer::MatchAnalyzer(std::int64_t burst_window_seconds, std::size_t burst_threshold)
    : burst_window_seconds_(burst_window_seconds), burst_threshold_(burst_threshold) {
    if (burst_window_seconds_ <= 0 || burst_threshold_ < 2) {
        throw std::invalid_argument("Burst window must be positive and threshold must be at least 2.");
    }
}

MatchReport MatchAnalyzer::analyze(const std::vector<GameEvent>& events, std::size_t invalid_rows) const {
    MatchReport report;
    report.invalid_rows = invalid_rows;
    if (events.empty()) return report;

    report.match_id = events.front().match_id;
    std::int64_t max_timestamp = 0;
    std::unordered_map<std::string, PlayerSummary> summaries;
    std::unordered_map<std::string, std::vector<std::int64_t>> timestamps;

    for (const auto& event : events) {
        if (event.match_id != report.match_id) {
            throw std::invalid_argument("Input contains multiple match_id values; analyze one match at a time.");
        }
        max_timestamp = std::max(max_timestamp, event.timestamp_seconds);
        auto [it, inserted] = summaries.try_emplace(event.player_id);
        if (inserted) it->second.player_id = event.player_id;
        auto& summary = it->second;
        timestamps[event.player_id].push_back(event.timestamp_seconds);

        switch (event.type) {
            case EventType::Kill: ++summary.kills; break;
            case EventType::Death: ++summary.deaths; break;
            case EventType::Assist: ++summary.assists; break;
            case EventType::Gold: summary.gold_earned += numeric_value(event.value); break;
            case EventType::Objective: ++summary.objectives; break;
            case EventType::TowerDamage: summary.tower_damage += numeric_value(event.value); break;
        }
    }

    report.duration_seconds = max_timestamp;
    for (auto& [player_id, summary] : summaries) {
        (void)player_id;
        const double minutes = static_cast<double>(std::max<std::int64_t>(report.duration_seconds, 60)) / 60.0;
        summary.gold_per_minute = static_cast<double>(summary.gold_earned) / minutes;
        report.players.push_back(summary);
    }
    std::sort(report.players.begin(), report.players.end(),
              [](const PlayerSummary& a, const PlayerSummary& b) { return a.player_id < b.player_id; });

    for (auto& [player_id, times] : timestamps) {
        std::sort(times.begin(), times.end());
        std::size_t left = 0;
        for (std::size_t right = 0; right < times.size(); ++right) {
            while (left <= right && times[right] - times[left] > burst_window_seconds_) ++left;
            const std::size_t count = right - left + 1;
            if (count >= burst_threshold_) {
                report.event_bursts.push_back(EventBurst{
                    player_id, times[right], count,
                    "At least " + std::to_string(count) + " events within " +
                    std::to_string(burst_window_seconds_) + " seconds; review context."
                });
                // Emit one alert for this timestamp rather than repeated alerts for every event in the same burst.
                while (right + 1 < times.size() && times[right + 1] == times[right]) ++right;
            }
        }
    }
    std::sort(report.event_bursts.begin(), report.event_bursts.end(),
              [](const EventBurst& a, const EventBurst& b) {
                  if (a.timestamp_seconds != b.timestamp_seconds) return a.timestamp_seconds < b.timestamp_seconds;
                  return a.player_id < b.player_id;
              });
    return report;
}
} // namespace riftpulse
