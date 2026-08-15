//
// Created by wegam on 2022/7/10.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestIFProcessor) {
    Parser_ parser;
    String_ event = R"(
        IF x >= 2 THEN
            y = 3 + x
        ELSE
            y = x
        END
    )";
    auto res = parser.Parse(event);

    VarIndexer_ visitor1;
    IFProcessor_ visitor2;

    res[0]->Accept(visitor1);
    res[0]->Accept(visitor2);

    ASSERT_EQ(visitor2.MaxNestedIFs(), 1);
    ASSERT_EQ(dynamic_cast<NodeIf_*>(res[0].get())->affectedVars_, Vector_<size_t>({1}));
}

TEST(ScriptTest, TestIFProcessorNested) {
    Parser_ parser;
    String_ event = R"(
        IF x >= 2 THEN
            IF x > 4 THEN
                y = x + 5
            ELSE
                y = x + 3
            END
        ELSE
            y = x
            z = x
        END
    )";
    auto res = parser.Parse(event);

    VarIndexer_ visitor1;
    IFProcessor_ visitor2;

    res[0]->Accept(visitor1);
    res[0]->Accept(visitor2);

    ASSERT_EQ(visitor2.MaxNestedIFs(), 2);
    ASSERT_EQ(dynamic_cast<NodeIf_*>(res[0].get())->affectedVars_, Vector_<size_t>({1, 2}));

    auto& nestedIF = res[0]->arguments_[1];
    ASSERT_EQ(dynamic_cast<NodeIf_*>(nestedIF.get())->affectedVars_, Vector_<size_t>({1}));
}


namespace {
    // Parse + index + if-process a single event; returns the statements (keeping
    // nodes alive) and the processor so affectedVars_/MaxNestedIFs can be read.
    std::pair<Vector_<Statement_>, IFProcessor_> ProcessIfs(const String_& src) {
        Parser_ parser;
        auto statements = parser.Parse(src);

        VarIndexer_ indexer;
        for (auto& stat : statements)
            stat->Accept(indexer);

        IFProcessor_ ifProc;
        for (auto& stat : statements)
            stat->Accept(ifProc);

        return {std::move(statements), ifProc};
    }
} // namespace

TEST(IFProcessorTest, TestPaysInsideIfMarksAffectedVar) {
    const auto [statements, ifProc] = ProcessIfs(R"(
        IF x > 1 THEN
            call pays x
        END
    )");

    ASSERT_EQ(ifProc.MaxNestedIFs(), 1u);
    // x is index 0, call is index 1; only call is written inside the if.
    ASSERT_EQ(dynamic_cast<NodeIf_*>(statements[0].get())->affectedVars_, Vector_<size_t>({1}));
}

TEST(IFProcessorTest, TestElseBranchVarsAreAffected) {
    const auto [statements, ifProc] = ProcessIfs(R"(
        IF x > 0 THEN
            a = 1
        ELSE
            b = 2
        END
    )");

    ASSERT_EQ(dynamic_cast<NodeIf_*>(statements[0].get())->affectedVars_, Vector_<size_t>({1, 2}));
}

TEST(IFProcessorTest, TestSequentialIfsDoNotShareAffectedVars) {
    const auto [statements, ifProc] = ProcessIfs(R"(
        IF x > 1 THEN
            y = 1
        END
        IF x > 2 THEN
            z = 1
        END
    )");

    ASSERT_EQ(ifProc.MaxNestedIFs(), 1u);
    ASSERT_EQ(dynamic_cast<NodeIf_*>(statements[0].get())->affectedVars_, Vector_<size_t>({1}));
    ASSERT_EQ(dynamic_cast<NodeIf_*>(statements[1].get())->affectedVars_, Vector_<size_t>({2}));
}

TEST(IFProcessorTest, TestAssignOutsideIfIsNotAffected) {
    const auto [statements, ifProc] = ProcessIfs(R"(
        y = 5
        IF x > 0 THEN
            z = 1
        END
    )");

    ASSERT_EQ(dynamic_cast<NodeIf_*>(statements[1].get())->affectedVars_, Vector_<size_t>({2}));
}

TEST(IFProcessorTest, TestNoIfMeansZeroNesting) {
    const auto [statements, ifProc] = ProcessIfs(R"(
        x = 1
        y = x + 2
    )");

    ASSERT_EQ(ifProc.MaxNestedIFs(), 0u);
}

TEST(IFProcessorTest, TestConditionVarsAreNotAffected) {
    // Vars read in the condition (x) must not appear in affectedVars_.
    const auto [statements, ifProc] = ProcessIfs(R"(
        IF x > 1 AND y < 2 THEN
            z = 1
        END
    )");

    ASSERT_EQ(dynamic_cast<NodeIf_*>(statements[0].get())->affectedVars_, Vector_<size_t>({2}));
}
