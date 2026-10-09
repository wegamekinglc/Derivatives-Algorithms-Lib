//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <memory>
#include <utility>

#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/math/aad/bumpoveraad.hpp>

namespace Dal {
    class RateCalibrationSnapshot_ {
    public:
        struct Data_;

    private:
        std::shared_ptr<const Data_> data_;
        explicit RateCalibrationSnapshot_(std::shared_ptr<const Data_> data) : data_(std::move(data)) {}
        friend RateCalibrationSnapshot_ NewRateCalibration(const CurveCalibrationSpec_&);
        friend RateCalibrationSnapshot_ NewRateCalibration(const JointMultiCurveCalibrationSpec_&);
        friend RateCalibrationSnapshot_ RecalibrateRateWithRisk(const RateCalibrationSnapshot_&, const Vector_<>&);

    public:
        [[nodiscard]] const Vector_<>& Point() const;
        [[nodiscard]] const Vector_<>& Parameters() const;
        [[nodiscard]] const RateQuoteRiskProvenance_& Provenance() const;
    };

    [[nodiscard]] RateCalibrationSnapshot_ NewRateCalibration(const CurveCalibrationSpec_& spec);
    [[nodiscard]] RateCalibrationSnapshot_ NewRateCalibration(const JointMultiCurveCalibrationSpec_& spec);
    [[nodiscard]] RateCalibrationSnapshot_ RecalibrateRateWithRisk(const RateCalibrationSnapshot_& calibration, const Vector_<>& quotes);

    struct RateQuoteCurvatureExecution_ {
        String_ method_ = "BumpOverRecalibratedNativeRateAAD";
        size_t quoteGradientEvaluations_ = 0;
        size_t calibrations_ = 0;
        size_t objectiveReverseSweeps_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
    };

    class RateQuoteCurvatureResult_ {
        double value_;
        Vector_<> gradient_;
        Vector_<> point_;
        AAD::BumpOverAADRequest_ request_;
        Matrix_<> products_;
        RateCalibrationSnapshot_ calibration_;
        RateQuoteCurvatureExecution_ execution_;

    public:
        RateQuoteCurvatureResult_(double value,
                                  Vector_<> gradient,
                                  Vector_<> point,
                                  AAD::BumpOverAADRequest_ request,
                                  Matrix_<> products,
                                  RateCalibrationSnapshot_ calibration,
                                  RateQuoteCurvatureExecution_ execution)
            : value_(value), gradient_(std::move(gradient)), point_(std::move(point)), request_(std::move(request)), products_(std::move(products)),
              calibration_(std::move(calibration)), execution_(std::move(execution)) {}
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const Vector_<>& Gradient() const { return gradient_; }
        [[nodiscard]] const Vector_<>& Point() const { return point_; }
        [[nodiscard]] const Matrix_<>& Directions() const { return request_.directions_; }
        [[nodiscard]] const Vector_<>& Steps() const { return request_.steps_; }
        [[nodiscard]] const Matrix_<>& HessianProducts() const { return products_; }
        [[nodiscard]] const RateCalibrationSnapshot_& BaseCalibration() const { return calibration_; }
        [[nodiscard]] const RateQuoteCurvatureExecution_& Execution() const { return execution_; }
    };

    [[nodiscard]] RateQuoteCurvatureResult_ EvaluateRateQuoteCurvature(const AAD::NativeScalarFunction_& objective,
                                                                       const RateCalibrationSnapshot_& calibration,
                                                                       const AAD::BumpOverAADRequest_& request);
} // namespace Dal
