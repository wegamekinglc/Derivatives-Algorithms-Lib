//
// Created by wegam on 2022/5/22.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestDebuggerVisit) {
    Expression_ var1 = MakeBaseNode<NodeVar_>("x");
    Expression_ const1 = MakeBaseNode<NodeConst_>(20);

    Debugger_ visitor;
    var1->Accept(visitor);
    ASSERT_EQ(visitor.String(), String_("VAR[x,-1,0.000000]\n"));

    const1->Accept(visitor);
    ASSERT_EQ(visitor.String(), String_("CONST[20.000000]\n"));
}

TEST(ScriptTest, TestDebuggerVisitNestedExpression) {
    //  MAX(spot() - STRIKE, 0.0)
    Expression_ spot = MakeBaseNode<NodeSpot_>();
    Expression_ strike = MakeBaseNode<NodeConstVar_>("STRIKE", 100.0);
    Expression_ diff = MakeBaseBinary<NodeSub_>(spot, strike);
    Expression_ zero = MakeBaseNode<NodeConst_>(0.0);
    Expression_ maxNode = MakeBaseBinary<NodeMax_>(diff, zero);

    Debugger_ visitor;
    maxNode->Accept(visitor);
    ASSERT_EQ(visitor.String(),
              String_("MAX(\n"
                      "\tSUBTRACT(\n"
                      "\t\tSPOT\n"
                      "\t,\n"
                      "\t\tCONST_VAR[STRIKE,-1,100.000000]\n"
                      "\t)\n"
                      ",\n"
                      "\tCONST[0.000000]\n"
                      ")\n"));
}

TEST(ScriptTest, TestDebuggerVisitIfStatement) {
    //  IF spot() > BARRIER THEN alive = 0 END
    Expression_ spot = MakeBaseNode<NodeSpot_>();
    Expression_ barrier = MakeBaseNode<NodeConstVar_>("BARRIER", 150.0);
    Expression_ diff = MakeBaseBinary<NodeSub_>(spot, barrier);
    auto cond = MakeNode<NodeSup_>();
    cond->arguments_.Resize(1);
    cond->arguments_[0] = std::move(diff);

    auto assign = MakeNode<NodeAssign_>();
    assign->arguments_.Resize(2);
    assign->arguments_[0] = MakeBaseNode<NodeVar_>("alive");
    assign->arguments_[1] = MakeBaseNode<NodeConst_>(0.0);

    auto ifNode = MakeNode<NodeIf_>();
    ifNode->arguments_.Resize(2);
    ifNode->arguments_[0] = std::move(cond);
    ifNode->arguments_[1] = std::move(assign);

    Debugger_ visitor;
    ifNode->Accept(visitor);
    ASSERT_EQ(visitor.String(),
              String_("IF[FIRSTELSE=-1](\n"
                      "\tGTZERO[CONT,EPS=0.000000](\n"
                      "\t\tSUBTRACT(\n"
                      "\t\t\tSPOT\n"
                      "\t\t,\n"
                      "\t\t\tCONST_VAR[BARRIER,-1,150.000000]\n"
                      "\t\t)\n"
                      "\t)\n"
                      ",\n"
                      "\tASSIGN(\n"
                      "\t\tVAR[alive,-1,0.000000]\n"
                      "\t,\n"
                      "\t\tCONST[0.000000]\n"
                      "\t)\n"
                      ")\n"));
}

TEST(ScriptTest, TestDebuggerVisitWithEmptyEvent) {
    Vector_<Cell_> dates;
    Vector_<String_> events;
    ScriptProduct_ product(dates, events);

    std::ostringstream out;
    product.Debug(out);
    String_ desc(out.str());
    Vector_<String_> rtn = String::Split(desc, '\n', true);
    ASSERT_EQ(rtn.size(), 0);
}

namespace {
    //  MAX(spot() - STRIKE, 0.0)
    Expression_ MakeMaxStrikeZero() {
        Expression_ spot = MakeBaseNode<NodeSpot_>();
        Expression_ strike = MakeBaseNode<NodeConstVar_>("STRIKE", 100.0);
        Expression_ diff = MakeBaseBinary<NodeSub_>(spot, strike);
        Expression_ zero = MakeBaseNode<NodeConst_>(0.0);
        return MakeBaseBinary<NodeMax_>(diff, zero);
    }

    //  IF spot() > BARRIER:eps THEN alive = 0 END
    Expression_ MakeKnockoutIf(double eps) {
        Expression_ spot = MakeBaseNode<NodeSpot_>();
        Expression_ barrier = MakeBaseNode<NodeConstVar_>("BARRIER", 150.0);
        Expression_ diff = MakeBaseBinary<NodeSub_>(spot, barrier);
        auto cond = MakeNode<NodeSup_>();
        cond->arguments_.Resize(1);
        cond->arguments_[0] = std::move(diff);
        cond->eps_ = eps;

        auto assign = MakeNode<NodeAssign_>();
        assign->arguments_.Resize(2);
        assign->arguments_[0] = MakeBaseNode<NodeVar_>("alive");
        assign->arguments_[1] = MakeBaseNode<NodeConst_>(0.0);

        auto ifNode = MakeNode<NodeIf_>();
        ifNode->arguments_.Resize(2);
        ifNode->arguments_[0] = std::move(cond);
        ifNode->arguments_[1] = std::move(assign);
        return ifNode;
    }
} // namespace

