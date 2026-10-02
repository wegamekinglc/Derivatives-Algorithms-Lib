//
// Created by Codex on 2026/10/3.
//

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <dal/platform/platform.hpp>

#include <dal/math/matrix/matrixutils.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/globals.hpp>
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curveprotocol.hpp>
#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/gsr.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

#include "../floatformat.hpp"

using namespace Dal;

namespace {
    const Date_ TODAY(2026, 10, 3);
    constexpr double GAUSSIAN_SCALE = 0.0001, PRICE_SCALE = 0.0005;
    const std::array<std::pair<int, std::string>, 5> VOL_EXPIRIES{{{365, "1Y"}, {547, "18M"}, {730, "2Y"}, {912, "30M"}, {1095, "3Y"}}};
    const std::array<std::pair<int, String_>, 3> CAPLET_TENORS{{{91, "3M"}, {182, "6M"}, {365, "12M"}}};
    const std::array<std::array<double, 3>, 5> SMILE_VOLS{{{{0.01120, 0.01050, 0.01005}},
                                                           {{0.01155, 0.01105, 0.01075}},
                                                           {{0.01175, 0.01140, 0.01105}},
                                                           {{0.01215, 0.01175, 0.01140}},
                                                           {{0.01250, 0.01210, 0.01175}}}};
    using Rows_ = std::vector<std::vector<std::string>>;

    std::string Text(const String_& value) { return {value.begin(), value.end()}; }

    std::string DiagnosticValue(double value) { return ExampleFloat(std::abs(value) < 1e-12 ? 0.0 : value); }

    std::string Status(bool passed) { return passed ? "true" : "false"; }

    std::string Precise(double value) {
        std::ostringstream result;
        result << std::setprecision(17) << value;
        return result.str();
    }

    void PrintTable(const std::string& title, const std::vector<std::string>& headers, const Rows_& rows) {
        std::vector<size_t> widths;
        for (size_t col = 0; col < headers.size(); ++col) {
            size_t width = headers[col].size();
            for (const auto& row : rows)
                width = std::max(width, row[col].size());
            widths.push_back(width + 2);
        }
        const size_t total = std::accumulate(widths.begin(), widths.end(), size_t(0));
        const std::string separator(total, '-'), heading(std::max({size_t(70), total, title.size()}), '=');
        std::cout << '\n' << heading << '\n' << title << '\n' << heading << '\n';
        const auto print = [&](const auto& row) {
            for (size_t col = 0; col < row.size(); ++col)
                std::cout << (col ? std::right : std::left) << std::setw(widths[col]) << row[col];
            std::cout << '\n';
        };
        print(headers);
        std::cout << separator << '\n';
        for (const auto& row : rows)
            print(row);
        std::cout << separator << '\n';
    }

    struct CurveInputs_ {
        Handle_<DiscountCurve_> discount_;
        Handle_<GSRCurveData_> snapshot_;
    };

