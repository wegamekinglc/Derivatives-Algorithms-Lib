//
// Created by wegamekinglc on 2026/8/15.
//
// Coverage for dal/script/simulation.hpp: determinism under fixed-seed RNGs,
// payoff aggregation against analytic expectations, and error/edge paths.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

#include <dal/curve/tapeguard.hpp>
#include <dal/math/aad/profiling.hpp>
#include <dal/math/specialfunctions.hpp>
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

TEST(SimulationTest, TestNormalPrecisionFactoryDefaultsToFastAndPreservesUniforms) {
    for (const auto* method : {"sobol", "mrg32", "irn"}) {
        for (const auto* precision : {"Default", "Fast", "Precise"}) {
            auto normal = CreateRNG(method, 52, false, std::nullopt, precision);
            auto uniform = CreateRNG(method, 52, false);
            Vector_<> actual(52), expected(52);
            const bool precise = String_(precision) == "Precise";
            for (int path = 0; path < 4; ++path) {
                normal->FillNormal(&actual);
                uniform->FillUniform(&expected);
                for (size_t i = 0; i < actual.size(); ++i)
                    ASSERT_EQ(actual[i], InverseNCDF(expected[i], precise, precise));
                if (String_(method) != "sobol")
                    uniform->FillUniform(&expected);
            }
        }
        auto fast = CreateRNG(method, 3, false, std::nullopt, "Fast");
        auto precise = CreateRNG(method, 3, false, std::nullopt, "Precise");
        auto defaultNormal = CreateRNG(method, 3, false);
        auto fastNormal = CreateRNG(method, 3, false, std::nullopt, "Fast");
        Vector_<> actual(3), expected(3);
        for (int path = 0; path < 5; ++path) {
            fast->FillUniform(&actual);
            precise->FillUniform(&expected);
            ASSERT_EQ(actual, expected);
            defaultNormal->FillNormal(&actual);
            fastNormal->FillNormal(&expected);
            ASSERT_EQ(actual, expected);
        }
    }
    ASSERT_THROW(CreateRNG("mrg32", 0, false, std::nullopt, "fast"), ScriptError_);
    ASSERT_THROW(CreateRNG("sobol", 3, false, std::nullopt, "invalid"), ScriptError_);
    auto shifted = CreateRNG("sobol", 3, false, 17, "Precise");
    auto expectedShifted = NewDigitallyShiftedSobol(3, 2048, 17, true, true);
    Vector_<> actual(3), expected(3);
    shifted->FillNormal(&actual);
    expectedShifted->FillNormal(&expected);
    ASSERT_EQ(actual, expected);
}

TEST(SimulationTest, TestNormalPrecisionPreservesBridgeCloneAndSeeking) {
    for (const auto* method : {"sobol", "mrg32", "irn"}) {
        for (const auto* precision : {"Fast", "Precise"}) {
            auto sought = CreateRNG(method, 52, true, std::nullopt, precision);
            Vector_<> actual(52), expected(52);
            for (const size_t offset : {0, 1, 17, 8193, 3}) {
                auto replay = CreateRNG(method, 52, true, std::nullopt, precision);
                replay->SkipNormalTo(0);
                for (size_t path = 0; path < offset; ++path)
                    replay->FillNormal(&expected);
                sought->SkipNormalTo(offset);
                auto clone = sought->Clone();
                replay->FillNormal(&expected);
                sought->FillNormal(&actual);
                ASSERT_EQ(actual, expected);
                clone->FillNormal(&actual);
                ASSERT_EQ(actual, expected);
            }
        }
    }
}

TEST(SimulationTest, TestPreparedNormalPrecisionMatchesCommonPathPriceAndAllGreeks) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const ScriptProductData_ data("", {Cell_(Date_(2025, 1, 1))}, {"payoff PAYS SPOT()"});
    const Handle_<ModelData_> modelData(new BSModelData_("", 100.0, 0.2, 0.05, 0.02));
    constexpr size_t PATHS = 8193;
    const double time = 366.0 / 365.0;
    for (const auto* method : {"sobol", "mrg32", "irn"}) {
        for (const auto* precision : {"Default", "Fast", "Precise"}) {
            auto rng = CreateRNG(method, 1, false, std::nullopt, precision);
            rng->SkipNormalTo(0);
            Vector_<> gaussian(1);
            double price = 0.0, vega = 0.0;
            for (size_t path = 0; path < PATHS; ++path) {
                rng->FillNormal(&gaussian);
                const double value = 100.0 * std::exp((-0.02 - 0.5 * 0.2 * 0.2) * time + 0.2 * std::sqrt(time) * gaussian[0]);
                price += value / PATHS;
                vega += value * (std::sqrt(time) * gaussian[0] - 0.2 * time) / PATHS;
            }
            for (const bool compiled : {false, true}) {
                MonteCarloSettings_ simulation;
                simulation.rsg_ = method;
                simulation.normalPrecision_ = precision;
                simulation.compiled_ = compiled;
                auto model = CreateModel<double>(modelData);
                const auto passive = PrepareScript(data, model.get(), {}, simulation);
                const auto plain = MCSimulation<double>(passive, modelData, PATHS, method, false, compiled);
                ASSERT_NEAR(plain.aggregated_ / PATHS, price, 1e-10);
                simulation.enableAad_ = true;
                const auto active = PrepareScript(data, model.get(), {}, simulation);
                const auto result = MCSimulation<AAD::Number_>(active, modelData, PATHS, method, false, compiled, active.MaxNestedIfs());
                ASSERT_EQ(result.risks_.size(), 4);
                ASSERT_NEAR(result.aggregated_ / PATHS, price, 1e-10);
                ASSERT_NEAR(result["spot"], price / 100.0, 1e-10);
                ASSERT_NEAR(result["vol"], vega, 1e-10);
                ASSERT_NEAR(result["rate"], 0.0, 1e-10);
                ASSERT_NEAR(result["div"], -time * price, 1e-10);
            }
        }
    }
    MonteCarloSettings_ invalid;
    invalid.normalPrecision_ = "fast";
    ASSERT_THROW(ValidateSimulationSettings(invalid), ScriptError_);
}

