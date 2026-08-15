//
// Created by wegam on 2023/8/20.
//

#include <gtest/gtest.h>

#include <map>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/parser.hpp>
#include <dal/script/visitor/all.hpp>
#include <dal/script/visitor/pastevaluator.hpp>

using namespace Dal;
using namespace Dal::Script;


TEST(ScriptTest, TestPastEvaluator) {
    auto const1 = MakeBaseNode<NodeConst_>(2.0);
    auto expExpr = MakeBaseNode<NodeSqrt_>();
    Vector_<Expression_> args;
    args.push_back(std::move(const1));
    expExpr->arguments_ = std::move(args);
    auto var = MakeBaseNode<NodeVar_>("x");
    auto assignExpr = MakeBinary<NodeAssign_>(var, expExpr);
    auto var2 = MakeBaseNode<NodeVar_>("y");
    var = MakeBaseNode<NodeVar_>("x");
    auto paysExpr = MakeBinary<NodePays_>(var2, var);

    VarIndexer_ visitor1;
    assignExpr->Accept(visitor1);
    paysExpr->Accept(visitor1);

    PastEvaluator_<double> visitor2(Vector_<>(visitor1.VarNames().size(), 0.0));
    assignExpr->Accept(visitor2);
    paysExpr->Accept(visitor2);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[0], 1.4142135623730951);
    ASSERT_DOUBLE_EQ(visitor2.VarVals()[1], 0.0);
}

namespace {
    // Parse + index a source snippet, then run the past evaluator over it.
    std::pair<Vector_<Statement_>, Vector_<double>> PastEvaluateSource(
        const String_& src,
        const std::map<String_, double>& constVariables = std::map<String_, double>()) {
        Parser_ parser(constVariables);
        auto statements = parser.Parse(src);

        VarIndexer_ indexer;
        for (auto& stat : statements)
            stat->Accept(indexer);

        PastEvaluator_<double> eval(Vector_<>(indexer.VarNames().size(), 0.0),
                                    indexer.ConstVarValues());
        for (auto& stat : statements)
            stat->Accept(eval);

        return {std::move(statements), eval.VarVals()};
    }
} // namespace

TEST(PastEvaluatorTest, TestSpotPushesPlaceholder) {
    // Known limitation locked in: past evaluation prices spot() as 30.0.
    const auto [statements, vals] = PastEvaluateSource("x = spot()");
    ASSERT_DOUBLE_EQ(vals[0], 30.0);
}

TEST(PastEvaluatorTest, TestPaysDoesNotAccumulate) {
    const auto [statements, vals] = PastEvaluateSource("call pays 5");
    ASSERT_DOUBLE_EQ(vals[0], 0.0);
}

TEST(PastEvaluatorTest, TestConstVariablesAreReadable) {
    const auto [statements, vals] = PastEvaluateSource("x = STRIKE + 1", {{"STRIKE", 11.0}});
    ASSERT_DOUBLE_EQ(vals[0], 12.0);
}

TEST(PastEvaluatorTest, TestInitRestoresInitialValues) {
    Parser_ parser;
    auto statements = parser.Parse("x = 99");

    VarIndexer_ indexer;
    for (auto& stat : statements)
        stat->Accept(indexer);

    PastEvaluator_<double> eval(Vector_<>(1, 5.0));
    for (auto& stat : statements)
        stat->Accept(eval);
    ASSERT_DOUBLE_EQ(eval.VarVals()[0], 99.0);

    eval.Init();
    ASSERT_DOUBLE_EQ(eval.VarVals()[0], 5.0);
}
