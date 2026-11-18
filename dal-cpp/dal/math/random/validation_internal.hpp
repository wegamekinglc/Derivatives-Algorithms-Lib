#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <dal/utilities/exceptions.hpp>

namespace Dal::RandomInput {
    inline void Integer(double value, double lower, double upper, const char* field, const char* expected) {
        REQUIRE(std::isfinite(value) && std::trunc(value) == value && value >= lower && value < upper,
                String_("InvalidRandomInput: field=") + field + "; expected=" + expected + "; actual=" + String_(std::to_string(value)));
    }

    inline int Seed(double value) {
        Integer(value, std::numeric_limits<int>::min(), static_cast<double>(std::numeric_limits<int>::max()) + 1.,
                "/seed", "finite integer in signed int range");
        return static_cast<int>(value);
    }

    inline size_t Dimension(double value) {
        Integer(value, 0., std::ldexp(1., std::numeric_limits<size_t>::digits), "/n_dim", "finite nonnegative integer below 2^size_t_bits");
        return static_cast<size_t>(value);
    }

    inline int PublicDimension(double value) {
        Integer(value, 0., static_cast<double>(std::numeric_limits<int>::max()) + 1., "/n_dim", "finite integer in [0,INT_MAX]");
        return static_cast<int>(value);
    }

    inline int SobolDimension(double value) {
        Integer(value, 0., 21201., "/n_dim", "finite integer in [0,21200]");
        return static_cast<int>(value);
    }

    inline size_t SobolPosition(double value) {
        Integer(value, 0., 4294967296., "/i_path", "finite integer in [0,UINT32_MAX]");
        return static_cast<size_t>(value);
    }

    inline void SobolPosition(size_t value) {
        REQUIRE(value <= std::numeric_limits<uint32_t>::max(),
                "InvalidRandomInput: field=/i_path; expected=integer in [0,UINT32_MAX]; actual=" + String_(std::to_string(value)));
    }
} // namespace Dal::RandomInput
