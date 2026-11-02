#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace riftpulse {

enum class EventType { Kill, Death, Assist, Gold, Objective, TowerDamage };

struct GameEvent {
    std::string match_id;
    std::int64_t timestamp_seconds{};
    std::string player_id;
    EventType type{};
    std::string value;
};

struct ParseResult {
    std::vector<GameEvent> events;
    std::size_t invalid_rows{};
    std::vector<std::string> warnings;
};

struct PlayerSummary {
    std::string player_id;
    std::uint64_t kills{};
    std::uint64_t deaths{};
    std::uint64_t assists{};
    std::uint64_t gold_earned{};
    double gold_per_minute{};
    std::uint64_t objectives{};
    std::uint64_t tower_damage{};
};

struct EventBurst {
    std::string player_id;
    std::int64_t timestamp_seconds{};
    std::size_t events_in_window{};
    std::string explanation;
};

struct MatchReport {
    std::string match_id;
    std::int64_t duration_seconds{};
    std::size_t invalid_rows{};
    std::vector<PlayerSummary> players;
    std::vector<EventBurst> event_bursts;
};

} // namespace riftpulse
