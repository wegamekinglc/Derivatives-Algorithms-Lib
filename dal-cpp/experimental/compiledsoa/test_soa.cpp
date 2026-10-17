#include <gtest/gtest.h>

#include <cfenv>
#include <limits>

#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>

#include "workload.hpp"

using Dal::Vector_;
using namespace Dal::Script;
using namespace Dal::Script::SoaStudy;

namespace {
    template <size_t L_> void CheckModelAndPayoffs(const std::string& name) {
        const auto product = Product(name);
        const auto compiled = product.Compile();
        const Plan_ plan(compiled, product.VarValues(), product.ConstVarValues());
        typename Plan_::State_<L_> state(plan);
        auto scalar = product.BuildEvalState<double>();
        Dal::AAD::BlackScholes_<double> model(100.0, 0.2, 0.05, 0.02);
        model.Allocate(product.TimeLine(), product.DefLine());
        model.Init(product.TimeLine(), product.DefLine());
        const BatchModel_ batch(product.TimeLine(), product.DefLine());
        const auto shocks = Shocks(2 * L_ + 3, model.SimDim());
        Dal::AAD::Scenario_<double> path;
        Dal::AAD::AllocatePath(product.DefLine(), path);
        Dal::AAD::InitializePath(path);
        std::vector<Row_<L_>> spots(path.size());
        for (const bool fallback : {false, true}) {
            for (size_t first = 0; first < shocks.size(); first += L_) {
                const size_t count = std::min(L_, shocks.size() - first);
                batch.Generate(shocks, first, count, &spots);
                Counters_ counters;
                plan.Evaluate(spots, batch.Numeraires(), count, fallback, &state, &counters);
                double reference = 0.0, actual = 0.0;
                for (size_t lane = 0; lane < count; ++lane) {
                    model.GeneratePath(shocks[first + lane], &path);
                    for (size_t e = 0; e < path.size(); ++e) {
                        ASSERT_NEAR(spots[e][lane], path[e].spot_, 1e-12 * std::max(1.0, std::abs(path[e].spot_)));
                        ASSERT_DOUBLE_EQ(batch.Numeraires()[e], path[e].numeraire_);
                    }
                    compiled.Evaluate(path, scalar);
                    for (size_t v = 0; v < scalar.VarVals().size(); ++v)
                        ASSERT_NEAR(state.variables_[v][lane], scalar.VarVals()[v], 1e-12 * std::max(1.0, std::abs(scalar.VarVals()[v])));
                    reference += scalar.VarVals()[product.PayOffIdx()];
                    actual += state.variables_[product.PayOffIdx()][lane];
                }
                ASSERT_NEAR(actual, reference, 1e-12 * std::max(1.0, std::abs(reference)));
            }
        }
    }

    template <size_t L_>
    void CheckBranchCase(const ScriptProduct_& product,
                         const ScriptCompiled_& compiled,
                         const Plan_& plan,
                         const std::vector<Row_<L_>>& spots,
                         size_t count,
                         bool balanced,
                         Plan_::State_<L_>* state) {
        for (const bool fallback : {false, true}) {
            Counters_ counters;
            plan.Evaluate(spots, Vector_<>{2.0}, count, fallback, state, &counters);
            for (size_t lane = 0; lane < count; ++lane) {
                auto scalar = product.BuildEvalState<double>();
                Dal::AAD::Scenario_<double> path(1);
                path[0].spot_ = spots[0][lane];
                path[0].numeraire_ = 2.0;
                compiled.Evaluate(path, scalar);
                for (size_t v = 0; v < scalar.VarVals().size(); ++v)
                    ASSERT_DOUBLE_EQ(state->variables_[v][lane], scalar.VarVals()[v]);
            }
            if (count == L_) {
                ASSERT_GT(counters.mixed_, 0U);
                ASSERT_EQ(counters.fallbackLanes_ > 0, fallback && balanced);
            }
        }
    }

