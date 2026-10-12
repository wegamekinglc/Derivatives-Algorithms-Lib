//
// Created by Codex on 2026/10/12.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/math/aad/recording.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/parser.hpp>

using Dal::Vector_;
using Dal::AAD::Number_;

namespace {
    void AssertIeeeValue(double actual, double expected) {
        if (std::isnan(expected))
            ASSERT_TRUE(std::isnan(actual));
        else
            ASSERT_DOUBLE_EQ(actual, expected);
        if (expected == 0.0)
            ASSERT_EQ(std::signbit(actual), std::signbit(expected));
    }

    double DiscountSlope(bool delayed, double amount, bool stored) { return delayed ? amount * (stored ? 2.5 : 0.5) : 0.0; }

    double RawPayment(bool delayed, double amount) { return amount * (delayed ? 0.75 : 1.0); }

    struct ArithmeticCase_ {
        Dal::String_ expression_;
        double value_, slope_;
        size_t leaves_;
    };

    void CheckArithmetic(const ArithmeticCase_& data, bool fuzzy) {
        Dal::Script::Parser_ parser;
        auto event = parser.Parse("pay = " + data.expression_);
        Dal::Script::VarIndexer_ indexer;
        event[0]->Accept(indexer);
        auto check = [&](auto* evaluator) {
            Dal::AAD::Scenario_<Number_> path(1);
            evaluator->SetScenario(&path);
            evaluator->SetCurEvt(0);
            Dal::AAD::RecordingScope_ recording;
            recording.RegisterInput(path[0].spot_, 2.0);
            recording.StartRecording();
            auto first = Dal::AAD::Tape()->nodes_.End();
            event[0]->Accept(*evaluator);
            size_t leaves = 0;
            for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
                leaves += first->NumArguments() == 0;
            ASSERT_EQ(leaves, data.leaves_) << data.expression_ << " fuzzy=" << fuzzy;
            ASSERT_NEAR(Dal::AAD::Value(evaluator->VarVals()[0]), data.value_, 1e-10);
            Dal::AAD::Adjoint(evaluator->VarVals()[0]) = 1.0;
            recording.FinishRecording();
            recording.Reverse();
            ASSERT_NEAR(Dal::AAD::Adjoint(path[0].spot_), data.slope_, 1e-10);
        };
        if (fuzzy) {
            Dal::Script::FuzzyEvaluator_<Number_> evaluator(Vector_<>{0.0}, {}, 0);
            ASSERT_NO_FATAL_FAILURE(check(&evaluator));
        } else {
            Dal::Script::Evaluator_<Number_> evaluator(Vector_<>{0.0});
            ASSERT_NO_FATAL_FAILURE(check(&evaluator));
        }
    }

