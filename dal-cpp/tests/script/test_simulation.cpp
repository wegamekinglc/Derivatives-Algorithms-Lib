//
// Created by wegamekinglc on 2026/8/15.
//
// Coverage for dal/script/simulation.hpp: determinism under fixed-seed RNGs,
// payoff aggregation against analytic expectations, and error/edge paths.

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <limits>

#include <dal/curve/tapeguard.hpp>
#include <dal/math/aad/profiling.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    ScriptProduct_ VanillaCallProduct(const Date_& exerciseDate, double strike) {
        Vector_<Cell_> eventDates{Cell_(String_("STRIKE")), Cell_(exerciseDate)};
        Vector_<String_> events{ToString(strike), "call pays MAX(spot() - STRIKE, 0.0)"};
        return ScriptProduct_(eventDates, events);
    }

    Handle_<ModelData_> StandardBSModel() { return Handle_<ModelData_>(new BSModelData_("bsmodel", 10.0, 0.20, 0.034, 0.021)); }

    struct SimulationObservation_ {
        double serialFirst_;
        double serialSecond_;
        double parallelFirst_;
        double parallelSecond_;
        double repeatedFirst_;
    };
} // namespace

#if defined(DAL_ENABLE_AAD_PROFILING)
TEST(SimulationTest, TestExplicitPassiveProfilingCapturesEachPathWithoutTapeSamples) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    const auto model = StandardBSModel();
    constexpr size_t N_PATHS = 67;
    for (const bool compiled : {false, true}) {
        ScriptProduct_ product({Cell_(Date_(2024, 6, 21))}, {"payoff PAYS SPOT()"});
        product.PreProcess(false, false);
        const auto expected = MCSimulation<double>(product, model, N_PATHS, "sobol", false, compiled);
        AAD::ProfilingData_ data;
        const auto actual = [&] {
            AAD::ProfilingScope_ profile(&data);
            return MCSimulation<double>(product, model, N_PATHS, "sobol", false, compiled);
        }();
        ASSERT_TRUE(data.complete_);
        ASSERT_DOUBLE_EQ(actual.aggregated_, expected.aggregated_);
        ASSERT_EQ(actual.risks_, expected.risks_);
        ASSERT_EQ(data.taskGroups_.size(), 1);
        ASSERT_EQ(data.Phase(AAD::AADProfilingPhase_::Value_::WAIT).calls_, 1);
        ASSERT_EQ(data.Phase(AAD::AADProfilingPhase_::Value_::REDUCE).calls_, 1);
        std::uint64_t forward = 0, payoff = 0;
        for (const auto& task : data.taskGroups_.front()) {
            ASSERT_TRUE(task.complete_);
            ASSERT_EQ(task.tapeSamples_, 0);
            forward += task.Phase(AAD::AADProfilingPhase_::Value_::PATH_FORWARD).calls_;
            payoff += task.Phase(AAD::AADProfilingPhase_::Value_::PAYOFF).calls_;
        }
        ASSERT_EQ(forward, N_PATHS);
        ASSERT_EQ(payoff, N_PATHS);
    }
}

TEST(SimulationTest, TestProfilingTaskFailureDrainsEveryAcceptedTaskBeforeRequestEnds) {
    std::atomic<size_t> completed{0};
    AAD::ProfilingData_ data;
    const auto fail = [&] {
        AAD::ProfilingScope_ profile(&data);
        SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), 2);
        tasks.Spawn([]() -> bool {
            AAD::ProfilingSpan_ span(AAD::AADProfilingPhase_::Value_::PAYOFF);
            THROW("profiled business failure");
        });
        tasks.Spawn([&] {
            ++completed;
            return true;
        });
        tasks.Complete();
    };
    ASSERT_THROW(fail(), Exception_);
    ASSERT_EQ(completed.load(), 1);
    ASSERT_FALSE(data.complete_);
    ASSERT_EQ(data.taskGroups_.size(), 1);
    ASSERT_FALSE(data.taskGroups_.front()[0].complete_);
    ASSERT_TRUE(data.taskGroups_.front()[1].complete_);
    AAD::ProfilingData_ next;
    {
        AAD::ProfilingScope_ profile(&next);
        SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), 1);
        tasks.Spawn([&] {
            ++completed;
            return true;
        });
        tasks.Complete();
    }
    ASSERT_EQ(completed.load(), 2);
    ASSERT_TRUE(next.complete_);
}

