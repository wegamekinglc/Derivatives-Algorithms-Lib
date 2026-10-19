#include <dal/platform/platform.hpp>

#include <iostream>

#include <dal/benchmarks/bench.hpp>
#include <dal/model/surface/marketivsurface.hpp>

#include "marketivperf.hpp"

using namespace Dal;

namespace {
    MarketIvFitRequest_ Request(int count, int terms) {
        const Date_ today(2026, 10, 19);
        Vector_<Date_> dates;
        for (int i = 1; i <= terms; ++i)
            dates.push_back(today.AddDays(i * 30));
        const auto market = NewFlatMarketIvMarket(today, 100., -.01, .02, dates);
        Vector_<MarketIvQuote_> quotes;
        quotes.reserve(count);
        for (int i = 0; i < count; ++i) {
            const int j = i % terms, wing = i / terms;
            const double t = (dates[j] - today) / 365., k = -.3 + .6 * wing / (count / terms), theta = .04 * t;
            const double phi = .8 / std::sqrt(theta * (1. + theta));
            const double w = theta / 2. * (1. - .4 * phi * k + std::sqrt(std::pow(phi * k - .4, 2) + .84));
            quotes.push_back({String_(std::to_string(i)), dates[j], market.Terms()[j].forward_ * std::exp(k), OptionType_("CALL"),
                              MarketIvQuoteType_("DECIMAL_IV"), std::sqrt(w / t)});
        }
        return {market, quotes, {}};
    }
    void Evidence(const char* name, const MarketIvExecution_& e) {
        REQUIRE(e.numericPayloadBytes_ <= e.plannedNumericPayloadBytes_ && e.peakWorkspaceBytes_ <= e.plannedWorkspaceBytes_,
                "Market IV budget envelope failure");
        std::cout << "capacity " << name << " payload=" << e.numericPayloadBytes_ << "/" << e.plannedNumericPayloadBytes_
                  << " workspace=" << e.peakWorkspaceBytes_ << "/" << e.plannedWorkspaceBytes_ << " rows=" << e.modelRowEvaluations_
                  << " conversions=" << e.quoteConversions_ << " outputs=" << e.outputEvaluations_ << " quotes=" << e.quoteCapacity_
                  << " history=" << e.iterationCapacity_ << " jacobian=" << e.jacobianCapacity_ << '\n';
    }
} // namespace

int RunMarketIvBenchmarks() {
    Bench::PrintHeader();
    for (const auto shape : {std::pair<int, int>{1, 1}, {300, 12}, {2048, 64}}) {
        const auto request = Request(shape.first, shape.second);
        const auto fit = FitMarketIvSurface(request);
        REQUIRE(fit.IsSuccess(), "Market IV benchmark fit failed: " + fit.Status() + "; " + fit.Failure());
        const std::string name = "market_iv fit " + std::to_string(shape.first) + "/" + std::to_string(shape.second);
        Evidence(name.c_str(), fit.Execution());
        Bench::Print(Bench::Run(
            name.c_str(),
            [&] {
                const auto result = FitMarketIvSurface(request);
                REQUIRE(result.IsSuccess(), "fit failed");
                Bench::DoNotOptimize(&result);
            },
            1, 10));
        if (shape.first != 300)
            continue;
        const auto base = fit.Snapshot();
        MarketIvRefitRiskRequest_ risk;
        for (size_t i = 0; i < 8; ++i)
            risk.quoteIds_.push_back(request.quotes_[i * 30].id_);
        for (int i = 0; i < 25; ++i)
            risk.outputs_.push_back({{90. + i, .5}, MarketIvQuantity_("LOCAL_VOL")});
        const auto response = EvaluateMarketIvRefitRisk(base, risk);
        REQUIRE(response.Complete(), "Market IV benchmark full refit failed");
        Evidence("refit 8/25", response.Execution());
        Bench::Print(Bench::Run(
            "market_iv refit 8/25",
            [&] {
                const auto r = EvaluateMarketIvRefitRisk(base, risk);
                REQUIRE(r.Complete(), "refit failed");
                Bench::DoNotOptimize(&r);
            },
            1, 10));
    }
    auto maximum = Request(10000, 64);
    Vector_<Date_> carryDates;
    for (int i = 1; i <= 64; ++i)
        for (int offset = -3; offset <= 0; ++offset)
            carryDates.push_back(maximum.market_.EvaluationDate().AddDays(i * 30 + offset));
    maximum.market_ = NewFlatMarketIvMarket(maximum.market_.EvaluationDate(), 100., -.01, .02, carryDates);
    maximum.settings_.maxModelRowEvaluations_ = 10001;
    const auto limited = FitMarketIvSurface(maximum);
    REQUIRE(limited.Status() == "WORK_LIMIT" && limited.Execution().modelRowEvaluations_ == 10001, "Large request failed deterministic work cap");
    Evidence("max work limited", limited.Execution());
    return 0;
}