TEST(ScriptTest, TestDebuggerJsonNode) {
    Expression_ maxNode = MakeMaxStrikeZero();

    Debugger_ visitor;
    maxNode->Accept(visitor);
    std::ostringstream out;
    size_t id = 0;
    DebugNodeJson(visitor.Top(), id, out);
    ASSERT_EQ(out.str(),
              "{\"id\":\"n0\",\"kind\":\"max\",\"children\":["
              "{\"id\":\"n1\",\"kind\":\"sub\",\"children\":["
              "{\"id\":\"n2\",\"kind\":\"spot\"},"
              "{\"id\":\"n3\",\"kind\":\"const_var\",\"name\":\"STRIKE\",\"index\":-1,\"value\":100}"
              "]},"
              "{\"id\":\"n4\",\"kind\":\"const\",\"value\":0}"
              "]}");
}

TEST(ScriptTest, TestDebuggerJsonIfNode) {
    Expression_ ifNode = MakeKnockoutIf(0.1);

    Debugger_ visitor;
    ifNode->Accept(visitor);
    std::ostringstream out;
    size_t id = 0;
    DebugNodeJson(visitor.Top(), id, out);
    ASSERT_EQ(out.str(),
              "{\"id\":\"n0\",\"kind\":\"if\","
              "\"condition\":{"
              "\"id\":\"n1\",\"kind\":\"gt0\",\"mode\":\"continuous\",\"eps\":0.1,\"children\":["
              "{\"id\":\"n2\",\"kind\":\"sub\",\"children\":["
              "{\"id\":\"n3\",\"kind\":\"spot\"},"
              "{\"id\":\"n4\",\"kind\":\"const_var\",\"name\":\"BARRIER\",\"index\":-1,\"value\":150}"
              "]}"
              "]},"
              "\"then\":[{"
              "\"id\":\"n5\",\"kind\":\"assign\","
              "\"target\":{\"id\":\"n6\",\"kind\":\"var\",\"name\":\"alive\",\"index\":-1,\"const_value\":0},"
              "\"value\":{\"id\":\"n7\",\"kind\":\"const\",\"value\":0}"
              "}],"
              "\"else\":[]}");
}

TEST(ScriptTest, TestDebuggerTreeInline) {
    Expression_ maxNode = MakeMaxStrikeZero();
    Debugger_ visitor;
    maxNode->Accept(visitor);

    Vector_<String_> lines;
    DebugNodeTree(visitor.Top(), "(1) ", "    ", TreeStyle(false), 125, lines);
    ASSERT_EQ(lines.size(), 1u);
    ASSERT_EQ(lines[0], String_("(1) max(spot() − STRIKE, 0)"));
}

TEST(ScriptTest, TestDebuggerTreeBranchesWhenTooWide) {
    Expression_ maxNode = MakeMaxStrikeZero();
    Debugger_ visitor;
    maxNode->Accept(visitor);

    Vector_<String_> lines;
    DebugNodeTree(visitor.Top(), "(1) ", "    ", TreeStyle(false), 20, lines);
    ASSERT_EQ(lines.size(), 5u);
    ASSERT_EQ(lines[0], String_("(1) max"));
    ASSERT_EQ(lines[1], String_("    ├── −"));
    ASSERT_EQ(lines[2], String_("    │   ├── spot()"));
    ASSERT_EQ(lines[3], String_("    │   └── STRIKE"));
    ASSERT_EQ(lines[4], String_("    └── 0"));
}

TEST(ScriptTest, TestDebuggerTreeAsciiStyle) {
    Expression_ maxNode = MakeMaxStrikeZero();
    Debugger_ visitor;
    maxNode->Accept(visitor);

    Vector_<String_> lines;
    DebugNodeTree(visitor.Top(), "(1) ", "    ", TreeStyle(true), 20, lines);
    ASSERT_EQ(lines.size(), 5u);
    ASSERT_EQ(lines[0], String_("(1) max"));
    ASSERT_EQ(lines[1], String_("    |-- -"));
    ASSERT_EQ(lines[2], String_("    |   |-- spot()"));
    ASSERT_EQ(lines[3], String_("    |   `-- STRIKE"));
    ASSERT_EQ(lines[4], String_("    `-- 0"));
}