TEST(SimulationTest, TestExplicitAadProfilingCapturesEveryPathAndBatchWithoutChangingRisk) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    const Handle_<ModelData_> model(new BSModelData_("", 10.0, 0.2, 0.034, 0.021));
    constexpr size_t N_PATHS = 64;
    for (const bool compiled : {false, true}) {
        ScriptProduct_ product({Cell_(Date_(2024, 6, 21))}, {"payoff PAYS SPOT()"});
        const int nestedIfs = product.PreProcess(true, false);
        const auto plain = MCSimulation<double>(product, model, N_PATHS, "sobol", false, compiled);
        const auto expected = MCSimulation<AAD::Number_>(product, model, N_PATHS, "sobol", false, compiled, nestedIfs);
        AAD::ProfilingData_ data;
        const auto actual = [&] {
            AAD::ProfilingScope_ profile(&data);
            return MCSimulation<AAD::Number_>(product, model, N_PATHS, "sobol", false, compiled, nestedIfs);
        }();
        ASSERT_TRUE(data.complete_);
        ASSERT_NEAR(actual.aggregated_, plain.aggregated_, 1e-10);
        ASSERT_NEAR(actual.aggregated_, expected.aggregated_, 1e-10);
        ASSERT_EQ(actual.names_, expected.names_);
        ASSERT_EQ(actual.risks_.size(), expected.risks_.size());
        for (size_t i = 0; i < actual.risks_.size(); ++i)
            ASSERT_NEAR(actual.risks_[i], expected.risks_[i], 1e-10 * std::max(1.0, std::abs(expected.risks_[i])));
        ASSERT_NEAR(actual.risks_[0], actual.aggregated_ / (N_PATHS * 10.0), 1e-10);
        ASSERT_EQ(data.taskGroups_.size(), 1);
        const BatchPlan_ batches(N_PATHS, ThreadPool_::GetInstance()->NumThreads());
        ASSERT_EQ(data.taskGroups_.front().size(), batches.BatchCount());
        ASSERT_GT(data.memory_.resultArrays_.liveBytes_, 0);
        std::uint64_t forward = 0, payoff = 0, suffix = 0, prefix = 0, samples = 0;
        for (const auto& task : data.taskGroups_.front()) {
            ASSERT_TRUE(task.complete_);
            forward += task.Phase(AAD::AADProfilingPhase_::Value_::PATH_FORWARD).calls_;
            payoff += task.Phase(AAD::AADProfilingPhase_::Value_::PAYOFF).calls_;
            suffix += task.Phase(AAD::AADProfilingPhase_::Value_::REVERSE_SUFFIX).calls_;
            prefix += task.Phase(AAD::AADProfilingPhase_::Value_::REVERSE_PREFIX).calls_;
            samples += task.tapeSamples_;
            ASSERT_GT(task.highWater_.nodes_, 0);
            ASSERT_GT(task.highWater_.edges_, 0);
            ASSERT_GT(task.tapeSamples_, 0);
            ASSERT_GT(task.memory_.pathArrays_.liveBytes_, 0);
            ASSERT_GT(task.memory_.workspaceArrays_.liveBytes_, 0);
        }
        ASSERT_EQ(forward, N_PATHS);
        ASSERT_EQ(payoff, N_PATHS);
        ASSERT_EQ(suffix, N_PATHS);
        ASSERT_EQ(prefix, batches.BatchCount());
        ASSERT_EQ(samples, N_PATHS + batches.BatchCount());
    }
}
#endif

