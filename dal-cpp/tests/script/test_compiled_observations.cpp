//
// Created by Codex on 2026/9/14.
//

#include <gtest/gtest.h>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::DateTime_;
using Dal::Handle_;
using Dal::MarketFixingSnapshot_;
using Dal::String_;
using Dal::Vector_;
using Dal::Script::EvalState_;
using Dal::Script::MonteCarloSettings_;
using Dal::Script::PrepareScript;
using Dal::Script::ScriptCompiled_;
using Dal::Script::ScriptProductData_;
using Dal::Script::ScriptValuationSettings_;
using Dal::Script::SimResults_;
namespace AAD = Dal::AAD;

namespace {
    Handle_<MarketFixingSnapshot_> CompiledHistory() {
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}}));
    }

    ScriptValuationSettings_ CompiledBindings() {
        ScriptValuationSettings_ settings;
        settings.modelBindings_ = {{"spot", "EQ[DAL196_TEST]"}};
        return settings;
    }

    ScriptProductData_ ObservationCase(size_t kind) {
        const Vector_<String_> payoffs{"pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-15)",
                                       "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-11) + FIX(EQ[DAL196_TEST], 2026-09-11)",
                                       "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15)", "pay PAYS x"};
        return {"",
                {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22)), Cell_(Date_(2026, 9, 23))},
                {"2", "x = SCALE * FIX(EQ[DAL196_TEST])", payoffs[kind],
                 kind == 2 ? "pay PAYS FIX(EQ[DAL196_TEST], 2026-09-15) - FIX(EQ[DAL196_TEST], 2026-09-15)" : "x = x"}};
    }

    SimResults_ ObservationPathRisk(size_t kind, bool compiled) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        AAD::BlackScholes_<double> metadata(100.0, 0.2, 0.03, 0.01);
        const auto prepared = PrepareScript(ObservationCase(kind), &metadata, CompiledBindings(), simulation, CompiledHistory());
        AAD::Activate(*AAD::Tape());
        Dal::TapeGuard_ guard(AAD::Tape());
        AAD::BlackScholes_<AAD::Number_> model(100.0, 0.2, 0.03, 0.01);
        model.Allocate(prepared.TimeLine(), prepared.DefLine());
        AAD::Scenario_<AAD::Number_> path;
        AAD::AllocatePath(prepared.DefLine(), path);
        AAD::InitializePath(path);
        SimResults_ result(Dal::Vector::Join(model.ParameterLabels(), prepared.ConstVarNames()));
        auto run = [&](auto& evaluator, auto evaluate) {
            for (auto* parameter : model.Parameters())
                AAD::PutOnTape(*parameter);
            for (auto& parameter : evaluator.ConstVarVals())
                AAD::PutOnTape(parameter);
            AAD::Number_ zero = 0.0;
            AAD::PutOnTape(zero);
            AAD::NewRecording(*AAD::Tape());
            model.Init(prepared.TimeLine(), prepared.DefLine());
            prepared.InitializeHistoricalState(&evaluator);
            model.GeneratePath(Vector_<>(model.SimDim(), 0.0), &path);
            evaluate(path, evaluator);
            AAD::Number_ root = AAD::PayoffRoot(evaluator.VarVals()[prepared.PayOffIdx()], zero);
            AAD::Adjoint(root) = 1.0;
            AAD::PropagateToStart(*AAD::Tape());
            result.aggregated_ = AAD::Value(root);
            size_t i = 0;
            for (const auto* parameter : model.Parameters())
                result.risks_[i++] = AAD::Adjoint(*parameter);
            for (const auto& parameter : evaluator.ConstVarVals())
                result.risks_[i++] = AAD::Adjoint(parameter);
        };
        if (compiled) {
            auto state = prepared.BuildEvalState<AAD::Number_>();
            const auto artifact = prepared.Compile(true);
            run(state, [&](const auto& scenario, auto& evaluator) { artifact.Evaluate(scenario, evaluator); });
        } else {
            auto state = prepared.BuildFuzzyEvaluator<AAD::Number_>(0, simulation.smooth_);
            run(state, [&](const auto& scenario, auto& evaluator) { prepared.Evaluate(scenario, evaluator); });
        }
        return result;
    }

    ScriptProductData_ FuzzyObservationProduct(double strike) {
        return {"",
                {Cell_("SCALE"), Cell_("K"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
                {"2", Dal::ToString(strike), "x = SCALE * FIX(EQ[DAL196_TEST])",
                 "IF FIX(EQ[DAL196_TEST], 2026-09-11) > K:0.2 THEN IF x > 0 THEN pay PAYS x END ELSE pay PAYS 0 END"}};
    }

    double FuzzyObservationPrice(double strike, bool compiled) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        AAD::BlackScholes_<double> model(100.0, 0.2, 0.03, 0.01);
        const auto prepared = PrepareScript(FuzzyObservationProduct(strike), &model, {}, simulation, CompiledHistory());
        AAD::Scenario_<double> path;
        AAD::AllocatePath(prepared.DefLine(), path);
        AAD::InitializePath(path);
        model.GeneratePath(Vector_<>(model.SimDim(), 0.0), &path);
        if (compiled) {
            auto state = prepared.BuildEvalState<double>();
            prepared.Compile(true).Evaluate(path, state);
            return state.VarVals()[prepared.PayOffIdx()];
        }
        auto state = prepared.BuildFuzzyEvaluator<double>(0, simulation.smooth_);
        prepared.Evaluate(path, state);
        return state.VarVals()[prepared.PayOffIdx()];
    }
} // namespace

