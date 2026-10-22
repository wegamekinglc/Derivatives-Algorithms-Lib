#pragma once

#include <dal-public/src/script.hpp>
#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/model/gsrrateoption.hpp>
#include <dal/script/calibrationmc.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal {
    struct ScriptInstrumentImportSettings_ {
        String_ instrumentId_;
        Ccy_ currency_;
        double notional_ = 0.;
        String_ discountRoute_;
        std::optional<double> minAbsAnnuity_;
        std::optional<Date_> pillar_;
    };
    Handle_<CalibrationInstrument_> NewDepositScriptInstrument(const Handle_<YCInstrument_>& deposit,
                                                               const ScriptInstrumentImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewEuropeanRateOptionScriptInstrument(const EuropeanRateOption_& option,
                                                                          const ScriptInstrumentImportSettings_& settings);
    Handle_<CalibrationInstrument_>
    NewFraScriptInstrument(const Handle_<YCInstrument_>& native, const Date_& payment, const ScriptInstrumentImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewSwapScriptInstrument(const Handle_<YCInstrument_>& native, const ScriptInstrumentImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewPeriodForwardOisScriptInstrument(const Handle_<YCInstrument_>& native,
                                                                        const ScriptInstrumentImportSettings_& settings);
    Handle_<CalibrationInstrument_> NewBasisSwapScriptInstrument(const Handle_<YCInstrument_>& native,
                                                                 const ScriptInstrumentImportSettings_& settings);
    struct CalibrationMonteCarloSampling_ {
        String_ mode_ = "PseudoRandom";
        int pathsPerBatch_ = 0, batches_ = 0, seed_ = 0;
        String_ streamVersion_;
    };
    struct CalibrationSimulationSettings_ {
        bool useBb_ = false;
        std::optional<bool> compiled_;
        double smooth_ = Script::DEFAULT_SMOOTH;
        String_ normalPrecision_ = "Precise";
    };
    struct CalibrationMonteCarloRequest_ {
        String_ measureId_;
        Script::ScriptValuationSettings_ valuation_;
        CalibrationMonteCarloSampling_ sampling_;
        CalibrationSimulationSettings_ simulation_;
        CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::None;
    };
    struct CalibrationMonteCarloResult_ {
        double mean_ = 0.;
        std::optional<double> standardError_;
        Handle_<CalibrationReport_> report_;
    };
    CalibrationMonteCarloResult_ ValueCalibrationInstrumentByMonteCarlo(const Handle_<CalibrationInstrument_>& instrument,
                                                                        const Handle_<ModelData_>& model,
                                                                        const CalibrationMonteCarloRequest_& request);
    String_ DumpCalibrationReportJson(const Handle_<CalibrationReport_>& report);
    Matrix_<Cell_> CalibrationReportTable(const Handle_<CalibrationReport_>& report, const String_& table);
    Matrix_<> CalibrationReportQuoteJacobian(const Handle_<CalibrationReport_>& report);
    [[noreturn]] void CalibrationInputFailure(const String_& code, const String_& field, const String_& expected, const String_& actual = "");
} // namespace Dal