    void CheckConstantPayment(int mode, bool delayed, double amount, size_t payoff) {
        Dal::Script::Parser_ parser;
        auto event = parser.Parse("pay PAYS 0");
        Dal::Script::VarIndexer_ indexer;
        event[0]->Accept(indexer);
        auto* payment = dynamic_cast<Dal::Script::NodePays_*>(event[0].get());
        ASSERT_NE(payment, nullptr);
        Dal::Script::Downcast<Dal::Script::NodeConst_>(payment->arguments_[1])->constVal_ = amount;
        if (delayed)
            payment->discountId_ = 0;
        Dal::Script::Evaluator_<Number_> ordinary(Vector_<>{3.0});
        Dal::Script::FuzzyEvaluator_<Number_> fuzzy(Vector_<>{3.0}, {}, 0);
        Dal::AAD::Scenario_<Number_> path(1);
        path[0].discounts_.Resize(1);
        Vector_<Number_> seed(1), payments(1);
        Dal::Script::LsmcFuzzySinks_<Number_> sinks;
        sinks.pays_ = &payments;
        sinks.payoffIdx_ = payoff;
        if (mode == 2)
            fuzzy.lsmcFuzzySinks_ = &sinks;
        auto check = [&](auto* evaluator) {
            evaluator->SetScenario(&path);
            evaluator->SetCurEvt(0);
            Dal::AAD::RecordingScope_ recording;
            recording.RegisterInput(seed[0], 3.0);
            recording.RegisterInput(path[0].numeraire_, 2.0);
            recording.RegisterInput(path[0].discounts_[0], 0.75);
            recording.RegisterInput(payments[0], 0.0);
            evaluator->SetHistoricalSeed(seed);
            evaluator->Init();
            recording.StartRecording();
            auto first = Dal::AAD::Tape()->nodes_.End();
            event[0]->Accept(*evaluator);
            size_t leaves = 0;
            for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
                leaves += first->NumArguments() == 0;
            ASSERT_EQ(leaves, 0U) << "mode=" << mode << " delayed=" << delayed << " amount=" << amount;
            const double raw = RawPayment(delayed, amount);
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(evaluator->VarVals()[0]), 3.0 + raw / 2.0);
            const bool stored = mode == 2 && payoff == 0;
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(payments[0]), stored ? raw : 0.0);
            Dal::AAD::Adjoint(evaluator->VarVals()[0]) = 1.0;
            Dal::AAD::Adjoint(payments[0]) += stored ? 2.0 : 0.0;
            recording.FinishRecording();
            recording.Reverse();
            ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(seed[0]), 1.0);
            ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].numeraire_), -raw / 4.0);
            ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].discounts_[0]), DiscountSlope(delayed, amount, stored));
        };
        if (mode == 0)
            ASSERT_NO_FATAL_FAILURE(check(&ordinary));
        else
            ASSERT_NO_FATAL_FAILURE(check(&fuzzy));
    }

    struct ExtremumCase_ {
        size_t literalIndex_, leaves_;
        double literal_, input_, value_, slope_;
    };

    ExtremumCase_ ExtremumCase(double left, double right, bool rightSelected, bool literalLeft) {
        const bool activeSelected = rightSelected == literalLeft;
        return {literalLeft ? 0U : 1U,      activeSelected ? 0U : 1U,     literalLeft ? left : right,
                literalLeft ? right : left, rightSelected ? right : left, activeSelected ? 1.0 : 0.0};
    }

    void CheckExtremum(const ExtremumCase_& data, bool maximum, bool fuzzy) {
        Dal::Script::Parser_ parser;
        const Dal::String_ function = maximum ? "MAX" : "MIN";
        auto event = parser.Parse("pay = " + function + (data.literalIndex_ == 0 ? "(0, SPOT())" : "(SPOT(), 0)"));
        Dal::Script::VarIndexer_ indexer;
        event[0]->Accept(indexer);
        const auto& expression = event[0]->arguments_[1];
        Dal::Script::Downcast<Dal::Script::NodeConst_>(expression->arguments_[data.literalIndex_])->constVal_ = data.literal_;
        auto check = [&](auto* evaluator) {
            Dal::AAD::Scenario_<Number_> path(1);
            evaluator->SetScenario(&path);
            evaluator->SetCurEvt(0);
            Dal::AAD::RecordingScope_ recording;
            recording.RegisterInput(path[0].spot_, data.input_);
            recording.StartRecording();
            auto first = Dal::AAD::Tape()->nodes_.End();
            event[0]->Accept(*evaluator);
            size_t leaves = 0;
            for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
                leaves += first->NumArguments() == 0;
            ASSERT_EQ(leaves, data.leaves_);
            ASSERT_NO_FATAL_FAILURE(AssertIeeeValue(Dal::AAD::Value(evaluator->VarVals()[0]), data.value_));
            Dal::AAD::Adjoint(evaluator->VarVals()[0]) = 1.0;
            recording.FinishRecording();
            recording.Reverse();
            ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].spot_), data.slope_);
        };
        if (fuzzy) {
            Dal::Script::FuzzyEvaluator_<Number_> evaluator(Vector_<>{0.0}, {}, 0);
            ASSERT_NO_FATAL_FAILURE(check(&evaluator));
        } else {
            Dal::Script::Evaluator_<Number_> evaluator(Vector_<>{0.0});
            ASSERT_NO_FATAL_FAILURE(check(&evaluator));
        }
    }

    template <class EVAL_> void CheckCheckpointReuse(const EVAL_& original) {
        EVAL_ copied(original);
        EVAL_ evaluator(std::move(copied));
        Dal::Script::Parser_ parser;
        auto event = parser.Parse("pay PAYS MAX(SPOT(), 3) pay PAYS 2");
        Dal::Script::VarIndexer_ indexer;
        for (auto& statement : event)
            statement->Accept(indexer);
        Dal::AAD::Scenario_<Number_> path(1);
        Vector_<Number_> seed(1);
        evaluator.SetScenario(&path);
        evaluator.SetCurEvt(0);
        Dal::AAD::RecordingScope_ recording;
        recording.RegisterInput(path[0].spot_, 1.0);
        recording.RegisterInput(path[0].numeraire_, 2.0);
        recording.RegisterInput(seed[0], 3.0);
        const Number_ initialSpot = path[0].spot_;
        evaluator.SetHistoricalSeed(seed);
        recording.StartRecording();
        const auto checkpoint = recording.MakeCheckpoint();
        recording.FinishRecording();
        for (const double offset : {0.0, 4.0}) {
            recording.Restore(checkpoint);
            path[0].spot_ = initialSpot + offset;
            evaluator.Init();
            auto first = Dal::AAD::Tape()->nodes_.End();
            for (const auto& statement : event)
                statement->Accept(evaluator);
            size_t leaves = 0;
            for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
                leaves += first->NumArguments() == 0;
            ASSERT_EQ(leaves, offset == 0.0 ? 1U : 0U);
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(evaluator.VarVals()[0]), 4.0 + std::max(1.0 + offset, 3.0) / 2.0);
            Dal::AAD::Adjoint(evaluator.VarVals()[0]) = 1.0;
            recording.FinishRecording();
            recording.ReverseSuffix(checkpoint);
        }
        recording.ReversePrefix(checkpoint);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(initialSpot), 0.5);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(seed[0]), 2.0);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].numeraire_), -3.0);
    }

    void AssertBranchRisks(const Dal::AAD::Scenario_<Number_>& path, bool delayed, bool rawOnly) {
        const double discount = delayed ? 0.75 : 1.0;
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].spot_), discount * (rawOnly ? 1.0 : 0.5));
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].numeraire_), rawOnly ? 0.0 : -0.75 * discount);
        ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].discounts_[0]), delayed ? (rawOnly ? 3.0 : 1.5) : 0.0);
    }
} // namespace

