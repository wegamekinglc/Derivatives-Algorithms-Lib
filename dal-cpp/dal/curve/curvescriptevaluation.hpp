#pragma once

#include <dal/curve/compoundedois.hpp>
#include <dal/curve/curveinstrumentcalibration.hpp>

namespace Dal {
    struct CurveScriptEvaluationRequest_ {
        CurveInstrumentGeometry_ geometry_;
        CurveInstrumentMarket_ market_;
        Script::ScriptValuationSettings_ valuation_;
        Vector_<> parameters_;
        Handle_<PreparedCompoundedOis_> compoundedOis_;
        Vector_<String_> measureIds_{"Price", "ParRate"};
        CalibrationEvaluation_ evaluation_ = CalibrationEvaluation_::Value_::AutoFast;
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::None;
        double differenceStep_ = 1e-6;
    };
    struct CurveScriptCashflowResult_ {
        String_ table_, couponId_;
        size_t row_ = 0, sourceRow_ = 0, statement_ = 0;
        Date_ payment_;
        bool included_ = false, settled_ = false;
        std::optional<double> amount_, discount_;
        double pv_ = 0.;
    };
    struct CurveScriptObservationResult_ {
        size_t row_ = 0, sourceRow_ = 0, statement_ = 0;
        String_ couponId_, fixingIndex_, projectionRoute_, source_;
        DateTime_ fixingTime_;
        Date_ start_, end_, rateStart_, rateEnd_;
        double accrual_ = 0., rateAccrual_ = 0.;
        std::optional<double> rate_;
    };
    struct CurveScriptEvaluationResult_ {
        Handle_<CalibrationInstrument_> instrument_;
        Handle_<DiscountCurve_> curve_;
        Handle_<CalibrationReport_> report_;
        Vector_<String_> measureIds_, measureUnits_;
        Vector_<> parameters_, values_;
        Vector_<CurveFreeParameter_> freeParameters_;
        Matrix_<> nativePartials_;
        Vector_<CurveScriptCashflowResult_> cashflows_;
        Vector_<CurveScriptObservationResult_> observations_;
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::None;
        String_ contextIdentity_, candidateIdentity_;
    };
    CurveScriptEvaluationResult_ EvaluateCurveScriptInstrument(const Handle_<CalibrationInstrument_>& instrument,
                                                               const CurveScriptEvaluationRequest_& request);
} // namespace Dal
