#include <gtest/gtest.h>

#include <numeric>
#include <type_traits>

#include <dal/math/aad/recording.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/script/marketivlocalvol.hpp>

#include "script_test_observers.hpp"

using namespace Dal;
using namespace Dal::Script;

namespace {
    MarketIvScriptRefitRequest_ Request(bool premiumBand = false, double volatility = .2) {
        const Date_ today(2026, 11, 16), expiry = today.AddDays(365);
        const MarketIvMarket_ market(today, 100., {{expiry, 105., .96}});
        const OptionType_ call("CALL");
        MarketIvFitRequest_ fit{market, {}, {}};
        for (const double strike : {90., 100., 110.}) {
            const bool premium = strike == 110.;
            const double mid = premium ? .96 * 105. * MarketIvDetail::NormalizedBlack(std::log(strike / 105.), volatility * volatility) : volatility;
            fit.quotes_.push_back({String_(std::to_string(static_cast<int>(strike))), expiry, strike, call, MarketIvQuoteType_(premium ? "PRICE" : "DECIMAL_IV"), mid});
            if (premium && premiumBand) {
                fit.quotes_.back().bid_ = fit.quotes_.back().mid_;
                fit.quotes_.back().ask_ = 16.;
            }
        }
        const auto result = FitMarketIvSurface(fit);
        REQUIRE(result.IsSuccess(), result.Failure());
        MarketIvScriptRefitRequest_ request{result.Snapshot(), {{80., 100., 120.}, {.1, 1.}, {}}, {"EQ[A]", "USD", "W_EQ", 1.},
                                            Handle_<ScriptProductData_>(new ScriptProductData_("call", {Cell_(expiry)}, {"pay PAYS MAX(spot() - 100, 0)"}))};
        request.valuation_.evaluationDate_ = today;
        request.quoteIds_ = {"110", "90"};
        return request;
    }

    MarketIvScriptRefitRequest_ TwoStepRequest() {
        auto request = Request();
        request.grid_.spots_ = {100.};
        request.grid_.times_ = {.25};
        request.model_.maxStep_ = 1.;
        request.product_ = Handle_<ScriptProductData_>(new ScriptProductData_("stock", request.product_->Dates(), {"pay PAYS spot()"}));
        request.quoteIds_ = {"90"};
        request.steps_ = {.01};
        request.blocks_ = 3;
        request.pathsPerBlock_ = 2;
        return request;
    }

    Vector_<> EnumeratedPvs(double vol, bool bridge) {
        // Seed 1024, consecutive MRG32 draws transformed independently with the normal quantile.
        const double normals[][2] = {{-.4055684161278695, .7560724540882088}, {.8931393704012665, -.030318933107075754},
                                     {.7600511022612914, .5440022882119729},  {1.9421280634017497, -.16742512224731482},
                                     {1.1872376013353831, .432597484966376},  {-.2192065064927242, -.43031290384969717}};
        Vector_<> pvs;
        for (const auto& z : normals) {
            const double first = bridge ? (z[0] + z[1]) / std::sqrt(2.) : z[0];
            const double second = bridge ? (z[0] - z[1]) / std::sqrt(2.) : z[1];
            pvs.push_back(.96 * 105. * std::exp(vol * (.5 * first + std::sqrt(.75) * second) - .5 * vol * vol));
        }
        return pvs;
    }

