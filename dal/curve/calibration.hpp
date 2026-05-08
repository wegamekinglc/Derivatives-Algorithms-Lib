#pragma once

#include <memory>
#include <map>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/date.hpp>
#include <dal/time/periodlength.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/ycinstrument.hpp>

namespace Dal {

    enum class CurveSolveMode_ {
        EXACT,
        APPROXIMATE
    };

    enum class CurveParameterization_ {
        PIECEWISE_LINEAR_FWD,
        PIECEWISE_CONSTANT_FWD,
        ZERO_RATE, // reserved for a future calibration parameterization
        LOG_DISCOUNT // reserved for a future calibration parameterization
    };

    enum class CurveKnotPolicy_ {
        INPUT,
        INSTRUMENTS,
        AUGMENTED
    };

    struct CurveCalibrationSpec_ {
        Date_ today_;
        String_ ccy_;
        Vector_<Handle_<YCInstrument_>> instruments_;
        Vector_<Date_> knotDates_;
        double smoothingWeight_ = 1.0;
        double tolerance_ = 1.0e-8;
        double fitTolerance_ = 1.0e-6;
        int maxEvaluations_ = 200;
        int maxRestarts_ = 20;
        double initialGuess_ = 0.05;
        CurveSolveMode_ solveMode_ = CurveSolveMode_::EXACT;
        CurveParameterization_ parameterization_ = CurveParameterization_::PIECEWISE_LINEAR_FWD;
        CurveKnotPolicy_ knotPolicy_ = CurveKnotPolicy_::INPUT;
    };

    struct CurveCalibrationDiagnostics_ {
        Vector_<String_> instrumentNames_;
        Vector_<> marketRates_;
        Vector_<> modelRates_;
        Vector_<> residuals_;
        Matrix_<> effJacobianInverse_;
        double maxAbsResidual_ = 0.0;
        double rmsResidual_ = 0.0;
        bool usedApproximateFit_ = false;
    };

    struct CurveCalibrationResult_ {
        std::unique_ptr<DiscountCurve_> curve_;
        CurveCalibrationDiagnostics_ diagnostics_;
    };

    Sparse::TriDiagonal_* BuildCurveCalibrationWeights(const Vector_<Date_>& knotDates,
                                                       int paramsPerKnot,
                                                       double smoothingWeight);
    Vector_<Date_> BuildCurveCalibrationKnots(const Date_& today,
                                              const Vector_<Handle_<YCInstrument_>>& instruments,
                                              const Vector_<Date_>& inputKnots,
                                              CurveKnotPolicy_ policy);
    void ValidateCurveCalibrationSpec(const CurveCalibrationSpec_& spec);
    void ValidateNoArbitrage(const DiscountCurve_& curve, const Date_& today, const Vector_<Date_>& checkDates);
    CurveCalibrationResult_ CalibrateYieldCurve(const CurveCalibrationSpec_& spec);

} // namespace Dal
