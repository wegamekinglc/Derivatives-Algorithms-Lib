#include <dal/platform/platform.hpp>

#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/script/marketivlocalvol.hpp>

namespace Dal::Script {
    namespace {
        MarketIvScriptRefitSide_ BuildSide(const MarketIvScriptRefitRequest_& request, size_t quoteIndex, double step) {
            MarketIvScriptRefitSide_ side;
            try {
                side.failureStage_ = "FIT";
                const auto bumped = MarketIvDetail::RefitBumpedQuote(request.snapshot_, quoteIndex, step);
                if (!bumped.fit_) {
                    side.failureStage_ = "BUMP";
                    side.failure_ = bumped.bumpFailure_;
                    return side;
                }
                const auto& fit = *bumped.fit_;
                side.fitStatus_ = fit.Status();
                side.selectedStart_ = fit.SelectedStart();
                if (!fit.IsSuccess()) {
                    side.failure_ = fit.Failure();
                    return side;
                }
                const auto snapshot = fit.Snapshot();
                side.regimeChanged_ = snapshot.RegimeSignature() != request.snapshot_.RegimeSignature();
                side.failureStage_ = "GRID";
                const auto grid = CalibrateMarketIvDupire(snapshot, request.grid_);
                if (!grid.IsSuccess()) {
                    side.failure_ = grid.Failure();
                    return side;
                }
                side.failureStage_ = "MODEL";
                side.model_ = BuildMarketIvLocalVolModelData("refit-side", grid, request.model_);
                side.failureStage_.clear();
            } catch (const Exception_& error) {
                side.failure_ = error.what();
            }
            return side;
        }
    } // namespace

    String_ MarketIvScriptRefitPair_::Status() const {
        if (!plus_.model_ || !minus_.model_)
            return "SIDE_FAILED";
        return plus_.regimeChanged_ || minus_.regimeChanged_ ? "REGIME_CHANGED" : "SUCCESS";
    }

    MarketIvScriptRefitPair_ BuildMarketIvScriptRefitPair(const MarketIvScriptRefitPlan_& plan, size_t rowIndex) {
        REQUIRE(rowIndex < plan.rows_.size(), "InvalidMarketIvRequest: /row_index outside selected quote rows");
        const auto& row = plan.rows_[rowIndex];
        return {row, BuildSide(plan.request_, row.quoteIndex_, row.step_), BuildSide(plan.request_, row.quoteIndex_, -row.step_), std::nullopt};
    }
} // namespace Dal::Script