    void CheckEnumeratedBlockPvs(
        const MarketIvScriptSamplingResult_& result, const Vector_<>& basePvs, const Vector_<>& plus, const Vector_<>& minus, bool record) {
        const size_t blockSize = result.pathsPerBlock_, blocks = basePvs.size() / blockSize;
        const auto& row = result.rows_.front();
        ASSERT_EQ(result.base_.blockMeans_.size(), blocks);
        ASSERT_EQ(row.plus_.blockMeans_.size(), blocks);
        ASSERT_EQ(row.minus_.blockMeans_.size(), blocks);
        Vector_<> slopes;
        for (size_t b = 0; b < blocks; ++b) {
            const auto average = [&](const Vector_<>& paths) {
                const auto first = paths.begin() + b * blockSize;
                return std::accumulate(first, first + blockSize, 0.) / blockSize;
            };
            ASSERT_NEAR(result.base_.blockMeans_[b], average(basePvs), 1e-10);
            ASSERT_NEAR(row.plus_.blockMeans_[b], average(plus), 1e-10);
            ASSERT_NEAR(row.minus_.blockMeans_[b], average(minus), 1e-10);
            slopes.push_back((average(plus) - average(minus)) / .02);
        }
        const double mean = std::accumulate(slopes.begin(), slopes.end(), 0.) / slopes.size();
        double sumSquares = 0.;
        for (const double slope : slopes)
            sumSquares += (slope - mean) * (slope - mean);
        ASSERT_NEAR(row.slope_, mean, 1e-9);
        ASSERT_NEAR(row.standardError_, std::sqrt(sumSquares / (slopes.size() - 1) / slopes.size()), 1e-9);
        ASSERT_NEAR(row.plus_.mean_, std::accumulate(plus.begin(), plus.end(), 0.) / 6., 1e-10);
        ASSERT_NEAR(row.minus_.mean_, std::accumulate(minus.begin(), minus.end(), 0.) / 6., 1e-10);
        if (record) {
            for (size_t b = 0; b < slopes.size(); ++b) {
                const auto suffix = std::to_string(b);
                ::testing::Test::RecordProperty("base" + suffix, ToString(result.base_.blockMeans_[b]).c_str());
                ::testing::Test::RecordProperty("plus" + suffix, ToString(row.plus_.blockMeans_[b]).c_str());
                ::testing::Test::RecordProperty("minus" + suffix, ToString(row.minus_.blockMeans_[b]).c_str());
                ::testing::Test::RecordProperty("slope" + suffix, ToString(slopes[b]).c_str());
            }
            ::testing::Test::RecordProperty("meanSlope", ToString(row.slope_).c_str());
            ::testing::Test::RecordProperty("pairedSE", ToString(row.standardError_).c_str());
        }
    }

    struct SamplingCapacity_ final : Dal::Detail::OwnedBufferAccount_ {
        size_t live_ = 0, peak_ = 0;
        void Reserve(size_t bytes) override {
            live_ += bytes;
            peak_ = std::max(peak_, live_);
        }
        void Release(size_t bytes) noexcept override { live_ -= bytes; }
    };

    MarketIvScriptSamplingResult_ SampleBaseOnly(const MarketIvScriptRefitPlan_& plan) {
        return SampleMarketIvScriptModelPairs(plan, [&](const auto&) { return MarketIvScriptModelPair_{plan.baseModel_, plan.baseModel_}; });
    }
} // namespace

TEST(MarketIvScriptRefitTest, TestMixedQuoteOrderAndSteps) {
    auto request = Request();
    request.steps_ = {.012, .0003};
    const auto explicitPlan = PlanMarketIvScriptRefit(request);
    const auto& rows = explicitPlan.rows_;
    ASSERT_EQ(rows.size(), 2U);
    ASSERT_EQ(rows[0].id_, "90");
    ASSERT_EQ(rows[0].quoteIndex_, 0U);
    ASSERT_EQ(rows[0].quoteType_, MarketIvQuoteType_::Value_::DECIMAL_IV);
    ASSERT_EQ(rows[0].inputUnit_, "decimal_iv");
    ASSERT_EQ(rows[0].step_, .0003);
    ASSERT_EQ(rows[1].id_, "110");
    ASSERT_EQ(rows[1].quoteIndex_, 2U);
    ASSERT_EQ(rows[1].quoteType_, MarketIvQuoteType_::Value_::PRICE);
    ASSERT_EQ(rows[1].inputUnit_, "premium");
    ASSERT_EQ(rows[1].step_, .012);
    ASSERT_DOUBLE_EQ(rows[1].forward_, 105.);
    ASSERT_DOUBLE_EQ(rows[1].discount_, .96);

    request.steps_.clear();
    const auto defaults = PlanMarketIvScriptRefit(request);
    ASSERT_EQ(defaults.rows_[0].step_, 1e-4);
    ASSERT_DOUBLE_EQ(defaults.rows_[1].step_, 1e-4 * .96 * 105.);
    ASSERT_EQ(explicitPlan.request_.steps_.size(), 2U);
}

