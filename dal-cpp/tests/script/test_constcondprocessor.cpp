//
// Created by wegam on 2026/05/30.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/visitor/evaluator.hpp>

using namespace Dal;
using namespace Dal::Script;

namespace {
    // Run the full preprocessing chain that the const-condition processor depends on:
    // variable indexing, if processing, and domain processing set the always-true / always-false
    // flags that the const-condition processor consumes, then process from the top of every statement.
    Event_ ParseAndConstCondProcess(const String_& event) {
        Parser_ parser;
        Event_ res = parser.Parse(event);

        VarIndexer_ indexer;
        for (auto& stat : res)
            stat->Accept(indexer);

        IFProcessor_ ifProc;
        for (auto& stat : res)
            stat->Accept(ifProc);

        DomainProcessor_ domProc(indexer.VarNames().size(), false);
        for (auto& stat : res)
            stat->Accept(domProc);

        ConstCondProcessor_ ccProc;
        for (auto& stat : res)
            ccProc.ProcessFromTop(stat);

        return res;
    }
} // namespace

TEST(ScriptTest, TestConstCondAlwaysTrueIfReplacedByCollection) {
    String_ event = R"(
        IF 2 >= 1 THEN
            x = 1
        END
    )";
    auto res = ParseAndConstCondProcess(event);

    const auto* collect = dynamic_cast<const NodeCollect_*>(res[0].get());
    ASSERT_NE(collect, nullptr);
    ASSERT_EQ(collect->arguments_.size(), 1u);
    ASSERT_NE(dynamic_cast<const NodeAssign_*>(collect->arguments_[0].get()), nullptr);
}

TEST(ScriptTest, TestConstCondAlwaysFalseIfReplacedByElse) {
    String_ event = R"(
        IF 2 < 1 THEN
            x = 1
        ELSE
            x = 2
        END
    )";
    auto res = ParseAndConstCondProcess(event);

    const auto* collect = dynamic_cast<const NodeCollect_*>(res[0].get());
    ASSERT_NE(collect, nullptr);
    ASSERT_EQ(collect->arguments_.size(), 1u);

    const auto* assign = dynamic_cast<const NodeAssign_*>(collect->arguments_[0].get());
    ASSERT_NE(assign, nullptr);
    const auto* rhs = dynamic_cast<const NodeConst_*>(assign->arguments_[1].get());
    ASSERT_NE(rhs, nullptr);
    ASSERT_NEAR(rhs->constVal_, 2.0, 1e-10);
}

TEST(ScriptTest, TestConstCondAlwaysFalseNoElseEmptyCollection) {
    String_ event = R"(
        IF 2 < 1 THEN
            x = 1
        END
    )";
    auto res = ParseAndConstCondProcess(event);

    const auto* collect = dynamic_cast<const NodeCollect_*>(res[0].get());
    ASSERT_NE(collect, nullptr);
    ASSERT_EQ(collect->arguments_.size(), 0u);
}

TEST(ScriptTest, TestConstCondAlwaysTrueConditionReplacedByTrueNode) {
    String_ event = R"(
        IF (2 >= 1) AND spot() >= 0 THEN
            x = 1
        END
    )";
    auto res = ParseAndConstCondProcess(event);

    // The if remains, because the compound condition can still be true or false.
    const auto* ifNode = dynamic_cast<const NodeIf_*>(res[0].get());
    ASSERT_NE(ifNode, nullptr);

    const auto* andNode = dynamic_cast<const NodeAnd_*>(ifNode->arguments_[0].get());
    ASSERT_NE(andNode, nullptr);
    ASSERT_NE(dynamic_cast<const NodeTrue_*>(andNode->arguments_[0].get()), nullptr);
}

TEST(ScriptTest, TestConstCondAlwaysFalseConditionReplacedByFalseNode) {
    String_ event = R"(
        IF (2 < 1) OR spot() >= 0 THEN
            x = 1
        END
    )";
    auto res = ParseAndConstCondProcess(event);

    const auto* ifNode = dynamic_cast<const NodeIf_*>(res[0].get());
    ASSERT_NE(ifNode, nullptr);

    const auto* orNode = dynamic_cast<const NodeOr_*>(ifNode->arguments_[0].get());
    ASSERT_NE(orNode, nullptr);
    ASSERT_NE(dynamic_cast<const NodeFalse_*>(orNode->arguments_[0].get()), nullptr);
}

TEST(ScriptTest, TestConstCondNonConstIfUnchanged) {
    String_ event = R"(
        IF spot() >= 1 THEN
            x = 1
        END
    )";
    auto res = ParseAndConstCondProcess(event);

    ASSERT_NE(dynamic_cast<const NodeIf_*>(res[0].get()), nullptr);
}

TEST(ScriptTest, TestConstCondNestedFalseElsePreservesStatementOrder) {
    const auto res = ParseAndConstCondProcess(R"(
        IF 2 < 1 THEN
            x = 90
            x = 99
        ELSE
            x = 1
            IF 3 < 2 THEN
                x = 80
                x = 88
            ELSE
                x = 10 * x + 2
                x = 10 * x + 3
            END
            x = 10 * x + 4
        END
        x = 10 * x + 5
    )");

    ASSERT_EQ(res.size(), 2u);
    const auto* outer = dynamic_cast<const NodeCollect_*>(res[0].get());
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(outer->arguments_.size(), 3u);
    ASSERT_NE(dynamic_cast<const NodeCollect_*>(outer->arguments_[1].get()), nullptr);

    Evaluator_<double> eval(Vector_<>({0.0}));
    for (const auto& stat : res)
        stat->Accept(eval);
    ASSERT_DOUBLE_EQ(eval.VarVals()[0], 12345.0);
}

TEST(ScriptTest, TestConstCondNestedFalseNoElsePreservesStatementOrder) {
    const auto res = ParseAndConstCondProcess(R"(
        x = 1
        IF 2 > 1 THEN
            x = 10 * x + 2
            IF 3 < 2 THEN
                x = 80
                x = 88
            END
            x = 10 * x + 3
        END
        IF 2 < 1 THEN
            x = 90
            x = 99
        END
        x = 10 * x + 4
    )");

    ASSERT_EQ(res.size(), 4u);
    const auto* outer = dynamic_cast<const NodeCollect_*>(res[1].get());
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(outer->arguments_.size(), 3u);
    const auto* nested = dynamic_cast<const NodeCollect_*>(outer->arguments_[1].get());
    ASSERT_NE(nested, nullptr);
    ASSERT_TRUE(nested->arguments_.empty());

    Evaluator_<double> eval(Vector_<>({0.0}));
    for (const auto& stat : res)
        stat->Accept(eval);
    ASSERT_DOUBLE_EQ(eval.VarVals()[0], 1234.0);
}
