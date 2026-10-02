//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <dal/curve/calibration.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/model/gsrmarketcalibration.hpp>
#include <dal/model/gsrslvpricinginternal.hpp>

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

    Dal::GSRSLVModelData_ Model(const Dal::GSRCurveData_& snapshot, int rows, int cols, double level = 1.0) {
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
        Vector_<> shifts(rows, 0.0), times(cols, 0.0);
        for (int row = 0; row < rows && rows > 1; ++row)
            shifts[row] = -0.02 + 0.04 * row / (rows - 1);
        for (int col = 0; col < cols && cols > 1; ++col)
            times[col] = static_cast<double>(col) / (cols - 1);
        const Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("leverage", shifts, times, Matrix_<>(rows, cols, level)));
        GSRSLVSettings_ settings;
        settings.maxStep_ = 0.25;
        return GSRSLVModelData_("smile", gaussian, leverage, settings);
    }

    void RunFits(const Dal::GSRCurveData_& snapshot,
                 const Dal::Vector_<Dal::GSRMarketQuote_>& quotes,
                 Dal::GSRSLVCalibrationSettings_ settings,
                 int rows,
                 int cols,
                 double* checksum) {
        using namespace Dal;
        const auto initial = Model(snapshot, rows, cols);
        Vector_<GSRSLVCalibrationParameter_> parameters;
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
                parameters.push_back({"leverage:" + String::FromInt(row) + ":" + String::FromInt(col), 0.2, 2.0, 1.0});
        for (bool aad : {false, true}) {
            settings.useAADJacobian_ = aad;
            Bench::Print(Bench::Run(
                "GSR market " + std::to_string(rows * cols) + "-node fit, 12 expiry/tenor/strike quotes, " + (aad ? "AAD" : "FD"),
                [&] {
                    const auto fit = CalibrateGSRSLVMarket(initial, quotes, parameters, settings);
                    REQUIRE(fit.converged_ && fit.fitWithinTolerance_, "market benchmark fit failed: " + fit.terminationReason_);
                    *checksum += fit.parameters_[0];
                },
                1, 3));
        }
    }

    Dal::GSRSLVModelData_ Bump(const Dal::GSRSLVModelData_& data, int row, int col, double change) {
        using namespace Dal;
        auto values = data.leverage_->values_;
        values(row, col) += change;
        const Handle_<GSRLeverageData_> leverage(new GSRLeverageData_("bump", data.leverage_->rateShifts_, data.leverage_->times_, values));
        return GSRSLVModelData_("bump", data.gaussian_, leverage, {data.kappa_, data.volOfVol_, data.varianceCorrelations_, data.maxStep_});
    }

    Dal::Matrix_<> DifferenceJacobian(const Dal::GSRSLVPricingInternal::PreparedPricer_& pricer, const Dal::GSRSLVModelData_& data, size_t quotes) {
        using namespace Dal;
        constexpr double STEP = 1e-5;
        const auto& nodes = data.leverage_->values_;
        Matrix_<> result(quotes, nodes.Rows() * nodes.Cols());
        for (int row = 0; row < nodes.Rows(); ++row)
            for (int col = 0; col < nodes.Cols(); ++col) {
                const auto high = pricer.Price(Bump(data, row, col, STEP)), low = pricer.Price(Bump(data, row, col, -STEP));
                for (size_t quote = 0; quote < quotes; ++quote)
                    result(quote, row * nodes.Cols() + col) = (high[quote].price_ - low[quote].price_) / (2.0 * STEP);
            }
        return result;
    }

    void RunJacobians(const Dal::GSRCurveData_& snapshot,
                      const Dal::Vector_<Dal::GSREuropeanOption_>& targets,
                      const Dal::GSRMonteCarloSettings_& settings,
                      double* checksum) {
        using namespace Dal;
        const auto data = Model(snapshot, 12, 4);
        const GSRSLVPricingInternal::PreparedPricer_ pricer(data, targets, settings);
        Vector_<String_> labels;
        for (int row = 0; row < 12; ++row)
            for (int col = 0; col < 4; ++col)
                labels.push_back("leverage:" + String::FromInt(row) + ":" + String::FromInt(col));
        const auto fd = DifferenceJacobian(pricer, data, targets.size()), aad = pricer.Jacobian(data, labels);
        for (int row = 0; row < fd.Rows(); ++row)
            for (int col = 0; col < fd.Cols(); ++col)
                REQUIRE(std::abs(fd(row, col) - aad(row, col)) < 2e-7, "market benchmark Jacobians disagree");
        for (bool useAAD : {false, true})
            Bench::Print(Bench::Run(
                std::string("GSR market 48-node price Jacobian, 12 quotes, ") + (useAAD ? "AAD" : "FD"),
                [&] {
                    const auto jacobian = useAAD ? pricer.Jacobian(data, labels) : DifferenceJacobian(pricer, data, targets.size());
                    *checksum += jacobian(0, 0);
                },
                1, 3));
    }
} // namespace

void RunGSRMarketCalibrationCases() {
    using namespace Dal;
    const auto bridge = Bridge();
    const auto& snapshot = bridge.Snapshot();
    const auto today = snapshot.evaluationDate_;
    const auto truth = Model(snapshot, 2, 2, 1.2);
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
    double checksum = 0.0;
    RunFits(snapshot, quotes, settings, 2, 2, &checksum);
    RunJacobians(snapshot, targets, settings.pricing_, &checksum);
    const auto single = Model(snapshot, 1, 1);
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