TEST(TreeScalarsTest, TestLiteralArithmeticDoesNotRecordOperandLeaf) {
    Dal::Script::Parser_ parser;
    auto event = parser.Parse("pay = SPOT() + 1");
    Dal::Script::VarIndexer_ indexer;
    event[0]->Accept(indexer);
    Dal::Script::Evaluator_<Number_> evaluator(Vector_<>{0.0});
    Dal::AAD::Scenario_<Number_> path(1);
    evaluator.SetScenario(&path);
    evaluator.SetCurEvt(0);
    Dal::AAD::RecordingScope_ recording;
    recording.RegisterInput(path[0].spot_, 2.0);
    recording.StartRecording();
    auto first = Dal::AAD::Tape()->nodes_.End();
    event[0]->Accept(evaluator);
    size_t leaves = 0;
    for (; first != Dal::AAD::Tape()->nodes_.End(); ++first)
        leaves += first->NumArguments() == 0;
    ASSERT_EQ(leaves, 0U);
    ASSERT_DOUBLE_EQ(Dal::AAD::Value(evaluator.VarVals()[0]), 3.0);
    Dal::AAD::Adjoint(evaluator.VarVals()[0]) = 1.0;
    recording.FinishRecording();
    recording.Reverse();
    ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].spot_), 1.0);
}

