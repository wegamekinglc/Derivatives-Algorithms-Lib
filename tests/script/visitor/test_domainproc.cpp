//
// Created by wegam on 2023/4/1.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/all.hpp>
#include <dal/script/parser.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VisitorTtest, TestDomainConst) {
    String_ event = R"(
        x = 2
    )";
    auto res = Parse(event);

    VarIndexer_ indexer;
    for (auto& stat : res)
        stat->Accept(indexer);

    DomainProcessor_ processor(indexer.VarNames().size(), false);
    for (auto& stat : res)
        stat->Accept(processor);

    const auto domains = processor.VarDomains();
    ASSERT_EQ(domains[0].MinBound(), 2.0);
    ASSERT_EQ(domains[0].MaxBound(), 2.0);
    ASSERT_EQ(domains[0].IsConstant(), true);
}

TEST(VisitorTtest, TestDomainContinus) {
    String_ event = R"(
        x = SQRT(spot())
    )";
    auto res = Parse(event);

    VarIndexer_ indexer;
    for (auto& stat : res)
        stat->Accept(indexer);

    DomainProcessor_ processor(indexer.VarNames().size(), false);
    for (auto& stat : res)
        stat->Accept(processor);

    const auto domains = processor.VarDomains();
    ASSERT_EQ(domains[0].MinBound(), 0.0);
    ASSERT_EQ(domains[0].IsPositive(), true);
}
