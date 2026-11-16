#include <cmath>
#include <iomanip>
#include <iostream>

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/scriptcurvequoterisk.hpp>

#include "../../../dal-cpp/examples/floatformat.hpp"

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ Contract(const String_& mode) {
        const Date_ start(2024, 3, 5), end(2024, 3, 8), pay(2024, 3, 12);
        CompoundedOisScriptSpec_ trade;
        trade.instrumentId_ = "facade-" + mode;
        trade.direction_ = "PayFixed";
        trade.discountRoute_ = "USD.OIS";
        trade.projectionRoute_ = "USD.SOFR";
        trade.fixingIndex_ = "USD-SOFR";
        trade.notional_ = 100.;
        trade.coupon_ = .02;
        trade.currency_ = trade.collateralCurrency_ = Ccy_("USD");
        trade.collateralType_ = CollateralType_("OIS");
        trade.observationBasis_ = DayBasis_("ACT_360");
        trade.fixedBasis_ = DayBasis_("ACT_365F");
        trade.calendar_ = HolidayCenterData_("explicit", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
        trade.fixedLeg_ = {{start, end, pay, 3. / 365.}};
        trade.coupons_ = {{"coupon", start, end, pay}};
        trade.ruleId_ = "dal.compounded-ois-cashflows/2";
        trade.observationConvention_ = CompoundedOisObservationConvention_{
            CompoundedOisObservationMode_(mode), mode == "Lookback" || mode == "ObservationShift" ? 1 : 0, mode == "Lockout" ? 1 : 0};
        return trade;
    }
    CurveInstrumentCalibrationSpec_ Context(const String_& mode) {
        const Date_ today(2024, 3, 1);
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = today;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.knotDates_ = {today, Date_(2024, 3, 21)};
        spec.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        spec.geometry_.calibrateDiscountCurve_ = false;
        spec.geometry_.targetOvernightRoute_ = "USD.SOFR";
        spec.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, true}, {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        spec.valuation_.evaluationDate_ = today;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        const auto prepared = PrepareCompoundedOis(Contract(mode), spec.valuation_);
        const auto instrument = NewCompoundedOisScriptInstrument(prepared);
        spec.compoundedOis_ = {prepared};
        spec.observations_ = {NewCalibrationObservation("par", instrument, "ParRate",
                                                        NewCalibrationMarketQuote("q", .025, CalibrationValueUnit_::Parse("Rate")), 1e-10)};
        return spec;
    }
    CurveInstrumentCalibrationResult_ Fit(const CurveInstrumentCalibrationSpec_& spec) {
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_("AAD");
        auto fit = CalibrateCurveInstruments(spec, options);
        REQUIRE(fit.fitWithinTolerance_ && std::abs(fit.rawResiduals_[0]) <= 1e-12, "accepted EXACT root required");
        return fit;
    }
    double Price(const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationResult_& fit) {
        CurveScriptEvaluationRequest_ request;
        request.geometry_ = spec.geometry_;
        request.market_ = spec.market_;
        request.valuation_ = spec.valuation_;
        request.compoundedOis_ = spec.compoundedOis_[0];
        request.parameters_ = fit.parameters_;
        request.measureIds_ = {"Price"};
        const auto result = EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
        REQUIRE(CurveScriptEvaluationTable(result, "observation_schedule").Cols() == 26, "owning evaluation audit");
        return CurveScriptEvaluationValue(result, "Price");
    }
    CurveInstrumentCalibrationSpec_ Bumped(CurveInstrumentCalibrationSpec_ spec, double bump) {
        const auto& row = *spec.observations_[0];
        spec.observations_[0] =
            NewCalibrationObservation(row.Id(), row.Instrument(), row.MeasureId(),
                                      NewCalibrationMarketQuote("q", row.Quote()->Value() + bump, row.Quote()->Unit()), row.ErrorScale());
        return spec;
    }
    void PrintSchedule(const Handle_<PreparedCompoundedOis_>& prepared) {
        REQUIRE(CompoundedOisObservationScheduleTable(prepared).Cols() == 22, "prepared facade audit");
        std::cout << "Frozen schedule (zero-based rows, ACT/360 weights)\n"
                  << std::left << std::setw(8) << "Row" << std::setw(14) << "Actual start" << std::setw(14) << "Rate start" << std::setw(16)
                  << "Weight"
                  << "Source key\n"
                  << std::string(86, '-') << '\n';
        for (const auto& row : prepared->ObservationSchedule())
            std::cout << std::setw(8) << row.row_ << std::setw(14) << Date::ToString(row.accrualStart_) << std::setw(14)
                      << Date::ToString(row.rateStart_) << std::setw(16) << ExampleFloat(row.compoundingAccrual_, 10) << row.sourceKey_ << '\n';
    }
    struct OwnedDemo_ {
        ScriptCurveQuoteRiskResult_ full_, rejected_;
        double reference_ = 0.;
    };
    OwnedDemo_ Run(const String_& mode) {
        const auto spec = Context(mode);
        PrintSchedule(spec.compoundedOis_[0]);
        const auto fit = Fit(spec);
        ScriptCurveQuoteRiskAccuracy_ accuracy{{1e-12}, 1e-10, 1e-12};
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
        REQUIRE(snapshot.RiskAvailable() && snapshot.MappingAvailable(), "full native quote risk required");
        const auto full = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec);
        accuracy.pvOnly_ = true;
        const auto lean = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
        REQUIRE(lean.RiskAvailable() && !lean.MappingAvailable(), "PV-only admission required");
        const auto pvOnly = ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), lean, spec);
        REQUIRE(full.Available() && pvOnly.Available() && full.RawSensitivity() == pvOnly.RawSensitivity(), "full/PV-only parity");
        double reference = 0.;
        for (const double h : {1e-4, 5e-5}) {
            const auto plus = Bumped(spec, h), minus = Bumped(spec, -h);
            reference = (Price(plus, Fit(plus)) - Price(minus, Fit(minus))) / (2. * h);
            REQUIRE(std::abs(full.RawSensitivity()[0] - reference) / 100. <= 1e-7 + 1e-5 * std::abs(reference / 100.), "fixed-contract quote FD");
        }
        REQUIRE(full.DV01()[0] == full.RawSensitivity()[0] * 1e-4, "signed decimal DV01 units");
        std::cout << "\nEXACT calibration: q=0.025, fixed K=0.02, N=100 USD\n"
                  << "Fitted ParRate=" << ExampleFloat(fit.modelMeasures_[0], 10) << " residual=" << ExampleFloat(fit.rawResiduals_[0])
                  << " theta=" << ExampleFloat(fit.parameters_[0], 10) << '\n';
        ScriptCurveQuoteRiskRequest_ unsupported;
        unsupported.liveQuotePayoff_ = true;
        return {full, ValueCalibrationInstrumentWithQuoteRisk(spec.observations_[0]->Instrument(), snapshot, spec, unsupported), reference};
    }
} // namespace