TEST(SimulationTest, TestInvalidPathDiagnosticsAcrossSamples) {
    AAD::Scenario_<double> path(3);
    for (auto& sample : path) {
        sample.Initialize();
        sample.observations_ = {-2.0, 0.0, 3.0};
        sample.discounts_ = {0.5, 1.0};
    }
    for (size_t i = 0; i < path.size(); ++i) {
        auto& sample = path[i];
        const Vector_<double*> fields{&sample.spot_, &sample.numeraire_, &sample.observations_[2], &sample.discounts_[0]};
        const Vector_<String_> messages{"InvalidModelPath: non-finite spot",
                                        "InvalidModelPath: non-finite or nonpositive numeraire",
                                        "InvalidModelPath: non-finite observation",
                                        "InvalidModelPath: non-finite or nonpositive discount factor"};
        for (size_t field = 0; field < fields.size(); ++field) {
            for (const double value :
                 {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
                SCOPED_TRACE(::testing::Message() << "sample=" << i << "; field=" << field << "; value=" << value);
                const double original = *fields[field];
                *fields[field] = value;
                try {
                    ValidateSimulationPath(path);
                    FAIL() << "every generated field must be validated";
                } catch (const ScriptError_& error) {
                    ASSERT_NE(String_(error.what()).find(messages[field]), String_::npos);
                }
                *fields[field] = original;
            }
        }
        for (const double nonpositive : {0.0, -1.0}) {
            sample.numeraire_ = nonpositive;
            ASSERT_THROW(ValidateSimulationPath(path), ScriptError_);
            sample.numeraire_ = 1.0;
            sample.discounts_[1] = nonpositive;
            ASSERT_THROW(ValidateSimulationPath(path), ScriptError_);
            sample.discounts_[1] = 1.0;
        }
        sample.numeraire_ = std::numeric_limits<double>::min();
        sample.spot_ = -1.0;
        ASSERT_NO_THROW(ValidateSimulationPath(path));
        sample.numeraire_ = 1.0;
    }
}

TEST(SimulationTest, TestInvalidDiscountsFailModelPathValidation) {
    AAD::Scenario_<double> path(1);
    path[0].Initialize();
    path[0].discounts_ = {1.0};
    ASSERT_TRUE(AAD::IsValidModelPath(path));
    for (const double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), 0.0, -0.5}) {
        SCOPED_TRACE(value);
        path[0].discounts_[0] = value;
        ASSERT_FALSE(AAD::IsValidModelPath(path));
    }
}

TEST(SimulationTest, TestInvalidPathPreservesFirstDiagnostic) {
    AAD::Scenario_<double> path(2);
    for (auto& sample : path)
        sample.Initialize();
    path[0].observations_ = {std::numeric_limits<double>::infinity()};
    path[1].spot_ = std::numeric_limits<double>::quiet_NaN();
    try {
        ValidateSimulationPath(path);
        FAIL() << "the first invalid sample must fail";
    } catch (const ScriptError_& error) {
        ASSERT_NE(String_(error.what()).find("InvalidModelPath: non-finite observation"), String_::npos);
    }
}

TEST(SimulationTest, TestDeterministicWithFixedSeed) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ first = MCSimulation<double>(product, model, 4096, "sobol", false, false);
    const SimResults_ second = MCSimulation<double>(product, model, 4096, "sobol", false, false);

    ASSERT_DOUBLE_EQ(first.aggregated_, second.aggregated_);
}

TEST(SimulationTest, TestPseudoBatchesMatchSequentialPaths) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    auto product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto modelData = StandardBSModel();
    auto model = CreateModel<double>(modelData);
    model->Allocate(product.TimeLine(), product.DefLine());
    model->Init(product.TimeLine(), product.DefLine());
    const size_t paths = 2 * BATCH_SIZE + 17;
    for (const auto* method : {"irn", "mrg32"}) {
        for (const bool useBb : {false, true}) {
            auto random = CreateRNG(method, *model, useBb);
            Vector_<> gauss(model->SimDim());
            Scenario_<> path;
            AllocatePath(product.DefLine(), path);
            InitializePath(path);
            auto evaluator = product.BuildEvaluator<double>();
            double sequential = 0.0;
            for (size_t i = 0; i < paths; ++i) {
                random->FillNormal(&gauss);
                model->GeneratePath(gauss, &path);
                product.Evaluate(path, evaluator);
                sequential += evaluator.VarVals()[product.PayOffIdx()];
            }
            for (const bool compiled : {false, true}) {
                const auto result = MCSimulation<double>(product, modelData, paths, method, useBb, compiled);
                ASSERT_NEAR(result.aggregated_, sequential, 1e-8) << method << "; bb=" << useBb;
            }
        }
    }
}

