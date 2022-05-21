//
// Created by wegam on 2022/5/21.
//

#include <gtest/gtest.h>
#include <dal/script/visitor/varindexer.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(VarIndexerTest, TestVarIndexerVisit) {
    NodeVar_ var1("x");
    NodeVar_ var2("y");
    NodeConst_ const1(20.0);

    std::unique_ptr<Visitor_> visitor = std::make_unique<VarIndexer_>();
    visitor->Visit(&var1);
    visitor->Visit(&const1);
    visitor->Visit(&var2);

    ASSERT_EQ(var1.index_, 0);
    ASSERT_EQ(var2.index_, 1);
}