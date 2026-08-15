//
// Created by wegam on 2023/4/1.
//

#include <gtest/gtest.h>

#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/parser.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestDomainConst) {
    Parser_ parser;

    String_ event = R"(
        x = 2
    )";
    auto res = parser.Parse(event);

    VarIndexer_ indexer;
    for (auto& stat : res)
        stat->Accept(indexer);

    DomainProcessor_ processor(indexer.VarNames().size(), false);
    for (auto& stat : res)
        stat->Accept(processor);

    const auto domains = processor.VarDomains();
    ASSERT_EQ(domains[0].MinBound(), Bound_(2.0));
    ASSERT_EQ(domains[0].MaxBound(), Bound_(2.0));
    ASSERT_EQ(domains[0].IsConstant(), true);
}

TEST(ScriptTest, TestDomainContinus) {
    Parser_ parser;
    String_ event = R"(
        x = SQRT(spot())
    )";
    auto res = parser.Parse(event);

    VarIndexer_ indexer;
    for (auto& stat : res)
        stat->Accept(indexer);

    DomainProcessor_ processor(indexer.VarNames().size(), false);
    for (auto& stat : res)
        stat->Accept(processor);

    const auto domains = processor.VarDomains();
    ASSERT_EQ(domains[0].MinBound(), Bound_(0.0));
    ASSERT_EQ(domains[0].IsPositive(), true);
}

namespace {
    // Full domain-processing pipeline for a single event: parse, index variables,
    // collect if-affected variables, then propagate domains. The parsed statements
    // are returned alongside the domains so node flags can be inspected.
    std::pair<Vector_<Statement_>, Vector_<Domain_>> ProcessDomains(const String_& src, bool fuzzy = false) {
        Parser_ parser;
        auto statements = parser.Parse(src);

        VarIndexer_ indexer;
        for (auto& stat : statements)
            stat->Accept(indexer);

        IFProcessor_ ifProc;
        for (auto& stat : statements)
            stat->Accept(ifProc);

        DomainProcessor_ domProc(indexer.VarNames().size(), fuzzy);
        for (auto& stat : statements)
            stat->Accept(domProc);

        return {std::move(statements), domProc.VarDomains()};
    }
} // namespace

TEST(DomainProcTest, TestIfElseMergesBranchDomains) {
    const auto [statements, domains] = ProcessDomains(R"(
        IF spot() > 0 THEN
            x = 1
        ELSE
            x = 2
        END
    )");

    ASSERT_EQ(domains[0].Size(), 2u);
    ASSERT_TRUE(domains[0].IsDiscrete());
    ASSERT_TRUE(domains[0].Includes(1.0));
    ASSERT_TRUE(domains[0].Includes(2.0));

    const auto* ifNode = dynamic_cast<const NodeIf_*>(statements[0].get());
    ASSERT_FALSE(ifNode->alwaysTrue_);
    ASSERT_FALSE(ifNode->alwaysFalse_);
}

TEST(DomainProcTest, TestIfWithoutElseUnionsPriorDomain) {
    const auto [statements, domains] = ProcessDomains(R"(
        x = 5
        IF spot() > 0 THEN
            x = 1
        END
    )");

    ASSERT_TRUE(domains[0].Includes(5.0));
    ASSERT_TRUE(domains[0].Includes(1.0));
}

TEST(DomainProcTest, TestAlwaysTrueConditionSkipsFalseBranch) {
    const auto [statements, domains] = ProcessDomains(R"(
        x = 2
        IF x > 1 THEN
            y = 1
        ELSE
            y = 2
        END
    )");

    const auto* ifNode = dynamic_cast<const NodeIf_*>(statements[1].get());
    ASSERT_TRUE(ifNode->alwaysTrue_);
    ASSERT_FALSE(ifNode->alwaysFalse_);

    double val;
    ASSERT_TRUE(domains[1].IsConstant(&val));
    ASSERT_DOUBLE_EQ(val, 1.0);
}

TEST(DomainProcTest, TestAlwaysFalseConditionSkipsTrueBranch) {
    const auto [statements, domains] = ProcessDomains(R"(
        x = 2
        IF x < 1 THEN
            y = 1
        ELSE
            y = 2
        END
    )");

    const auto* ifNode = dynamic_cast<const NodeIf_*>(statements[1].get());
    ASSERT_FALSE(ifNode->alwaysTrue_);
    ASSERT_TRUE(ifNode->alwaysFalse_);

    double val;
    ASSERT_TRUE(domains[1].IsConstant(&val));
    ASSERT_DOUBLE_EQ(val, 2.0);
}

TEST(DomainProcTest, TestSpotComparisonIsTrueOrFalse) {
    const auto [statements, domains] = ProcessDomains(R"(
        IF spot() > 0 THEN
            x = 1
        END
    )");

    const auto* ifNode = dynamic_cast<const NodeIf_*>(statements[0].get());
    ASSERT_FALSE(ifNode->alwaysTrue_);
    ASSERT_FALSE(ifNode->alwaysFalse_);

    const auto* condNode = dynamic_cast<const NodeSup_*>(ifNode->arguments_[0].get());
    ASSERT_FALSE(condNode->alwaysTrue_);
    ASSERT_FALSE(condNode->alwaysFalse_);
}

TEST(DomainProcTest, TestReassignmentOverwritesDomain) {
    const auto [statements, domains] = ProcessDomains(R"(
        x = 2
        x = x + 3
    )");

    double val;
    ASSERT_TRUE(domains[0].IsConstant(&val));
    ASSERT_DOUBLE_EQ(val, 5.0);
}

TEST(DomainProcTest, TestExpOfSpotYieldsRealDomain) {
    // ApplyFunc on a continuous input returns the visitor-supplied function
    // domain; for EXP that is the whole real line (deliberately conservative).
    const auto [statements, domains] = ProcessDomains("x = EXP(spot())");

    ASSERT_TRUE(domains[0].MinBound().IsMinusInf());
    ASSERT_TRUE(domains[0].MaxBound().IsPlusInf());
}

TEST(DomainProcTest, TestDivisionByZeroDomainThrows) {
    // Unassigned variables start as the singleton {0}; 1 / y must fail.
    ASSERT_THROW(static_cast<void>(ProcessDomains("x = 1 / y")), Dal::Exception_);
}

TEST(DomainProcTest, TestFuzzyEqualityOnDiscreteDomain) {
    const auto [statements, domains] = ProcessDomains(R"(
        x = 1
        IF spot() > 0 THEN
            x = 2
        ELSE
            x = 3
        END
        IF x = 2 THEN
            y = 1
        END
    )",
                                                      true);

    // x - 2 lives on {0, 1}: zero is discrete, so fuzzy bounds are derived from
    // the neighbouring singletons (default -0.5 on the empty left side).
    const auto* ifNode = dynamic_cast<const NodeIf_*>(statements[2].get());
    const auto* condNode = dynamic_cast<const NodeEqual_*>(ifNode->arguments_[0].get());
    ASSERT_TRUE(condNode->isDiscrete_);
    ASSERT_DOUBLE_EQ(condNode->lb_, -0.5);
    ASSERT_DOUBLE_EQ(condNode->rb_, 1.0);
}
