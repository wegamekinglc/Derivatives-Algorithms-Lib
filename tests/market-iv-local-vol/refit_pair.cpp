#include <cmath>
#include <iostream>

#include <dal/script/marketivlocalvol.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    MarketIvScriptRefitRequest_ Request() {
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
        REQUIRE(fit.IsSuccess(), fit.Failure());
        MarketIvScriptRefitRequest_ request{
            fit.Snapshot(),
            {{85., 100., 115.}, {.25, 1.}, {}},
            {"EQ[A]", "USD", "W_EQ", 1.},
            Handle_<ScriptProductData_>(new ScriptProductData_("call", {Cell_(expiry)}, {"pay PAYS MAX(spot() - 100, 0)"}))};
        request.valuation_.evaluationDate_ = today;
        request.quoteIds_ = {"atm", "left"};
        request.steps_ = {.001, .0001};
        return request;
    }

    void CompareQuotes(const Vector_<MarketIvQuote_>& actual, const Vector_<MarketIvQuote_>& expected) {
        REQUIRE(actual.size() == expected.size(), "quote count mismatch");
        for (size_t i = 0; i < expected.size(); ++i) {
            const auto& a = actual[i];
            const auto& e = expected[i];
            REQUIRE(a.id_ == e.id_, "quote ID mismatch at " + e.id_);
            REQUIRE(a.mid_ == e.mid_, "quote mid mismatch at " + e.id_);
            REQUIRE(a.bid_ == e.bid_ && a.ask_ == e.ask_, "quote band mismatch at " + e.id_);
            REQUIRE(a.weight_ == e.weight_, "quote weight mismatch at " + e.id_);
            REQUIRE(a.strike_ == e.strike_ && a.expiry_ == e.expiry_ && a.optionType_ == e.optionType_ && a.quoteType_ == e.quoteType_,
                    "quote terms mismatch at " + e.id_);
        }
    }

    void CompareModel(const MarketIvLocalVolModelData_& actual, const MarketIvDupireResult_& grid, const MarketIvScriptRefitRequest_& request) {
        const auto manual = BuildMarketIvLocalVolModelData("manual", grid, request.model_);
        const auto& surface = actual.SurfaceData();
        const auto& expected = grid.SurfaceData();
        REQUIRE(surface.spots_ == request.grid_.spots_ && surface.times_ == request.grid_.times_, "grid axis mismatch");
        REQUIRE(surface.vols_.Rows() == expected.vols_.Rows() && surface.vols_.Cols() == expected.vols_.Cols(), "grid shape mismatch");
        for (int i = 0; i < surface.vols_.Rows(); ++i)
            for (int j = 0; j < surface.vols_.Cols(); ++j)
                REQUIRE(surface.vols_(i, j) == expected.vols_(i, j), "grid node mismatch");
        REQUIRE(actual.SourceIdentity() == manual->SourceIdentity() && actual.ModelIdentity() == manual->ModelIdentity(),
                "source or model identity mismatch");
    }

    void CompareSide(const MarketIvScriptRefitRequest_& request, size_t originalRow, double step, const MarketIvScriptRefitSide_& side) {
        auto quotes = request.snapshot_.Request().quotes_;
        auto& quote = quotes[originalRow];
        quote.mid_ += step;
        if (quote.bid_) {
            *quote.bid_ += step;
            *quote.ask_ += step;
        }
        const auto fit = RefitMarketIvSurface(request.snapshot_, quotes);
        REQUIRE(side.fitStatus_ == fit.Status(), "fit status mismatch");
        if (!fit.IsSuccess()) {
            REQUIRE(!side.model_ && side.failureStage_ == "FIT" && side.failure_ == fit.Failure(), "fit failure diagnostic mismatch");
            std::cout << quote.id_ << " " << step << " FIT " << fit.Status() << '\n';
            return;
        }
        REQUIRE(side.model_ && side.failureStage_.empty() && side.failure_.empty(), "expected a successful model");
        const auto snapshot = fit.Snapshot();
        const auto grid = CalibrateMarketIvDupire(snapshot, request.grid_);
        REQUIRE(grid.IsSuccess(), grid.Failure());
        const auto& actual = side.model_->Snapshot();
        CompareQuotes(actual.Request().quotes_, quotes);
        REQUIRE(actual.Certificate().certified_, "expected a certified fit");
        REQUIRE(side.selectedStart_ == fit.SelectedStart(), "selected start mismatch");
        REQUIRE(actual.RegimeSignature() == snapshot.RegimeSignature(), "regime signature mismatch");
        REQUIRE(side.regimeChanged_ == (snapshot.RegimeSignature() != request.snapshot_.RegimeSignature()), "regime flag mismatch");
        CompareModel(*side.model_, grid, request);
        std::cout << quote.id_ << " " << step << " PASS start=" << fit.SelectedStart() << " quotes=" << quotes.size()
                  << " nodes=" << grid.Nodes().size() << " identities=match\n";
    }

    void CheckPairs(const MarketIvScriptRefitRequest_& request) {
        const auto plan = PlanMarketIvScriptRefit(request);
        for (size_t i = 0; i < 2; ++i) {
            const auto pair = BuildMarketIvScriptRefitPair(plan, i);
            const double step = i == 0 ? .0001 : .001;
            REQUIRE(pair.row_.id_ == request.snapshot_.Request().quotes_[i].id_ && pair.row_.step_ == step, "quote selection mismatch");
            REQUIRE(pair.plus_.model_ && pair.minus_.model_, "expected two successful sides for " + pair.row_.id_);
            CompareSide(request, i, step, pair.plus_);
            CompareSide(request, i, -step, pair.minus_);
            if (i == 0) {
                REQUIRE(pair.plus_.model_->Snapshot().RegimeSignature() != pair.minus_.model_->Snapshot().RegimeSignature() &&
                            pair.Status() == "REGIME_CHANGED",
                        "expected two successful branches");
                std::cout << "left " << pair.Status() << '\n';
            }
        }
    }

    void CheckParityFailure(MarketIvScriptRefitRequest_ request) {
        request.quoteIds_ = {"atm", "right"};
        request.steps_ = {.01, .0001};
        const auto plan = PlanMarketIvScriptRefit(request);
        {
            const auto pair = BuildMarketIvScriptRefitPair(plan, 0);
            CompareSide(request, 1, .01, pair.plus_);
            CompareSide(request, 1, -.01, pair.minus_);
            REQUIRE(pair.plus_.fitStatus_ == "INCONSISTENT_QUOTES" && pair.minus_.model_ && pair.Status() == "SIDE_FAILED" &&
                        !pair.symmetricBumpSlope_,
                    "expected isolated positive-side parity failure");
            std::cout << "atm " << pair.Status() << " central_slope=empty\n";
        }
        const auto next = BuildMarketIvScriptRefitPair(plan, 1);
        REQUIRE(next.row_.id_ == "right" && next.plus_.model_ && next.minus_.model_, "following quote missing");
        CompareSide(request, 2, .0001, next.plus_);
        CompareSide(request, 2, -.0001, next.minus_);
    }

    void CheckWorkLimitAfterSelection(MarketIvScriptRefitRequest_ request) {
        constexpr double step = .0001;
        auto base = request.snapshot_.Request();
        auto minusQuotes = base.quotes_;
        minusQuotes.front().mid_ -= step;
        const auto full = RefitMarketIvSurface(request.snapshot_, minusQuotes);
        REQUIRE(full.IsSuccess(), full.Failure());
        const size_t rows = full.Execution().modelRowEvaluations_;
        REQUIRE(rows > 1, "expected a nonzero final diagnostic budget");
        base.settings_.maxModelRowEvaluations_ = rows - 1;
        request.snapshot_ = FitMarketIvSurface(base).Snapshot();
        request.quoteIds_ = {"left"};
        request.steps_ = {step};

        const auto reference = RefitMarketIvSurface(request.snapshot_, minusQuotes);
        REQUIRE(reference.Status() == "WORK_LIMIT", "expected a work-limited negative refit");
        REQUIRE(reference.Execution().modelRowEvaluations_ == rows - 1, "negative refit did not exhaust the configured row budget");
        REQUIRE(reference.SelectedStart() >= 0 && reference.SelectedStart() == full.SelectedStart(), "negative refit lost its selected start");
        const auto pair = BuildMarketIvScriptRefitPair(PlanMarketIvScriptRefit(request), 0);
        REQUIRE(pair.plus_.model_, "expected a readable positive-side model");
        CompareSide(request, 0, step, pair.plus_);
        REQUIRE(pair.minus_.fitStatus_ == reference.Status() && pair.minus_.failure_ == reference.Failure(), "negative fit diagnostic mismatch");
        REQUIRE(pair.minus_.failureStage_ == "FIT" && !pair.minus_.model_, "expected a fit failure with no negative model");
        REQUIRE(pair.Status() == "SIDE_FAILED" && !pair.symmetricBumpSlope_, "expected a failed row with no central slope");
        std::cout << "left negative full_rows=" << rows << " budget=" << rows - 1 << " " << pair.minus_.fitStatus_
                  << " reference_start=" << reference.SelectedStart() << " pair_start=" << pair.minus_.selectedStart_ << " model=empty\n";
        REQUIRE(pair.minus_.selectedStart_ == reference.SelectedStart(), "failed negative side lost its selected start");
    }
} // namespace

int main() {
    try {
        RegisterAll_::Init();
        const auto request = Request();
        CheckPairs(request);
        CheckParityFailure(request);
        CheckWorkLimitAfterSelection(request);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
