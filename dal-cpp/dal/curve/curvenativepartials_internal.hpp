#pragma once

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/curveparameterization.hpp>

namespace Dal::CurveInstrumentInternal {
    inline std::pair<double, double> DifferencePoints(double parameter, double bump, const CurveFreeParameter_& label, CalibrationDiagnostic_ d) {
        const double lower = parameter - bump, upper = parameter + bump;
        d.parameterKey_ = String_(label.component_.String()) + ":" + Date::ToString(label.date_);
        CalibrationInternal::Check(std::isfinite(lower) && std::isfinite(upper) && lower < parameter && parameter < upper &&
                                       std::isfinite(upper - lower) && upper - lower > 0.,
                                   "CalibrationNumericalFailure", "/options/difference_step",
                                   "finite resolvable two-sided native parameter perturbations", CalibrationInternal::NumberText(bump), d);
        return {lower, upper};
    }
    template <class F_>
    Matrix_<> NativeDifferenceJacobian(const Vector_<>& parameters,
                                       int rows,
                                       double bump,
                                       const Vector_<CurveFreeParameter_>& labels,
                                       const CalibrationDiagnostic_& d,
                                       const F_& measures) {
        Matrix_<> result(rows, static_cast<int>(parameters.size()));
        auto up = parameters, down = parameters;
        for (int j = 0; j < result.Cols(); ++j) {
            const auto [lower, upper] = DifferencePoints(parameters[j], bump, labels[j], d);
            up[j] = upper;
            down[j] = lower;
            const auto plus = measures(up), minus = measures(down);
            CalibrationInternal::Check(plus.size() == static_cast<size_t>(rows) && minus.size() == static_cast<size_t>(rows),
                                       "CalibrationNumericalFailure", "/native_partials", "fixed raw measure row count", "", d);
            for (int i = 0; i < rows; ++i)
                result(i, j) = (plus[i] - minus[i]) / (upper - lower);
            up[j] = down[j] = parameters[j];
        }
        return result;
    }
} // namespace Dal::CurveInstrumentInternal
