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

    res[0]->AcceptVisitor(&visitor1);
    res[0]->AcceptVisitor(&visitor2);

    ASSERT_EQ(visitor2.MaxNestedIFs(), 1);
    ASSERT_EQ(dynamic_cast<NodeIf_*>(res[0].get())->affectedVars_, Vector_<int>({1}));
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

    res[0]->AcceptVisitor(&visitor1);
    res[0]->AcceptVisitor(&visitor2);

    ASSERT_EQ(visitor2.MaxNestedIFs(), 2);
    ASSERT_EQ(dynamic_cast<NodeIf_*>(res[0].get())->affectedVars_, Vector_<int>({1, 2}));

    auto& nestedIF = dynamic_cast<NodeIf_*>(res[0].get())->arguments_[1];
    ASSERT_EQ(dynamic_cast<NodeIf_*>(nestedIF.get())->affectedVars_, Vector_<int>({1}));
}