TEST(TreeScalarsTest, TestArithmeticOperandsKeepValuesAndGradients) {
    const Vector_<ArithmeticCase_> cases{{"SPOT() + 3", 5.0, 1.0, 0},
                                         {"3 + SPOT()", 5.0, 1.0, 0},
                                         {"SPOT() + SPOT()", 4.0, 2.0, 0},
                                         {"2 + 3", 5.0, 0.0, 1},
                                         {"SPOT() - 3", -1.0, 1.0, 0},
                                         {"3 - SPOT()", 1.0, -1.0, 0},
                                         {"SPOT() - SPOT()", 0.0, 0.0, 0},
                                         {"2 - 3", -1.0, 0.0, 1},
                                         {"SPOT() * 3", 6.0, 3.0, 0},
                                         {"3 * SPOT()", 6.0, 3.0, 0},
                                         {"SPOT() * SPOT()", 4.0, 4.0, 0},
                                         {"2 * 3", 6.0, 0.0, 1},
                                         {"SPOT() / 3", 2.0 / 3.0, 1.0 / 3.0, 0},
                                         {"3 / SPOT()", 1.5, -0.75, 0},
                                         {"SPOT() / SPOT()", 1.0, 0.0, 0},
                                         {"6 / 3", 2.0, 0.0, 1},
                                         {"SPOT() ^ 3", 8.0, 12.0, 0},
                                         {"3 ^ SPOT()", 9.0, 9.0 * std::log(3.0), 0},
                                         {"SPOT() ^ SPOT()", 4.0, 4.0 * (std::log(2.0) + 1.0), 0},
                                         {"2 ^ 3", 8.0, 0.0, 1}};
    for (const auto& data : cases)
        for (const bool fuzzy : {false, true})
            ASSERT_NO_FATAL_FAILURE(CheckArithmetic(data, fuzzy));
}

TEST(TreeScalarsTest, TestUnselectedExtremumDoesNotRecordLiteral) {
    const Vector_<ArithmeticCase_> cases{{"MAX(SPOT(), 1)", 2.0, 1.0, 0},      {"MAX(1, SPOT())", 2.0, 1.0, 0},     {"MIN(SPOT(), 3)", 2.0, 1.0, 0},
                                         {"MIN(3, SPOT())", 2.0, 1.0, 0},      {"MAX(2, 3)", 3.0, 0.0, 1},          {"MIN(3, 2)", 2.0, 0.0, 1},
                                         {"MAX(SPOT(), SPOT())", 2.0, 1.0, 0}, {"MIN(SPOT(), SPOT())", 2.0, 1.0, 0}};
    for (const auto& data : cases)
        for (const bool fuzzy : {false, true})
            ASSERT_NO_FATAL_FAILURE(CheckArithmetic(data, fuzzy));
}

TEST(TreeScalarsTest, TestConstantPaymentsPreserveAccumulationAndRawRisks) {
    for (const int mode : {0, 1, 2})
        for (const bool delayed : {false, true})
            for (const double amount : {0.0, -4.0, 4.0})
                for (const size_t payoff : {0U, 1U})
                    ASSERT_NO_FATAL_FAILURE(CheckConstantPayment(mode, delayed, amount, payoff));
}

TEST(TreeScalarsTest, TestExtremaPreserveSourceLeftAtTiesAndIeeeBoundaries) {
    struct Case_ {
        double left_, right_;
        bool maxRight_, minRight_;
    };
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const Vector_<Case_> cases{{1.0, 2.0, true, false},   {2.0, 1.0, false, true},  {1.0, 1.0, false, false}, {-0.0, 0.0, false, false},
                               {0.0, -0.0, false, false}, {nan, 1.0, false, false}, {1.0, nan, false, false}, {inf, 1.0, false, true},
                               {-inf, 1.0, true, false},  {1.0, inf, true, false},  {1.0, -inf, false, true}, {inf, inf, false, false},
                               {-inf, -inf, false, false}};
    for (const auto& data : cases)
        for (const bool literalLeft : {false, true})
            for (const bool fuzzy : {false, true}) {
                ASSERT_NO_FATAL_FAILURE(CheckExtremum(ExtremumCase(data.left_, data.right_, data.maxRight_, literalLeft), true, fuzzy));
                ASSERT_NO_FATAL_FAILURE(CheckExtremum(ExtremumCase(data.left_, data.right_, data.minRight_, literalLeft), false, fuzzy));
            }
}

