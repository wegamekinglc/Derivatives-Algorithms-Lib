#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>

#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/script/marketivlocalvol.hpp>

#include "mcprotocol.hpp"

using namespace Dal;
using namespace Dal::Script;

namespace {
    constexpr double SPOT = 100., FORWARD = 105., DISCOUNT = .96, STRIKE = 105., IV = .20, H = 1e-4;
    const Vector_<> TIMES{0., .125, .5, 1.};
    using Blocks_ = std::array<double, DalTest::BATCHES>;

    MarketIvScriptRefitRequest_ Request(size_t pathsPerBlock) {
        RegisterAll_::Init(1);
        const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
        const MarketIvMarket_ market(today, SPOT, {{expiry, FORWARD, DISCOUNT}});
        const OptionType_ call("CALL");
        const MarketIvQuoteType_ iv("DECIMAL_IV");
        const auto base = FitMarketIvSurface(
            {market, {{"left", expiry, 90., call, iv, IV}, {"atm", expiry, STRIKE, call, iv, IV}, {"right", expiry, 120., call, iv, IV}}, {}});
        REQUIRE(base.IsSuccess(), base.Failure());
        MarketIvScriptRefitRequest_ request{
            base.Snapshot(),
            {{80., 100., 120.}, {.125, .5, 1.}, {}},
            {"EQ[A]", "USD", "W_EQ", 1., 1.},
            Handle_<ScriptProductData_>(new ScriptProductData_("call", {Cell_(expiry)}, {"pay PAYS MAX(spot() - 105, 0)"}))};
        request.valuation_.evaluationDate_ = today;
        request.simulation_.compiled_ = true;
        request.simulation_.useBb_ = false;
        request.quoteIds_ = {"atm"};
        request.steps_ = {H};
        request.blocks_ = DalTest::BATCHES;
        request.pathsPerBlock_ = pathsPerBlock;
        request.budget_.maxTotalPaths_ = 3 * request.blocks_ * pathsPerBlock;
        return request;
    }

    MarketIvDupireResult_ Grid(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& axes) {
        const auto grid = CalibrateMarketIvDupire(snapshot, axes);
        REQUIRE(grid.IsSuccess(), grid.Failure());
        return grid;
    }

    double CallPv(const AAD::LocalVolSurface_<double>& surface, PseudoRandom_* scalar) {
        double logSpot = std::log(SPOT);
        for (size_t j = 1; j < TIMES.size(); ++j) {
            const double t = TIMES[j - 1], dt = TIMES[j] - t;
            const double sigma = surface.Vol(t, std::exp(logSpot));
            const double z = InverseNCDF(scalar->NextUniform());
            logSpot += (std::log(FORWARD / SPOT) - .5 * sigma * sigma) * dt + sigma * std::sqrt(dt) * z;
        }
        return DISCOUNT * std::max(std::exp(logSpot) - STRIKE, 0.);
    }

    Blocks_ ReferenceBlocks(const MarketIvDupireResult_& grid, size_t pathsPerBlock) {
        const AAD::LocalVolSurface_<double> surface(grid.SurfaceData());
        // Read consecutive scalar draws across block boundaries to check the compiled sampler's absolute seeks.
        const auto scalar = New(RNGType_("MRG32"), 1024, 1, true);
        Blocks_ blocks{};
        for (double& mean : blocks) {
            double sum = 0.;
            for (size_t p = 0; p < pathsPerBlock; ++p)
                sum += CallPv(surface, scalar.get());
            mean = sum / pathsPerBlock;
        }
        return blocks;
    }

    MarketIvFitResult_ ShiftedFit(const MarketIvCalibrationSnapshot_& base, double shift, const std::optional<String_>& onlyQuote) {
        auto quotes = base.Request().quotes_;
        for (auto& quote : quotes)
            if (!onlyQuote || quote.id_ == *onlyQuote)
                quote.mid_ += shift;
        const auto fit = RefitMarketIvSurface(base, quotes);
        REQUIRE(fit.IsSuccess(), fit.Failure());
        return fit;
    }

    struct ReferenceSide_ {
        Blocks_ blocks_;
        int start_;
        bool changed_;
        [[nodiscard]] double Mean() const { return std::accumulate(blocks_.begin(), blocks_.end(), 0.) / blocks_.size(); }
    };

    ReferenceSide_ ReferenceSide(const MarketIvScriptRefitRequest_& request, double shift) {
        const auto fit = ShiftedFit(request.snapshot_, shift, request.quoteIds_.front());
        const auto snapshot = fit.Snapshot();
        return {ReferenceBlocks(Grid(snapshot, request.grid_), request.pathsPerBlock_), fit.SelectedStart(),
                snapshot.RegimeSignature() != request.snapshot_.RegimeSignature()};
    }

