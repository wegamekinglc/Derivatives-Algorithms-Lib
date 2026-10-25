#pragma once

#include <cmath>

#include "xccygaussianmomentfixture.hpp"
#include "xccygaussianrecoverycashflows.hpp"

namespace Dal::TestXccyRecovery {
    inline Vector_<> Target() {
        Vector_<> result;
        for (const auto& date : {Date_(2027, 11, 8), Date_(2028, 5, 8), Date_(2028, 11, 8)})
            result.push_back(-.003 * (date - TestXccy::TODAY) / 365.);
        return result;
    }

    struct Quote_ {
        long double value_;
        std::array<long double, 3> gradient_;
    };

    inline Quote_ Reference(size_t row, const Vector_<>& theta) {
        const auto target = Target();
        std::array<long double, 2> values{};
        std::array<std::array<long double, 3>, 2> gradients{};
        for (const auto& term : Cashflows()[row]) {
            long double exponent = 0.L;
            for (size_t j = 0; j < 3; ++j)
                exponent += term.load_[j] * (static_cast<long double>(theta[j]) - target[j]);
            const auto quantity = term.annuity_ ? 0U : 1U;
            const long double value = term.weight_ * std::exp(exponent);
            values[quantity] += value;
            for (size_t j = 0; j < 3; ++j)
                gradients[quantity][j] += value * term.load_[j];
        }
        Quote_ result{-values[1] / values[0], {}};
        for (size_t j = 0; j < 3; ++j)
            result.gradient_[j] = (-gradients[1][j] - result.value_ * gradients[0][j]) / values[0];
        return result;
    }

    inline long double Secant(size_t row, const Vector_<>& theta, int intervals) {
        const auto target = Target();
        long double result = 0.L;
        for (int k = 0; k <= intervals; ++k) {
            const double t = double(k) / intervals;
            Vector_<> point = target;
            for (size_t j = 0; j < 3; ++j)
                point[j] += t * (theta[j] - target[j]);
            const auto quote = Reference(row, point);
            const int weight = k == 0 || k == intervals ? 1 : (k % 2 ? 4 : 2);
            for (size_t j = 0; j < 3; ++j)
                result += weight * quote.gradient_[j] * (static_cast<long double>(theta[j]) - target[j]) / (3.L * intervals);
        }
        return result;
    }

    inline double Determinant(const Matrix_<>& j) {
        return j(0, 0) * (j(1, 1) * j(2, 2) - j(1, 2) * j(2, 1)) - j(0, 1) * (j(1, 0) * j(2, 2) - j(1, 2) * j(2, 0)) +
               j(0, 2) * (j(1, 0) * j(2, 1) - j(1, 1) * j(2, 0));
    }

    inline bool NodeShape(const DiscountLogDF_* curve, const XccyInstrumentCalibrationResult_& result) {
        return curve && curve->NodeDates().size() == 4 && result.parameters_.size() == 3 && result.freeParameters_.size() == 3;
    }

    inline bool SnapshotAgrees(const DiscountLogDF_& curve, const XccyInstrumentCalibrationResult_& result) {
        if (!result.gaussianModel_ || result.gaussianModel_->market_->basis_->nodeDates_ != curve.NodeDates())
            return false;
        const auto& snapshot = result.gaussianModel_->market_->basis_->logDF_;
        for (size_t j = 0; j < 3; ++j)
            if (std::abs(snapshot[j + 1] - result.parameters_[j]) > 1e-15)
                return false;
        return true;
    }

    inline bool NodesAgree(const XccyInstrumentCalibrationSpec_& spec, const XccyInstrumentCalibrationResult_& result) {
        const auto* curve = dynamic_cast<const DiscountLogDF_*>(result.curve_.get());
        if (!NodeShape(curve, result))
            return false;
        const auto values = curve->NodeLogDF();
        if (curve->NodeDates()[0] != TestXccy::TODAY || values[0] != 0.)
            return false;
        for (size_t j = 0; j < 3; ++j) {
            if (result.freeParameters_[j].date_ != spec.knotDates_[j] || curve->NodeDates()[j + 1] != spec.knotDates_[j])
                return false;
            if (values[j + 1] != result.parameters_[j])
                return false;
        }
        return SnapshotAgrees(*curve, result);
    }

    struct Checks_ {
        bool nodes_, values_ = true, partials_ = true, secant_ = true, quotes_ = true, rank_ = false;
        long double refinement_ = 0.L, secantError_ = 0.L, scaledMove_ = 0.L;
        [[nodiscard]] bool Accepted() const { return nodes_ && values_ && partials_ && secant_ && quotes_ && rank_; }
    };

    inline bool ValuesAgree(const XccyInstrumentCalibrationResult_& result, size_t row, long double expected, long double quote) {
        return std::abs(expected - result.modelMeasures_[row]) <= 1e-13L && std::abs(expected - quote - result.rawResiduals_[row]) <= 1e-13L;
    }

    inline bool RecoveryShape(const XccyInstrumentCalibrationSpec_& spec, const XccyInstrumentCalibrationResult_& result) {
        return spec.knotDates_.size() == 3 && spec.observations_.size() == 3 && result.parameters_.size() == 3 && result.modelMeasures_.size() == 3 &&
               result.rawResiduals_.size() == 3 && result.quoteJacobian_.Rows() == 3 && result.quoteJacobian_.Cols() == 3;
    }

    inline Checks_ Qualify(const XccyInstrumentCalibrationSpec_& spec, const XccyInstrumentCalibrationResult_& result) {
        REQUIRE(RecoveryShape(spec, result), "Recovery qualification requires three dated nodes, values, residuals and full raw Jacobian");
        Checks_ checks{NodesAgree(spec, result)};
        checks.rank_ = std::abs(Determinant(result.quoteJacobian_)) > 1e-4;
        const auto target = Target();
        for (size_t row = 0; row < 3; ++row) {
            const auto truth = Reference(row, target), actual = Reference(row, result.parameters_);
            const long double quote = spec.observations_[row]->Quote()->Value();
            checks.values_ &= std::abs(truth.value_ - quote) <= 1e-13L && ValuesAgree(result, row, actual.value_, quote);
            for (size_t j = 0; j < 3; ++j)
                checks.partials_ &= std::abs(actual.gradient_[j] - result.quoteJacobian_(int(row), int(j))) <= 1e-10L;
            const auto coarse = Secant(row, result.parameters_, 4), medium = Secant(row, result.parameters_, 8);
            const auto fine = Secant(row, result.parameters_, 16);
            const auto refinement = 4.L * std::max(std::abs(fine - medium), std::abs(medium - coarse));
            const auto error = std::abs(fine - (actual.value_ - truth.value_));
            checks.refinement_ = std::max(checks.refinement_, refinement);
            checks.secantError_ = std::max(checks.secantError_, error);
            checks.secant_ &= refinement <= 2.5e-13L && error <= refinement + 1e-13L && error <= 1e-12L;
            const long double scale = spec.observations_[row]->ErrorScale();
            checks.scaledMove_ = std::max(checks.scaledMove_, std::abs(fine / scale));
            checks.quotes_ &= std::abs((actual.value_ - quote) / scale) <= 1.L && std::abs(fine / scale) <= 1.L;
        }
        return checks;
    }

    inline bool RecoveredTo1eMinus5(const Vector_<>& theta) {
        const auto target = Target();
        for (size_t j = 0; j < 3; ++j)
            if (std::abs(theta[j] - target[j]) > 1e-5)
                return false;
        return true;
    }
} // namespace Dal::TestXccyRecovery
