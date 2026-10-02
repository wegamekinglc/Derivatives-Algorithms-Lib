//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <dal/curve/calibration.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/model/gsrmarketcalibration.hpp>

#include "gsrperf.hpp"
#include "gsrslvcalibrationperf.hpp"

namespace {
    Dal::GSRCurveQuoteRisk_ Bridge() {
        using namespace Dal;
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2026, 10, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "rates";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.knotDates_ = {spec.today_, spec.today_.AddDays(1825)};
        spec.tolerance_ = 1e-10;
        RateIndexConvention_ index;
        index.forecastTenor_ = PeriodLength_("12M");
        index.dayBasis_ = DayBasis_("ACT_365F");
        index.businessDayConvention_ = BizDayConvention_("Unadjusted");
        index.fixingHolidays_ = index.accrualHolidays_ = Holidays::None();
        spec.instruments_ = {Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.today_, spec.knotDates_.back(), std::expm1(0.15) / 5.0, index))};
        CurveCalibrationOptions_ options;
        options.computeEffJacobianInverse_ = true;
        const auto fit = CalibrateYieldCurve(spec, options);
        RatePricingMarket_ market;
        market.valuationTime_ = DateTime_(spec.today_, 9, 0);
        market.resultCurrency_ = Ccy_("USD");
        market.curveComponents_["discount"] =
            Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(std::shared_ptr<void>(), fit.curve_.get()));
        RateQuoteRiskProvenanceConfig_ config;
        config.calibrationId_ = "curve-fit";
        config.componentKeyByParameterBlock_["rates"] = "discount";
        const GSRCurveData_ snapshot("curve", spec.today_, "USD", spec.knotDates_,
                                     {0.0, std::log((*fit.curve_)(spec.today_, spec.knotDates_.back()))}, {}, Matrix_<>(0, 0));
        return BuildGSRCurveQuoteRisk(snapshot, market, BuildSingleCurveQuoteRiskProvenance(spec, fit, options, market, config), "discount");
    }

    Dal::GSRSLVModelData_ Model(const Dal::GSRCurveData_& snapshot, bool grid, double level = 1.0) {
        using namespace Dal;
        const Handle_<GSRCurveData_> curve(new GSRCurveData_(snapshot.name_, snapshot.evaluationDate_, snapshot.currency_, snapshot.nodeDates_,
                                                             snapshot.discountLogDF_, snapshot.projectionTenors_, snapshot.projectionLogDF_));
        MultiFactorGSRVolSettings_ vol;
        vol.factorNames_ = {"level"};
        vol.gKnotDates_ = vol.hKnotDates_ = {curve->evaluationDate_};
        vol.gValues_ = Matrix_<>(1, 1, 0.02);
        vol.hValues_ = vol.correlations_ = Matrix_<>(1, 1, 1.0);
        const Handle_<MultiFactorGSRModelData_> gaussian(
            new MultiFactorGSRModelData_("rates", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
        const Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("leverage", grid ? Vector_<>{-0.02, 0.02} : Vector_<>{0.0},
                                                                      grid ? Vector_<>{0.0, 1.0} : Vector_<>{0.0},
                                                                      Matrix_<>(grid ? 2 : 1, grid ? 2 : 1, level)));
        GSRSLVSettings_ settings;
        settings.maxStep_ = 0.25;
        return GSRSLVModelData_("smile", gaussian, leverage, settings);
    }
} // namespace

void RunGSRMarketCalibrationCases() {
    using namespace Dal;
    const auto bridge = Bridge();
    const auto& snapshot = bridge.Snapshot();
    const auto today = snapshot.evaluationDate_;
    const auto initial = Model(snapshot, true), truth = Model(snapshot, true, 1.2);
    GSRSLVCalibrationSettings_ settings;
    settings.pricing_.paths_ = 1024;
    settings.validation_.paths_ = 2048;
    settings.solver_.priorWeight_ = 0.1;
    Vector_<GSRMarketQuote_> quotes;
    Vector_<GSREuropeanOption_> targets;
    for (int expiry : {1, 2})
        for (int tenor : {1, 2})
            for (double offset : {-0.005, 0.0, 0.005}) {
                const int days = tenor == 1 ? 182 : 365;
                const double accrual = days / DAYS_PER_YEAR;
                const auto exercise = today.AddDays(365 * expiry), end = exercise.AddDays(days);
                const double forward = std::expm1(0.03 * accrual) / accrual;
                const GSRCaplet_ option{exercise,         exercise,           end, end, accrual, accrual, String::FromInt(6 * tenor) + "M",
                                        forward + offset, OptionType_("CALL")};
                const double price = PriceGSRSLVEuropeanOptions(truth, {option}, settings.pricing_)[0].price_;
                const double annuity = accrual * std::exp(-0.03 * (expiry + accrual));
                quotes.push_back({"quote" + String::FromInt(quotes.size()), option,
                                  Distribution::BachelierIV(forward, option.strike_, option.type_, price / annuity) / std::sqrt(double(expiry)),
                                  0.005});
                targets.push_back(option);
            }
    const Vector_<GSRSLVCalibrationParameter_> parameters{
        {"leverage:0:0", 0.2, 2.0, 1.0}, {"leverage:0:1", 0.2, 2.0, 1.0}, {"leverage:1:0", 0.2, 2.0, 1.0}, {"leverage:1:1", 0.2, 2.0, 1.0}};
    double checksum = 0.0;
    for (bool aad : {false, true}) {
        settings.useAADJacobian_ = aad;
        Bench::Print(Bench::Run(
            std::string("GSR market four-node fit, 12 expiry/tenor/strike quotes, ") + (aad ? "AAD" : "FD"),
            [&] {
                const auto fit = CalibrateGSRSLVMarket(initial, quotes, parameters, settings);
                REQUIRE(fit.converged_ && fit.fitWithinTolerance_, "market benchmark fit failed");
                checksum += fit.parameters_[0];
            },
            1, 3));
    }
    const auto single = Model(snapshot, false);
    const auto exercise = today.AddDays(365), fixing = today.AddDays(540), start = fixing.AddDays(2), end = start.AddDays(365), pay = end.AddDays(2);
    const GSRSwaption_ lagged{exercise, {{pay, 1.0}}, {{fixing, start, end, pay, 1.0, 1.0, "12M"}}, 0.03, OptionType_("CALL")};
    Bench::Print(Bench::Run(
        "GSR lagged swaption, 512 outer x 32 inner paths",
        [&] { checksum += PriceGSRSLVEuropeanOptions(single, {lagged}, {512, 1729, 32})[0].price_; }, 1, 3));
    settings.useAADJacobian_ = false;
    quotes.Resize(1);
    targets.Resize(1);
    Bench::Print(Bench::Run(
        "GSR market vol + native curve quote risk, one fitted node",
        [&] {
            const auto risk = GSRSLVMarketQuoteRisk(single, quotes, {{"leverage:0:0", 0.2, 2.0, 1.0}}, targets, settings, {}, &bridge);
            REQUIRE(risk.curveRiskIncluded_, "market benchmark curve risk missing");
            checksum += risk.sensitivities_(0, 1);
        },
        1, 3));
    REQUIRE(std::isfinite(checksum), "invalid GSR market benchmark checksum");
    Bench::DoNotOptimize(&checksum);
}
