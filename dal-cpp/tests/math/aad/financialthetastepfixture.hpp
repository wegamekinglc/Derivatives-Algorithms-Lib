//
// Created by Codex on 2026/10/8.
//

#pragma once

#include <array>
#include <cmath>
#include <utility>

#include <dal/math/aad/native.hpp>

#include "test-support/europeanaadfd.hpp"

namespace DalTest::FinancialPDE {
    using Parameters_ = std::array<double, 3>;
    using Lane_ = std::array<double, 3>;
    inline constexpr Parameters_ PARAMETERS = {0.05, 0.20, 110.0};

    struct Sweep_ {
        Dal::Matrix_<> risks_;
        Dal::AAD::SolveAccuracyReports_ reports_;
    };

    struct Result_ {
        std::array<double, 2> prices_;
        double objective_;
        double forwardError_;
        Dal::Vector_<Dal::AAD::SolveAccuracyEvent_> events_;
        Dal::Vector_<Sweep_> sweeps_;
    };

    inline void SeedLanes(std::array<Dal::AAD::Number_, 2>* prices, Dal::AAD::Number_* objective, const Dal::Vector_<Lane_>& lanes, double scale) {
        for (size_t channel = 0; channel < lanes.size(); ++channel) {
            for (size_t layer = 0; layer < 2; ++layer)
                Dal::AAD::NativeOperations_::SetSeed((*prices)[layer], scale * lanes[channel][layer], channel);
            Dal::AAD::NativeOperations_::SetSeed(*objective, scale * lanes[channel][2], channel);
        }
    }

    inline Dal::Matrix_<> ReadRisks(const std::array<Dal::AAD::Number_, 3>& parameters, size_t channels) {
        Dal::Matrix_<> result(static_cast<int>(channels), 3);
        for (size_t channel = 0; channel < channels; ++channel)
            for (size_t coordinate = 0; coordinate < parameters.size(); ++coordinate)
                result(static_cast<int>(channel), static_cast<int>(coordinate)) =
                    Dal::AAD::NativeOperations_::ReadAdjoint(parameters[coordinate], channel);
        return result;
    }

    inline Result_ Evaluate(const Dal::AAD::Example::EuropeanThetaSettings_& settings,
                            size_t width,
                            const Dal::Vector_<Lane_>& lanes,
                            const Dal::Vector_<>& scales = {1.0}) {
        using namespace Dal::AAD;
        REQUIRE(lanes.size() == std::max(size_t(1), width), "Financial fixture seed/channel mismatch");
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(width != 0, lanes.size());
        RecordingScope_ scope;
        std::array<Number_, 3> parameters;
        for (size_t coordinate = 0; coordinate < parameters.size(); ++coordinate)
            scope.RegisterInput(parameters[coordinate], PARAMETERS[coordinate]);
        scope.StartRecording();
        auto recording = Example::RecordEuropeanOptions(&scope, settings, parameters);
        Number_ objective =
            1.25 * recording.prices_[0] - 0.75 * recording.prices_[1] + 2.0 * parameters[0] - 3.0 * parameters[1] + 0.01 * parameters[2];
        scope.FinishRecording();
        Result_ result{
            {Value(recording.prices_[0]), Value(recording.prices_[1])}, Value(objective), recording.largestForwardError_, recording.events_, {}};
        for (double scale : scales) {
            scope.ClearAdjoints();
            SeedLanes(&recording.prices_, &objective, lanes, scale);
            auto reports = ReverseWithSolveAccuracy(&scope);
            result.sweeps_.push_back({ReadRisks(parameters, lanes.size()), std::move(reports)});
        }
        scope.Close();
        Clear(*Tape());
        return result;
    }

    using SmallState_ = std::array<std::array<double, 2>, 9>;
    using SmallMatrix_ = std::array<std::array<double, 9>, 9>;
    struct FrozenDependencies_ {
        bool terminal_ = false;
        bool boundaries_ = false;
    };

    inline SmallState_ DenseSolve(SmallMatrix_ matrix, SmallState_ rhs) {
        for (size_t column = 0; column < 9; ++column) {
            REQUIRE(std::abs(matrix[column][column]) > 1e-14, "Independent dense reference requires a nonzero pivot");
            for (size_t row = column + 1; row < 9; ++row) {
                const double factor = matrix[row][column] / matrix[column][column];
                for (size_t next = column + 1; next < 9; ++next)
                    matrix[row][next] -= factor * matrix[column][next];
                for (size_t layer = 0; layer < 2; ++layer)
                    rhs[row][layer] -= factor * rhs[column][layer];
            }
        }
        SmallState_ result{};
        for (int row = 8; row >= 0; --row)
            for (size_t layer = 0; layer < 2; ++layer) {
                double residual = rhs[row][layer];
                for (size_t column = row + 1; column < 9; ++column)
                    residual -= matrix[row][column] * result[column][layer];
                result[row][layer] = residual / matrix[row][row];
            }
        return result;
    }