TEST(MarketIvScriptRefitTest, TestFixedPairsShareOneBaseAndDisjointBlocks) {
    auto request = Request();
    request.blocks_ = 3;
    request.pathsPerBlock_ = 2;
    request.budget_.maxScenarios_ = 5;
    request.budget_.maxTotalPaths_ = 30;
    request.budget_.workspaceBudgetBytes_ = PlanMarketIvScriptRefit(request).work_.reservedWorkspaceBytes_;
    const auto plan = PlanMarketIvScriptRefit(request);
    Vector_<String_> quoted;
    size_t fits = 0;
    const MarketIvDetail::ScopedFitStartCounter_ observeFits(&fits);
    const auto result = SampleMarketIvScriptModelPairs(plan, [&](const auto& quote) {
        quoted.push_back(quote.id_);
        return MarketIvScriptModelPair_{plan.baseModel_, plan.baseModel_};
    });
    ASSERT_EQ(quoted, (Vector_<String_>{"90", "110"}));
    ASSERT_EQ(fits, 0U);
    ASSERT_EQ(result.base_.blockMeans_.size(), 3U);
    ASSERT_NE(result.base_.blockMeans_[0], result.base_.blockMeans_[1]);
    ASSERT_EQ(result.rows_.size(), 2U);
    for (const auto& row : result.rows_) {
        ASSERT_EQ(row.plus_.blockMeans_, result.base_.blockMeans_);
        ASSERT_EQ(row.minus_.blockMeans_, result.base_.blockMeans_);
        ASSERT_EQ(row.slope_, 0.);
        ASSERT_EQ(row.standardError_, 0.);
    }
    ASSERT_EQ(result.scenarios_, 5U);
    ASSERT_EQ(result.totalPaths_, 30U);
    ASSERT_EQ(result.refits_, 0U);
    ASSERT_EQ(result.grids_, 0U);
    ASSERT_GT(result.peakWorkspaceBytes_, 0U);
    ASSERT_LE(result.peakWorkspaceBytes_, plan.work_.reservedWorkspaceBytes_);
    RecordProperty("samplingPeakBytes", std::to_string(result.peakWorkspaceBytes_));
}

TEST(MarketIvScriptRefitTest, TestEnumeratedPathsAndPairedStandardError) {
    auto request = TwoStepRequest();
    const auto grid = [&](const auto& snapshot) { return CalibrateMarketIvDupire(snapshot, request.grid_); };
    const auto base = grid(request.snapshot_), up = grid(Request(false, .21).snapshot_), down = grid(Request(false, .19).snapshot_);
    const MarketIvScriptModelPair_ models{BuildMarketIvLocalVolModelData("up", up, request.model_),
                                          BuildMarketIvLocalVolModelData("down", down, request.model_)};
    for (bool bridge : {false, true}) {
        const auto reference = [&](const auto& scenario) { return EnumeratedPvs(scenario.SurfaceData().vols_(0, 0), bridge); };
        const auto basePvs = reference(base), plus = reference(up), minus = reference(down);
        for (bool compiled : {true, false})
            for (size_t blockSize : {1U, 2U}) {
                SCOPED_TRACE(::testing::Message() << "compiled=" << compiled << " bridge=" << bridge << " paths per block=" << blockSize);
                request.simulation_.useBb_ = bridge;
                request.simulation_.compiled_ = compiled;
                request.blocks_ = 6 / blockSize;
                request.pathsPerBlock_ = blockSize;
                const auto plan = PlanMarketIvScriptRefit(request);
                const auto result = SampleMarketIvScriptModelPairs(plan, [&](const auto&) { return models; });
                ASSERT_EQ(result.simulationTimes_, (Vector_<>{0., .25, 1.}));
                ASSERT_EQ(result.factor_, "W_EQ");
                ASSERT_EQ(result.useBb_, bridge);
                ASSERT_EQ(result.pathsPerBlock_, blockSize);
                ASSERT_EQ(result.rows_.size(), 1U);
                CheckEnumeratedBlockPvs(result, basePvs, plus, minus, !bridge && compiled && blockSize == 2);
            }
    }
}

