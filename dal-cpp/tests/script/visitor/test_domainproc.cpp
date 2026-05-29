//
// Created by wegam on 2023/4/1.
//

#include <gtest/gtest.h>
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
