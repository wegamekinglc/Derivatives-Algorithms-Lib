#pragma once

#include "mcprotocol.hpp"

namespace DalTest {
    constexpr size_t A1_DIMENSION = 8736, A1_COARSE = 1092;

    inline size_t A1PreviousPrefix(size_t exponent) {
        REQUIRE(exponent >= 16 && exponent <= 20, "Invalid A/3 formal tier");
        return exponent == 16 ? 1 : size_t{1} << (exponent - 1);
    }

    inline size_t A1Address(size_t family, size_t batch, size_t path, size_t coordinate = 0, size_t dimension = A1_DIMENSION) {
        REQUIRE(dimension > 0, "Invalid A/1 scalar dimension");
        REQUIRE(family < 2 && batch < 16 && path < MAX_PATHS && coordinate < A1_DIMENSION, "Invalid A/3 coordinate");
        CheckedProduct(32 * MAX_PATHS, dimension);
        REQUIRE(dimension == A1_DIMENSION, "Invalid A/3 fixed coordinate stride");
        const size_t ordinal = (family * 16 + batch) * MAX_PATHS + path;
        const size_t base = CheckedProduct(ordinal, dimension);
        REQUIRE(coordinate <= std::numeric_limits<size_t>::max() - base, "A/1 coordinate overflow");
        return base + coordinate;
    }

    inline size_t A1CoarseCount(size_t index, double left, double right) {
        const bool matching = left == 0x1.0006f5248e2e8p-4 && right == 0x1.2006f5248e2e8p-4;
        if (index == 60) {
            REQUIRE(matching && std::ceil(384 * (right - left)) == 3., "A/3 segment 60 endpoint/count mismatch");
            return 4;
        }
        REQUIRE(!matching, "A/3 exception at wrong segment index");
        return static_cast<size_t>(std::ceil(384 * (right - left)));
    }

    inline Dal::Vector_<> A1Timeline(const Dal::Vector_<>& anchors, size_t level) {
        REQUIRE(level < 4 && anchors.size() == 438 && anchors.front() == 0., "Invalid A/1 anchors");
        Dal::Vector_<> result{0.};
        for (size_t i = 1; i < anchors.size(); ++i) {
            const double left = anchors[i - 1], right = anchors[i];
            REQUIRE(right > left, "A/1 anchors must increase");
            const size_t count = A1CoarseCount(i - 1, left, right) * (size_t{1} << level);
            for (size_t j = 1; j < count; ++j)
                result.push_back(left + (right - left) * j / count);
            result.push_back(right);
        }
        REQUIRE(result.size() - 1 == A1_COARSE * (size_t{1} << level), "A/3 dimension mismatch");
        return result;
    }
} // namespace DalTest
