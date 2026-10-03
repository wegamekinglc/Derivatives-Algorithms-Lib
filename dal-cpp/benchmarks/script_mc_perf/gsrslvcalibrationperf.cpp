//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <cmath>

#include <dal/model/gsrslvcalibration.hpp>

#include "gsrperf.hpp"
#include "gsrslvcalibrationperf.hpp"

namespace {
    Dal::Handle_<Dal::GSRSLVModelData_> Model(int factors, double leverage) {
        using namespace Dal;
        const auto curve = GSRBenchmarkCurve();
        MultiFactorGSRVolSettings_ vol;
        vol.gKnotDates_ = vol.hKnotDates_ = {curve->evaluationDate_};
        vol.gValues_ = Matrix_<>(factors, 1);
        vol.hValues_ = Matrix_<>(factors, 1);
        vol.correlations_ = Matrix_<>(factors, factors, 0.2);
        for (int i = 0; i < factors; ++i) {
            vol.factorNames_.push_back("factor" + String::FromInt(i));
            vol.gValues_(i, 0) = 0.02 / (i + 1);
            vol.hValues_(i, 0) = 1.0 / (i + 1);
            vol.correlations_(i, i) = 1.0;
        }
        const Handle_<MultiFactorGSRModelData_> gaussian(
            new MultiFactorGSRModelData_("rates", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
        return Handle_<GSRSLVModelData_>(new GSRSLVModelData_(
            "smile", gaussian, Handle_<GSRLeverageData_>(new GSRLeverageData_("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, leverage)))));
    }
} // namespace

void RunGSRSLVCalibrationCases() {
    using namespace Dal;
    const Date_ today(2026, 10, 2), expiry = today.AddDays(365), maturity = today.AddDays(730);
    const Vector_<EuropeanRateOption_> options{BondOption_{expiry, maturity, 0.94, OptionType_("CALL")},
                                               BondOption_{expiry, maturity, 0.97, OptionType_("CALL")},
                                               BondOption_{expiry, maturity, 1.0, OptionType_("CALL")}};
    double checksum = 0.0;
    for (int factors : {1, 2, 3}) {
        const auto truth = Model(factors, 1.2), initial = Model(factors, 0.8);
        GSRSLVCalibrationSettings_ settings;
        settings.pricing_ = {8192, 1729};
        settings.validation_ = {16384, 81173};
        const auto prices = PriceGSRSLVEuropeanOptions(*truth, options, settings.pricing_);
        Vector_<CalibrationQuote_> quotes;
        for (size_t i = 0; i < prices.size(); ++i)
            quotes.push_back({"bond" + String::FromInt(i), options[i], prices[i].price_, 0.003});
        const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0, 1.0}};
        const std::string prefix = "GSR-SLV " + std::to_string(factors) + "F ";
        Bench::Print(Bench::Run(
            prefix + "3 bond strikes (100000 paths, maxStep=1/52)",
            [&] {
                const auto result = PriceGSRSLVEuropeanOptions(*truth, options, {100000, 1729});
                checksum += result[0].price_;
            },
            1, 3));
        Bench::Print(Bench::Run(
            prefix + "leverage fit (3 quotes, 8192 fit + 16384 validation paths)",
            [&] {
                const auto result = CalibrateGSRSLV(*initial, quotes, parameters, settings);
                REQUIRE(result.converged_ && result.fitWithinTolerance_ && result.numericalValidationPassed_, "SLV benchmark calibration failed");
                checksum += result.parameters_[0];
            },
            1, 3));
        Bench::Print(Bench::Run(
            prefix + "recalibrated quote risk (3 quotes x 3 targets, two bump sizes)",
            [&] {
                const auto result = GSRSLVQuoteRisk(*initial, quotes, parameters, options, settings);
                REQUIRE(std::all_of(result.stable_.begin(), result.stable_.end(), [](bool stable) { return stable; }), "SLV benchmark risk unstable");
                checksum += result.sensitivities_(0, 0);
            },
            1, 3));
    }
    REQUIRE(std::isfinite(checksum), "invalid SLV calibration benchmark checksum");
    Bench::DoNotOptimize(&checksum);
}