TEST(ScriptTest, TestDebuggerTreeIfStatement) {
    Expression_ ifNode = MakeKnockoutIf(0.1);
    Debugger_ visitor;
    ifNode->Accept(visitor);

    Vector_<String_> lines;
    DebugNodeTree(visitor.Top(), "(1) ", "    ", TreeStyle(false), 125, lines);
    ASSERT_EQ(lines.size(), 1u);
    ASSERT_EQ(lines[0], String_("(1) if spot() > BARRIER ⟨ε=0.1⟩ then alive ← 0"));
}

TEST(ScriptTest, TestDebuggerTreeNegPowIsParenthesized) {
    //  -(a ^ b) must not render as the ambiguous "−a ^ b"
    Expression_ a = MakeBaseNode<NodeVar_>("a");
    Expression_ b = MakeBaseNode<NodeVar_>("b");
    Expression_ powNode = MakeBaseBinary<NodePow_>(a, b);
    auto neg = MakeNode<NodeUMinus_>();
    neg->arguments_.Resize(1);
    neg->arguments_[0] = std::move(powNode);

    Debugger_ visitor;
    neg->Accept(visitor);
    Vector_<String_> lines;
    DebugNodeTree(visitor.Top(), "", "", TreeStyle(false), 125, lines);
    ASSERT_EQ(lines.size(), 1u);
    ASSERT_EQ(lines[0], String_("−(a ^ b)"));
}

TEST(ScriptTest, TestDebuggerJsonStringPassesUtf8Through) {
    //  Multi-byte sequences must pass through as UTF-8, never \u-escaped per byte
    std::ostringstream utf8;
    JsonWriteString(String_("naïve"), utf8);
    ASSERT_EQ(utf8.str(), "\"naïve\"");

    std::ostringstream quoted;
    JsonWriteString(String_("a\"b\\c"), quoted);
    ASSERT_EQ(quoted.str(), "\"a\\\"b\\\\c\"");

    std::ostringstream control;
    JsonWriteString(String_(std::string("a\nb", 3)), control);
    ASSERT_EQ(control.str(), "\"a\\nb\"");
}

TEST(ScriptTest, TestDebuggerProductJson) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));
    const Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 9, 25)), Cell_(Date_(2025, 9, 25))};
    const Vector_<String_> events = {"100.0", "alive = 1", "call pays MAX(spot() - STRIKE, 0.0)"};
    const ScriptProduct_ product(dates, events);

    std::ostringstream out;
    product.DebugJson(out);
    ASSERT_EQ(out.str(),
              "{\"schema\":\"dal.script-product/1\",\"events\":["
              "{\"index\":0,\"date\":\"2023-09-25\",\"phase\":\"future\",\"statements\":["
              "{\"id\":\"n0\",\"kind\":\"assign\","
              "\"target\":{\"id\":\"n1\",\"kind\":\"var\",\"name\":\"alive\",\"index\":-1,\"const_value\":0},"
              "\"value\":{\"id\":\"n2\",\"kind\":\"const\",\"value\":1}"
              "}]},"
              "{\"index\":1,\"date\":\"2025-09-25\",\"phase\":\"future\",\"statements\":["
              "{\"id\":\"n3\",\"kind\":\"pays\","
              "\"target\":{\"id\":\"n4\",\"kind\":\"var\",\"name\":\"call\",\"index\":-1,\"const_value\":0},"
              "\"value\":{\"id\":\"n5\",\"kind\":\"max\",\"children\":["
              "{\"id\":\"n6\",\"kind\":\"sub\",\"children\":["
              "{\"id\":\"n7\",\"kind\":\"spot\"},"
              "{\"id\":\"n8\",\"kind\":\"const_var\",\"name\":\"STRIKE\",\"index\":-1,\"value\":100}"
              "]},"
              "{\"id\":\"n9\",\"kind\":\"const\",\"value\":0}"
              "]}"
              "}]}"
              "]}");
}

TEST(ScriptTest, TestDebuggerProductTree) {
    Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));
    const Vector_<Cell_> dates = {Cell_("STRIKE"), Cell_(Date_(2023, 9, 25)), Cell_(Date_(2025, 9, 25))};
    const Vector_<String_> events = {"100.0", "alive = 1", "call pays MAX(spot() - STRIKE, 0.0)"};
    const ScriptProduct_ product(dates, events);

    std::ostringstream out;
    product.DebugTree(out);
    ASSERT_EQ(out.str(),
              "📅 1 · 2023-09-25 · future\n"
              "└── (1) alive ← 1\n"
              "\n"
              "📅 2 · 2025-09-25 · future\n"
              "└── (1) call ⇐ max(spot() − STRIKE, 0)\n"
              "\n");
}

TEST(ScriptTest, TestDebuggerEmptyProductJsonAndTree) {
    Vector_<Cell_> dates;
    Vector_<String_> events;
    const ScriptProduct_ product(dates, events);

    std::ostringstream json;
    product.DebugJson(json);
    ASSERT_EQ(json.str(), "{\"schema\":\"dal.script-product/1\",\"events\":[]}");

    std::ostringstream tree;
    product.DebugTree(tree);
    ASSERT_EQ(tree.str(), "");
}
