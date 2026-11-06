#include <cmath>
#include <iomanip>
#include <iostream>
#include <utility>

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/scriptcurvequoterisk.hpp>

#include "../../../dal-cpp/examples/floatformat.hpp"

using namespace Dal;

namespace {
    CurveInstrumentCalibrationSpec_ DepositSpec() {
        const Date_ today(2026, 10, 21), first(2027, 10, 21), last(2028, 10, 23);
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = today;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.knotDates_ = {today, first, last};
        spec.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        spec.solver_.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.market_.routes_ = {{"USD.OIS"}};
        spec.valuation_.evaluationDate_ = today;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        for (int i = 0; i < 2; ++i) {
            const String_ id = i == 0 ? "usd-1y" : "usd-2y";
            const Handle_<YCInstrument_> native(new Deposit_(today, i == 0 ? first : last, .025, DayBasis_("ACT_360")));
            const auto instrument = NewDepositScriptInstrument(native, {id, Ccy_("USD"), 100., "USD.OIS", 1e-12});
            const auto quote = NewCalibrationMarketQuote(id, .035 + .005 * i, CalibrationValueUnit_::Parse("Rate"));
            spec.observations_.push_back(NewCalibrationObservation(id, instrument, "ParRate", quote, 1e-12));
        }
        return spec;
    }

    void PrintCalibration(const CurveInstrumentCalibrationResult_& fit, const ScriptCurveQuoteRiskSnapshot_& snapshot) {
        std::cout << "EXACT Deposit calibration (decimal rates, fixed K=0.025, notional=100 USD)\n"
                  << std::left << std::setw(16) << "Quote ID" << std::right << std::setw(16) << "Market" << std::setw(16) << "Fitted" << std::setw(26)
                  << "Raw residual" << '\n'
                  << std::string(74, '-') << '\n';
        for (size_t i = 0; i < snapshot.QuoteAxis().size(); ++i)
            std::cout << std::left << std::setw(16) << snapshot.QuoteAxis()[i].id_ << std::right << std::setw(16)
                      << ExampleFloat(snapshot.QuoteAxis()[i].value_, 6) << std::setw(16) << ExampleFloat(fit.modelMeasures_[i], 6) << std::setw(26)
                      << ExampleFloat(snapshot.Numerics().rawResiduals_[i]) << '\n';
        std::cout << "\nFitted native parameters\n"
                  << std::left << std::setw(44) << "Parameter" << std::right << std::setw(18) << "Value" << '\n'
                  << std::string(62, '-') << '\n';
        for (const auto& parameter : snapshot.ParameterAxis())
            std::cout << std::left << std::setw(44) << parameter.id_ << std::right << std::setw(18) << ExampleFloat(*parameter.value_, 8) << '\n';
        std::cout << "\nSnapshot admission\n"
                  << std::left << std::setw(30) << "Check" << "Value\n"
                  << std::string(60, '-') << '\n'
                  << std::setw(30) << "Structure valid" << std::boolalpha << snapshot.StructureValid() << '\n'
                  << std::setw(30) << "Mapping available" << snapshot.MappingAvailable() << '\n'
                  << std::setw(30) << "Reciprocal condition" << ExampleFloat(*snapshot.Numerics().reciprocalCondition_, 8) << '\n'
                  << std::setw(30) << "Cached factorizations" << snapshot.Numerics().factorizationCount_ << '\n';
    }

    void PrintDiagnostics(const ScriptCurveQuoteRiskResult_& result) {
        std::cout << "\nOwning failure after inputs leave scope\n"
                  << std::left << std::setw(14) << "Status" << std::setw(46) << "Code"
                  << "Field / message\n"
                  << std::string(100, '-') << '\n';
        for (const auto& diagnostic : result.Diagnostics())
            std::cout << std::left << std::setw(14) << (result.Available() ? "Available" : "Unavailable") << std::setw(46) << diagnostic.code_
                      << diagnostic.field_.value_or("") << " / " << diagnostic.message_ << '\n';
    }

