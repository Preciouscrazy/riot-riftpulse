#pragma once
#include "riftpulse/models.hpp"
#include <iosfwd>

namespace riftpulse {
class JsonReporter {
public:
    void write(const MatchReport& report, std::ostream& output) const;
};
} // namespace riftpulse