TEST(ScriptCompiledParityTest, TestIndexFixingsSamePathAndArtifactLifetime) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const Vector_<> expected{200.0, 160.0, 120.0, 160.0};
    for (size_t kind = 0; kind < expected.size(); ++kind) {
        std::optional<ScriptCompiled_> artifact;
        std::optional<EvalState_<double>> state;
        AAD::Scenario_<double> path;
        size_t payoff = 0;
        {
            AAD::BlackScholes_<double> model(100.0, 0.2, 0.03, 0.01);
            MonteCarloSettings_ simulation;
            simulation.compiled_ = true;
            const auto prepared = PrepareScript(ObservationCase(kind), &model, CompiledBindings(), simulation, CompiledHistory());
            AAD::AllocatePath(prepared.DefLine(), path);
            AAD::InitializePath(path);
            for (size_t i = 0; i < path.size(); ++i) {
                path[i].spot_ = 999.0;
                path[i].numeraire_ = 1.0;
                for (auto& observation : path[i].observations_)
                    observation = prepared.Plan().SampleDates()[i] == Date_(2026, 9, 15) ? 120.0 : 999.0;
            }
            auto tree = prepared.BuildEvaluator<double>();
            prepared.Evaluate(path, tree);
            payoff = prepared.PayOffIdx();
            ASSERT_DOUBLE_EQ(tree.VarVals()[payoff], expected[kind]);
            artifact = prepared.Compile();
            state = prepared.BuildEvalState<double>();
        }
        for (size_t repeat = 0; repeat < 3; ++repeat) {
            artifact->Evaluate(path, *state);
            ASSERT_DOUBLE_EQ(state->VarVals()[payoff], expected[kind]);
        }
    }
}

TEST(ScriptCompiledParityTest, TestIndexFixingsAnalyticPathRisks) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const double fixingTime = 3.0 / Dal::DAYS_PER_YEAR;
    const double paymentTime = 10.0 / Dal::DAYS_PER_YEAR;
    const double discount = exp(-0.03 * paymentTime);
    const double future = 100.0 * exp((0.03 - 0.01 - 0.5 * 0.2 * 0.2) * fixingTime);
    for (size_t kind = 0; kind < 4; ++kind) {
        const double futurePart = kind == 0 || kind == 2 ? future : 0.0;
        const double historyPart = kind == 0 ? 80.0 : (kind == 2 ? 0.0 : 160.0);
        const double pv = (historyPart + futurePart) * discount;
        const Vector_<> expected{futurePart / 100.0 * discount, -0.2 * fixingTime * futurePart * discount,
                                 fixingTime * futurePart * discount - paymentTime * pv, -fixingTime * futurePart * discount,
                                 kind == 3 ? 80.0 * discount : 0.0};
        const auto tree = ObservationPathRisk(kind, false);
        const auto compiled = ObservationPathRisk(kind, true);
        ASSERT_NEAR(tree.aggregated_, pv, pv * 1.0e-12);
        ASSERT_NEAR(compiled.aggregated_, pv, pv * 1.0e-12);
        ASSERT_EQ(compiled.names_, tree.names_);
        ASSERT_EQ(compiled.risks_.size(), expected.size());
        for (size_t j = 0; j < expected.size(); ++j) {
            ASSERT_NEAR(tree.risks_[j], expected[j], 1.0e-10);
            ASSERT_NEAR(compiled.risks_[j], expected[j], 1.0e-10);
        }
    }
}

TEST(ScriptCompiledParityTest, TestKnownFixingFuzzyBandAndFiniteDifference) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    const double discount = exp(-0.03 * 10.0 / Dal::DAYS_PER_YEAR);
    const Handle_<Dal::ModelData_> model(new Dal::BSModelData_("", 100.0, 0.2, 0.03, 0.01));
    for (bool compiled : {false, true}) {
        for (double strike : {79.93, 79.95, 80.0, 80.03, 80.07}) {
            SCOPED_TRACE(::testing::Message() << "compiled=" << compiled << " strike=" << strike);
            MonteCarloSettings_ simulation;
            simulation.compiled_ = compiled;
            const auto result =
                Dal::Script::MCSimulation<AAD::Number_>(FuzzyObservationProduct(strike), model, 8193, {}, simulation, CompiledHistory());
            const double weight = (80.0 - strike + 0.1) / 0.2;
            const double expected = 160.0 * weight * discount;
            ASSERT_NEAR(result.aggregated_ / 8193, expected, expected * 1.0e-12);
            ASSERT_NEAR(result["SCALE"], 80.0 * weight * discount, 1.0e-10);
            ASSERT_NEAR(result["K"], -800.0 * discount, 1.0e-10);
            ASSERT_NEAR(result["rate"], -10.0 / Dal::DAYS_PER_YEAR * expected, 1.0e-10);
            ASSERT_NEAR(result["spot"], 0.0, 1.0e-10);
            ASSERT_NEAR(result["vol"], 0.0, 1.0e-10);
            ASSERT_NEAR(FuzzyObservationPrice(strike, compiled), result.aggregated_ / 8193, expected * 1.0e-12);
            const double derivative = (FuzzyObservationPrice(strike + 0.0001, compiled) - FuzzyObservationPrice(strike - 0.0001, compiled)) / 0.0002;
            ASSERT_NEAR(derivative, result["K"], 1.0e-5);
        }
    }
}