TEST(SimulationTest, TestDeterministicWithMrg32) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ first = MCSimulation<double>(product, model, 4096, "mrg32", false, false);
    const SimResults_ second = MCSimulation<double>(product, model, 4096, "mrg32", false, false);

    ASSERT_DOUBLE_EQ(first.aggregated_, second.aggregated_);
}

TEST(SimulationTest, TestParallelDoubleStateMatchesSerialAcrossRequests) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    auto firstProduct = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    auto secondProduct = VanillaCallProduct(Date_(2024, 6, 21), 12.0);
    firstProduct.PreProcess(false, false);
    secondProduct.PreProcess(false, false);
    const auto model = StandardBSModel();
    const size_t paths = 8 * BATCH_SIZE + 17;
    auto* pool = ThreadPool_::GetInstance();
    const size_t originalThreads = pool->NumThreads();
    Vector_<SimulationObservation_> observations;
    try {
        for (const auto* method : {"sobol", "mrg32", "irn"}) {
            for (const bool compiled : {false, true}) {
                const auto value = [&](const ScriptProduct_& product) {
                    return MCSimulation<double>(product, model, paths, method, false, compiled).aggregated_;
                };
                pool->Start(1, true);
                const double serialFirst = value(firstProduct);
                const double serialSecond = value(secondProduct);
                pool->Start(4, true);
                observations.push_back({serialFirst, serialSecond, value(firstProduct), value(secondProduct), value(firstProduct)});
            }
        }
    } catch (...) {
        pool->Start(originalThreads, true);
        pool->Stop();
        throw;
    }
    pool->Start(originalThreads, true);
    pool->Stop();
    for (const auto& observed : observations) {
        ASSERT_GT(observed.serialFirst_, observed.serialSecond_);
        ASSERT_DOUBLE_EQ(observed.serialFirst_, observed.parallelFirst_);
        ASSERT_DOUBLE_EQ(observed.serialSecond_, observed.parallelSecond_);
        ASSERT_DOUBLE_EQ(observed.serialFirst_, observed.repeatedFirst_);
    }
}

TEST(SimulationTest, TestPseudoAadMatchesValueAndDeltaAcrossBatches) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2022, 6, 22));
    ScriptProduct_ product({Cell_(Date_(2024, 6, 21))}, {"payoff PAYS SPOT()"});
    const int maxNestedIfs = static_cast<int>(product.PreProcess(true, false));
    const auto model = StandardBSModel();
    const size_t paths = 2 * BATCH_SIZE + 17;
    for (const auto* method : {"irn", "mrg32"}) {
        for (const bool useBb : {false, true}) {
            for (const bool compiled : {false, true}) {
                const auto plain = MCSimulation<double>(product, model, paths, method, useBb, compiled);
                const auto active = MCSimulation<AAD::Number_>(product, model, paths, method, useBb, compiled, maxNestedIfs);
                ASSERT_NEAR(active.aggregated_ / paths, plain.aggregated_ / paths, 1e-10);
                ASSERT_NEAR(active["spot"], plain.aggregated_ / paths / 10.0, 1e-10);
            }
        }
    }
}

TEST(SimulationTest, TestAggregatesTowardsAnalyticBlackScholes) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const size_t nPaths = 65536;
    const SimResults_ results = MCSimulation<double>(product, model, nPaths, "sobol", false, false);

    // Closed-form Black-Scholes PV for spot=10, strike=11, vol=0.20,
    // rate=0.034, div=0.021, maturity~2y (same golden value as TestBlackScholes).
    ASSERT_NEAR(results.aggregated_ / static_cast<double>(nPaths), 0.806119, 1e-3);
}