    template <size_t L_> void CheckBranches() {
        const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2024, 1, 1));
        ScriptProduct_ product({Dal::Cell_(Dal::Date_(2025, 1, 1))},
                               {"x = 3 if spot() > 100 then if spot() > 120 then x = 7 else x = 5 end "
                                "else if spot() > 80 then x = 2 end end pay pays x"},
                               "pay");
        product.PreProcess(false, true);
        const auto compiled = product.Compile();
        const Plan_ plan(compiled, product.VarValues(), product.ConstVarValues());
        typename Plan_::State_<L_> state(plan);
        std::vector<Row_<L_>> spots(1);
        for (const size_t count : {size_t(1), L_ - 1, L_}) {
            for (const bool balanced : {false, true}) {
                for (size_t lane = 0; lane < count; ++lane)
                    spots[0][lane] = balanced ? 70.0 + 20.0 * (lane % 4) : (lane == 0 ? 130.0 : 90.0);
                ASSERT_NO_FATAL_FAILURE(CheckBranchCase(product, compiled, plan, spots, count, balanced, &state));
            }
        }
    }
} // namespace

TEST(CompiledSoaTest, TestBlackScholesWeeklyAndVanillaAllWidthsAndTail) {
    for (const std::string name : {"vanilla", "weekly", "divergent"}) {
        ASSERT_NO_FATAL_FAILURE(CheckModelAndPayoffs<8>(name));
        ASSERT_NO_FATAL_FAILURE(CheckModelAndPayoffs<16>(name));
        ASSERT_NO_FATAL_FAILURE(CheckModelAndPayoffs<32>(name));
        ASSERT_NO_FATAL_FAILURE(CheckModelAndPayoffs<64>(name));
    }
}

TEST(CompiledSoaTest, TestNestedIfMasksAndHighDivergenceFallback) {
    ASSERT_NO_FATAL_FAILURE(CheckBranches<8>());
    ASSERT_NO_FATAL_FAILURE(CheckBranches<16>());
    ASSERT_NO_FATAL_FAILURE(CheckBranches<32>());
    ASSERT_NO_FATAL_FAILURE(CheckBranches<64>());
}

TEST(CompiledSoaTest, TestInactiveBranchDoesNotEvaluateInvalidArithmetic) {
    const Dal::Vector_<int> stream{Spot, Sup, IfElse, 12, 17, Spot, Log, Pays, 0, PaysConst, 0, 0, Spot, UMinus, Sqrt, Pays, 0};
    const ScriptCompiled_ compiled({stream}, {{1.0}});
    const Plan_ plan(compiled, {0.0});
    Plan_::State_<8> state(plan);
    std::vector<Row_<8>> spots(1);
    for (size_t lane = 0; lane < 8; ++lane)
        spots[0][lane] = lane == 0 ? 4.0 : -4.0;
    std::feclearexcept(FE_ALL_EXCEPT);
    Counters_ counters;
    plan.Evaluate(spots, Vector_<>{1.0}, 8, false, &state, &counters);
    ASSERT_EQ(std::fetestexcept(FE_INVALID | FE_DIVBYZERO), 0);
    ASSERT_NEAR(state.variables_[0][0], std::log(4.0) + 1.0, 1e-14);
    for (size_t lane = 1; lane < 8; ++lane)
        ASSERT_DOUBLE_EQ(state.variables_[0][lane], 2.0);
}