TEST(MarketIvScriptRefitTest, TestSwappedIntervalsIdentifySideAndPreserveCallerTape) {
    auto request = TwoStepRequest();
    const auto plan = PlanMarketIvScriptRefit(request);
    request.grid_.times_ = {.75}; // Exchange dt=1/4 and dt=3/4, preserving SimDim, factor and RNG.
    const auto swapped = BuildMarketIvLocalVolModelData("swapped", CalibrateMarketIvDupire(request.snapshot_, request.grid_), request.model_);
    AAD::RecordingScope_ caller;
    caller.StartRecording();
    AAD::Number_ input(3.);
    AAD::Number_ output = input * input;
    const auto nodes = AAD::Tape()->nodes_.Size();
    const auto reverses = AAD::Tape()->ReverseEventCount();
    for (bool mismatch : {false, true}) {
        try {
            const auto result = SampleMarketIvScriptModelPairs(
                plan, [&](const auto&) { return MarketIvScriptModelPair_{plan.baseModel_, mismatch ? swapped : plan.baseModel_}; });
            ASSERT_FALSE(mismatch);
            ASSERT_EQ(result.totalPaths_, 18U);
        } catch (const MarketIvLocalVolError_& e) {
            ASSERT_TRUE(mismatch);
            ASSERT_EQ(e.code_, "MarketIvCoordinateMismatch");
            ASSERT_EQ(e.field_, "/models/90/minus/simulation/times/1");
            ASSERT_DOUBLE_EQ(std::stod(e.expected_.c_str()), .25);
            ASSERT_DOUBLE_EQ(std::stod(e.actual_.c_str()), .75);
        }
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), nodes);
        ASSERT_EQ(AAD::Tape()->ReverseEventCount(), reverses);
    }
    caller.FinishRecording();
    caller.ClearAdjoints();
    Adjoint(output) = 1.;
    caller.Reverse();
    ASSERT_DOUBLE_EQ(Adjoint(input), 6.);
}

TEST(MarketIvScriptRefitTest, TestSideCannotExpandFrozenWorkspaceEnvelope) {
    auto request = TwoStepRequest();
    const auto plan = PlanMarketIvScriptRefit(request);
    ++request.model_.workspaceBudgetBytes_;
    const auto larger = BuildMarketIvLocalVolModelData("larger", CalibrateMarketIvDupire(request.snapshot_, request.grid_), request.model_);
    try {
        static_cast<void>(SampleMarketIvScriptModelPairs(plan, [&](const auto&) { return MarketIvScriptModelPair_{larger, plan.baseModel_}; }));
        FAIL() << "Expected the frozen model workspace envelope";
    } catch (const MarketIvLocalVolError_& error) {
        ASSERT_EQ(error.code_, "MarketIvLocalVolBudgetExceeded");
        ASSERT_EQ(error.stage_, "Sample");
        ASSERT_EQ(error.field_, "/models/90/plus/settings/workspace_budget_bytes");
    }
}

TEST(MarketIvScriptRefitTest, TestFrozenMarketRejectsSideDrift) {
    auto request = TwoStepRequest();
    request.product_ = Handle_<ScriptProductData_>(new ScriptProductData_("cash", request.product_->Dates(), {"pay PAYS 1"}));
    const auto plan = PlanMarketIvScriptRefit(request);
    const Date_ today = *request.valuation_.evaluationDate_, expiry = today.AddDays(365);
    const auto reject = [&](double spot, double forward, double discount, const String_& field) {
        auto fit = request.snapshot_.Request();
        fit.market_ = MarketIvMarket_(today, spot, {{expiry, forward, discount}});
        fit.quotes_.back().mid_ = discount * forward * MarketIvDetail::NormalizedBlack(std::log(110. / forward), .04);
        const auto fitted = FitMarketIvSurface(fit);
        ASSERT_TRUE(fitted.IsSuccess());
        const auto side = BuildMarketIvLocalVolModelData("drift", CalibrateMarketIvDupire(fitted.Snapshot(), request.grid_), request.model_);
        try {
            const auto result = SampleMarketIvScriptModelPairs(plan, [&](const auto&) { return MarketIvScriptModelPair_{plan.baseModel_, side}; });
            FAIL() << "Accepted market drift with slope " << result.rows_.front().slope_;
        } catch (const MarketIvLocalVolError_& e) {
            ASSERT_EQ(e.code_, "MarketIvSourceMismatch");
            ASSERT_EQ(e.stage_, "Sample");
            ASSERT_EQ(e.field_, "/models/90/minus/market/" + field);
        }
    };
    reject(100., 105., .95, "terms/0/discount");
    reject(100., 104., .96, "terms/0/forward");
    reject(101., 105., .96, "spot");
}

