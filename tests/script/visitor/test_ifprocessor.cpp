//
// Created by wegam on 2022/7/10.
//

#include <gtest/gtest.h>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/varindexer.hpp>
#include <dal/script/visitor/ifprocessor.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(IFProcessorTest, TestIFProcessor) {
    String_ event = R"(
        IF x >= 2 THEN
            y = 3 + x
        ELSE
            y = x
        ENDIF
    )";
    auto res = Parse(event);

    VarIndexer_ visitor1;
    IFProcessor_ visitor2;

    visitor1.Visit(res[0]);
    visitor2.Visit(res[0]);

    ASSERT_EQ(visitor2.MaxNestedIFs(), 1);
    ASSERT_EQ(std::get<std::unique_ptr<NodeIf_>>(res[0])->affectedVars_, Vector_<int>({1}));
}

TEST(IFProcessorTest, TestIFProcessorNested) {
    String_ event = R"(
        IF x >= 2 THEN
            IF x > 4 THEN
                y = x + 5
            ELSE
                y = x + 3
            ENDIF
        ELSE
            y = x
            z = x
        ENDIF
    )";
    auto res = Parse(event);

    VarIndexer_ visitor1;
    IFProcessor_ visitor2;

    visitor1.Visit(res[0]);
    visitor2.Visit(res[0]);

    ASSERT_EQ(visitor2.MaxNestedIFs(), 2);
    ASSERT_EQ(std::get<std::unique_ptr<NodeIf_>>(res[0])->affectedVars_, Vector_<int>({1, 2}));

    auto& nestedIF = std::get<std::unique_ptr<NodeIf_>>(res[0])->arguments_[1];
    ASSERT_EQ(std::get<std::unique_ptr<NodeIf_>>(nestedIF)->affectedVars_, Vector_<int>({1}));
}

