//
// Created by Codex on 2026/10/13.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using Dal::Cell_;
using Dal::Date_;
using Dal::String_;
using Dal::Vector_;
namespace AAD = Dal::AAD;
namespace Script = Dal::Script;

namespace {
    Script::ScriptProduct_ RawProduct(bool history) {
        Vector_<Cell_> dates{Cell_("K")};
        Vector_<String_> events{"2"};
        if (history) {
            dates.push_back(Cell_(Date_(2026, 9, 30)));
            events.push_back("x = 3 * K APPEND(v, 7 * K)");
        }
        dates.push_back(Cell_(Date_(2027, 10, 1)));
        events.push_back("APPEND(v, SPOT()) pay PAYS x + SUM(v) + K * SPOT()");
        Script::ScriptProduct_ product(dates, events);
        product.PreProcess(true, true);
        return product;
    }

    template <class E_> void CheckInitialization(const Script::ScriptProduct_& product, E_* evaluator) {
        AAD::BlackScholes_<AAD::Number_> model(100.0, 0.2, 0.03, 0.01);
        model.Allocate(product.TimeLine(), product.DefLine());
        AAD::Scenario_<AAD::Number_> path;
        AAD::AllocatePath(product.DefLine(), path);
        for (int generation = 0; generation < 2; ++generation) {
            AAD::RecordingScope_ recording;
            const auto checkpoint = Script::InitModel4ParallelAAD(product, model, path, *evaluator, recording);
            recording.FinishRecording();
            for (int run = 0; run < 3; ++run) {
                recording.Restore(checkpoint);
                const size_t before = AAD::Tape()->nodes_.OccupiedSlots();
                evaluator->Init();
                ASSERT_EQ(AAD::Tape()->nodes_.OccupiedSlots() - before, 0U);
                ASSERT_EQ(evaluator->VarVals().size(), product.VarValues().size());
                for (size_t i = 0; i < product.VarValues().size(); ++i)
                    ASSERT_DOUBLE_EQ(AAD::Value(evaluator->VarVals()[i]), product.VarValues()[i]);
                ASSERT_EQ(evaluator->VectorVals().size(), product.VectorValues().size());
                for (size_t i = 0; i < product.VectorValues().size(); ++i) {
                    ASSERT_EQ(evaluator->VectorVals()[i].size(), product.VectorValues()[i].size());
                    for (size_t j = 0; j < product.VectorValues()[i].size(); ++j)
                        ASSERT_DOUBLE_EQ(AAD::Value(evaluator->VectorVals()[i][j]), product.VectorValues()[i][j]);
                    evaluator->vectors_[i].push_back(AAD::Number_(73.0));
                }
                for (auto& value : evaluator->variables_)
                    value = 91.0;
                recording.FinishRecording();
                recording.ReverseSuffix(checkpoint);
            }
            recording.ReversePrefix(checkpoint);
        }
    }

    void CheckRawInitialization(bool compiled, bool history) {
        const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
        const auto product = RawProduct(history);
        if (compiled) {
            auto evaluator = product.BuildEvalState<AAD::Number_>(0, 0.01);
            ASSERT_NO_FATAL_FAILURE(CheckInitialization(product, &evaluator));
        } else {
            auto evaluator = product.BuildFuzzyEvaluator<AAD::Number_>(0, 0.01);
            ASSERT_NO_FATAL_FAILURE(CheckInitialization(product, &evaluator));
        }
    }