TEST(MarketIvScriptRefitTest, TestPathCountFollowsAdmittedBatches) {
    auto request = TwoStepRequest();
    request.budget_.maxTotalPaths_ = 18;
    const auto plan = PlanMarketIvScriptRefit(request);
    request.pathsPerBlock_ = 4;
    ASSERT_THROW(static_cast<void>(PlanMarketIvScriptRefit(request)), MarketIvLocalVolError_);
    ASSERT_FALSE(std::is_aggregate_v<MarketIvScriptRefitPlan_>);
    ASSERT_FALSE((std::is_constructible_v<MarketIvScriptRefitPlan_, decltype(request), decltype(plan.rows_), decltype(plan.work_),
                                          decltype(plan.baseModel_), decltype(plan.prepared_)>));
    const auto result = SampleBaseOnly(plan);
    ASSERT_EQ(result.totalPaths_, 18U);
    RecordProperty("pathsAtBudget18", std::to_string(result.totalPaths_));
    request.budget_.maxTotalPaths_ = 36;
    const auto larger = PlanMarketIvScriptRefit(request);
    const auto expanded = SampleBaseOnly(larger);
    ASSERT_EQ(expanded.totalPaths_, 36U);
    RecordProperty("pathsAtBudget36", std::to_string(expanded.totalPaths_));
}

TEST(MarketIvScriptRefitTest, TestSamplingPeakMatchesLiveBuffers) {
    auto request = TwoStepRequest();
    request.grid_.spots_.Resize(801);
    for (size_t i = 0; i < request.grid_.spots_.size(); ++i)
        request.grid_.spots_[i] = 80. + i * .05;
    const auto plan = PlanMarketIvScriptRefit(request);
    const auto construction = plan.baseModel_->ConstructionExecution();
    SamplingCapacity_ measured;
    size_t reported;
    {
        const Dal::Detail::OwnedBufferScope_ meter(&measured);
        const auto result = SampleBaseOnly(plan);
        reported = result.peakWorkspaceBytes_;
    }
    ASSERT_EQ(measured.live_, 0U);
    // OwnedBufferScope observes vector storage. The capacity ledger also charges the heap-allocated
    // hybrid model and its components, which remain live from preparation through the last path.
    SamplingCapacity_ preparation;
    size_t modelObjects, preparationPeak;
    {
        const Dal::Detail::OwnedBufferScope_ meter(&preparation);
        AAD::MarketIvLocalVolModel_ model(*plan.baseModel_);
        model.ValidateRequest(false, false, request.valuation_.evaluationDate_, request.simulation_.rsg_);
        model.Allocate(plan.prepared_->TimeLine(), plan.prepared_->DefLine());
        model.Init(plan.prepared_->TimeLine(), plan.prepared_->DefLine());
        const auto shared = model.Execution().sharedNumericBytes_;
        RecordProperty("sharedModel", std::to_string(shared));
        RecordProperty("preparedVectors", std::to_string(preparation.live_));
        RecordProperty("prepareVectorPeak", std::to_string(preparation.peak_));
        RecordProperty("worker", std::to_string(model.Execution().perWorkerNumericBytes_));
        ASSERT_GT(shared, preparation.live_);
        modelObjects = shared - preparation.live_;
        preparationPeak = model.PreparationPeakBytes();
    }
    ASSERT_EQ(preparation.live_, 0U);
    ASSERT_EQ(preparationPeak, preparation.peak_ + modelObjects);
    RecordProperty("modelPayload", std::to_string(construction.numericPayloadBytes_));
    RecordProperty("historicalConstructionPeak", std::to_string(construction.peakWorkspaceBytes_));
    RecordProperty("samplingVectorPeak", std::to_string(measured.peak_));
    RecordProperty("modelObjects", std::to_string(modelObjects));
    RecordProperty("preparationPeak", std::to_string(preparationPeak));
    RecordProperty("reportedPeak", std::to_string(reported));
    ASSERT_EQ(reported, construction.numericPayloadBytes_ + measured.peak_ + modelObjects);
}

