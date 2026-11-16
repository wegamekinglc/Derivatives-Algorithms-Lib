#pragma once

#include "mcprotocol.hpp"

namespace DalTest {
    inline size_t A1Address(size_t family, size_t batch, size_t path, size_t coordinate = 0, size_t dimension = 8728) {
        REQUIRE(dimension > 0, "Invalid A/1 scalar dimension");
        REQUIRE(family < 2 && batch < 16 && path < MAX_PATHS && coordinate < 8728, "Invalid A/1 coordinate");
        CheckedProduct(32 * MAX_PATHS, dimension);
        const size_t ordinal = (family * 16 + batch) * MAX_PATHS + path;
        const size_t base = CheckedProduct(ordinal, dimension);
        REQUIRE(coordinate <= std::numeric_limits<size_t>::max() - base, "A/1 coordinate overflow");
        return base + coordinate;
    }

    inline Dal::Vector_<> A1Timeline(const Dal::Vector_<>& anchors, size_t level) {
        REQUIRE(level < 4 && anchors.size() == 438 && anchors.front() == 0., "Invalid A/1 anchors");
        Dal::Vector_<> result{0.};
        for (size_t i = 1; i < anchors.size(); ++i) {
            const double left = anchors[i - 1], right = anchors[i];
            REQUIRE(right > left, "A/1 anchors must increase");
            const size_t count = static_cast<size_t>(std::ceil(384 * (right - left))) * (size_t{1} << level);
            for (size_t j = 1; j < count; ++j)
                result.push_back(left + (right - left) * j / count);
            result.push_back(right);
        }
        REQUIRE(result.size() - 1 == 1091 * (size_t{1} << level), "A/1 dimension mismatch");
        return result;
    }
} // namespace DalTest