    template <class E_, class F_> void CheckHistoricalRisks(const Script::ScriptProduct_& product, E_* evaluator, F_ evaluate) {
        AAD::BlackScholes_<AAD::Number_> model(100.0, 0.2, 0.03, 0.01);
        model.Allocate(product.TimeLine(), product.DefLine());
        AAD::Scenario_<AAD::Number_> path;
        AAD::AllocatePath(product.DefLine(), path);
        const double discount = std::exp(-0.03);
        Vector_<> expected(5, 0.0);
        double expectedPrice = 0.0;
        for (const double normal : {0.0, 0.3}) {
            const double spot = std::exp(std::log(100.0) + 0.2 * normal);
            const double amount = 20.0 + 3.0 * spot;
            expectedPrice += amount * discount;
            expected[0] += 3.0 * spot / 100.0 * discount;
            expected[1] += 3.0 * spot * (normal - 0.2) * discount;
            expected[2] += (3.0 * spot - amount) * discount;
            expected[3] -= 3.0 * spot * discount;
            expected[4] += spot * discount;
        }
        for (int generation = 0; generation < 2; ++generation) {
            AAD::RecordingScope_ recording;
            const auto checkpoint = Script::InitModel4ParallelAAD(product, model, path, *evaluator, recording);
            recording.FinishRecording();
            double price = 0.0;
            for (const double normal : {0.0, 0.3}) {
                recording.Restore(checkpoint);
                model.GeneratePath(Vector_<>(model.SimDim(), normal), &path);
                evaluate(path, *evaluator);
                const auto& payoff = evaluator->VarVals()[product.PayOffIdx()];
                price += AAD::Value(payoff);
                AAD::Adjoint(payoff) = 1.0;
                recording.FinishRecording();
                recording.ReverseSuffix(checkpoint);
            }
            recording.ReversePrefix(checkpoint);
            ASSERT_NEAR(price, expectedPrice, 1e-10);
            const auto parameters = model.Parameters();
            ASSERT_EQ(parameters.size(), 4);
            for (size_t i = 0; i < parameters.size(); ++i)
                ASSERT_NEAR(AAD::Adjoint(*parameters[i]), expected[i], 1e-10) << "parameter=" << i;
            ASSERT_EQ(evaluator->ConstVarVals().size(), 1);
            ASSERT_NEAR(AAD::Adjoint(evaluator->ConstVarVals()[0]), expected[4], 1e-10);
        }
    }

    void CheckRawHistoricalRisks(bool compiled) {
        const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
        const auto product = RawProduct(true);
        if (compiled) {
            const auto program = product.Compile(true);
            auto evaluator = product.BuildEvalState<AAD::Number_>(0, 0.01);
            ASSERT_NO_FATAL_FAILURE(CheckHistoricalRisks(product, &evaluator, [&](const auto& path, auto& state) { program.Evaluate(path, state); }));
        } else {
            auto evaluator = product.BuildFuzzyEvaluator<AAD::Number_>(0, 0.01);
            ASSERT_NO_FATAL_FAILURE(CheckHistoricalRisks(product, &evaluator, [&](const auto& path, auto& state) { product.Evaluate(path, state); }));
        }
    }
} // namespace

TEST(RawPrefixTest, TestTreeZeroInitialValuesReusePrefix) { ASSERT_NO_FATAL_FAILURE(CheckRawInitialization(false, false)); }

TEST(RawPrefixTest, TestCompiledZeroInitialValuesReusePrefix) { ASSERT_NO_FATAL_FAILURE(CheckRawInitialization(true, false)); }

TEST(RawPrefixTest, TestTreeHistoricalValuesReusePrefix) { ASSERT_NO_FATAL_FAILURE(CheckRawInitialization(false, true)); }

TEST(RawPrefixTest, TestCompiledHistoricalValuesReusePrefix) { ASSERT_NO_FATAL_FAILURE(CheckRawInitialization(true, true)); }

TEST(RawPrefixTest, TestTreeHistoricalRisks) { ASSERT_NO_FATAL_FAILURE(CheckRawHistoricalRisks(false)); }

TEST(RawPrefixTest, TestCompiledHistoricalRisks) { ASSERT_NO_FATAL_FAILURE(CheckRawHistoricalRisks(true)); }

TEST(RawPrefixTest, TestRawHistoricalAadStillRequiresPreparation) {
    const auto date = Dal::XGLOBAL::SetEvaluationDateInScope(Date_(2026, 10, 1));
    const auto product = RawProduct(true);
    const Dal::Handle_<Dal::ModelData_> model(new Dal::BSModelData_("", 100.0, 0.2, 0.03, 0.01));
    for (const bool compiled : {false, true})
        ASSERT_THROW((void)Script::MCSimulation<AAD::Number_>(product, model, 17, "sobol", false, compiled, 0, 0.01), Dal::Exception_);
}