TEST(MarketIvScriptRefitTest, TestRepresentableMeanOfLargeCashPayments) {
    auto request = TwoStepRequest();
    request.product_ = Handle_<ScriptProductData_>(new ScriptProductData_("large", request.product_->Dates(), {"pay PAYS 1E308"}));
    const auto plan = PlanMarketIvScriptRefit(request);
    const auto result = SampleBaseOnly(plan);
    for (double pv : result.base_.blockMeans_)
        ASSERT_NEAR(pv / 1e308, .96, 1e-14);
    ASSERT_NEAR(result.base_.mean_ / 1e308, .96, 1e-14);
    ASSERT_EQ(result.rows_[0].slope_, 0.);
    ASSERT_EQ(result.rows_[0].standardError_, 0.);
}

TEST(MarketIvScriptRefitTest, TestSamplingUsesCanonicalModelCurrency) {
    auto request = TwoStepRequest();
    request.model_.currency_ = " USD ";
    const auto plan = PlanMarketIvScriptRefit(request);
    ASSERT_EQ(plan.baseModel_->Configuration().currency_, "USD");
    const auto same = SampleBaseOnly(plan);
    ASSERT_EQ(same.totalPaths_, 18U);
    request.model_.currency_ = "EUR";
    const auto other = BuildMarketIvLocalVolModelData("other", CalibrateMarketIvDupire(request.snapshot_, request.grid_), request.model_);
    ASSERT_THROW(
        static_cast<void>(SampleMarketIvScriptModelPairs(plan, [&](const auto&) { return MarketIvScriptModelPair_{other, plan.baseModel_}; })),
        Exception_);
}

TEST(MarketIvScriptRefitTest, TestPreflightRejectsInvalidFieldsBeforeWork) {
    size_t fitStarts = 0;
    const MarketIvDetail::ScopedFitStartCounter_ observeFits(&fitStarts);
    const auto base = Request();
    ASSERT_EQ(fitStarts, 1U);
    fitStarts = 0;
    TestSupport::SubmissionCounter_ sampling;
    const Script::Detail::ScopedSimulationObserver_ observeSampling(&sampling);
    const auto reject = [&](const char* field, auto change) {
        SCOPED_TRACE(field);
        auto request = base;
        change(&request);
        try {
            static_cast<void>(PlanMarketIvScriptRefit(request));
            FAIL() << "Expected preflight failure for " << field;
        } catch (const Exception_& e) {
            ASSERT_NE(std::string(e.what()).find(field), std::string::npos) << e.what();
        }
        ASSERT_EQ(fitStarts, 0U);
        ASSERT_EQ(sampling.submissions_, 0U);
    };
    reject("/quote_ids", [](auto* r) { r->quoteIds_ = {"90", "90"}; });
    reject("/quote_ids", [](auto* r) { r->quoteIds_ = {"100", "missing"}; });
    reject("/steps", [](auto* r) { r->steps_ = {1.}; });
    for (double h : {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::denorm_min()})
        reject("/steps", [h](auto* r) { r->steps_ = {h, .01}; });
    reject("/valuation/evaluation_date", [](auto* r) { r->valuation_.evaluationDate_ = r->valuation_.evaluationDate_->AddDays(1); });
    reject("/simulation/enable_aad", [](auto* r) { r->simulation_.enableAad_ = true; });
    reject("/product/exercise", [](auto* r) {
        r->product_ = Handle_<ScriptProductData_>(new ScriptProductData_("exercise", r->product_->Dates(), {"EXERCISE MAX(spot() - 100, 0)"}));
    });
    reject("/budget/max_scenarios", [](auto* r) { r->budget_.maxScenarios_ = 4; });
    reject("/budget/workspace_bytes", [](auto* r) { r->budget_.workspaceBudgetBytes_ = 0; });
    reject("/budget/max_total_paths", [](auto* r) { r->budget_.maxTotalPaths_ = 81919; });
    reject("/budget/max_model_row_evaluations", [](auto* r) { r->budget_.maxModelRowEvaluations_ = 19999999; });
    reject("/simulation/rsg", [](auto* r) { r->simulation_.rsg_ = "sobol"; });
    reject("/simulation/normal_precision", [](auto* r) { r->simulation_.normalPrecision_ = "Fast"; });
    reject("/blocks", [](auto* r) { r->blocks_ = 1; });
    reject("/paths_per_block", [](auto* r) { r->pathsPerBlock_ = 0; });
}

