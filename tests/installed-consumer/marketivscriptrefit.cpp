#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

#include <dal-public/src/global.hpp>
#include <dal-public/src/marketivlocalvol.hpp>
#include <dal-public/src/script.hpp>

using namespace Dal;

namespace {
    MarketIvScriptRefitRequest_ FlatRequest(double volatility = .2) {
        const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
        const MarketIvMarket_ market(today, 100., {{expiry, 105., .96}});
        const OptionType_ call("CALL");
        const MarketIvQuoteType_ iv("DECIMAL_IV");
        const auto fit = FitMarketIvSurface({market,
                                             {{"left", expiry, 90., call, iv, volatility},
                                              {"atm", expiry, 105., call, iv, volatility},
                                              {"right", expiry, 120., call, iv, volatility}},
                                             {}});
        MarketIvScriptRefitRequest_ request{fit.Snapshot(),
                                            {{80., 100., 120.}, {.125, .5, 1.}, {}},
                                            {"EQ[A]", "USD", "W_EQ", 1., 1.},
                                            NewScriptProduct("call", {Cell_(expiry)}, {"pay PAYS MAX(spot() - 105, 0)"})};
        request.valuation_.evaluationDate_ = today;
        request.quoteIds_ = {"atm"};
        request.blocks_ = 16;
        request.pathsPerBlock_ = 257;
        return request;
    }

    MarketIvScriptRefitRequest_ PremiumRequest() {
        const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
        const MarketIvMarket_ market(today, 100., {{expiry, 100., .96}});
        const double atm = 96. * std::erf(.1 / std::sqrt(2.));
        const OptionType_ call("CALL"), put("PUT");
        const MarketIvQuoteType_ iv("DECIMAL_IV"), price("PRICE");
        const auto fit = FitMarketIvSurface({market,
                                             {{"left", expiry, 90., call, iv, .2},
                                              {"atm", expiry, 100., call, price, atm, atm - .002, atm + .002},
                                              {"right", expiry, 110., call, iv, .2},
                                              {"atm-put", expiry, 100., put, price, atm, atm - .1, atm + .003, .75}},
                                             {}});
        MarketIvScriptRefitRequest_ request{fit.Snapshot(),
                                            {{85., 100., 115.}, {.25, 1.}, {}},
                                            {"EQ[A]", "USD", "W_EQ", 1.},
                                            NewScriptProduct("call", {Cell_(expiry)}, {"pay PAYS MAX(spot() - 100, 0)"})};
        request.valuation_.evaluationDate_ = today;
        request.quoteIds_ = {"right", "atm"};
        request.steps_ = {.0001, .01};
        request.blocks_ = 16;
        request.pathsPerBlock_ = 257;
        return request;
    }
    template <class T_> void Write(const T_& value) { std::cout << value; }
    template <class T_> void Write(const std::optional<T_>& value) {
        if (value)
            Write(*value);
        else
            std::cout << "null";
    }

    void Report(const char* name, const MarketIvScriptRefitRiskResult_& result) {
        for (const auto& row : result.rows_) {
            std::cout << "REFIT\t" << name;
            MarketIvLocalVolFields::Visit(row, [](const char* key, const auto& value) {
                std::cout << '\t' << key << '=';
                Write(value);
            });
            std::cout << '\n';
        }
    }

    void CheckPaired(const Script::MarketIvScriptRiskRow_& row) {
        REQUIRE(row.currency_ == "USD" && row.plus_.pv_ && row.minus_.pv_, "missing paired prices");
        REQUIRE(row.slope_ && row.standardError_ && *row.standardError_ >= 0., "missing paired statistics");
        REQUIRE(std::abs(*row.slope_ - (*row.plus_.pv_ - *row.minus_.pv_) / (2. * row.quote_.step_)) < 1e-8, "wrong central slope");
    }

    MarketIvScriptRefitRiskResult_ FlatResult(const Vector_<String_>& ids, const Vector_<>& steps) {
        auto request = FlatRequest();
        request.quoteIds_ = ids;
        request.steps_ = steps;
        return EvaluateMarketIvScriptRefitRisk(request);
    }