TEST(TreeScalarsTest, TestRawAndProcessedVariableReadsKeepHistoricalSeedRisks) {
    for (const bool processed : {false, true})
        for (const bool literalLeft : {false, true})
            for (const bool fuzzy : {false, true}) {
                Dal::Script::Parser_ parser;
                auto event = parser.Parse(literalLeft ? "pay = 3 + x" : "pay = x + 3");
                Dal::Script::VarIndexer_ indexer;
                event[0]->Accept(indexer);
                if (processed) {
                    Dal::Script::ConstProcessor_ constants(indexer.VarNames().size());
                    event[0]->Accept(constants);
                }
                const auto& expression = event[0]->arguments_[1];
                const auto* variable = Dal::Script::Downcast<Dal::Script::NodeVar_>(expression->arguments_[literalLeft ? 1 : 0]);
                ASSERT_TRUE(variable->isConst_);
                ASSERT_DOUBLE_EQ(variable->constVal_, 0.0);
                auto check = [&](auto* evaluator) {
                    Vector_<Number_> seed(2);
                    Dal::AAD::RecordingScope_ recording;
                    recording.RegisterInput(seed[0], 0.0);
                    recording.RegisterInput(seed[1], 2.0);
                    evaluator->SetHistoricalSeed(seed);
                    evaluator->Init();
                    recording.StartRecording();
                    event[0]->Accept(*evaluator);
                    ASSERT_DOUBLE_EQ(Dal::AAD::Value(evaluator->VarVals()[0]), 5.0);
                    Dal::AAD::Adjoint(evaluator->VarVals()[0]) = 1.0;
                    recording.FinishRecording();
                    recording.Reverse();
                    ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(seed[1]), 1.0);
                };
                if (fuzzy) {
                    Dal::Script::FuzzyEvaluator_<Number_> evaluator(Vector_<>{0.0, 0.0}, {}, 0);
                    ASSERT_NO_FATAL_FAILURE(check(&evaluator));
                } else {
                    Dal::Script::Evaluator_<Number_> evaluator(Vector_<>{0.0, 0.0});
                    ASSERT_NO_FATAL_FAILURE(check(&evaluator));
                }
            }
}

TEST(TreeScalarsTest, TestBoundConstantsRemainMutableActiveInputs) {
    const Vector_<ArithmeticCase_> cases{{"SPOT() + K", 5.0, 1.0, 0}, {"K + SPOT()", 5.0, 1.0, 0},     {"SPOT() * K", 6.0, 3.0, 0},
                                         {"K * SPOT()", 6.0, 3.0, 0}, {"MAX(SPOT(), K)", 3.0, 0.0, 0}, {"MIN(K, SPOT())", 2.0, 1.0, 0}};
    const Vector_<> boundSlopes{1.0, 1.0, 2.0, 2.0, 1.0, 0.0};
    for (size_t i = 0; i < cases.size(); ++i)
        for (const bool fuzzy : {false, true}) {
            Dal::Script::Parser_ parser({{"K", 10.0}});
            auto event = parser.Parse("pay = " + cases[i].expression_);
            Dal::Script::VarIndexer_ indexer;
            event[0]->Accept(indexer);
            auto check = [&](auto* evaluator) {
                Dal::AAD::Scenario_<Number_> path(1);
                evaluator->SetScenario(&path);
                evaluator->SetCurEvt(0);
                Dal::AAD::RecordingScope_ recording;
                recording.RegisterInput(path[0].spot_, 2.0);
                recording.RegisterInput(evaluator->ConstVarVals()[0], 3.0);
                recording.StartRecording();
                event[0]->Accept(*evaluator);
                ASSERT_DOUBLE_EQ(Dal::AAD::Value(evaluator->VarVals()[0]), cases[i].value_);
                Dal::AAD::Adjoint(evaluator->VarVals()[0]) = 1.0;
                recording.FinishRecording();
                recording.Reverse();
                ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(path[0].spot_), cases[i].slope_);
                ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(evaluator->ConstVarVals()[0]), boundSlopes[i]);
            };
            if (fuzzy) {
                Dal::Script::FuzzyEvaluator_<Number_> evaluator(Vector_<>{0.0}, Vector_<Number_>{10.0}, 0);
                ASSERT_NO_FATAL_FAILURE(check(&evaluator));
            } else {
                Dal::Script::Evaluator_<Number_> evaluator(Vector_<>{0.0}, Vector_<Number_>{10.0});
                ASSERT_NO_FATAL_FAILURE(check(&evaluator));
            }
        }
}

