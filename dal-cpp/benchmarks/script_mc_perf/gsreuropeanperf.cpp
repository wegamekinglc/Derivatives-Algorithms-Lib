//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <cmath>

#include <dal/model/gsrcalibration.hpp>

#include "gsreuropeanperf.hpp"
#include "gsrperf.hpp"

namespace {
    Dal::Handle_<Dal::MultiFactorGSRModelData_> Model(int factors, double scale) {
        using namespace Dal;
        const auto curve = GSRBenchmarkCurve();
        const Date_ today = curve->evaluationDate_;
        MultiFactorGSRVolSettings_ settings;
        settings.gKnotDates_ = {today, today.AddDays(365), today.AddDays(730)};
        settings.hKnotDates_ = {today, today.AddDays(1095)};
        settings.gValues_ = Matrix_<>(factors, 3, 0.0);
        settings.hValues_ = Matrix_<>(factors, 2, 0.0);
        settings.correlations_ = Matrix_<>(factors, factors, 0.2);
        for (int factor = 0; factor < factors; ++factor) {
            settings.factorNames_.push_back("factor" + String::FromInt(factor));
            settings.correlations_(factor, factor) = 1.0;
            for (int knot = 0; knot < 3; ++knot)
                settings.gValues_(factor, knot) = scale * (0.012 + knot * 0.003) / (factor + 1);
            settings.hValues_(factor, 0) = 1.0 / (factor + 1);
            settings.hValues_(factor, 1) = factor == 0 ? 1.0 : -0.3;
        }
        return Handle_<MultiFactorGSRModelData_>(
            new MultiFactorGSRModelData_("rates", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", settings))));
    }
} // namespace

void RunGSREuropeanCases() {
    using namespace Dal;
    const Date_ today(2026, 10, 2), expiry = today.AddDays(365);
    const GSRBondOption_ bond{expiry, today.AddDays(1095), 0.94, OptionType_("CALL")};
    GSRSwaption_ swaption;
    swaption.expiry_ = expiry;
    swaption.strike_ = 0.03;
    for (int year = 2; year <= 6; ++year) {
        const Date_ start = today.AddDays(365 * (year - 1)), end = today.AddDays(365 * year);
        swaption.fixed_.push_back({end, 1.0});
        swaption.floating_.push_back({start, start, end, end, 1.0, 1.0, "12M"});
    }
    double checksum = 0.0;
    for (int factors : {1, 2, 3}) {
        const auto model = Model(factors, 1.0);
        const std::string label = "GSR " + std::to_string(factors) + "F ";
        Bench::Print(Bench::Run(
            label + "bond option (1000 prices)",
            [&] {
                for (int i = 0; i < 1000; ++i)
                    checksum += PriceGSREuropeanOption(*model, bond).price_;
            },
            1, 3));
        Bench::Print(Bench::Run(
            label + "5Y swaption (1 price, order 16/32)",
            [&] {
                const auto result = PriceGSREuropeanOption(*model, swaption);
                REQUIRE(result.numericalError_ < 1e-7, "GSR swaption benchmark numerical error exceeds tolerance");
                checksum += result.price_;
            },
            1, 3));
    }
    const auto target = Model(1, 1.0), initial = Model(1, 0.75);
    Vector_<GSRCalibrationQuote_> quotes;
    for (int year = 1; year <= 3; ++year) {
        const GSRBondOption_ option{today.AddDays(365 * year), today.AddDays(365 * (year + 1)), 0.97, OptionType_("CALL")};
        quotes.push_back({"bond" + String::FromInt(year), option, PriceGSREuropeanOption(*target, option).price_, 1e-6});
    }
    const Vector_<GSRCalibrationParameter_> parameters{{0, 0, 0.0, 0.1}, {0, 1, 0.0, 0.1}, {0, 2, 0.0, 0.1}};
    Bench::Print(Bench::Run(
        "GSR g calibration (3 quotes x 3 buckets)",
        [&] {
            const auto result = CalibrateGSRVolatility(*initial, quotes, parameters);
            REQUIRE(result.converged_ && result.fitWithinTolerance_ && result.numericalValidationPassed_, "GSR benchmark calibration failed");
            checksum += result.parameters_[0];
        },
        1, 3));
    REQUIRE(std::isfinite(checksum), "invalid GSR European benchmark checksum");
    Bench::DoNotOptimize(&checksum);
}
