#pragma once
#include "riftpulse/models.hpp"
#include <cstddef>
#include <vector>

namespace riftpulse {
class MatchAnalyzer {
public:
    explicit MatchAnalyzer(std::int64_t burst_window_seconds = 10,
                           std::size_t burst_threshold = 8);
    MatchReport analyze(const std::vector<GameEvent>& events,
                        std::size_t invalid_rows = 0) const;
private:
    std::int64_t burst_window_seconds_;
    std::size_t burst_threshold_;
};
} // namespace riftpulse