    inline SmallState_ DenseSmallStep(const SmallState_& state, const Parameters_& parameters, int step, const Parameters_& boundaryParameters) {
        const double rate = parameters[0], sigma = parameters[1];
        const double dt = step < 4 ? 0.0625 : 0.125;
        const double theta = step < 4 ? 1.0 : 0.5;
        const double tau = (step < 4 ? 0.5 * (step + 1) : step - 1.0) / 8.0;
        SmallMatrix_ matrix{};
        auto rhs = state;
        for (size_t row = 0; row < matrix.size(); ++row)
            matrix[row][row] = 1.0;
        for (size_t row = 1; row < 8; ++row) {
            const double diffusion = sigma * sigma * row * row;
            const double drift = (rate - 0.02) * row;
            const std::array<double, 3> generator = {0.5 * (diffusion - drift), -diffusion - rate, 0.5 * (diffusion + drift)};
            for (size_t local = 0; local < 3; ++local) {
                matrix[row][row + local - 1] -= dt * theta * generator[local];
                for (size_t layer = 0; layer < 2; ++layer)
                    rhs[row][layer] += dt * (1.0 - theta) * generator[local] * state[row + local - 1][layer];
            }
        }
        const double discountedStrike = boundaryParameters[2] * std::exp(-boundaryParameters[0] * tau);
        rhs[0] = {0.0, discountedStrike};
        rhs[8] = {400.0 * std::exp(-0.02 * tau) - discountedStrike, 0.0};
        return DenseSolve(matrix, rhs);
    }

    inline std::array<double, 2> DenseSmallPrice(const Parameters_& parameters, const FrozenDependencies_& frozen = {}) {
        SmallState_ state{};
        const double strike = frozen.terminal_ ? PARAMETERS[2] : parameters[2];
        const auto& boundary = frozen.boundaries_ ? PARAMETERS : parameters;
        for (size_t row = 0; row < state.size(); ++row) {
            state[row][0] = std::max(50.0 * row - strike, 0.0);
            state[row][1] = std::max(strike - 50.0 * row, 0.0);
        }
        for (int step = 0; step < 10; ++step)
            state = DenseSmallStep(state, parameters, step, boundary);
        return state[2];
    }

    struct Quote_ {
        std::array<double, 2> prices_;
        std::array<std::array<double, 3>, 2> risks_;
    };

    struct GridReference_ {
        Dal::AAD::Example::EuropeanThetaSettings_ settings_;
        Quote_ quote_;
    };

    inline const std::array<GridReference_, 3> REFERENCES = {
        {{{21, 40},
          {{5.208583823151467, 11.824022041016411},
           {{{35.67121228059187, 36.44850008915173, -0.2820765589289714}, {-68.96087649769089, 36.44850008896985, 0.6691536025272339}}}}},
         {{61, 120},
          {{5.1900030895691085, 11.805380104352425},
           {{{35.05018913872579, 38.01512316376091, -0.315791003495686}, {-69.58469778820111, 38.01512316376076, 0.6354385028889284}}}}},
         {{181, 360},
          {{5.188741383496399, 11.804111597941695},
           {{{35.03842184368543, 38.10297819286888, -0.31823291676289306}, {-69.59677598827004, 38.102978192869166, 0.6329965168360354}}}}}}};

    inline Quote_ Continuum() {
        const double rate = PARAMETERS[0], sigma = PARAMETERS[1], strike = PARAMETERS[2];
        const double d1 = (std::log(100.0 / strike) + rate - 0.02 + 0.5 * sigma * sigma) / sigma;
        const double d2 = d1 - sigma;
        const auto cdf = [](double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); };
        const double spotDiscount = 100.0 * std::exp(-0.02), strikeDiscount = strike * std::exp(-rate);
        const double vega = spotDiscount * std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * std::acos(-1.0));
        return {{spotDiscount * cdf(d1) - strikeDiscount * cdf(d2), strikeDiscount * cdf(-d2) - spotDiscount * cdf(-d1)},
                {{{strikeDiscount * cdf(d2), vega, -std::exp(-rate) * cdf(d2)}, {-strikeDiscount * cdf(-d2), vega, std::exp(-rate) * cdf(-d2)}}}};
    }
} // namespace DalTest::FinancialPDE