TEST(TreeScalarsTest, TestSelectedConstantsAndPaymentsSurviveCopyMoveAndCheckpointReuse) {
    for (int generation = 0; generation < 2; ++generation) {
        Dal::Script::Evaluator_<Number_> ordinary(Vector_<>{3.0});
        ASSERT_NO_FATAL_FAILURE(CheckCheckpointReuse(ordinary));
        Dal::Script::FuzzyEvaluator_<Number_> fuzzy(Vector_<>{3.0}, {}, 0);
        ASSERT_NO_FATAL_FAILURE(CheckCheckpointReuse(fuzzy));
    }
}

TEST(TreeScalarsTest, TestLiteralPaymentsBlendRawRowsInsideFuzzyBranches) {
    for (const bool delayed : {false, true})
        for (const bool rawOnly : {false, true}) {
            Dal::Script::Parser_ parser;
            auto event = parser.Parse("IF SPOT() > 0:2 THEN pay PAYS 4 ELSE pay PAYS 2 END");
            Dal::Script::VarIndexer_ indexer;
            event[0]->Accept(indexer);
            auto* branch = dynamic_cast<Dal::Script::NodeIf_*>(event[0].get());
            ASSERT_NE(branch, nullptr);
            branch->affectedVars_ = {0};
            if (delayed)
                for (const size_t i : {1U, 2U})
                    dynamic_cast<Dal::Script::NodePays_*>(branch->arguments_[i].get())->discountId_ = 0;
            Dal::Script::FuzzyEvaluator_<Number_> evaluator(Vector_<>{0.0}, {}, 1, 2.0);
            Dal::AAD::Scenario_<Number_> path(1);
            path[0].discounts_.Resize(1);
            Vector_<Number_> seed(1), payments(1);
            Dal::Script::LsmcFuzzySinks_<Number_> sinks;
            sinks.pays_ = &payments;
            sinks.payoffIdx_ = 0;
            evaluator.lsmcFuzzySinks_ = &sinks;
            evaluator.SetScenario(&path);
            evaluator.SetCurEvt(0);
            Dal::AAD::RecordingScope_ recording;
            recording.RegisterInput(path[0].spot_, 0.0);
            recording.RegisterInput(path[0].numeraire_, 2.0);
            recording.RegisterInput(path[0].discounts_[0], 0.75);
            recording.RegisterInput(seed[0], 0.0);
            recording.RegisterInput(payments[0], 0.0);
            evaluator.SetHistoricalSeed(seed);
            evaluator.Init();
            recording.StartRecording();
            evaluator.InitRecordingConstants();
            event[0]->Accept(evaluator);
            const double discount = delayed ? 0.75 : 1.0;
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(payments[0]), 3.0 * discount);
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(evaluator.VarVals()[0]), 1.5 * discount);
            Dal::AAD::Adjoint(rawOnly ? payments[0] : evaluator.VarVals()[0]) = 1.0;
            recording.FinishRecording();
            recording.Reverse();
            ASSERT_NO_FATAL_FAILURE(AssertBranchRisks(path, delayed, rawOnly));
        }
}