    CurveInputs_ CalibrateCurve() {
        const std::array<int, 9> days{{30, 90, 180, 365, 730, 1095, 1825, 2555, 3650}};
        const std::array<double, 9> rates{{0.0270, 0.0280, 0.0290, 0.0302, 0.0310, 0.0315, 0.0322, 0.0325, 0.0328}};
        const auto fixed = RateLegConvention_New(PeriodLength_("12M"), DayBasis_("ACT_365F"));
        const auto floating = RateLegConvention_New(PeriodLength_("12M"), DayBasis_("ACT_360"));
        const auto index = RateIndexConvention_New(PeriodLength_("1M"), DayBasis_("ACT_360"), CollateralType_OIS());
        CurveCalibrationSpecBuilder_ builder;
        builder.today_ = TODAY;
        builder.ccy_ = "USD";
        builder.curveName_ = "calibrated_ois";
        for (size_t i = 0; i < days.size(); ++i) {
            const auto date = TODAY.AddDays(days[i]);
            builder.knotDates_.push_back(date);
            builder.instruments_.push_back(i < 3 ? DepositNew(TODAY, TODAY, date, rates[i], index)
                                                 : OISSwapNew(TODAY, TODAY, date, rates[i], fixed, index, floating));
        }
        const auto spec = builder.Build();
        REQUIRE(ValidateSingleCurveAnalyticEligibility(spec).eligible_, "Yield curve instruments must support the AAD Jacobian");
        const auto fit = CalibrateSingleCurve(spec, CurveJacobianMode_(CurveJacobianMode_::Value_::ANALYTIC));
        const auto& diagnostics = fit.diagnostics_;
        REQUIRE(diagnostics.maxAbsResidual_ <= builder.fitTolerance_, "Yield curve calibration exceeded the quote tolerance");
        Rows_ rows;
        for (size_t i = 0; i < days.size(); ++i)
            rows.push_back({std::string(i < 3 ? "Deposit " : "OIS ") + Text(Date::ToString(builder.knotDates_[i])),
                            ExampleFloat(diagnostics.marketRates_[i] * 100), ExampleFloat(diagnostics.modelRates_[i] * 100),
                            ExampleFloat(diagnostics.residuals_[i] * 10000), ExampleFloat((*fit.curve_)(TODAY, builder.knotDates_[i]))});
        PrintTable("Yield curve calibration: deposits and OIS; AAD Jacobian", {"Instrument", "Market(%)", "Model(%)", "Error(bp)", "Fitted DF"},
                   rows);
        std::set<Date_> nodes(builder.knotDates_.begin(), builder.knotDates_.end());
        nodes.insert(TODAY);
        for (const auto& expiry : VOL_EXPIRIES) {
            const auto date = TODAY.AddDays(expiry.first);
            nodes.insert(date);
            for (const auto& tenor : CAPLET_TENORS)
                nodes.insert(date.AddDays(tenor.first));
        }
        const auto source = CurveBlockNew(fit.curve_);
        const Vector_<Date_> dates(nodes.begin(), nodes.end());
        return {fit.curve_, NewGSRCurveDataFromYieldCurve("gsr_curve", *source, TODAY, dates, {})};
    }

    double Forward(const DiscountCurve_& curve, const Date_& expiry, int days) {
        return (curve(TODAY, expiry) / curve(TODAY, expiry.AddDays(days)) - 1.0) / (days / 365.0);
    }

    GSRCaplet_ Caplet(const DiscountCurve_& curve, int expiryDays, double offset = 0.0, int days = 182, const String_& tenor = "6M") {
        const auto expiry = TODAY.AddDays(expiryDays), end = expiry.AddDays(days);
        const double accrual = days / 365.0;
        return {expiry, expiry, end, end, accrual, accrual, tenor, Forward(curve, expiry, days) + offset, OptionType_("CALL")};
    }

    Handle_<ModelData_> GaussianModel(const Handle_<GSRCurveData_>& curve) {
        MultiFactorGSRVolSettings_ settings;
        settings.factorNames_ = {"level", "slope", "curvature"};
        settings.gKnotDates_ = {TODAY, TODAY.AddDays(365), TODAY.AddDays(730)};
        settings.gValues_ = Matrix::FromVectors<double>({{0.009, 0.009, 0.009}, {0.003, 0.003, 0.003}, {0.002, 0.002, 0.002}});
        settings.hKnotDates_ = {TODAY, TODAY.AddDays(365), TODAY.AddDays(1095)};
        settings.hValues_ = Matrix::FromVectors<double>({{1.0, 1.0, 1.0}, {0.4, 0.3, 0.2}, {0.1, 0.2, 0.1}});
        settings.correlations_ = Matrix::FromVectors<double>({{1.0, 0.25, 0.1}, {0.25, 1.0, 0.15}, {0.1, 0.15, 1.0}});
        return NewMultiFactorGSRModelData("gaussian", curve, NewMultiFactorGSRVolData("gaussian_vol", settings));
    }

