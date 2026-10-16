//
// Created by Codex on 2026/10/16.
//

#include <gtest/gtest.h>

#include <cfenv>
#include <limits>

#include <dal/script/compiledsoa.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    class SingleWorker_ {
        size_t original_ = ThreadPool_::GetInstance()->NumThreads();
        bool active_ = ThreadPool_::GetInstance()->IsActive();

    public:
        SingleWorker_() { ThreadPool_::GetInstance()->Start(1, true); }
        ~SingleWorker_() {
            ThreadPool_::GetInstance()->Start(original_, true);
            if (!active_)
                ThreadPool_::GetInstance()->Stop();
        }
    };

    ScriptProduct_ Product(const String_& name) {
        Vector_<Cell_> dates{Cell_(Date_(2024, 1, 1))};
        Vector_<String_> events{"alive = 1"};
        if (name != "vanilla") {
            dates.push_back(Cell_("START: 2024-01-01 END: 2025-01-01 FREQ: 1W"));
            events.push_back(name == "weekly" ? "if spot() > 150 then alive = 0 end" : "if spot() > 103 then alive = 0 else alive = 1 end");
        }
        dates.push_back(Cell_(Date_(2025, 1, 1)));
        events.push_back("pay pays alive * MAX(spot() - 100, 0)");
        ScriptProduct_ product(dates, events);
        product.PreProcess(false, false);
        return product;
    }

    template <size_t L_> void CheckRequests() {
        for (const auto* name : {"vanilla", "weekly", "divergent"}) {
            const auto product = Product(name);
            for (const size_t paths : {1, 7, 8, 9, 65, 129}) {
                AAD::BlackScholes_<double> model(107.0, 0.31, -0.007, 0.041);
                const auto expected = MCDoubleSimulation(product, &model, paths, "sobol", false, true);
                const auto actual = MCDoubleSimulationSoa<L_>(product, &model, paths);
                ASSERT_NEAR(actual.aggregated_, expected.aggregated_, 1e-12 * std::max(1.0, std::fabs(expected.aggregated_)));
                ASSERT_EQ(actual.names_, expected.names_);
                ASSERT_EQ(actual.risks_, expected.risks_);
            }
        }
    }
} // namespace

TEST(CompiledSoaTest, TestRequestsLanesTailsAndSmallScalarBypass) {
    const SingleWorker_ worker;
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    CheckRequests<8>();
    CheckRequests<16>();
    CheckRequests<32>();
    CheckRequests<64>();
}

TEST(CompiledSoaTest, TestPreparedHistoryObservationGridRngAndPrecision) {
    const SingleWorker_ worker;
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const ScriptProductData_ data("", {Cell_(Date_(2023, 12, 30)), Cell_(Date_(2024, 6, 1)), Cell_(Date_(2025, 1, 1))},
                                  {"x = 2", "x = x + FIX(EQ[SOA], 2024-03-01)", "pay pays x + FIX(EQ[SOA], 2024-06-01) + FIX(EQ[SOA])"});
    for (const auto* method : {"sobol", "mrg32", "irn"}) {
        for (const auto* precision : {"Fast", "Precise"}) {
            for (const bool bridge : {false, true}) {
                MonteCarloSettings_ settings;
                settings.compiled_ = true;
                settings.rsg_ = method;
                settings.normalPrecision_ = precision;
                settings.useBb_ = bridge;
                AAD::BlackScholes_<double> model(82.0, 0.27, 0.013, -0.005);
                const auto prepared = PrepareScript(data, &model, {}, settings);
                ASSERT_GT(prepared.TimeLine().size(), prepared.EventDates().size());
                const auto expected = MCDoubleSimulation(prepared, &model, 257, method, bridge, true);
                const auto actual = MCDoubleSimulationSoa<32>(prepared, &model, 257, method, bridge);
                ASSERT_DOUBLE_EQ(actual.aggregated_, expected.aggregated_);
                ASSERT_DOUBLE_EQ(actual.aggregated_, MCDoubleSimulationSoa<32>(prepared, &model, 257, method, bridge).aggregated_);
            }
        }
    }
}