TEST(SimulationTest, TestNormalPrecisionAllGreeksAtNarrowSmoothingBoundary) {
    const auto evaluationDate = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const Handle_<ModelData_> modelData(new BSModelData_("", 100.0, 0.2, 0.05, 0.02));
    const double time = 366.0 / 365.0;
    const double discount = std::exp(-0.05 * time);
    constexpr double WIDTH = 1e-6;
    constexpr double STRIKE = 0.0;
    for (const auto* method : {"mrg32", "irn"}) {
        const auto terminal = [&](const char* precision) {
            auto rng = CreateRNG(method, 1, false, std::nullopt, precision);
            Vector_<> normal(1);
            rng->FillNormal(&normal);
            const double logReturn = (0.05 - 0.02 - 0.5 * 0.2 * 0.2) * time + 0.2 * std::sqrt(time) * normal[0];
            return std::make_pair(normal[0], std::exp(std::log(100.0) + logReturn));
        };
        const auto fast = terminal("Fast"), precise = terminal("Precise");
        ASSERT_NE(fast.second, precise.second);
        ASSERT_LT(std::abs(fast.second - precise.second), WIDTH);
        // Put the two policies on opposite sides of the lower smoothing edge.
        const double barrier = 0.5 * (fast.second + precise.second) + 0.5 * WIDTH;
        std::ostringstream barrierText;
        barrierText << std::setprecision(17) << barrier;
        const ScriptProductData_ data(
            "", {Cell_("STRIKE"), Cell_("BARRIER"), Cell_(Date_(2025, 1, 1))},
            {"0.0", String_(barrierText.str()), "if spot() >= BARRIER:0.000001 then payoff PAYS spot() - STRIKE else payoff PAYS 0.0 end"});
        for (const bool compiled : {false, true}) {
            Vector_<SimResults_> results;
            for (const auto* precision : {"Default", "Fast", "Precise"}) {
                SCOPED_TRACE(::testing::Message() << method << "; " << precision << "; compiled=" << compiled);
                MonteCarloSettings_ settings;
                settings.rsg_ = method;
                settings.normalPrecision_ = precision;
                settings.enableAad_ = true;
                settings.compiled_ = compiled;
                settings.smooth_ = WIDTH;
                auto model = CreateModel<double>(modelData);
                const auto prepared = PrepareScript(data, model.get(), {}, settings);
                const auto result = MCSimulation<AAD::Number_>(prepared, modelData, 1, method, false, compiled, prepared.MaxNestedIfs(), WIDTH);
                const auto point = String_(precision) == "Precise" ? precise : fast;
                const double spot = point.second;
                const double degree = std::clamp((spot - barrier + 0.5 * WIDTH) / WIDTH, 0.0, 1.0);
                const double slope = degree > 0.0 && degree < 1.0 ? 1.0 / WIDTH : 0.0;
                const double pathDelta = discount * (degree + slope * (spot - STRIKE));
                const Vector_<> expected{pathDelta * spot / 100.0,
                                         pathDelta * spot * (std::sqrt(time) * point.first - 0.2 * time),
                                         time * (pathDelta * spot - discount * degree * (spot - STRIKE)),
                                         -time * pathDelta * spot,
                                         -discount * slope * (spot - STRIKE),
                                         -discount * degree};
                ASSERT_NEAR(result.aggregated_, discount * degree * (spot - STRIKE), 1e-7);
                const Vector_<String_> names{"spot", "vol", "rate", "div", "BARRIER", "STRIKE"};
                ASSERT_EQ(result.risks_.size(), names.size());
                for (size_t i = 0; i < names.size(); ++i)
                    ASSERT_NEAR(result[names[i]], expected[i], 1e-7 * std::max(1.0, std::abs(expected[i]))) << names[i];
                results.push_back(result);
            }
            ASSERT_EQ(results[0].aggregated_, results[1].aggregated_);
            ASSERT_EQ(results[0].risks_, results[1].risks_);
            ASSERT_GT(std::abs(results[1]["BARRIER"] - results[2]["BARRIER"]), 1e6);
        }
    }
}

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
        const Vector_<String_> messages{"InvalidModelPath: non-finite spot", "InvalidModelPath: non-finite or nonpositive numeraire",
                                        "InvalidModelPath: non-finite observation", "InvalidModelPath: non-finite or nonpositive discount factor"};
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
