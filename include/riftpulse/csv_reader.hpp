#pragma once
#include "riftpulse/models.hpp"
#include <istream>

namespace riftpulse {
class CsvReader {
public:
    ParseResult read(std::istream& input) const;
};
} // namespace riftpulse