TEST(MarketIvScriptRefitTest, TestBothBumpsMustMoveTheWholeBand) {
    auto request = Request(true);
    request.steps_ = {1e-15, .0003};
    const auto& quote = request.snapshot_.Request().quotes_[2];
    ASSERT_NE(quote.mid_ + request.steps_[0], quote.mid_);
    ASSERT_EQ(*quote.ask_ + request.steps_[0], *quote.ask_);
    ASSERT_THROW(static_cast<void>(PlanMarketIvScriptRefit(request)), Exception_);
}

TEST(MarketIvScriptRefitTest, TestBudgetOverflowsIdentifyPreflightFieldBeforeWork) {
    const auto base = Request();
    const auto maximum = std::numeric_limits<size_t>::max();
    const auto rejectOverflow = [&](const char* operation, const char* field, auto configure) {
        SCOPED_TRACE(operation);
        auto request = base;
        configure(&request);
        size_t fitStarts = 0;
        const MarketIvDetail::ScopedFitStartCounter_ observeFits(&fitStarts);
        TestSupport::SubmissionCounter_ sampling;
        const Script::Detail::ScopedSimulationObserver_ observeSampling(&sampling);
        try {
            static_cast<void>(PlanMarketIvScriptRefit(request));
            FAIL() << "Expected budget overflow";
        } catch (const MarketIvLocalVolError_& error) {
            EXPECT_STREQ(error.code_.c_str(), "MarketIvLocalVolBudgetExceeded");
            EXPECT_STREQ(error.stage_.c_str(), "Preflight");
            EXPECT_STREQ(error.field_.c_str(), field);
            EXPECT_STREQ(error.actual_.c_str(), "overflow");
        }
        EXPECT_EQ(fitStarts, 0U);
        EXPECT_EQ(sampling.submissions_, 0U);
    };

    rejectOverflow("paths per scenario", "/budget/max_total_paths", [&](auto* r) {
        r->blocks_ = 2;
        r->pathsPerBlock_ = maximum;
    });
    rejectOverflow("paths across all scenarios", "/budget/max_total_paths", [&](auto* r) {
        r->blocks_ = 2;
        r->pathsPerBlock_ = maximum / 2;
        r->budget_.maxTotalPaths_ = maximum;
    });
    auto fitRequest = base.snapshot_.Request();
    fitRequest.settings_.maxModelRowEvaluations_ = maximum;
    const auto fit = FitMarketIvSurface(fitRequest);
    ASSERT_TRUE(fit.IsSuccess()) << fit.Failure().c_str();
    rejectOverflow("fit rows across refits", "/budget/max_model_row_evaluations", [&](auto* r) { r->snapshot_ = fit.Snapshot(); });
    rejectOverflow("three model payloads", "/budget/workspace_bytes", [&](auto* r) { r->model_.numericPayloadBudgetBytes_ = maximum; });
    rejectOverflow("grid plus model payload", "/budget/workspace_bytes", [&](auto* r) { r->grid_.settings_.numericPayloadBudgetBytes_ = maximum; });
}

