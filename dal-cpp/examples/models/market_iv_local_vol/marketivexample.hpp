#pragma once

#include <cmath>
#include <iomanip>
#include <iostream>

#include <dal/model/marketivlocalvol.hpp>
#include <dal/script/event.hpp>

#include "../../floatformat.hpp"

namespace Dal::MarketIvExample {
    inline Date_ Today() { return {2026, 11, 16}; }

    inline MarketIvFitResult_ Fit(int terms = 8, bool flat = false) {
        Vector_<MarketIvTerm_> carry;
        Vector_<Date_> expiries;
        for (int i = 1; i <= terms; ++i) {
            const int days = 365 * i / terms;
            const double t = days / 365.;
            carry.push_back({Today().AddDays(days), 100. * std::exp(.02 * t + .01 * t * t), std::exp(-.015 * t - .01 * t * t)});
            if (i % (terms / 4) == 0)
                expiries.push_back(Today().AddDays(days));
        }
        const auto market = flat ? NewFlatMarketIvMarket(Today(), 100., .03, .01, expiries) : MarketIvMarket_(Today(), 100., carry);
        MarketIvFitRequest_ request{market, {}, {}};
        for (const auto& expiry : expiries) {
            const double t = (expiry - Today()) / 365.;
            const double forward = 100. * std::exp(flat ? .02 * t : .02 * t + .01 * t * t);
            for (const double k : {-.15, 0., .15}) {
                const double theta = .04 * t, phi = (flat ? 0. : .4) / std::sqrt(theta * (1. + theta));
                const double w = .5 * theta * (1. - .3 * phi * k + std::sqrt(std::pow(phi * k - .3, 2) + .91));
                request.quotes_.push_back({String_(std::to_string(request.quotes_.size())), expiry, forward * std::exp(k), OptionType_("CALL"),
                                           MarketIvQuoteType_("DECIMAL_IV"), std::sqrt(w / t)});
            }
        }
        return FitMarketIvSurface(request);
    }

    inline MarketIvDupireResult_ Grid(const MarketIvCalibrationSnapshot_& snapshot, int rows = 21, int columns = 13) {
        Vector_<> spots, times;
        for (int i = 0; i < rows; ++i)
            spots.push_back(100. * std::exp(-.7 + 1.4 * i / (rows - 1)));
        for (int j = 0; j < columns; ++j)
            times.push_back(.01 + .99 * j / (columns - 1));
        return CalibrateMarketIvDupire(snapshot, {spots, times, {}});
    }

    inline MarketIvLocalVolModelSettings_ Settings(int steps = 52) {
        MarketIvLocalVolModelSettings_ result;
        result.index_ = "EQ[A]";
        result.currency_ = "USD";
        result.factor_ = "W_EQ";
        result.horizon_ = 1.25;
        result.maxStep_ = 1. / steps;
        return result;
    }

    inline Script::ScriptProductData_ Product(const String_& name, const String_& event) { return {name, {Cell_(Today().AddDays(365))}, {event}}; }

    template <class Price_, class Inspect_>
    void Run(const MarketIvFitResult_& fit, const Handle_<MarketIvLocalVolModelData_>& model, Price_ price, Inspect_ inspect) {
        std::cout << "Calibration (decimal IV)\n"
                  << std::left << std::setw(12) << "Quote" << std::setw(16) << "Market" << std::setw(16) << "Fitted" << "Residual\n"
                  << std::string(60, '-') << '\n';
        for (size_t i = 0; i < fit.Quotes().size(); ++i) {
            const auto& row = fit.Quotes()[i];
            std::cout << std::setw(12) << fit.Request().quotes_[i].id_ << std::setw(16) << ExampleFloat(fit.Request().quotes_[i].mid_)
                      << std::setw(16) << ExampleFloat(*row.nativeFitted_) << ExampleFloat(*row.nativeResidual_) << '\n';
        }
        std::cout << "Fit status=" << fit.Status() << " rho=" << ExampleFloat(fit.Snapshot().Rho()) << " eta=" << ExampleFloat(fit.Snapshot().Eta())
                  << " certified=" << fit.Snapshot().Certificate().certified_ << '\n';
        std::cout << "source=" << model->SourceIdentity() << "\nmodel=" << model->ModelIdentity() << '\n';
        std::cout << "Valuation (USD, 16384 paths; smoke tolerances)\n"
                  << std::setw(16) << "Product" << std::setw(18) << "Method" << std::setw(16) << "PV" << "Reference / error\n"
                  << std::string(74, '-') << '\n';
        const auto call = Product("call", "pay PAYS MAX(FIX(EQ[A]) - 100, 0)");
        const double callPv = price(call);
        const double reference = *EvaluateMarketIvSurface(fit.Snapshot(), {100., 1.}).CallPrice();
        REQUIRE(std::isfinite(callPv) && callPv > 0. && std::fabs(callPv - reference) < .5, "Call smoke check failed");
        std::cout << std::setw(16) << "Call" << std::setw(18) << "Local-vol MC" << std::setw(16) << ExampleFloat(callPv) << ExampleFloat(reference)
                  << " / " << ExampleFloat(callPv - reference) << '\n';
        const double forwardPv = price(Product("forward", "pay PAYS FIX(EQ[A])"));
        const double expectedForward = 100. * std::exp(.03) * std::exp(-.025);
        REQUIRE(std::fabs(forwardPv - expectedForward) < .8, "Forward smoke check failed");
        std::cout << std::setw(16) << "Forward claim" << std::setw(18) << "Local-vol MC" << std::setw(16) << ExampleFloat(forwardPv)
                  << ExampleFloat(expectedForward) << " / " << ExampleFloat(forwardPv - expectedForward) << '\n';
        const double df = price(Product("df", "pay PAYS 1"));
        REQUIRE(std::fabs(df - std::exp(-.025)) < 2e-12, "DF check failed");
        std::cout << std::setw(16) << "Unit payment" << std::setw(18) << "Deterministic DF" << std::setw(16) << ExampleFloat(df)
                  << ExampleFloat(std::exp(-.025)) << " / " << ExampleFloat(df - std::exp(-.025)) << '\n';
        const auto report = inspect(call);
        REQUIRE(report.perPathMarketQueries_ == 0 && report.preparedColumns_ == report.internalSteps_, "Preparation check failed");
        REQUIRE(!model->Capabilities().aad_ && !model->Capabilities().archive_, "Capability check failed");
        std::cout << "steps=" << report.internalSteps_ << " dimension=" << report.simulationDimension_
                  << " workspace_bytes=" << report.peakWorkspaceBytes_ << " per_path_market_queries=" << report.perPathMarketQueries_ << '\n';
        for (const auto& warning : model->Warnings())
            std::cout << warning.code_ << ": " << warning.message_ << '\n';
        std::cout << "Market-grid checks passed\n";
    }
} // namespace Dal::MarketIvExample