    void PrintRisk(const ScriptCurveQuoteRiskResult_& result) {
        if (!result.Available()) {
            PrintDiagnostics(result);
            return;
        }
        std::cout << "\nFixed-contract Price and AAD risk through calibration\n"
                  << std::left << std::setw(20) << "Contract" << std::setw(34) << "Method" << std::setw(12) << "Currency" << std::right
                  << std::setw(18) << "Signed PV" << '\n'
                  << std::string(84, '-') << '\n'
                  << std::left << std::setw(20) << "usd-1y K=0.025" << std::setw(34) << result.Method() << std::setw(12) << result.Currency().String()
                  << std::right << std::setw(18) << ExampleFloat(result.PV(), 8) << '\n'
                  << "\nQuote buckets in requested order\n"
                  << std::left << std::setw(10) << "Ordinal" << std::setw(16) << "Quote ID" << std::setw(16) << "Convention" << std::right
                  << std::setw(20) << "dPV/dDecimalQuote" << std::setw(20) << "DV01 (USD, +1bp)" << '\n'
                  << std::string(82, '-') << '\n';
        for (size_t i = 0; i < result.Axes().quotes_.size(); ++i) {
            const auto& quote = result.Axes().quotes_[i];
            std::cout << std::left << std::setw(10) << quote.ordinal_ << std::setw(16) << quote.id_ << std::setw(16) << quote.convention_.id_.String()
                      << std::right << std::setw(20) << ExampleFloat(result.RawSensitivity()[i], 8) << std::setw(20)
                      << ExampleFloat(result.DV01()[i], 8) << '\n';
        }
    }
} // namespace

int main() {
    InitGlobalData(1);
    const auto [price, rejected] = [] {
        const auto spec = DepositSpec();
        const auto fit = CalibrateCurveInstruments(spec);
        const ScriptCurveQuoteRiskAccuracy_ accuracy{{1e-12, 1e-12}, 1e-10, 1e-12};
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
        if (!snapshot.RiskAvailable()) {
            for (const auto& diagnostic : snapshot.Diagnostics())
                std::cerr << diagnostic.code_ << ": " << diagnostic.message_ << '\n';
            THROW("Deposit snapshot did not pass explicit numerical admission");
        }
        PrintCalibration(fit, snapshot);
        const auto instrument = spec.observations_[0]->Instrument();
        const auto value = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{"usd-2y", "usd-1y"}});
        ScriptCurveQuoteRiskRequest_ liveRequest;
        liveRequest.liveQuotePayoff_ = true;
        return std::make_pair(value, ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, liveRequest));
    }();
    PrintRisk(price);
    REQUIRE(price.Available(), "fixed-contract Price risk must be available");
    const double accrual = 365. / 360., denominator = 1. + .035 * accrual;
    REQUIRE(std::abs(price.PV() - (-100. + 100. * (1. + .025 * accrual) / denominator)) < 1e-10, "fixed-coupon Deposit PV mismatch");
    REQUIRE(price.Axes().quotes_[0].id_ == "usd-2y" && price.Axes().quotes_[0].ordinal_ == 1, "requested order must retain quote ordinals");
    REQUIRE(std::abs(price.RawSensitivity()[0]) < 1e-10, "later quote must not move the first Deposit PV");
    const double expected = -100. * (1. + .025 * accrual) * accrual / (denominator * denominator);
    REQUIRE(std::abs(price.RawSensitivity()[1] - expected) < 1e-8, "fixed-coupon Deposit quote sensitivity mismatch");
    for (size_t i = 0; i < price.DV01().size(); ++i)
        REQUIRE(price.DV01()[i] == price.RawSensitivity()[i] * 1e-4, "DV01 must scale decimal quote risk once");
    REQUIRE(!rejected.Available() && !rejected.Diagnostics().empty(), "live quote payoff must retain a rejection diagnostic");
    REQUIRE(rejected.Diagnostics()[0].code_ == "ScriptCurveQuoteRiskLiveQuoteUnsupported", "unexpected live quote rejection");
    PrintDiagnostics(rejected);
}