    struct Bucket_ {
        MarketIvScriptRefitPair_ pair_;
        MarketIvScriptSamplingResult_ sample_;
        ReferenceSide_ plus_, minus_;
        double referenceSlope_, referenceSe_;
    };

    Bucket_ RepriceBucket(const MarketIvScriptRefitRequest_& request) {
        const auto plan = PlanMarketIvScriptRefit(request);
        REQUIRE(plan.prepared_->Simulation().compiled_.value_or(false), "Expected a compiled script");
        auto pair = BuildMarketIvScriptRefitPair(plan, 0);
        REQUIRE(pair.plus_.model_, pair.plus_.failure_);
        REQUIRE(pair.minus_.model_, pair.minus_.failure_);
        auto sample =
            SampleMarketIvScriptModelPairs(plan, [&](const auto&) { return MarketIvScriptModelPair_{pair.plus_.model_, pair.minus_.model_}; });
        const double s = request.steps_.front();
        const auto plus = ReferenceSide(request, s), minus = ReferenceSide(request, -s);
        Blocks_ slopes{};
        for (size_t b = 0; b < slopes.size(); ++b)
            slopes[b] = (plus.blocks_[b] - minus.blocks_[b]) / (2. * s);
        const auto [slope, se] = DalTest::Estimate(slopes);
        return {std::move(pair), std::move(sample), plus, minus, slope, se};
    }

    void CheckBlocks(const MarketIvScriptBlockPvs_& actual, const Blocks_& expected) {
        ASSERT_EQ(actual.blockMeans_.size(), expected.size());
        for (size_t b = 0; b < expected.size(); ++b)
            ASSERT_NEAR(actual.blockMeans_[b], expected[b], 1e-12 * std::max(1., std::abs(expected[b]))) << "block " << b;
    }

    void CheckCoordinates(const MarketIvScriptSamplingResult_& sample, size_t pathsPerBlock) {
        ASSERT_EQ(sample.simulationTimes_, TIMES);
        ASSERT_EQ(sample.factor_, "W_EQ");
        ASSERT_FALSE(sample.useBb_);
        ASSERT_EQ(sample.pathsPerBlock_, pathsPerBlock);
        ASSERT_EQ(sample.scenarios_, 3U);
        ASSERT_EQ(sample.totalPaths_, 3 * DalTest::BATCHES * pathsPerBlock);
    }

    void CheckSide(const MarketIvScriptBlockPvs_& pvs, const MarketIvScriptRefitSide_& fit, const ReferenceSide_& reference, double tolerance) {
        ASSERT_EQ(fit.selectedStart_, reference.start_);
        ASSERT_EQ(fit.regimeChanged_, reference.changed_);
        ASSERT_NEAR(pvs.mean_, reference.Mean(), tolerance);
        CheckBlocks(pvs, reference.blocks_);
    }

    void CheckBucket(const MarketIvScriptRefitRequest_& request, const Bucket_& bucket) {
        const double s = request.steps_.front();
        ASSERT_EQ(bucket.pair_.row_.id_, request.quoteIds_.front());
        ASSERT_EQ(bucket.pair_.row_.inputUnit_, "decimal_iv");
        ASSERT_EQ(bucket.pair_.row_.step_, s);
        ASSERT_NO_FATAL_FAILURE(CheckCoordinates(bucket.sample_, request.pathsPerBlock_));
        ASSERT_NO_FATAL_FAILURE(CheckBlocks(bucket.sample_.base_, ReferenceBlocks(Grid(request.snapshot_, request.grid_), request.pathsPerBlock_)));
        ASSERT_EQ(bucket.sample_.rows_.size(), 1U);
        const auto& row = bucket.sample_.rows_.front();
        const double tolerance = 1e-12 * std::max({1., std::abs(bucket.plus_.Mean()), std::abs(bucket.minus_.Mean())});
        ASSERT_NO_FATAL_FAILURE(CheckSide(row.plus_, bucket.pair_.plus_, bucket.plus_, tolerance));
        ASSERT_NO_FATAL_FAILURE(CheckSide(row.minus_, bucket.pair_.minus_, bucket.minus_, tolerance));
        ASSERT_NEAR(row.slope_, bucket.referenceSlope_, tolerance / s);
        ASSERT_NEAR(row.standardError_, bucket.referenceSe_, tolerance / s);
        ASSERT_EQ(bucket.pair_.Status(), bucket.plus_.changed_ || bucket.minus_.changed_ ? "REGIME_CHANGED" : "SUCCESS");
    }