TEST(CompiledSoaTest, TestMasksNestedInactiveInvalidBranchesAndReuse) {
    namespace Soa = Dal::Script::Detail::CompiledSoa;
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    ScriptProduct_ product({Cell_(Date_(2025, 1, 1))}, {"x = 0 if spot() > 0 then if spot() > 100 then x = LOG(spot()) else x = SQRT(spot()) end "
                                                        "else x = LOG(-spot()) end pay pays x"});
    product.PreProcess(false, false);
    const auto plan = Soa::BuildPlan(product);
    Soa::Plan_::State_<64> state(plan);
    std::vector<Soa::Row_<64>> spots(1);
    auto scalar = product.BuildEvalState<double>();
    AAD::Scenario_<> path(1);
    path[0].numeraire_ = 1.0;
    const auto compiled = product.Compile();
    for (const size_t count : {64, 9, 1, 63}) {
        for (size_t lane = 0; lane < count; ++lane)
            spots[0][lane] = lane % 3 == 0 ? -25.0 : lane % 3 == 1 ? 36.0 : 121.0;
        std::feclearexcept(FE_ALL_EXCEPT);
        plan.Evaluate(spots, {1.0}, count, &state);
        ASSERT_EQ(std::fetestexcept(FE_INVALID | FE_DIVBYZERO), 0);
        for (size_t lane = 0; lane < count; ++lane) {
            path[0].spot_ = spots[0][lane];
            compiled.Evaluate(path, scalar);
            for (size_t v = 0; v < scalar.variables_.size(); ++v)
                ASSERT_DOUBLE_EQ(state.variables_[v][lane], scalar.variables_[v]);
        }
    }
}

TEST(CompiledSoaTest, TestTodayOnlyZeroDimensionAndExpiredRequests) {
    const SingleWorker_ worker;
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    AAD::BlackScholes_<double> model(83.0, 0.0, -0.01, 0.02);
    ScriptProduct_ today({Cell_(Date_(2024, 1, 1))}, {"pay pays spot()"});
    today.PreProcess(false, false);
    for (const size_t count : {7, 9, 65}) {
        ASSERT_DOUBLE_EQ(MCDoubleSimulationSoa(today, &model, count).aggregated_, 83.0 * count);
        ASSERT_EQ(model.SimDim(), 0U);
    }
    const auto empty = PrepareScript(ScriptProductData_("", {Cell_(Date_(2023, 12, 30))}, {"pay pays 1"}));
    ASSERT_EQ(MCDoubleSimulationSoa(empty, &model, 65).aggregated_, 0.0);
    ASSERT_THROW(MCDoubleSimulationSoa(empty, &model, 0), ScriptError_);
    ASSERT_THROW(MCDoubleSimulationSoa(empty, &model, 65, "invalid"), ScriptError_);
}

TEST(CompiledSoaTest, TestExtremaNanSignedZeroAndOperandOrder) {
    namespace Soa = Dal::Script::Detail::CompiledSoa;
    for (const int op : {Max2, Min2, Max2Const, Min2Const, ConstMax2, ConstMin2}) {
        const bool binary = op == Max2 || op == Min2;
        Vector_<int> code{Spot};
        if (binary) {
            code.push_back(Const);
            code.push_back(0);
        }
        code.push_back(op);
        if (!binary)
            code.push_back(0);
        code.push_back(Assign);
        code.push_back(0);
        const ScriptCompiled_ compiled({code}, {{-0.0}});
        const Soa::Plan_ plan(compiled, {0.0}, {}, 1, {0});
        Soa::Plan_::State_<8> state(plan);
        std::vector<Soa::Row_<8>> spots(1);
        spots[0] = {0.0, -0.0, std::numeric_limits<double>::quiet_NaN(), -1.0, 1.0, 2.0, 3.0, 4.0};
        plan.Evaluate(spots, {1.0}, 8, &state);
        EvalState_<double> scalar({0.0});
        AAD::Scenario_<> path(1);
        for (size_t lane = 0; lane < 8; ++lane) {
            path[0].spot_ = spots[0][lane];
            compiled.Evaluate(path, scalar);
            const double expected = scalar.variables_[0], actual = state.variables_[0][lane];
            if (std::isnan(expected))
                ASSERT_TRUE(std::isnan(actual));
            else {
                ASSERT_EQ(actual, expected);
                ASSERT_EQ(std::signbit(actual), std::signbit(expected));
            }
        }
    }
}