int main() {
    InitGlobalData(1);
    for (const String_ mode : {"None", "Lookback", "ObservationShift", "Lockout"}) {
        std::cout << "\nObservation mode: " << mode << '\n';
        const auto owned = Run(mode);
        std::cout << "\nOwning results after calibration inputs leave scope\n"
                  << std::left << std::setw(20) << "PV (USD)" << std::setw(24) << "Raw USD/decimal rate" << std::setw(24) << "Signed DV01 USD/+1bp"
                  << "Recalibration FD\n"
                  << std::string(92, '-') << '\n'
                  << std::setw(20) << ExampleFloat(owned.full_.PV(), 10) << std::setw(24) << ExampleFloat(owned.full_.RawSensitivity()[0], 10)
                  << std::setw(24) << ExampleFloat(owned.full_.DV01()[0], 10) << ExampleFloat(owned.reference_, 10) << '\n';
        REQUIRE(!owned.rejected_.Available(), "live-quote payoff must remain unavailable");
        REQUIRE(!owned.rejected_.Diagnostics().empty() && owned.rejected_.Diagnostics()[0].code_ == "ScriptCurveQuoteRiskLiveQuoteUnsupported",
                "expected live-quote diagnostic");
        for (const auto& diagnostic : owned.rejected_.Diagnostics())
            std::cout << diagnostic.code_ << " / " << diagnostic.field_.value_or("") << " / " << diagnostic.message_ << '\n';
    }
}