    Handle_<MarketIvLocalVolModelData_> ShiftedFlatModel(const MarketIvScriptRefitRequest_& request, double shift) {
        const auto fit = ShiftedFit(request.snapshot_, shift, std::nullopt);
        const auto snapshot = fit.Snapshot();
        const auto& quotes = snapshot.Request().quotes_;
        REQUIRE(quotes.size() == 3, "Expected all three IV quotes in the refit");
        for (const auto& quote : quotes)
            REQUIRE(quote.quoteType_ == MarketIvQuoteType_::Value_::DECIMAL_IV && quote.mid_ == IV + shift,
                    "Incomplete parallel IV shift at " + quote.id_);
        return BuildMarketIvLocalVolModelData("flat-shift", Grid(snapshot, request.grid_), request.model_);
    }

    double BlackCall(double vol) {
        const double d1 = std::log(FORWARD / STRIKE) / vol + .5 * vol;
        const auto cdf = [](double x) { return .5 * std::erfc(-x / std::sqrt(2.)); };
        return DISCOUNT * (FORWARD * cdf(d1) - STRIKE * cdf(d1 - vol));
    }
} // namespace

TEST(MarketIvRefitPvTest, TestCompiledPathCoordinates) {
    const auto request = Request(1);
    CheckBucket(request, RepriceBucket(request));
}

TEST(MarketIvRefitPvTest, TestSingleQuoteAtBothSteps) {
    auto request = Request(257);
    std::array<double, 2> slopes{}, references{};
    for (size_t tier = 0; tier < slopes.size(); ++tier) {
        const double step = tier == 0 ? H : H / 2.;
        SCOPED_TRACE(::testing::Message() << "quote=atm step=" << step);
        request.steps_ = {step};
        const auto bucket = RepriceBucket(request);
        ASSERT_NO_FATAL_FAILURE(CheckBucket(request, bucket));
        const auto& row = bucket.sample_.rows_.front();
        slopes[tier] = row.slope_;
        references[tier] = bucket.referenceSlope_;
        std::cout << std::setprecision(17) << "step=" << step << " base=" << bucket.sample_.base_.mean_
                  << " compiled(+PV,-PV,slope,SE)=" << row.plus_.mean_ << ' ' << row.minus_.mean_ << ' ' << row.slope_ << ' ' << row.standardError_
                  << " reference=" << bucket.plus_.Mean() << ' ' << bucket.minus_.Mean() << ' ' << bucket.referenceSlope_ << ' '
                  << bucket.referenceSe_ << " starts(+,-)=" << bucket.pair_.plus_.selectedStart_ << ' ' << bucket.pair_.minus_.selectedStart_
                  << " changed(+,-)=" << bucket.pair_.plus_.regimeChanged_ << ' ' << bucket.pair_.minus_.regimeChanged_ << ' '
                  << bucket.pair_.Status() << '\n';
    }
    std::cout << "slope(h/2)-slope(h) compiled=" << slopes[1] - slopes[0] << " reference=" << references[1] - references[0] << '\n';
}

TEST(MarketIvRefitPvTest, TestParallelIvShiftAgainstBlack) {
    const auto request = Request(131072);
    const auto plan = PlanMarketIvScriptRefit(request);
    const double step = request.steps_.front();
    const MarketIvScriptModelPair_ parallel{ShiftedFlatModel(request, step), ShiftedFlatModel(request, -step)};
    const auto sample = SampleMarketIvScriptModelPairs(plan, [&](const auto&) { return parallel; });
    ASSERT_NO_FATAL_FAILURE(CheckCoordinates(sample, request.pathsPerBlock_));
    ASSERT_EQ(sample.rows_.size(), 1U);
    const auto& row = sample.rows_.front();
    ASSERT_EQ(row.plus_.blockMeans_.size(), DalTest::BATCHES);
    ASSERT_EQ(row.minus_.blockMeans_.size(), DalTest::BATCHES);
    const double black = (BlackCall(IV + step) - BlackCall(IV - step)) / (2. * step);
    const double normalizedSe = row.standardError_ / SPOT, error = std::abs(row.slope_ - black) / SPOT;
    const double limit = 1e-3 + 4.5 * normalizedSe;
    std::cout << std::setprecision(17) << "parallel IV estimate=" << row.slope_ << " Black=" << black << " SE=" << row.standardError_
              << " SE/S0=" << normalizedSe << " abs(estimate-Black)/S0=" << error << " limit=" << limit << " total paths=" << sample.totalPaths_
              << '\n';
    ASSERT_GT(normalizedSe, 0.);
    ASSERT_LE(normalizedSe, 1e-3) << "NOT_QUALIFIED: fixed path budget exhausted";
    ASSERT_LE(error, limit) << "NOT_QUALIFIED: parallel IV direction misses Black";
}
