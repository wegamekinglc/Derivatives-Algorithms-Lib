//
// Created by wegam on 2022/7/10.
//

#include <gtest/gtest.h>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VisitorTest, TestIFProcessor) {
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

TEST(VisitorTest, TestIFProcessorNested) {
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