TEST(SimulationTest, TestCompiledAggregationMatchesAnalyticToo) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const size_t nPaths = 65536;
    const SimResults_ results = MCSimulation<double>(product, model, nPaths, "sobol", false, true);

    ASSERT_NEAR(results.aggregated_ / static_cast<double>(nPaths), 0.806119, 1e-3);
}

TEST(SimulationTest, TestZeroPathsReturnsZeroAggregate) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ results = MCSimulation<double>(product, model, 0, "sobol", false, false);
    ASSERT_DOUBLE_EQ(results.aggregated_, 0.0);
}

TEST(SimulationTest, TestSinglePathIsDeterministicAndFinite) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ first = MCSimulation<double>(product, model, 1, "sobol", false, false);
    const SimResults_ second = MCSimulation<double>(product, model, 1, "sobol", false, false);

    ASSERT_TRUE(std::isfinite(first.aggregated_));
    ASSERT_DOUBLE_EQ(first.aggregated_, second.aggregated_);
}

TEST(SimulationTest, TestMismatchedDatesAndEventsThrow) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(Date_(2023, 6, 21)), Cell_(Date_(2024, 6, 21))};
    Vector_<String_> events{"call pays spot()"};

    ASSERT_THROW(ScriptProduct_ product(eventDates, events), Dal::ScriptError_);
}

TEST(SimulationTest, TestInvalidRngNameThrows) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    ScriptProduct_ product = VanillaCallProduct(Date_(2024, 6, 21), 11.0);
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    ASSERT_THROW(MCSimulation<double>(product, model, 64, "not_a_rng", false, false), Dal::Exception_);
}

TEST(SimulationTest, TestEmptyScriptPaysNothing) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 6, 22));
    Vector_<Cell_> eventDates{Cell_(Date_(2024, 6, 21))};
    Vector_<String_> events{"call pays 0"};
    ScriptProduct_ product(eventDates, events, "call");
    product.PreProcess(false, false);
    const auto model = StandardBSModel();

    const SimResults_ results = MCSimulation<double>(product, model, 1024, "sobol", false, false);
    ASSERT_DOUBLE_EQ(results.aggregated_, 0.0);
}

TEST(SimulationTest, TestHybridLocalVolAadCallerInitializationPreservesValueAndRisks) {
    const auto restore = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    ScriptProduct_ product({Cell_(Date_(2027, 9, 12))}, {"payoff PAYS SPOT()"});
    const int maxNestedIfs = static_cast<int>(product.PreProcess(true, false));
    const Handle_<ModelData_> blackScholes(new BSModelData_("", 100.0, 0.2, 0.03, 0.01));
    const auto localVol =
        MakeFlatRateLocalVolHybridModelData("local_vol", "EQ[DAL196_TEST]", 100.0, 0.03, 0.01, {50.0, 150.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.2));
    const size_t paths = 257;
    const auto expected = MCSimulation<AAD::Number_>(product, blackScholes, paths, "sobol", false, false, maxNestedIfs);
    for (const bool compiled : {false, true}) {
        const auto plain = MCSimulation<double>(product, localVol, paths, "sobol", false, compiled);
        const auto actual = MCSimulation<AAD::Number_>(product, localVol, paths, "sobol", false, compiled, maxNestedIfs);
        ASSERT_NEAR(plain.aggregated_ / paths, expected.aggregated_ / paths, 1.0e-10);
        ASSERT_NEAR(actual.aggregated_ / paths, expected.aggregated_ / paths, 1.0e-10);
        ASSERT_NEAR(actual["spot:EQ[DAL196_TEST]"], expected["spot"], 1.0e-10);
        ASSERT_NEAR(actual["rate:USD"], expected["rate"], 1.0e-10);
        ASSERT_NEAR(actual["div:EQ[DAL196_TEST]"], expected["div"], 1.0e-10);
        ASSERT_EQ(actual.risks_.size(), 7);
        double parallelVega = 0.0;
        for (size_t i = 0; i < actual.risks_.size(); ++i)
            if (actual.names_[i].find("lvol:") == 0)
                parallelVega += actual.risks_[i];
        ASSERT_NEAR(parallelVega, expected["vol"], 1.0e-10);
    }
}