    template <class F_> void ReportDiagnostics(const std::string& title, const F_& fit, const std::optional<bool>& heldOut = std::nullopt) {
        Rows_ rows{{"Converged", Status(fit.converged_)},
                   {"Price fit passed", Status(fit.fitWithinTolerance_)},
                   {"Numerical validation passed", Status(fit.numericalValidationPassed_)},
                   {"Jacobian rank", std::to_string(fit.jacobianRank_)},
                   {"Iterations", std::to_string(fit.iterations_)}};
        if (heldOut)
            rows.push_back({"Held-out passed", Status(*heldOut)});
        PrintTable(title, {"Diagnostic", "Result"}, rows);
        REQUIRE(fit.converged_ && fit.fitWithinTolerance_ && fit.numericalValidationPassed_,
                "Calibration diagnostics failed: " + fit.terminationReason_);
        REQUIRE(!heldOut || *heldOut, "Held-out quote validation failed");
    }

    Handle_<ModelData_> CalibrateGaussian(const CurveInputs_& curve) {
        Vector_<GSRMarketQuote_> market;
        for (size_t i = 0; i < VOL_EXPIRIES.size(); ++i)
            for (const auto& tenor : CAPLET_TENORS)
                market.push_back({String_(VOL_EXPIRIES[i].second) + " " + tenor.second + " ATM",
                                  Caplet(*curve.discount_, VOL_EXPIRIES[i].first, 0.0, tenor.first, tenor.second), SMILE_VOLS[i][1], GAUSSIAN_SCALE});
        const auto prices = ConvertGSRMarketQuotes(Handle_<Storable_>(curve.snapshot_), market);
        Vector_<GSRCalibrationQuote_> quotes;
        for (size_t i = 0; i < market.size(); ++i)
            quotes.push_back({market[i].name_, market[i].option_, prices[i].price_, GAUSSIAN_SCALE});
        const auto fit =
            CalibrateGSRVolatility(GaussianModel(curve.snapshot_), quotes, {{0, 0, 0.001, 0.04}, {0, 1, 0.001, 0.04}, {0, 2, 0.001, 0.04}});
        Rows_ rows;
        for (size_t i = 0; i < market.size(); ++i)
            rows.push_back({Text(market[i].name_), ExampleFloat(market[i].volatility_ * 10000), ExampleFloat(prices[i].price_),
                            ExampleFloat(fit.modelPrices_[i]), ExampleFloat(fit.residuals_[i] / GAUSSIAN_SCALE)});
        PrintTable("Gaussian volatility calibration: 15 ATM caplets", {"Instrument", "Normal(bp)", "Market PV", "Fitted PV", "Error/scale"}, rows);
        rows.clear();
        for (size_t i = 0; i < fit.parameters_.size(); ++i)
            rows.push_back({"g:level:" + Text(Date::ToString(TODAY.AddDays(365 * i))), ExampleFloat(0.009), ExampleFloat(fit.parameters_[i])});
        PrintTable("Calibrated Gaussian parameters; slope, curvature, H and correlations fixed", {"Parameter", "Initial", "Fitted"}, rows);
        ReportDiagnostics("Gaussian calibration diagnostics", fit);
        return Handle_<ModelData_>(fit.model_);
    }