TEST(MarketIvScriptRefitTest, TestPlanOwnsInputsAndAdmitsExactBudget) {
    const auto plan = [] {
        auto request = Request();
        request.blocks_ = 2;
        request.pathsPerBlock_ = 3;
        request.budget_.maxScenarios_ = 5;
        request.budget_.maxTotalPaths_ = 30;
        request.budget_.maxModelRowEvaluations_ = 20000000;
        const auto initial = PlanMarketIvScriptRefit(request);
        request.budget_.workspaceBudgetBytes_ = initial.work_.reservedWorkspaceBytes_;
        const auto exact = PlanMarketIvScriptRefit(request);
        --request.budget_.workspaceBudgetBytes_;
        EXPECT_THROW(static_cast<void>(PlanMarketIvScriptRefit(request)), Exception_);
        request.grid_.spots_[0] = 1.;
        request.model_.maxStep_ = .5;
        request.simulation_.useBb_ = true;
        return exact;
    }();
    ASSERT_EQ(plan.rows_[0].id_, "90");
    ASSERT_EQ(plan.request_.product_->EventTexts().front(), "pay PAYS MAX(spot() - 100, 0)");
    ASSERT_EQ(plan.request_.grid_.spots_, (Vector_<>{80., 100., 120.}));
    ASSERT_EQ(plan.request_.grid_.times_, (Vector_<>{.1, 1.}));
    ASSERT_EQ(plan.request_.model_.maxStep_, 1. / 52.);
    ASSERT_FALSE(plan.request_.simulation_.useBb_);
    ASSERT_TRUE(plan.baseModel_);
    ASSERT_EQ(plan.work_.refits_, 4U);
    ASSERT_EQ(plan.work_.grids_, 5U);
    ASSERT_EQ(plan.work_.scenarios_, 5U);
    ASSERT_EQ(plan.work_.pathsPerScenario_, 6U);
    ASSERT_EQ(plan.work_.totalPaths_, 30U);
    ASSERT_EQ(plan.work_.modelRowEvaluationLimit_, 20000000U);
    ASSERT_EQ(plan.work_.reservedWorkspaceBytes_, plan.request_.budget_.workspaceBudgetBytes_);
}

TEST(MarketIvScriptRefitTest, TestMovedRequestCannotChangePlanThroughSavedReferences) {
    auto request = Request();
    request.steps_ = {.012, .0003};
    auto& time = request.grid_.times_.front();
    auto& step = request.steps_.front();
    auto& id = request.quoteIds_.front();

    const auto plan = PlanMarketIvScriptRefit(std::move(request));
    time = .2;
    step = .04;
    id = "100";

    const auto summary = plan.baseModel_->Summary();
    ASSERT_EQ(summary.gridFirstTime_, .1);
    EXPECT_EQ(plan.request_.grid_.times_.front(), summary.gridFirstTime_);
    ASSERT_EQ(plan.rows_.size(), 2U);
    ASSERT_EQ(plan.rows_[1].id_, "110");
    ASSERT_EQ(plan.rows_[1].step_, .012);
    EXPECT_EQ(plan.request_.quoteIds_.front(), plan.rows_[1].id_);
    EXPECT_EQ(plan.request_.steps_.front(), plan.rows_[1].step_);
}

TEST(MarketIvScriptRefitTest, TestPlanFreezesGlobalFixingAndValuationDate) {
    auto request = Request();
    const Date_ today = *request.valuation_.evaluationDate_;
    const auto dateScope = XGLOBAL::SetEvaluationDateInScope(today);
    request.valuation_.evaluationDate_.reset();
    const String_ index = "EQ[DAL561_HISTORY]";
    const XGLOBAL::ScopedOverride_<FixHistory_> restore([](const FixHistory_& saved) { XGLOBAL::StoreFixings("EQ[DAL561_HISTORY]", saved, false); },
                                                        Global::Fixings_().History(index));
    const DateTime_ fixingTime(today.AddDays(-1), 0.);
    request.product_ = Handle_<ScriptProductData_>(new ScriptProductData_("fixed", request.product_->Dates(), {"pay PAYS FIX(" + index + ", 2026-11-15)"}));
    TestSupport::StoreScriptTestFixing(index, 87., fixingTime);
    const auto plan = PlanMarketIvScriptRefit(request);
    TestSupport::StoreScriptTestFixing(index, 91., fixingTime);
    const auto changed = PlanMarketIvScriptRefit(request);
    ASSERT_EQ(plan.request_.valuation_.evaluationDate_, today);
    ASSERT_TRUE(plan.request_.valuation_.fixings_);
    ASSERT_EQ(plan.request_.valuation_.fixings_->Find(index, fixingTime), 87.);
    ASSERT_EQ(changed.request_.valuation_.fixings_->Find(index, fixingTime), 91.);
    request.valuation_.fixings_ = plan.request_.valuation_.fixings_;
    ASSERT_EQ(PlanMarketIvScriptRefit(request).request_.valuation_.fixings_, request.valuation_.fixings_);
}
