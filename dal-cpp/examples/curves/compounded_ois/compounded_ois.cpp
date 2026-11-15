#include <iomanip>
#include <iostream>

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
} // namespace

int main() {
    RegisterAll_::Init();
    std::cout << std::setprecision(12);
    for (const auto& mode : CompoundedOisObservationModeListAll())
        Run(mode);
    std::cout << "Compounded OIS direct evaluation verified; native partials are not quote DV01.\n";
}