    Handle_<ModelData_> CalibrateSLV(const CurveInputs_& curve, const Handle_<ModelData_>& gaussian) {
        GSRSLVSettings_ modelSettings;
        modelSettings.kappa_ = 1.0;
        modelSettings.volOfVol_ = 0.5;
        modelSettings.varianceCorrelations_ = {-0.2, 0.0, 0.0};
        modelSettings.maxStep_ = 1.0 / 12.0;
        const auto leverage = NewGSRLeverageData("leverage", {-0.02, 0.0, 0.02}, {0.0}, Matrix_<>(3, 1, 1.0));
        const auto initial = NewGSRSLVModelData("initial_slv", gaussian, leverage, modelSettings);
        GSRSLVCalibrationSettings_ settings;
        settings.pricing_ = {16384, 1729};
        settings.validation_ = {32768, 81173};
        settings.solver_.priorWeight_ = 0.01;
        settings.solver_.smoothingWeight_ = 0.0001;
        const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0}, {"leverage:1:0", 0.2, 2.0}, {"leverage:2:0", 0.2, 2.0}};
        const std::array<double, 3> offsets{{-0.005, 0.0, 0.005}};
        Vector_<GSRMarketQuote_> quotes;
        for (size_t i = 0; i < VOL_EXPIRIES.size(); ++i)
            for (size_t j = 0; j < offsets.size(); ++j) {
                std::ostringstream name;
                name << VOL_EXPIRIES[i].second << ' ' << std::showpos << std::fixed << std::setprecision(3) << offsets[j];
                quotes.push_back({String_(name.str()), Caplet(*curve.discount_, VOL_EXPIRIES[i].first, offsets[j]), SMILE_VOLS[i][j], PRICE_SCALE});
            }
        const Vector_<GSRMarketQuote_> heldOut{{"2Y +0.0025 held-out", Caplet(*curve.discount_, 730, 0.0025), 0.01120, PRICE_SCALE}};
        const auto fit = CalibrateGSRSLVMarket(initial, quotes, parameters, settings, heldOut);
        const auto prices = ConvertGSRMarketQuotes(Handle_<Storable_>(curve.snapshot_), quotes);
        Rows_ rows;
        for (size_t i = 0; i < quotes.size(); ++i)
            rows.push_back({Text(quotes[i].name_), ExampleFloat(quotes[i].volatility_ * 10000), ExampleFloat(prices[i].price_),
                            ExampleFloat(fit.modelPrices_[i]), ExampleFloat(fit.residuals_[i] / PRICE_SCALE), ExampleFloat(fit.standardErrors_[i])});
        PrintTable("SLV volatility calibration: 15 smile caplets", {"Instrument", "Normal(bp)", "Market PV", "Fitted PV", "Error/scale", "Pair SE"},
                   rows);
        rows.clear();
        for (size_t i = 0; i < fit.parameters_.size(); ++i)
            rows.push_back({"leverage:" + std::to_string(i) + ":0", ExampleFloat(1.0), ExampleFloat(fit.parameters_[i])});
        PrintTable("Calibrated SLV parameters; Gaussian, CIR and correlations fixed", {"Parameter", "Initial", "Fitted"}, rows);
        ReportDiagnostics("SLV calibration diagnostics", fit, fit.heldOutWithinTolerance_);
        const double heldOutError =
            std::abs(fit.heldOutResiduals_[0]) + settings.validationSigma_ * fit.heldOutStandardErrors_[0] + fit.heldOutConditionalErrors_[0];
        PrintTable("Independent validation", {"Result", "PV", "Error estimate", "PV budget"},
                   {{"Max numerical error", "N/A", ExampleFloat(*std::max_element(fit.numericalErrors_.begin(), fit.numericalErrors_.end())),
                     ExampleFloat(settings.solver_.numericalErrorFraction_ * PRICE_SCALE)},
                    {"Held-out 2Y +0.0025", ExampleFloat(fit.heldOutPrices_[0]), ExampleFloat(heldOutError), ExampleFloat(PRICE_SCALE)}});
        return Handle_<ModelData_>(fit.model_);
    }

    struct ScriptValues_ {
        double plain_;
        std::map<std::string, double> adjoint_;
    };

    ScriptValues_
    ValueScript(const Handle_<ScriptProductData_>& product, const Handle_<ModelData_>& model, const GSRMonteCarloPrice_& reference, int paths) {
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = TODAY;
        MonteCarloSettings_ execution;
        execution.compiled_ = true;
        execution.useBb_ = true;
        execution.smooth_ = 1e-8;
        const auto plain = ValueByMonteCarlo(product, model, paths, valuation, execution);
        execution.enableAad_ = true;
        const auto adjoint = ValueByMonteCarlo(product, model, paths, valuation, execution);
        REQUIRE(std::abs(adjoint.at("PV") - plain.at("PV")) <= 1e-10, "AAD and plain PV differ");
        REQUIRE(std::abs(adjoint.at("PV") - reference.price_) <= 5 * reference.standardError_ + 0.0001, "Script and European pricer differ");
        std::map<std::string, double> risks;
        for (const auto& entry : adjoint) {
            REQUIRE(std::isfinite(entry.second), "Non-finite AAD result");
            risks.emplace(Text(entry.first), entry.second);
        }
        return {plain.at("PV"), std::move(risks)};
    }

    void ReportRisks(const std::array<std::map<std::string, double>, 2>& risks, const Date_& expiry) {
        Rows_ rows;
        for (const auto& entry : risks[0])
            if (entry.first != "PV" && std::max(std::abs(entry.second), std::abs(risks[1].at(entry.first))) >= 1e-12) {
                rows.push_back({entry.first, DiagnosticValue(entry.second), DiagnosticValue(risks[1].at(entry.first))});
            }
        std::vector<std::string> required{"d_logdf:OIS:" + Text(Date::ToString(expiry)), "d_kappa", "d_volOfVol", "d_leverage:1:0", "d_STRIKE"};
        for (const auto& factor : {"level", "slope", "curvature"})
            required.push_back(std::string("d_g:") + factor + ":" + Text(Date::ToString(TODAY)));
        for (const auto& key : required)
            REQUIRE(risks[0].count(key) && std::abs(risks[0].at(key)) >= 1e-12, "Expected model-input AAD risk is missing: " + String_(key));
        PrintTable("AAD input derivatives; calibration held fixed; active inputs", {"Risk", "1Y ATM caplet", "1Y into 2Y payer"}, rows);
    }

    void PriceProducts(const DiscountCurve_& discount, const Handle_<ModelData_>& model) {
        const auto expiry = TODAY.AddDays(365), first = expiry.AddDays(365), end = first.AddDays(365);
        const double strike = (discount(TODAY, expiry) - discount(TODAY, end)) / (discount(TODAY, first) + discount(TODAY, end));
        const GSRSwaption_ swaption{expiry,
                                    {{first, 1.0}, {end, 1.0}},
                                    {{expiry, expiry, first, first, 1.0, 1.0, "12M"}, {first, first, end, end, 1.0, 1.0, "12M"}},
                                    strike,
                                    OptionType_("CALL")};
        const GSRMonteCarloSettings_ settings{16384, 27183};
        const auto caplet = Caplet(discount, 365);
        const auto prices = PriceGSRSLVEuropeanOptions(model, {caplet, swaption}, settings);
        const auto capletScript = NewScriptProduct(
            "caplet", {Cell_("STRIKE"), Cell_(expiry)},
            {String_(Precise(caplet.strike_)), String_("pay PAYS MAX(1 - (1 + " + Precise(182.0 / 365.0) + " * STRIKE) * FIX(IR[USD,DF," +
                                                       Text(Date::ToString(expiry.AddDays(182))) + "]), 0)")});
        const auto swaptionScript = NewScriptProduct(
            "swaption", {Cell_("STRIKE"), Cell_(expiry)},
            {String_(Precise(strike)), String_("pay PAYS MAX(1 - FIX(IR[USD,DF," + Text(Date::ToString(end)) + "]) - STRIKE * (FIX(IR[USD,DF," +
                                               Text(Date::ToString(first)) + "]) + FIX(IR[USD,DF," + Text(Date::ToString(end)) + "])), 0)")});
        const std::array<std::string, 2> names{{"1Y ATM caplet", "1Y into 2Y ATM payer"}};
        const std::array<Handle_<ScriptProductData_>, 2> products{{capletScript, swaptionScript}};
        std::array<std::map<std::string, double>, 2> risks;
        Rows_ rows;
        for (size_t i = 0; i < products.size(); ++i) {
            const auto values = ValueScript(products[i], model, prices[i], settings.paths_);
            const double adjoint = values.adjoint_.at("PV");
            risks[i] = values.adjoint_;
            rows.push_back({names[i], ExampleFloat(prices[i].price_), ExampleFloat(prices[i].standardError_), ExampleFloat(values.plain_),
                            ExampleFloat(adjoint), DiagnosticValue(adjoint - values.plain_)});
        }
        PrintTable("Product values: unit notional; identical contracts", {"Product", "MRG32 PV", "Pair SE", "Sobol PV", "AAD PV", "AAD-plain"}, rows);
        ReportRisks(risks, expiry);
    }
} // namespace

int main() {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(TODAY);
    std::cout << "Three-factor GSR + SLV: calibrated yield curve, illustrative Normal volatility quotes\n";
    const auto curve = CalibrateCurve();
    const auto gaussian = CalibrateGaussian(curve);
    const auto smile = CalibrateSLV(curve, gaussian);
    PriceProducts(*curve.discount_, smile);
    return 0;
}