TEST(CompiledSoaTest, TestScalarArithmeticBooleanAndSeedReuse) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Dal::Date_(2024, 1, 1));
    ScriptProduct_ product({Dal::Cell_(Dal::Date_(2025, 1, 1))},
                           {"x = spot() / 3 y = 5 / x z = sqrt(x) + exp(log(x)) - (-x) "
                            "if x != 0 AND (x > 1 OR x >= 2) then "
                            "pay pays MAX(2, MIN(z, y)) + MIN(3, MAX(x, 1)) else pay pays 7 end"},
                           "pay");
    product.PreProcess(false, true);
    const auto compiled = product.Compile();
    const Plan_ plan(compiled, product.VarValues(), product.ConstVarValues());
    Plan_::State_<8> state(plan);
    std::vector<Row_<8>> spots(1);
    for (int reuse = 0; reuse < 2; ++reuse) {
        for (size_t lane = 0; lane < 8; ++lane)
            spots[0][lane] = 1.0 + lane + reuse;
        for (const bool fallback : {false, true}) {
            Counters_ counters;
            plan.Evaluate(spots, Vector_<>{1.2}, 8, fallback, &state, &counters);
            for (size_t lane = 0; lane < 8; ++lane) {
                auto scalar = product.BuildEvalState<double>();
                Dal::AAD::Scenario_<double> path(1);
                path[0].spot_ = spots[0][lane];
                path[0].numeraire_ = 1.2;
                compiled.Evaluate(path, scalar);
                for (size_t v = 0; v < scalar.VarVals().size(); ++v)
                    ASSERT_DOUBLE_EQ(state.variables_[v][lane], scalar.VarVals()[v]);
            }
        }
    }
}

TEST(CompiledSoaTest, TestExtremaKeepNanAndSignedZeroOperandOrder) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const int op : {Max2Const, Min2Const, ConstMax2, ConstMin2}) {
        const ScriptCompiled_ compiled({{Spot, op, 0, Assign, 0}}, {{0.0}});
        const Plan_ plan(compiled, {1.0});
        Plan_::State_<8> state(plan);
        std::vector<Row_<8>> spots(1);
        spots[0] = {nan, -0.0, 0.0, -1.0, 1.0, nan, -0.0, 0.0};
        Counters_ counters;
        plan.Evaluate(spots, Vector_<>{1.0}, 8, false, &state, &counters);
        for (size_t lane = 0; lane < 8; ++lane) {
            EvalState_<double> scalar({1.0});
            Dal::AAD::Scenario_<double> path(1);
            path[0].spot_ = spots[0][lane];
            path[0].numeraire_ = 1.0;
            compiled.Evaluate(path, scalar);
            const double expected = scalar.VarVals()[0], actual = state.variables_[0][lane];
            if (std::isnan(expected))
                ASSERT_TRUE(std::isnan(actual));
            else {
                ASSERT_DOUBLE_EQ(actual, expected);
                ASSERT_EQ(std::signbit(actual), std::signbit(expected));
            }
        }
    }
}

TEST(CompiledSoaTest, TestRejectUnsupportedAndMalformedProgramsBeforeExecution) {
    for (const Vector_<int> stream : {Vector_<int>{LoadObservation, 0, Assign, 0},
                                      {Spot, PowConst, 0, Assign, 0},
                                      {PaysOnConst, 0, 0, 0},
                                      {Add},
                                      {Var, 1, Assign, 0},
                                      {Const, 1, Assign, 0},
                                      {Spot},
                                      {True, If, 99},
                                      {True, IfElse, 4, 4, Spot}})
        ASSERT_THROW((Plan_(ScriptCompiled_({stream}, {{1.0}}), {0.0})), Dal::Exception_);
    const Plan_ plan(ScriptCompiled_({{Spot, Assign, 0}}, {{}}), {0.0});
    Plan_::State_<8> state(plan);
    Counters_ counters;
    std::vector<Row_<8>> spots(1);
    ASSERT_THROW(plan.Evaluate(spots, Vector_<>{1.0}, 0, false, &state, &counters), Dal::Exception_);
    ASSERT_THROW(plan.Evaluate(spots, Vector_<>{1.0}, 9, false, &state, &counters), Dal::Exception_);
}

TEST(CompiledSoaTest, TestBatchModelRejectsPreparedObservationsAndWrongShockShape) {
    Vector_<Dal::AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[TEST]"};
    ASSERT_THROW((BatchModel_({1.0}, definitions)), Dal::Exception_);
    definitions[0].indexNames_.clear();
    const BatchModel_ model({1.0}, definitions);
    std::vector<Row_<8>> spots(1);
    ASSERT_THROW(model.Generate(Vector_<Vector_<>>{{}}, 0, 1, &spots), Dal::Exception_);
}

int main(int argc, char** argv) {
    Dal::RegisterAll_::Init(1);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