    void CheckFlat() {
        for (const double step : {1e-4, 5e-5}) {
            const auto result = FlatResult({"atm"}, {step});
            REQUIRE(result.rows_.size() == 1, "expected one ATM bucket after request destruction");
            const auto& row = result.rows_.front();
            REQUIRE(row.quote_.id_ == "atm" && row.quote_.inputUnit_ == "decimal_iv" && row.quote_.step_ == step, "wrong quote axis");
            CheckPaired(row);
            Report(step == 1e-4 ? "full" : "half", result);
        }
        const auto all = FlatResult({"right", "atm", "left"}, {});
        REQUIRE(all.rows_.size() == 3 && all.execution_.scenarios_ == 7, "expected base and six side PVs");
        const Vector_<String_> expected{"left", "atm", "right"};
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(all.rows_[i].quote_.id_ == expected[i], "flat market bucket order");
            CheckPaired(all.rows_[i]);
        }
        Report("all_buckets", all);
    }

    void CheckPartial() {
        const auto partial = EvaluateMarketIvScriptRefitRisk(PremiumRequest());
        REQUIRE(partial.rows_.size() == 2 && partial.execution_.scenarios_ == 4, "expected one base and three priced sides");
        REQUIRE(partial.execution_.refits_ == 4, "expected four refits");
        REQUIRE(partial.execution_.grids_ == 4, "expected one base and three side grids");
        REQUIRE(partial.execution_.totalPaths_ == 4 * 16 * 257, "wrong path count");
        const auto& failed = partial.rows_[0];
        REQUIRE(failed.quote_.id_ == "atm" && failed.quote_.inputUnit_ == "premium" && failed.quote_.step_ == .01, "wrong failed bucket");
        REQUIRE(failed.status_ == "SIDE_FAILED", "expected partial result");
        REQUIRE(failed.plus_.failureStage_ == "FIT" && failed.plus_.fitStatus_ == "INCONSISTENT_QUOTES", "lost fit failure");
        REQUIRE(!failed.plus_.pv_ && failed.minus_.pv_ && !failed.slope_ && !failed.standardError_, "missing values in wrong columns");
        const auto& next = partial.rows_[1];
        REQUIRE(next.quote_.id_ == "right", "lost following bucket");
        CheckPaired(next);
        Report("partial", partial);
    }

    MarketIvScriptRefitRiskResult_ ShiftedFlatResult(double shift) {
        auto request = FlatRequest(.2 + shift);
        request.pathsPerBlock_ = 131072;
        return EvaluateMarketIvScriptRefitRisk(request);
    }

    void CheckParallelShift() {
        constexpr double H = 1e-4;
        const auto plus = ShiftedFlatResult(H), minus = ShiftedFlatResult(-H);
        REQUIRE(plus.baseBlockMeans_.size() == 16 && minus.baseBlockMeans_.size() == 16, "expected 16 paired base blocks");
        Vector_<> slopes;
        for (size_t b = 0; b < 16; ++b)
            slopes.push_back((plus.baseBlockMeans_[b] - minus.baseBlockMeans_[b]) / (2. * H));
        const double slope = std::accumulate(slopes.begin(), slopes.end(), 0.) / slopes.size();
        double squares = 0.;
        for (double d : slopes)
            squares += (d - slope) * (d - slope);
        const double se = std::sqrt(squares / (16. * 15.));
        const auto black = [](double vol) { return .96 * 105. * std::erf(vol / std::sqrt(8.)); };
        const double reference = (black(.2 + H) - black(.2 - H)) / (2. * H);
        REQUIRE(se / 100. <= .001 && std::abs(slope - reference) / 100. <= .001 + 4.5 * se / 100., "parallel flat-IV Black qualification failed");
        std::cout << "BLACK\t" << plus.rows_[0].basePv_ << '\t' << minus.rows_[0].basePv_ << '\t' << slope << '\t' << se << '\t' << reference << '\n';
    }

    void CheckPayoffFailure() {
        auto request = FlatRequest();
        auto fit = request.snapshot_.Request();
        fit.quotes_ = {fit.quotes_[1]};
        request.snapshot_ = FitMarketIvSurface(fit).Snapshot();
        request.grid_.times_ = {1.};
        request.blocks_ = 2;
        request.pathsPerBlock_ = 1;
        request.steps_ = {.01};
        // The first MRG32 path ends near 94.32, 94.90 and 95.47 for +h, base and -h.
        request.product_ =
            NewScriptProduct("domain", request.product_->Dates(), {"IF spot() < 94.6 THEN pay PAYS LOG(-spot()) ELSE pay PAYS spot() END"});
        const auto result = EvaluateMarketIvScriptRefitRisk(request);
        REQUIRE(result.rows_.size() == 1, "expected one sampled bucket");
        const auto& row = result.rows_.front();
        REQUIRE(row.status_ == "SIDE_FAILED" && row.plus_.failureStage_ == "SAMPLE", "lost payoff failure");
        REQUIRE(row.plus_.failure_.find("InvalidPayoff") != String_::npos, "missing payoff diagnostic");
        REQUIRE(!row.plus_.pv_ && row.minus_.pv_ && !row.slope_ && !row.standardError_, "incorrect payoff failure row");

        // Only the second path of the positive shock ends between 120 and 121.
        request.product_ = NewScriptProduct("second", request.product_->Dates(),
                                            {"IF spot() > 120 AND spot() < 121 THEN pay PAYS LOG(-spot()) ELSE pay PAYS spot() END"});
        Vector_<size_t> counts;
        for (size_t blocks : {2U, 4U}) {
            request.blocks_ = blocks;
            request.pathsPerBlock_ = 4 / blocks;
            const auto sample = EvaluateMarketIvScriptRefitRisk(request);
            const auto& failed = sample.rows_.front();
            REQUIRE(failed.plus_.failureStage_ == "SAMPLE" && failed.minus_.pv_, "expected positive-side failure");
            counts.push_back(sample.execution_.totalPaths_);
        }
        std::cout << "PATHS 2x2=" << counts[0] << " 4x1=" << counts[1] << std::endl;
        REQUIRE(counts[0] == 10 && counts[1] == 10, "path count must include the failing path");
    }
} // namespace

int main() {
    InitGlobalData(1);
    std::cout << std::boolalpha << std::setprecision(17);
    CheckFlat();
    CheckPartial();
    CheckParallelShift();
    CheckPayoffFailure();
}