TEST(CompiledSoaTest, TestAdmissionErrorsAndExceptionRecovery) {
    const SingleWorker_ worker;
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
    const auto product = Product("vanilla");
    AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
    struct DerivedBS_ : AAD::BlackScholes_<double> {
        DerivedBS_() : AAD::BlackScholes_<double>(100.0, 0.2) {}
        void GeneratePath(const Vector_<>&, AAD::Scenario_<>*) const override { FAIL() << "rejected model must not generate paths"; }
    } derived;
    ASSERT_THROW(MCDoubleSimulationSoa(product, &derived, 65), ScriptError_);
    ASSERT_THROW(MCDoubleSimulationSoa(product, &model, 0), ScriptError_);
    ASSERT_THROW(MCDoubleSimulationSoa(product, &model, 65, "invalid"), ScriptError_);
    ScriptProduct_ missing({Cell_(Date_(2025, 1, 1))}, {"pay pays FIX(EQ[SOA], 2023-12-30)"});
    ASSERT_THROW(MCDoubleSimulationSoa(missing, &model, 65), ScriptError_);
    for (const auto* script : {"pay pays spot() ^ 2", "pay pays LOG(-spot())"}) {
        ScriptProduct_ invalid({Cell_(Date_(2025, 1, 1))}, {script});
        invalid.PreProcess(false, false);
        ASSERT_THROW(MCDoubleSimulationSoa(invalid, &model, 65), Exception_);
    }
    ScriptProduct_ explosive({Cell_(Date_(2025, 1, 1))}, {"pay pays spot()"});
    explosive.PreProcess(false, false);
    AAD::BlackScholes_<double> overflow(100.0, 0.0, 0.0, -1000.0);
    ASSERT_THROW(MCDoubleSimulationSoa(explosive, &overflow, 65), ScriptError_);
    const auto expected = MCDoubleSimulation(product, &model, 65, "sobol", false, true);
    ASSERT_DOUBLE_EQ(MCDoubleSimulationSoa(product, &model, 65).aggregated_, expected.aggregated_);
    const ScriptProductData_ data("", {Cell_(Date_(2025, 1, 1))}, {"pay pays spot()"});
    MonteCarloSettings_ settings;
    const auto tree = PrepareScript(data, &model, {}, settings);
    ASSERT_THROW(MCDoubleSimulationSoa(tree, &model, 65), ScriptError_);
    settings.compiled_ = true;
    const auto compiled = PrepareScript(data, &model, {}, settings);
    ASSERT_THROW(MCDoubleSimulationSoa(compiled, &model, 65, "mrg32"), ScriptError_);
    const ScriptProductData_ delayed("", {Cell_(Date_(2025, 1, 1))}, {"pay pays spot() ON 2025-02-01"});
    const auto delayedRequest = PrepareScript(delayed, &model, {}, settings);
    ASSERT_TRUE(std::isfinite(MCDoubleSimulation(delayedRequest, &model, 65, "sobol", false, true).aggregated_));
    ASSERT_THROW(MCDoubleSimulationSoa(delayedRequest, &model, 65), ScriptError_);
    const ScriptProductData_ vectorData("", {Cell_(Date_(2025, 1, 1))}, {"APPEND(v, spot()) pay pays SUM(v)"});
    const auto vectorRequest = PrepareScript(vectorData, &model, {}, settings);
    ASSERT_THROW(MCDoubleSimulationSoa(vectorRequest, &model, 65), ScriptError_);
    *model.Parameters()[1] = -0.1;
    ASSERT_THROW(MCDoubleSimulationSoa(product, &model, 65), Exception_);
    *model.Parameters()[1] = 0.2;
    ASSERT_DOUBLE_EQ(MCDoubleSimulationSoa(product, &model, 65).aggregated_, expected.aggregated_);
    ThreadPool_::GetInstance()->Start(2, true);
    ASSERT_THROW(MCDoubleSimulationSoa(product, &model, 65), ScriptError_);
}

TEST(CompiledSoaTest, TestMalformedAndUnsupportedBytecodeRejectedBeforeEvaluation) {
    namespace Soa = Dal::Script::Detail::CompiledSoa;
    for (const auto& code : Vector_<Vector_<int>>{{Const}, {Var, 1}, {Const, -1}, {Add}, {Spot}, {True, If, 100}, {Spot, PowConst, 0}}) {
        ASSERT_THROW(Soa::Plan_(ScriptCompiled_({code}, {{1.0}}), {0.0}, {}, 1, {0}), ScriptError_);
    }
}
