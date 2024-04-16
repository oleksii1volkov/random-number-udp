#pragma once

#include <cstdint>

namespace utils {

uint64_t calculate_checksum(const auto &numbers) {
    uint64_t checksum{0};

    for (const auto &number : numbers) {
        checksum += number;
    }

    return checksum;
}

} // namespace utils
