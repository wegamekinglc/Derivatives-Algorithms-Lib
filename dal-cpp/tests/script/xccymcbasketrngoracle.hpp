#pragma once

#include <array>
#include <cstdint>

namespace Dal::TestXccyBasket {
    using RngState_ = std::array<uint64_t, 3>;
    using RngStep_ = std::array<RngState_, 3>;
    inline RngState_ Apply(const RngStep_& step, const RngState_& state, uint64_t modulus) {
        RngState_ result{};
        for (size_t i = 0; i < 3; ++i)
            for (size_t j = 0; j < 3; ++j)
                result[i] = (result[i] + step[i][j] * state[j] % modulus) % modulus;
        return result;
    }
    inline RngStep_ Square(const RngStep_& step, uint64_t modulus) {
        RngStep_ result{};
        for (size_t i = 0; i < 3; ++i)
            for (size_t j = 0; j < 3; ++j)
                for (size_t k = 0; k < 3; ++k)
                    result[i][j] = (result[i][j] + step[i][k] * step[k][j] % modulus) % modulus;
        return result;
    }
    inline RngState_ Seek(RngStep_ step, uint64_t seed, size_t draws, uint64_t modulus) {
        RngState_ state{seed, seed, seed};
        while (draws > 0) {
            if (draws % 2)
                state = Apply(step, state, modulus);
            step = Square(step, modulus);
            draws /= 2;
        }
        return state;
    }
    class ScalarOracle_ {
        RngState_ x_, y_;
        static constexpr uint64_t M1 = 4294967087ULL, M2 = 4294944443ULL;
        const RngStep_ sx_{{{0, 1, 0}, {0, 0, 1}, {M1 - 810728, 1403580, 0}}};
        const RngStep_ sy_{{{0, 1, 0}, {0, 0, 1}, {M2 - 1370589, 0, 527612}}};

    public:
        ScalarOracle_(uint64_t seed, size_t coordinate) {
            x_ = Seek(sx_, seed, coordinate, M1);
            y_ = Seek(sy_, seed + 1, coordinate, M2);
        }
        double NextUniform() {
            x_ = Apply(sx_, x_, M1);
            y_ = Apply(sy_, y_, M2);
            const uint64_t difference = x_[2] > y_[2] ? x_[2] - y_[2] : x_[2] + M1 - y_[2];
            return static_cast<double>(difference) / 4294967088.;
        }
    };
} // namespace Dal::TestXccyBasket
