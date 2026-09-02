//
// Created by dal-implementer on 2026/9/2.
//

#include <iomanip>
#include <iostream>
#include <memory>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/quoteriskaggregation.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>
#include <dal/time/holidays.hpp>

using namespace Dal;

namespace {
    RateIndexConvention_ Index() {
        RateIndexConvention_ result;
        result.forecastTenor_ = PeriodLength_("6M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return result;
    }

    CurveCalibrationSpec_ CalibrationSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "usd_quote_risk";
        spec.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-10;
        spec.initialGuess_ = 0.02;
        spec.knotDates_ = {Date::AddMonths(spec.today_, 6), Date::AddMonths(spec.today_, 12), Date::AddMonths(spec.today_, 24)};

        const Handle_<DiscountCurve_> known(NewDiscountPWC("known_usd", spec.ccy_, PiecewiseConstant_(spec.knotDates_, {0.020, 0.023, 0.026})));
        const CurveBlock_ knownBlock(known, spec.liborBasis_);
        const RateIndexConvention_ index = Index();
        for (const auto& maturity : spec.knotDates_) {
            const Handle_<YCInstrument_> prototype(new Deposit_(spec.today_, spec.today_, maturity, 0.0, index));
            const double quote = (*prototype->Precompute(Handle_<YieldCurve_>()))(knownBlock);
            spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.today_, maturity, quote, index)));
        }
        return spec;
    }

    RateTradeDefinition_ Trade(const CurveCalibrationSpec_& spec) {
        DepositTradeTerms_ terms;
        terms.notional_ = 1'000'000.0;
        terms.contractRate_ = 0.022;
        terms.lend_ = true;
        terms.index_ = Index();
        terms.discountComponentKey_ = "discount";
        return {"deposit-1", RateInstrumentType_::Value_::DEPOSIT, spec.today_, spec.today_, spec.knotDates_.back(), Ccy_(spec.ccy_), terms};
    }
} // namespace

int main() {
    RegisterAll_::Init();
    const CurveCalibrationSpec_ spec = CalibrationSpec();
    CurveCalibrationOptions_ options;
    options.computeForwardJacobian_ = false;
    const auto calibration = std::make_shared<CurveCalibrationResult_>(CalibrateYieldCurve(spec, options));

    RatePricingMarket_ market;
    market.valuationTime_ = DateTime_(spec.today_, 9, 0);
    market.resultCurrency_ = Ccy_(spec.ccy_);
    market.curveComponents_["discount"] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(calibration, calibration->curve_.get()));
    market.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());

    RateQuoteRiskProvenanceConfig_ config;
    config.calibrationId_ = "usd-ois";
    config.componentKeyByParameterBlock_[spec.curveName_] = "discount";
    const auto provenance = BuildSingleCurveQuoteRiskProvenance(spec, *calibration, options, market, config);
    const auto risk = AggregateRatePortfolioQuoteRisk({Trade(spec)}, market, {provenance});
    REQUIRE(provenance.Available(), provenance.Reason());
    REQUIRE(risk.policy_ == "UnconvertedByActualPvCcy", "Unexpected quote-risk currency policy");

    std::cout << "axis scheme: " << provenance.Axis().scheme_ << '\n';
    std::cout << "axis fingerprint: " << provenance.Axis().fingerprint_ << '\n';
    std::cout << "state scheme: " << provenance.State().scheme_ << '\n';
    std::cout << "state fingerprint: " << provenance.State().fingerprint_ << '\n';
    std::cout << "policy: " << risk.policy_ << '\n';
    std::cout << "quote_key,currency,dPV/dDecimalQuote,DV01\n";
    std::cout << std::setprecision(12);
    for (const auto& bucket : risk.buckets_)
        std::cout << bucket.quoteKey_ << ',' << bucket.actualPvCcy_.String() << ',' << bucket.dPvDDecimalQuote_ << ',' << bucket.dv01_ << '\n';
    return 0;
}
