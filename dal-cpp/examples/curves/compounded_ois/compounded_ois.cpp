#include <iomanip>
#include <iostream>

#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/curvescriptevaluation.hpp>
#include <dal/platform/initall.hpp>

#include "../../aadreport.hpp"

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ Contract(const CompoundedOisObservationMode_& mode) {
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = "observation-example";
        s.direction_ = "PayFixed";
        s.notional_ = 1'000'000.;
        s.coupon_ = .025;
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.observationBasis_ = DayBasis_("ACT_360");
        s.fixedBasis_ = DayBasis_("ACT_365F");
        s.calendar_ = HolidayCenterData_("explicit-Mar4", {Date_(2024, 3, 4)});
        s.ruleId_ = "dal.compounded-ois-cashflows/2";
        const bool shifted =
            mode == CompoundedOisObservationMode_::Value_::Lookback || mode == CompoundedOisObservationMode_::Value_::ObservationShift;
        s.observationConvention_ =
            CompoundedOisObservationConvention_{mode, shifted ? 1 : 0, mode == CompoundedOisObservationMode_::Value_::Lockout ? 1 : 0};
        s.fixedLeg_ = {{Date_(2024, 3, 5), Date_(2024, 3, 8), Date_(2024, 3, 8), 3. / 365.}};
        s.coupons_ = {{"coupon", Date_(2024, 3, 5), Date_(2024, 3, 8), Date_(2024, 3, 8)}};
        return s;
    }
    CurveScriptEvaluationRequest_ Request(const CompoundedOisScriptSpec_& s) {
        CurveScriptEvaluationRequest_ r;
        r.geometry_.today_ = Date_(2024, 3, 1);
        r.geometry_.ccy_ = "USD";
        r.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        r.geometry_.knotDates_ = {Date_(2024, 3, 1), Date_(2024, 3, 5), Date_(2024, 3, 20)};
        r.geometry_.calibrateDiscountCurve_ = false;
        r.geometry_.targetOvernightRoute_ = s.projectionRoute_;
        r.market_.routes_ = {{s.discountRoute_, false, CollateralType_("OIS"), {}, false, true},
                             {s.projectionRoute_, true, CollateralType_("OIS"), {}, true}};
        r.valuation_.evaluationDate_ = r.geometry_.today_;
        r.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        r.parameters_ = {-.0003, -.002};
        r.derivativeMethod_ = CalibrationDerivative_("AAD");
        r.compoundedOis_ = PrepareCompoundedOis(s, r.valuation_);
        return r;
    }
    void Run(const CompoundedOisObservationMode_& mode) {
        const auto s = Contract(mode);
        auto r = Request(s);
        const auto instrument = NewCompoundedOisScriptInstrument(r.compoundedOis_);
        const auto result = EvaluateCurveScriptInstrument(instrument, r);
        std::cout << '\n' << mode.String() << ": Price=" << result.values_[0] << " USD, ParRate=" << result.values_[1] << " decimal\n";
        std::cout << "First row actual / rate / compounding accrual: " << result.observationSchedule_[0].accrual_ << " / "
                  << result.observationSchedule_[0].rateAccrual_ << " / " << result.observationSchedule_[0].compoundingAccrual_ << '\n';
        Examples::PrintReferenceHeader("Price native log-discount partials (USD per unit native parameter)");
        r.derivativeMethod_ = CalibrationDerivative_("None");
        for (int j = 0; j < 2; ++j) {
            auto up = r, down = r;
            up.parameters_[j] += 1e-6;
            down.parameters_[j] -= 1e-6;
            const auto plus = EvaluateCurveScriptInstrument(instrument, up), minus = EvaluateCurveScriptInstrument(instrument, down);
            const double fd = (plus.values_[0] - minus.values_[0]) / 2e-6;
            Examples::Compare(("node " + std::to_string(j)).c_str(), result.nativePartials_(0, j), fd, .001);
        }
    }
    void Calibrate(const CompoundedOisObservationMode_& mode) {
        const auto pvContract = Contract(mode);
        auto request = Request(pvContract);
        request.valuation_.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 2, 29)), -.012}}}}));
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_ = request.geometry_;
        spec.market_ = request.market_;
        spec.valuation_ = request.valuation_;
        for (const bool shortMaturity : {true, false}) {
            auto contract = pvContract;
            contract.instrumentId_ = shortMaturity ? "ois-short" : "ois-long";
            const Date_ start(2024, 3, 1), end(2024, 3, shortMaturity ? 6 : 8);
            contract.fixedLeg_[0] = {start, end, end, (end - start) / 365.};
            contract.coupons_[0] = {"coupon", start, end, end};
            request.compoundedOis_ = PrepareCompoundedOis(contract, spec.valuation_);
            const auto instrument = NewCompoundedOisScriptInstrument(request.compoundedOis_);
            const double quote = EvaluateCurveScriptInstrument(instrument, request).values_[1];
            spec.compoundedOis_.push_back(request.compoundedOis_);
            spec.observations_.push_back(
                NewCalibrationObservation(contract.instrumentId_, instrument, "ParRate",
                                          NewCalibrationMarketQuote(contract.instrumentId_, quote, CalibrationValueUnit_::Parse("Rate")), 1e-12));
        }
        request.compoundedOis_ = PrepareCompoundedOis(pvContract, spec.valuation_);
        spec.compoundedOis_.push_back(request.compoundedOis_);
        spec.solver_.initialGuessPerNode_ = request.parameters_ * .95;
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_("AAD");
        const auto fit = CalibrateCurveInstruments(spec, options);
        REQUIRE(fit.fitWithinTolerance_, "same-source OIS calibration must fit");
        Examples::PrintReferenceHeader("Calibration recovers the native parameters used to generate these illustrative quotes");
        for (int j = 0; j < 2; ++j)
            Examples::Compare(("calibrated node " + std::to_string(j)).c_str(), fit.parameters_[j], request.parameters_[j], 1e-8);
        const PreparedCurveInstruments_ prepared(*fit.resolvedSpec_, options);
        const auto context = CurveInstrumentInternal::PrepareCurvePriceContext(prepared);
        CurveInstrumentInternal::RequireCurvePriceContext(context, *fit.resolvedSpec_);
        const auto instrument = NewCompoundedOisScriptInstrument(request.compoundedOis_);
        const auto price = CurveInstrumentInternal::PrepareCurvePrice(context, instrument, "Price");
        const double value =
            CurveScriptInternal::Measure(price.plan_, CandidateCurveProvider_<double>(price.geometry_, fit.parameters_), spec.geometry_.today_);
        request.parameters_ = fit.parameters_;
        Examples::Compare("registered PV (USD)", value, EvaluateCurveScriptInstrument(instrument, request).values_[0], 1e-6);
    }
} // namespace

int main() {
    RegisterAll_::Init();
    std::cout << std::setprecision(12);
    for (const auto& mode : CompoundedOisObservationModeListAll()) {
        Run(mode);
        Calibrate(mode);
    }
    std::cout << "Compounded OIS direct evaluation verified; native partials are not quote DV01.\n";
    std::cout << "Same-source OIS calibration verified; OIS quote risk remains unavailable.\n";
}
