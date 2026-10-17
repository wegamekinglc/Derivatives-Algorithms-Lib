//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <utility>

#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/model/dupirerisk.hpp>

namespace Dal {
    struct DupireQuoteCurvatureExecution_ {
        String_ method_ = "BumpOverRecalibratedNativeDupireAAD";
        size_t quoteGradientEvaluations_ = 0;
        size_t calibrations_ = 0;
        size_t objectiveReverseSweeps_ = 0;
        size_t calibrationReverseSweeps_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
    };

    class DupireQuoteCurvatureResult_ {
        double value_;
        Vector_<> gradient_;
        Vector_<> point_;
        AAD::BumpOverAADRequest_ request_;
        Matrix_<> products_;
        DupireCalibrationSnapshot_ calibration_;
        DupireQuoteCurvatureExecution_ execution_;

        DupireQuoteCurvatureResult_(double value,
                                    Vector_<> gradient,
                                    Vector_<> point,
                                    AAD::BumpOverAADRequest_ request,
                                    Matrix_<> products,
                                    DupireCalibrationSnapshot_ calibration,
                                    DupireQuoteCurvatureExecution_ execution)
            : value_(value), gradient_(std::move(gradient)), point_(std::move(point)), request_(std::move(request)), products_(std::move(products)),
              calibration_(std::move(calibration)), execution_(std::move(execution)) {}
        friend DupireQuoteCurvatureResult_
        EvaluateDupireQuoteCurvature(const AAD::NativeScalarFunction_&, const DupireCalibrationSnapshot_&, const AAD::BumpOverAADRequest_&);

    public:
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return request_.directions_; }
        [[nodiscard]] const Vector_<>& Steps() const { return request_.steps_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const DupireCalibrationSnapshot_& Calibration() const { return calibration_; }
        [[nodiscard]] const DupireQuoteCurvatureExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] DupireQuoteCurvatureResult_ EvaluateDupireQuoteCurvature(const AAD::NativeScalarFunction_& objective,
                                                                           const DupireCalibrationSnapshot_& calibration,
                                                                           const AAD::BumpOverAADRequest_& request);
} // namespace Dal
